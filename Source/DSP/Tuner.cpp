#include "Tuner.h"
#include <cmath>
#include <chrono>

static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B" };

/**
 * @brief MIDI 음정 인덱스(0~11)를 음이름 문자열로 변환한다.
 */
const char* Tuner::getNoteName (int index)
{
    if (index < 0 || index > 11) return "?";
    return noteNames[index];
}

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief 생성자 — 백그라운드 YIN 분석 스레드를 시작한다.
 *
 * YIN 연산(O(N²))을 오디오 스레드 밖으로 분리하여 RT 안전성 보장.
 */
Tuner::Tuner()
{
    analysisThread = std::thread (&Tuner::analysisThreadFunc, this);
}

/**
 * @brief 소멸자 — 분석 스레드를 안전하게 종료한다.
 */
Tuner::~Tuner()
{
    threadShouldStop.store (true, std::memory_order_relaxed);
    if (analysisThread.joinable())
        analysisThread.join();
}

// ────────────────────────────────────────────────────────────────────────────
// DSP 인터페이스
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief 오디오 초기화: 샘플레이트 설정 및 내부 상태 클리어.
 * @note [메인 스레드] PluginProcessor::prepareToPlay()에서 호출.
 */
void Tuner::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
}

/**
 * @brief 내부 상태를 초기화한다.
 * @note [오디오 스레드] 재생 중지 시 호출될 수 있음.
 */
void Tuner::reset()
{
    inputBuffer.fill (0.0f);
    inputWritePos = 0;
    bufferFull    = false;
    detectedHz.store    (0.0f);
    centsDeviation.store(0.0f);
    noteIndex.store     (-1);
    noteDetected.store  (false);
}

/**
 * @brief APVTS 파라미터 포인터 등록.
 * @note [메인 스레드] SignalChain::connectParameters()에서 호출.
 */
void Tuner::setParameterPointers (std::atomic<float>* referenceA,
                                   std::atomic<float>* muteEnabled)
{
    referenceAParam = referenceA;
    muteParam       = muteEnabled;
}

/**
 * @brief 입력 오디오를 처리한다.
 *
 * 오디오 스레드 역할:
 *  1. 샘플을 링버퍼에 누적한다 (4096 samples ~93ms @ 44.1kHz).
 *  2. 버퍼가 가득 차면 pendingBuffer로 복사하고 플래그를 설정한다.
 *     → 실제 YIN 연산은 백그라운드 스레드에서 실행 (RT 안전성 보장).
 *  3. Mute 모드이면 buffer.clear()로 무음 출력.
 *
 * @note [오디오 스레드] new/delete/mutex/파일 I/O 절대 금지.
 */
void Tuner::process (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 1)
        return;

    const int numSamples  = buffer.getNumSamples();
    const float* readPtr  = buffer.getReadPointer (0);

    // 샘플을 내부 링버퍼에 누적
    for (int i = 0; i < numSamples; ++i)
    {
        inputBuffer[(size_t) inputWritePos] = readPtr[i];
        if (++inputWritePos >= kYinBufferSize)
        {
            inputWritePos = 0;
            bufferFull    = true;
        }
    }

    // 버퍼가 가득 찼으면 분석 스레드로 전달
    if (bufferFull)
    {
        bufferFull = false;

        // lock-free 단일 생산자/소비자 패턴:
        // pendingReady가 false일 때만 덮어쓴다 (이전 분석이 끝난 경우).
        // 분석 스레드가 아직 처리 중이면 이번 프레임을 스킵 — 피치는 천천히 변하므로 무방.
        if (!pendingReady.load (std::memory_order_relaxed))
        {
            std::copy (inputBuffer.begin(), inputBuffer.end(), pendingBuffer.begin());
            // release 순서 보장: pendingBuffer 쓰기가 이 store 이전에 완료됨
            pendingReady.store (true, std::memory_order_release);
        }
    }

    // Mute 모드: 튜닝 중 신호를 소거
    if (muteParam != nullptr && muteParam->load() >= 0.5f)
        buffer.clear();
}

// ────────────────────────────────────────────────────────────────────────────
// 백그라운드 분석 스레드
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief YIN 분석 백그라운드 스레드 함수.
 *
 * pendingReady 플래그를 5ms 간격으로 폴링하여 새 데이터가 도착하면
 * detectPitch()를 실행한다. sleep_for(5ms)는 UI 갱신(30Hz=33ms)보다
 * 충분히 짧아 사용자가 지연을 느끼지 못한다.
 *
 * @note [백그라운드 스레드] 오디오 스레드와 독립적으로 실행.
 */
void Tuner::analysisThreadFunc()
{
    while (!threadShouldStop.load (std::memory_order_relaxed))
    {
        // acquire: pendingBuffer 쓰기가 완료된 후에 읽기 시작
        if (pendingReady.load (std::memory_order_acquire))
        {
            pendingReady.store (false, std::memory_order_relaxed);
            if (sampleRate > 0.0)
                detectPitch();
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// YIN 피치 감지
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief YIN 알고리즘으로 피치를 감지한다 (pendingBuffer 사용).
 *
 * 지원 악기:
 *  - 4현 베이스: E1(41Hz) ~ G4(392Hz)
 *  - 5현 베이스: B0(31Hz) ~ C5(523Hz)
 *  - 6현 베이스: B0(31Hz) ~ C5(523Hz)
 *
 * 감지 범위: 28Hz(하한 여유) ~ 600Hz(상한 여유)
 *
 * YIN 4-단계:
 * 1. Difference Function: d(τ) = Σ(x[j] - x[j+τ])²
 * 2. CMND: d'(τ) = d(τ) / ((1/τ) Σ d(j))  — 정규화로 크기 변화에 강건
 * 3. Absolute Threshold + 포물선 보간 — τ 범위를 베이스 음역으로 제한
 * 4. Hz = sampleRate / τ → MIDI 노트 + 센트 편차
 *
 * @note [백그라운드 스레드] analysisThreadFunc()에서 호출.
 */
void Tuner::detectPitch()
{
    const int halfSize = kYinBufferSize / 2;  // 2048

    // 감지 범위 (샘플 단위 라그):
    //   tauMin = sr/600Hz ≈ 73  (6현 고음 C5 상한)
    //   tauMax = sr/28Hz  ≈ 1575 (5·6현 B0 하한)
    const int tauMin = std::max (2, static_cast<int> (sampleRate / 600.0));
    const int tauMax = std::min (halfSize - 1, static_cast<int> (sampleRate / 28.0));

    // --- Step 1: Difference Function ---
    // τ값이 필요한 범위(1..tauMax)에 대해서만 계산하여 연산량 감소
    yinBuffer[0] = 1.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfSize; ++j)
        {
            float diff = pendingBuffer[(size_t) j] - pendingBuffer[(size_t) (j + tau)];
            sum += diff * diff;
        }
        yinBuffer[(size_t) tau] = sum;
    }

    // --- Step 2: Cumulative Mean Normalized Difference (CMND) ---
    float runningSum = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        runningSum += yinBuffer[(size_t) tau];
        if (runningSum > 0.0f)
            yinBuffer[(size_t) tau] *= static_cast<float> (tau) / runningSum;
        else
            yinBuffer[(size_t) tau] = 1.0f;
    }

    // --- Step 3: Absolute Threshold + 포물선 보간 ---
    int bestTau = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (yinBuffer[(size_t) tau] < kYinThreshold)
        {
            // 임계값 이하 딥의 최솟값으로 이동
            while (tau + 1 <= tauMax && yinBuffer[(size_t) (tau + 1)] < yinBuffer[(size_t) tau])
                ++tau;
            bestTau = tau;
            break;
        }
    }

    if (bestTau < 0)
    {
        noteDetected.store (false);
        return;
    }

    // --- Step 4: 포물선 보간으로 서브샘플 정밀도 향상 ---
    float betterTau = static_cast<float> (bestTau);
    if (bestTau > 0 && bestTau < tauMax)
    {
        float s0 = yinBuffer[(size_t) (bestTau - 1)];
        float s1 = yinBuffer[(size_t)  bestTau];
        float s2 = yinBuffer[(size_t) (bestTau + 1)];
        float denom = 2.0f * s1 - s0 - s2;
        if (std::abs (denom) > 1e-12f)
            betterTau = static_cast<float> (bestTau) + (s2 - s0) / (2.0f * denom);
    }

    float hz = static_cast<float> (sampleRate) / betterTau;

    // 범위 재검증 (보간 후 벗어날 수 있음)
    if (hz < 28.0f || hz > 600.0f)
    {
        noteDetected.store (false);
        return;
    }

    detectedHz.store  (hz);
    noteDetected.store(true);

    float refA = (referenceAParam != nullptr) ? referenceAParam->load() : 440.0f;
    hzToNoteAndCents (hz, refA);
}

/**
 * @brief 주파수(Hz)를 MIDI 노트 + 센트 편차로 변환한다.
 *
 * MIDI Note = 69 + 12 × log₂(f / referenceA)
 * 센트 편차 = (midiNote - round(midiNote)) × 100
 */
void Tuner::hzToNoteAndCents (float hz, float referenceA)
{
    float semitonesFromA4 = 12.0f * std::log2 (hz / referenceA);
    float midiNote        = 69.0f + semitonesFromA4;

    int   nearestMidi = static_cast<int> (std::round (midiNote));
    float cents       = (midiNote - static_cast<float> (nearestMidi)) * 100.0f;

    int note = nearestMidi % 12;
    if (note < 0) note += 12;

    noteIndex.store      (note);
    centsDeviation.store (cents);
}
