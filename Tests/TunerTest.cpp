#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Tuner.h"
#include <cmath>
#include <string>
#include <thread>
#include <chrono>

//==============================================================================
namespace {

/**
 * @brief 위상 연속성을 유지하는 사인파 생성 (다중 블록)
 *
 * @param buffer      채울 오디오 버퍼
 * @param freqHz      파형 주파수 (Hz)
 * @param sampleRate  샘플 레이트 (Hz)
 * @param amplitude   진폭 (0~1 권장)
 * @param phaseState  현재 위상(라디안), 참조로 다음 블록 위상 누적
 *
 * **중요**: YIN 알고리즘은 신호의 시간적 연속성을 분석하므로
 * 매 블록마다 위상 0부터 시작하면 블록 경계 클릭 발생 → 피치 감지 실패.
 * phaseState로 위상 누적하여 연속 사인파 구현.
 *
 * @note [테스트 유틸] 실제 오디오 신호 처리에서는 호스트의 오디오 버스에서 버퍼를 받음.
 */
void fillSineContinuous (juce::AudioBuffer<float>& buffer, float freqHz,
                         double sampleRate, float amplitude, double& phaseState)
{
    const int n = buffer.getNumSamples();
    const double omega = 2.0 * juce::MathConstants<double>::pi
                        * static_cast<double> (freqHz) / sampleRate;
    for (int i = 0; i < n; ++i)
    {
        buffer.setSample (0, i, amplitude * std::sin (static_cast<float> (phaseState)));
        phaseState += omega;
        if (phaseState > 2.0 * juce::MathConstants<double>::pi)
            phaseState -= 2.0 * juce::MathConstants<double>::pi;
    }
}

/**
 * @brief 튜너의 주파수 감지 기능 테스트
 *
 * @param hz          테스트할 목표 주파수 (Hz)
 * @param sampleRate  샘플 레이트 (Hz)
 * @param blockSize   오디오 버퍼 크기 (기본 1024 샘플)
 * @return            검출된 주파수 (Hz), 실패 시 0
 *
 * **과정**:
 * 1. 주어진 주파수의 위상 연속 사인파 생성
 * 2. 튜너에 8192 샘플 이상 공급 (링버퍼 채우기)
 * 3. YIN 백그라운드 분석 대기 (~50회 × 20ms = 1초)
 * 4. detectPitch() 완료 후 Hz 반환
 *
 * @note YIN은 O(N²) 연산 (4096 샘플 = ~1600만 ops)
 *       Debug 빌드에서 수백 ms 소요 → 충분한 대기 필수.
 */
float detectFrequency (float hz, double sampleRate, int blockSize = 1024)
{
    Tuner tuner;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    tuner.prepare (spec);

    std::atomic<float> refA     { 440.0f };
    std::atomic<float> muteFlag { 0.0f };
    tuner.setParameterPointers (&refA, &muteFlag);

    juce::AudioBuffer<float> buffer (1, blockSize);

    // 4096 샘플 링버퍼를 확실히 채우기 위해 넉넉히 반복 (8192 샘플 이상 공급)
    double phase = 0.0;
    const int totalBlocks = (int) std::ceil (8192.0 / blockSize) + 2;
    for (int rep = 0; rep < totalBlocks; ++rep)
    {
        fillSineContinuous (buffer, hz, sampleRate, 0.4f, phase);
        tuner.process (buffer);
    }

    // 백그라운드 스레드가 detectPitch()를 완료할 시간을 넉넉히 확보
    // (debug 빌드 YIN 4096-샘플 O(N²) ≈ 수십-수백 ms)
    for (int waitIter = 0; waitIter < 50; ++waitIter)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
        if (tuner.isNoteDetected() && tuner.getDetectedHz() > 0.0f)
            break;
    }

    return tuner.getDetectedHz();
}

/**
 * @brief 튜너의 노트 이름 인덱스 감지 기능 테스트
 *
 * @param hz          테스트할 목표 주파수 (Hz)
 * @param sampleRate  샘플 레이트 (Hz)
 * @param blockSize   오디오 버퍼 크기 (기본 1024 샘플)
 * @return            노트 인덱스 (0~11, C=0, D=2, E=4, G=7, A=9 등)
 *
 * detectFrequency()와 동일 로직이나 Hz 대신 노트 인덱스 반환.
 * 베이스 현(E, A, D, G) 감지 검증용.
 */
int detectNoteIndex (float hz, double sampleRate, int blockSize = 1024)
{
    Tuner tuner;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    tuner.prepare (spec);

    std::atomic<float> refA     { 440.0f };
    std::atomic<float> muteFlag { 0.0f };
    tuner.setParameterPointers (&refA, &muteFlag);

    juce::AudioBuffer<float> buffer (1, blockSize);
    double phase = 0.0;
    const int totalBlocks = (int) std::ceil (8192.0 / blockSize) + 2;
    for (int rep = 0; rep < totalBlocks; ++rep)
    {
        fillSineContinuous (buffer, hz, sampleRate, 0.4f, phase);
        tuner.process (buffer);
    }

    for (int waitIter = 0; waitIter < 50; ++waitIter)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
        if (tuner.isNoteDetected())
            break;
    }

    return tuner.getNoteIndex();
}

} // namespace

/**
 * @brief Tuner: E1(41.2Hz) 저역 피치 감지 검증
 *
 * **테스트 목표**:
 * - 베이스 4현 중 가장 낮은 E 현(E1, 41.2Hz) 감지
 * - 저역 피치는 주파수 해상도 제한으로 오차 ±30 센트 허용
 * - 센트: 음정 미세 편차 (1 세미톤 = 100 센트)
 */
TEST_CASE ("Tuner: detects E1 (41.2 Hz) within acceptable tolerance",
           "[tuner][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    const float detected = detectFrequency (41.2f, sampleRate);

    REQUIRE (detected > 0.0f);
    // 감지된 Hz를 센트로 변환: 1200 * log2(f_detected / f_target)
    const float cents = 1200.0f * std::log2 (detected / 41.2f);
    // 저역 피치는 주파수 해상도가 제한적이므로 ±30 cents 허용 (±0.3 세미톤)
    REQUIRE (std::abs (cents) < 30.0f);
}

/**
 * @brief Tuner: A2(110Hz) 중저역 피치 감지 검증
 *
 * **테스트 목표**:
 * - 베이스 2현 A 음(A2, 110Hz) 감지
 * - 중저역 피치는 해상도가 더 좋으므로 ±15 센트 허용
 * - 가청 음악 범위의 정확도 확인
 */
TEST_CASE ("Tuner: detects A2 (110 Hz) within acceptable tolerance",
           "[tuner][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    const float detected = detectFrequency (110.0f, sampleRate);

    REQUIRE (detected > 0.0f);
    const float cents = 1200.0f * std::log2 (detected / 110.0f);
    // 중저역: ±15 cents 허용 (±0.15 세미톤, E1보다 엄격)
    REQUIRE (std::abs (cents) < 15.0f);
}

/**
 * @brief Tuner: Hz → 노트 이름 변환 검증 (E/A/D/G)
 *
 * **테스트 목표**:
 * - 베이스 4현(E, A, D, G) 주파수를 정확한 노트 이름으로 변환
 * - 12음 음악 체계에서의 올바른 인덱싱
 *
 * **베이스 현 주파수**:
 * - E1: 41.2 Hz
 * - A1: 55.0 Hz
 * - D2: 73.4 Hz
 * - G2: 98.0 Hz
 */
TEST_CASE ("Tuner: Hz-to-note name conversion covers E/A/D/G",
           "[tuner][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;

    struct Case { float hz; const char* expected; };
    const Case cases[] = {
        { 41.2f, "E" },   // E1 (4현)
        { 55.0f, "A" },   // A1 (3현)
        { 73.4f, "D" },   // D2 (2현)
        { 98.0f, "G" },   // G2 (1현)
    };

    for (const auto& c : cases)
    {
        const int idx = detectNoteIndex (c.hz, sampleRate);
        REQUIRE (idx >= 0);
        REQUIRE (idx < 12);  // 12음 체계 (C=0, C#=1, ... B=11)
        // 인덱스로부터 노트 이름 획득 및 검증
        REQUIRE (std::string (Tuner::getNoteName (idx)) == std::string (c.expected));
    }
}

/**
 * @brief Tuner: 무음 입력 처리 안정성 검증
 *
 * **테스트 목표**:
 * - 무음(0) 입력 시 크래시 없음
 * - NaN/Inf 없음
 * - 피치 미감지 (noteDetected == false)
 *
 * **YIN 알고리즘 특성**:
 * 무음에서는 주기성 강도가 약하므로 피치 추정 실패 → 피치 미감지
 */
TEST_CASE ("Tuner: silence input does not crash",
           "[tuner][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 1024;

    Tuner tuner;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    tuner.prepare (spec);

    std::atomic<float> refA     { 440.0f };
    std::atomic<float> muteFlag { 0.0f };
    tuner.setParameterPointers (&refA, &muteFlag);

    juce::AudioBuffer<float> buffer (1, blockSize);
    buffer.clear();

    // 무음을 여러 번 처리해도 크래시/NaN 없어야 함
    for (int rep = 0; rep < 10; ++rep)
    {
        tuner.process (buffer);
        // 백그라운드 스레드 동작 대기 (YIN 분석)
        std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }

    // 버퍼 모든 샘플이 유한값 확인
    const float* d = buffer.getReadPointer (0);
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (std::isfinite (d[i]));

    // 무음에서는 피치 미감지
    REQUIRE_FALSE (tuner.isNoteDetected());
}
