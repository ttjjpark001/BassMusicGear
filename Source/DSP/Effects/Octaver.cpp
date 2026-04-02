#include "Octaver.h"

Octaver::Octaver() = default;

void Octaver::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // --- YIN 차분 함수 버퍼 할당 ---
    // d[tau] 계산 및 정규화 결과 저장 (yinBufferSize의 절반만 사용)
    yinBuffer.resize (static_cast<size_t> (yinBufferSize / 2), 0.0f);

    // --- 입력 신호 링 버퍼 (YIN 분석 누적용) ---
    // 리얼타임 샘플 스트림을 순환 저장하여 충분한 데이터 확보 후 YIN 실행
    inputRingBuffer.resize (static_cast<size_t> (yinBufferSize), 0.0f);
    // YIN 분석용 컨티그 버퍼 (링 버퍼를 일렬로 펼쳐 전달)
    contiguousBuffer.resize (static_cast<size_t> (yinBufferSize), 0.0f);
    ringWritePos = 0;
    ringSamplesAccumulated = 0;

    // --- 위상 누적기 초기화 (사인파 생성용) ---
    subPhase = 0.0;
    upPhase  = 0.0;

    currentFrequency = 0.0f;
    envelopeLevel = 0.0f;

    // --- 엔벨로프 팔로워 계수 계산 ---
    // Attack: 어택 시간(5ms) 시간상수로 빠른 응답
    // Release: 릴리즈 시간(50ms) 시간상수로 부드러운 감쇠
    // 공식: coeff = 1 - exp(-1 / (SR * time_ms / 1000))
    const float attackMs  = 5.0f;
    const float releaseMs = 50.0f;
    envelopeAttack  = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * attackMs  / 1000.0f));
    envelopeRelease = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * releaseMs / 1000.0f));
}

void Octaver::reset()
{
    subPhase = 0.0;
    upPhase  = 0.0;
    currentFrequency = 0.0f;
    envelopeLevel = 0.0f;
    ringWritePos = 0;
    ringSamplesAccumulated = 0;

    std::fill (inputRingBuffer.begin(), inputRingBuffer.end(), 0.0f);
}

void Octaver::setParameterPointers (std::atomic<float>* enabled,
                                     std::atomic<float>* subLevel,
                                     std::atomic<float>* upLevel,
                                     std::atomic<float>* dryLevel)
{
    enabledParam  = enabled;
    subLevelParam = subLevel;
    upLevelParam  = upLevel;
    dryLevelParam = dryLevel;
}

//==============================================================================
// YIN 피치 감지 알고리즘
//==============================================================================
float Octaver::detectPitch (const float* data, int numSamples)
{
    // **YIN 알고리즘**: 자기상관 기반 피치 추정
    // 부음역에 강건하므로 베이스 음역(41Hz~330Hz)에 최적화
    // 참고: de Cheveigné & Kawahara (2002) "YIN, a fundamental frequency estimator for speech and music"

    const int halfN = numSamples / 2;
    if (halfN < 2) return 0.0f;

    auto& d = yinBuffer;
    if (static_cast<int> (d.size()) < halfN)
        return 0.0f; // 버퍼 크기 체크

    // --- 단계 1: 차분 함수 계산 ---
    // d[tau] = Σ(x[n] - x[n+tau])² for n=0~halfN-1
    // tau가 음의 주기를 나타낼 때 차분이 작아짐 (피치 주기성 반영)
    d[0] = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfN; ++j)
        {
            float diff = data[j] - data[j + tau];
            sum += diff * diff;
        }
        d[static_cast<size_t> (tau)] = sum;
    }

    // --- 단계 2: CMND (누적 정규화 차분) 계산 ---
    // d'[tau] = d[tau] * tau / Σd[1..tau]
    // 정규화로 에너지에 무관한 신뢰도 계산 (큰 에너지 신호도 작은 신호도 비교 가능)
    d[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
    {
        runningSum += d[static_cast<size_t> (tau)];
        if (runningSum > 0.0f)
            d[static_cast<size_t> (tau)] = d[static_cast<size_t> (tau)] * static_cast<float> (tau) / runningSum;
        else
            d[static_cast<size_t> (tau)] = 1.0f;
    }

    // --- 단계 3: 절대 임계값 및 주기 추정 ---
    // 임계값 0.15 이하인 첫 번째 tau를 찾아 주기로 선택
    // 이후 local minimum까지 탐색해 정확도 개선
    constexpr float threshold = 0.15f;
    int tauEstimate = -1;

    // 베이스 음역 제약: F0 ∈ [41Hz, 330Hz]
    // 최소 tau: F0_max=330Hz → tau_min = SR/330
    // 최대 tau: F0_min=41Hz → tau_max = SR/41
    const int minTau = static_cast<int> (currentSampleRate / 330.0);
    const int maxTau = std::min (halfN - 1, static_cast<int> (currentSampleRate / 41.0));

    for (int tau = minTau; tau <= maxTau; ++tau)
    {
        if (d[static_cast<size_t> (tau)] < threshold)
        {
            // 임계값 아래로 내려간 후, 그 부근의 local minimum 찾기
            while (tau + 1 <= maxTau &&
                   d[static_cast<size_t> (tau + 1)] < d[static_cast<size_t> (tau)])
                ++tau;
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate < 1)
        return 0.0f;  // 피치 감지 실패

    // --- 단계 4: 포물선 보간으로 서브샘플 정확도 확보 ---
    // 3점을 통과하는 포물선의 최솟값을 구해 tau를 정제
    float betterTau = static_cast<float> (tauEstimate);
    if (tauEstimate > 0 && tauEstimate < halfN - 1)
    {
        float s0 = d[static_cast<size_t> (tauEstimate - 1)];
        float s1 = d[static_cast<size_t> (tauEstimate)];
        float s2 = d[static_cast<size_t> (tauEstimate + 1)];

        // 포물선 y = a*x² + b*x + c의 최솟값 x 위치 계산
        // betterTau = tauEstimate + (s0 - s2) / (2*s1 - s0 - s2) / 2
        float denom = 2.0f * (2.0f * s1 - s0 - s2);
        if (std::abs (denom) > 1e-10f)
            betterTau = static_cast<float> (tauEstimate) + (s0 - s2) / denom;
    }

    // 추정 주기(tau)를 주파수(Hz)로 변환: F0 = SR / tau
    return static_cast<float> (currentSampleRate) / betterTau;
}

//==============================================================================
// 프로세스 (오디오 스레드)
//==============================================================================
void Octaver::process (juce::AudioBuffer<float>& buffer)
{
    // --- ON/OFF 체크 ---
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    // --- 파라미터 로드 (atomic, 락프리) ---
    const float subLevel = subLevelParam != nullptr ? subLevelParam->load() : 0.0f;
    const float upLevel  = upLevelParam  != nullptr ? upLevelParam->load()  : 0.0f;
    const float dryLevel = dryLevelParam != nullptr ? dryLevelParam->load() : 1.0f;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);

    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // --- 입력 신호를 링 버퍼에 축적 (YIN 분석용) ---
        inputRingBuffer[static_cast<size_t> (ringWritePos)] = inputSample;
        ringWritePos = (ringWritePos + 1) % yinBufferSize;
        ringSamplesAccumulated++;

        // --- YIN 실행 (hop size = yinBufferSize/4 = 512 샘플) ---
        // 매 512 샘플마다 한 번 피치 감지 실행 (계산 비용 절감, 부드러운 추적)
        if (ringSamplesAccumulated >= yinBufferSize / 4)
        {
            ringSamplesAccumulated = 0;

            // 링 버퍼를 일렬로 펼침 (YIN은 연속 메모리 필요)
            // contiguousBuffer는 prepareToPlay에서 할당했으므로 매 호출마다 재할당 없음
            for (int j = 0; j < yinBufferSize; ++j)
            {
                int idx = (ringWritePos + j) % yinBufferSize;
                contiguousBuffer[static_cast<size_t> (j)] = inputRingBuffer[static_cast<size_t> (idx)];
            }

            float detected = detectPitch (contiguousBuffer.data(), yinBufferSize);

            // --- 주파수 스무딩: 지터 제거 ---
            // 매 hop마다 갑자기 주파수가 튀지 않도록 이전값과 선형 보간
            if (detected > 0.0f)
            {
                if (currentFrequency > 0.0f)
                    // 스무딩: new_F = 0.7 * old_F + 0.3 * detected_F
                    currentFrequency = currentFrequency * 0.7f + detected * 0.3f;
                else
                    currentFrequency = detected;  // 첫 감지
            }
        }

        // --- 엔벨로프 팔로워: 입력 신호 진폭 추적 ---
        // Attack(5ms)과 Release(50ms) 시간상수로 부드러운 추적
        const float absInput = std::abs (inputSample);
        if (absInput > envelopeLevel)
            // Attack: 빠르게 상승
            envelopeLevel += envelopeAttack * (absInput - envelopeLevel);
        else
            // Release: 천천히 감소 (끌어당김 효과)
            envelopeLevel += envelopeRelease * (absInput - envelopeLevel);

        // --- 합성 사인파: 서브옥타브(F0/2) + 옥타브업(F0*2) ---
        float subSample = 0.0f;
        float upSample  = 0.0f;

        if (currentFrequency > 0.0f && envelopeLevel > 0.001f)
        {
            // 서브옥타브: F0/2 사인파 생성
            // 위상: phase += freq / SR (정규화)
            const double subFreq = static_cast<double> (currentFrequency) * 0.5;
            subPhase += subFreq / currentSampleRate;
            if (subPhase >= 1.0) subPhase -= 1.0;  // wrap-around: [0~1)
            subSample = static_cast<float> (std::sin (twoPi * subPhase)) * envelopeLevel;

            // 옥타브업: F0*2 사인파 생성
            const double upFreq = static_cast<double> (currentFrequency) * 2.0;
            upPhase += upFreq / currentSampleRate;
            if (upPhase >= 1.0) upPhase -= 1.0;
            upSample = static_cast<float> (std::sin (twoPi * upPhase)) * envelopeLevel;
        }

        // --- 최종 혼합: 드라이(원본) + 서브옥타브 + 옥타브업 ---
        data[i] = dryLevel * inputSample
                + subLevel * subSample
                + upLevel  * upSample;
    }
}
