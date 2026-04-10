#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <atomic>

//==============================================================================
// Phase 8 — Active/Passive 입력 패드 단위 테스트
//
// **설계 배경**:
// Active/Passive 패드 로직은 PluginProcessor::processBlock() 최상단에서
// atomic load + buffer.applyGain() 으로만 구성된다. PluginProcessor 전체를
// 테스트에 링크하면 BinaryData, JUCE 이벤트 루프 의존성이 생기므로,
// 동일한 로직(게인 적용 + 수치 안정성)을 AudioBuffer 레벨에서 직접 검증한다.
//
// **검증 항목**:
// 1. Passive(false): 버퍼에 게인 미적용 → 입출력 동일
// 2. Active(true): 버퍼에 -10dB(0.3162) 게인 적용 → RMS 비율 검증
// 3. Active vs Passive RMS 비율: -10dB ±0.5dB
// 4. NaN/Inf 없음 (0dBFS 입력)
//==============================================================================

namespace {

/** 상수: Active 패드 게인 (-10dB) — PluginProcessor와 동일 값 */
static constexpr float kActivePadGain = 0.3162277660f;

/**
 * @brief 테스트용 정현파 버퍼 채우기
 *
 * @param buffer      채울 오디오 버퍼 (채널 0)
 * @param freqHz      파형 주파수 (Hz)
 * @param sampleRate  샘플 레이트 (Hz)
 * @param amplitude   진폭 (0~1)
 */
void fillSine (juce::AudioBuffer<float>& buffer, float freqHz,
               double sampleRate, float amplitude)
{
    const int n = buffer.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < n; ++i)
        buffer.setSample (0, i, amplitude * std::sin (omega * static_cast<float> (i)));
}

/**
 * @brief 버퍼의 RMS 레벨 측정 (채널 0)
 *
 * @param buffer  오디오 버퍼
 * @return        RMS 값
 */
float rmsOf (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const float* d = buffer.getReadPointer (0);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += d[i] * d[i];
    return std::sqrt (sum / static_cast<float> (n));
}

/**
 * @brief 버퍼에 NaN 또는 Inf 포함 여부 확인 (채널 0)
 *
 * @param buffer  오디오 버퍼
 * @return        NaN/Inf 발견 시 true
 */
bool hasNanOrInf (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const float* d = buffer.getReadPointer (0);
    for (int i = 0; i < n; ++i)
        if (!std::isfinite (d[i]))
            return true;
    return false;
}

/**
 * @brief PluginProcessor::processBlock() Active 패드 로직 재현
 *
 * @param buffer      처리할 오디오 버퍼
 * @param inputActive Active(true) / Passive(false)
 *
 * PluginProcessor와 동일한 조건 분기 + applyGain 사용.
 */
void applyActivePad (juce::AudioBuffer<float>& buffer, bool inputActive)
{
    if (inputActive)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGain (ch, 0, buffer.getNumSamples(), kActivePadGain);
    }
    // Passive: 아무 처리도 하지 않음
}

} // namespace

//==============================================================================

/**
 * @brief InputPad: Passive 모드에서 입출력 동일 (bypass)
 *
 * inputActive = false(Passive) 일 때 게인 적용 없음.
 * 입력 신호가 그대로 통과해야 한다.
 */
TEST_CASE ("InputPad: Passive mode passes signal unchanged", "[inputpad][phase8]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 1024;

    juce::AudioBuffer<float> buffer (1, blockSize);
    fillSine (buffer, 100.0f, sampleRate, 0.8f);

    // 원본 백업
    juce::AudioBuffer<float> orig (1, blockSize);
    orig.copyFrom (0, 0, buffer, 0, 0, blockSize);

    // Passive 모드 적용 (아무것도 하지 않아야 함)
    applyActivePad (buffer, false);

    // 모든 샘플이 동일해야 함
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (buffer.getSample (0, i) == Catch::Approx (orig.getSample (0, i)).margin (1e-7f));

    REQUIRE_FALSE (hasNanOrInf (buffer));
}

/**
 * @brief InputPad: Active 모드에서 -10dB 게인 감쇄 적용
 *
 * inputActive = true 일 때 버퍼 전체에 0.3162f(-10dB) 곱셈.
 * 출력 RMS가 입력 RMS × 0.3162 와 일치해야 한다.
 */
TEST_CASE ("InputPad: Active mode applies -10 dB attenuation", "[inputpad][phase8]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 1024;

    juce::AudioBuffer<float> buffer (1, blockSize);
    fillSine (buffer, 100.0f, sampleRate, 0.8f);

    const float rmsIn = rmsOf (buffer);

    // Active 모드 적용
    applyActivePad (buffer, true);

    const float rmsOut = rmsOf (buffer);

    // 예상 RMS = rmsIn × 0.3162 (−10dB)
    const float expected = rmsIn * kActivePadGain;
    REQUIRE (rmsOut == Catch::Approx (expected).margin (1e-5f));
    REQUIRE_FALSE (hasNanOrInf (buffer));
}

/**
 * @brief InputPad: Active vs Passive RMS 비율 -10dB ±0.5dB 검증
 *
 * 동일한 입력 신호를 Active / Passive 각각 처리한 후 RMS 비율을 dB로 계산.
 * PLAN.md Phase 8 테스트 기준: Active RMS / Passive RMS = -10dB ±0.5dB
 */
TEST_CASE ("InputPad: Active-vs-Passive RMS ratio is -10 dB (+-0.5 dB)", "[inputpad][phase8]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 2048;

    // Passive 버퍼
    juce::AudioBuffer<float> passive (1, blockSize);
    fillSine (passive, 100.0f, sampleRate, 1.0f);  // 0dBFS 입력
    applyActivePad (passive, false);
    const float rmsPassive = rmsOf (passive);

    // Active 버퍼 (동일 입력)
    juce::AudioBuffer<float> active (1, blockSize);
    fillSine (active, 100.0f, sampleRate, 1.0f);   // 0dBFS 입력
    applyActivePad (active, true);
    const float rmsActive = rmsOf (active);

    // RMS 비율을 dB로 변환
    REQUIRE (rmsPassive > 0.0f);
    const float ratiodB = 20.0f * std::log10 (rmsActive / rmsPassive);

    // -10dB ±0.5dB 범위 내
    REQUIRE (ratiodB == Catch::Approx (-10.0f).margin (0.5f));
    REQUIRE_FALSE (hasNanOrInf (active));
    REQUIRE_FALSE (hasNanOrInf (passive));
}

/**
 * @brief InputPad: 0dBFS 풀 스케일 입력 → NaN/Inf 없음
 *
 * 가장 큰 신호(진폭 1.0, 0dBFS)에서도 Active 패드 적용 후
 * 수치 오류가 없는지 확인.
 */
TEST_CASE ("InputPad: 0 dBFS input with Active pad produces no NaN/Inf", "[inputpad][phase8]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    juce::AudioBuffer<float> buffer (1, blockSize);
    fillSine (buffer, 100.0f, sampleRate, 1.0f);  // 0dBFS

    applyActivePad (buffer, true);

    REQUIRE_FALSE (hasNanOrInf (buffer));

    // Active 후 최대 절대값이 0.3162 이하 (게인 적용 결과)
    const float* d = buffer.getReadPointer (0);
    float peak = 0.0f;
    for (int i = 0; i < blockSize; ++i)
        peak = std::max (peak, std::abs (d[i]));

    // 입력 최대진폭 1.0 × 0.3162 = 0.3162 (오차 허용 1e-5)
    REQUIRE (peak == Catch::Approx (kActivePadGain).margin (1e-5f));
}

/**
 * @brief InputPad: Active/Passive 전환 일관성 (멀티 블록)
 *
 * 여러 블록에 걸쳐 Active/Passive 전환 시 각 블록 독립 처리 확인.
 * 블록 간 상태 누수가 없어야 한다.
 */
TEST_CASE ("InputPad: repeated Active/Passive toggling is consistent", "[inputpad][phase8]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    constexpr int numBlocks = 10;

    for (int rep = 0; rep < numBlocks; ++rep)
    {
        juce::AudioBuffer<float> buffer (1, blockSize);
        fillSine (buffer, 100.0f, sampleRate, 0.7f);

        const float rmsIn = rmsOf (buffer);

        // 짝수 블록: Passive, 홀수 블록: Active
        const bool isActive = (rep % 2 == 1);
        applyActivePad (buffer, isActive);

        const float rmsOut = rmsOf (buffer);
        REQUIRE_FALSE (hasNanOrInf (buffer));

        if (!isActive)
        {
            // Passive: RMS 변화 없음
            REQUIRE (rmsOut == Catch::Approx (rmsIn).margin (1e-5f));
        }
        else
        {
            // Active: RMS가 0.3162 비율로 감소
            REQUIRE (rmsOut == Catch::Approx (rmsIn * kActivePadGain).margin (1e-5f));
        }
    }
}
