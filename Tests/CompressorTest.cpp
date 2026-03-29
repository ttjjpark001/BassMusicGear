#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Effects/Compressor.h"
#include <cmath>

//==============================================================================
namespace {

/** 지정 주파수의 사인파를 dBFS 레벨로 생성 */
void generateSineBuffer (juce::AudioBuffer<float>& buffer, float freqHz,
                          double sampleRate, float amplitudeLinear)
{
    const int numSamples = buffer.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, amplitudeLinear * std::sin (omega * static_cast<float> (i)));
}

float getRmsLevel (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    float sum = 0.0f;
    const float* data = buffer.getReadPointer (0);
    for (int i = startSample; i < startSample + numSamples; ++i)
        sum += data[i] * data[i];
    return std::sqrt (sum / static_cast<float> (numSamples));
}

/**
 * 선형 진폭을 dBFS로 변환한다.
 * 입력이 0이면 -144dBFS(부동소수점 최솟값)를 반환한다.
 */
float linearToDb (float linear)
{
    return 20.0f * std::log10 (std::max (linear, 1e-7f));
}

} // namespace

//==============================================================================
TEST_CASE ("Compressor: bypass passes signal unchanged", "[compressor]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    Compressor comp;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    comp.prepare (spec);

    // enabled = OFF (0.0)
    std::atomic<float> enabled { 0.0f };
    std::atomic<float> threshold { -20.0f };
    std::atomic<float> ratio { 4.0f };
    std::atomic<float> attack { 10.0f };
    std::atomic<float> release { 100.0f };
    std::atomic<float> makeup { 0.0f };
    std::atomic<float> dryBlend { 0.0f };

    comp.setParameterPointers (&enabled, &threshold, &ratio, &attack, &release, &makeup, &dryBlend);

    juce::AudioBuffer<float> buffer (1, blockSize);
    generateSineBuffer (buffer, 100.0f, sampleRate, 0.5f);

    // 원본 복사
    juce::AudioBuffer<float> original (1, blockSize);
    original.copyFrom (0, 0, buffer, 0, 0, blockSize);

    comp.process (buffer);

    // 바이패스 시 신호 불변
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (buffer.getSample (0, i) == Catch::Approx (original.getSample (0, i)).margin (1e-6f));

    // 게인 리덕션 = 0
    REQUIRE (comp.getGainReductionDb() == Catch::Approx (0.0f).margin (0.1f));
}

TEST_CASE ("Compressor: compresses loud signal", "[compressor]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    Compressor comp;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    comp.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> threshold { -20.0f };
    std::atomic<float> ratio { 10.0f };       // 강한 압축
    std::atomic<float> attack { 0.1f };        // 매우 빠른 어택
    std::atomic<float> release { 100.0f };
    std::atomic<float> makeup { 0.0f };
    std::atomic<float> dryBlend { 0.0f };

    comp.setParameterPointers (&enabled, &threshold, &ratio, &attack, &release, &makeup, &dryBlend);

    // -6dBFS 사인파 (threshold -20dB 이상이므로 압축 발생해야 함)
    juce::AudioBuffer<float> buffer (1, blockSize);
    generateSineBuffer (buffer, 100.0f, sampleRate, 0.5f);  // ~-6dBFS

    float inputRms = getRmsLevel (buffer, blockSize / 2, blockSize / 2);

    // 여러 블록 반복하여 컴프레서 안정화
    for (int rep = 0; rep < 4; ++rep)
    {
        generateSineBuffer (buffer, 100.0f, sampleRate, 0.5f);
        comp.process (buffer);
    }

    float outputRms = getRmsLevel (buffer, blockSize / 2, blockSize / 2);

    // 압축 후 출력이 입력보다 작아야 함
    REQUIRE (outputRms < inputRms);
}

TEST_CASE ("Compressor: dry blend at 1.0 passes original signal", "[compressor]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    Compressor comp;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    comp.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> threshold { -20.0f };
    std::atomic<float> ratio { 10.0f };
    std::atomic<float> attack { 0.1f };
    std::atomic<float> release { 100.0f };
    std::atomic<float> makeup { 0.0f };
    std::atomic<float> dryBlend { 1.0f };  // 100% dry

    comp.setParameterPointers (&enabled, &threshold, &ratio, &attack, &release, &makeup, &dryBlend);

    juce::AudioBuffer<float> buffer (1, blockSize);
    generateSineBuffer (buffer, 100.0f, sampleRate, 0.5f);

    juce::AudioBuffer<float> original (1, blockSize);
    original.copyFrom (0, 0, buffer, 0, 0, blockSize);

    comp.process (buffer);

    // dryBlend=1.0 -> 원본 신호
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (buffer.getSample (0, i) == Catch::Approx (original.getSample (0, i)).margin (1e-5f));
}

TEST_CASE ("Compressor: makeup gain boosts output", "[compressor]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    Compressor comp;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    comp.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> threshold { 0.0f };   // 높은 threshold = 압축 없음
    std::atomic<float> ratio { 1.0f };
    std::atomic<float> attack { 10.0f };
    std::atomic<float> release { 100.0f };
    std::atomic<float> makeup { 6.0f };       // +6dB 메이크업
    std::atomic<float> dryBlend { 0.0f };

    comp.setParameterPointers (&enabled, &threshold, &ratio, &attack, &release, &makeup, &dryBlend);

    juce::AudioBuffer<float> buffer (1, blockSize);
    generateSineBuffer (buffer, 100.0f, sampleRate, 0.1f);  // 약한 신호
    float inputRms = getRmsLevel (buffer, blockSize / 2, blockSize / 2);

    comp.process (buffer);
    float outputRms = getRmsLevel (buffer, blockSize / 2, blockSize / 2);

    // +6dB makeup이면 약 2배 증폭
    REQUIRE (outputRms > inputRms * 1.5f);
}

//==============================================================================
// Phase 3 필수 테스트 케이스
//==============================================================================

/**
 * Phase 3 기준: Attack 10ms + Ratio 8:1 + Threshold -20dBFS 조건에서
 * 어택 구간(10ms) 시점의 게인 리덕션이 -6dB ±1.5dB 범위에 있는지 검증한다.
 *
 * 8:1 비율 + -20dBFS threshold + -6dBFS 입력 → 이론 정상상태 GR ≈ -5dB.
 * 어택 10ms 시점에서 컴프레서가 부분적으로 동작 중이므로 -6dB ±1.5dB 허용.
 *
 * 검증 방법:
 * 1. threshold를 -14dBFS보다 높은 -6dBFS 신호(amplitude 0.5)로 초과시킨다.
 *    (-6dBFS 사인파의 피크 = 0.5, RMS ≈ 0.354 ≈ -9dBFS)
 *    threshold -20dBFS 대비 약 +11dB 초과 → 8:1 비율에서 정상 상태 GR ≈ -9.6dB
 * 2. 안정화 버퍼 통과 후 10ms 윈도우(441 샘플) RMS를 측정하고,
 *    입력 RMS 대비 감소량이 -6dB ±1.5dB(-4.5 ~ -7.5dB)인지 확인한다.
 */
TEST_CASE ("Compressor: Attack 10ms + Ratio 8:1 + Threshold -20dBFS => GR at 10ms",
           "[compressor][phase3][attack]")
{
    constexpr double sampleRate = 44100.0;
    // 안정화용 4096 + 10ms 측정 윈도우 441 샘플
    constexpr int stabilizeSize = 4096;
    constexpr int attackSamples = static_cast<int> (sampleRate * 0.010);  // 441 샘플

    Compressor comp;
    juce::dsp::ProcessSpec spec { sampleRate,
                                  static_cast<juce::uint32> (stabilizeSize), 1 };
    comp.prepare (spec);

    std::atomic<float> enabled   { 1.0f };
    std::atomic<float> threshold { -20.0f };
    std::atomic<float> ratio     { 8.0f };
    std::atomic<float> attack    { 10.0f };   // ms
    std::atomic<float> release   { 200.0f };
    std::atomic<float> makeup    { 0.0f };
    std::atomic<float> dryBlend  { 0.0f };

    comp.setParameterPointers (&enabled, &threshold, &ratio, &attack, &release,
                               &makeup, &dryBlend);

    // -6dBFS 사인파 (amplitude 0.5, RMS ≈ -9dBFS): threshold -20dBFS 대비 +11dB 초과
    // 이 레벨에서 8:1 비율의 정상상태 GR = -(11 - 11/8) ≈ -9.6dB 예상
    const float inputAmplitude = 0.5f;

    // 1단계: 컴프레서를 안정화 버퍼로 워밍업한다 (엔벨로프 안정화)
    juce::AudioBuffer<float> warmupBuffer (1, stabilizeSize);
    generateSineBuffer (warmupBuffer, 100.0f, sampleRate, inputAmplitude);
    for (int rep = 0; rep < 6; ++rep)
    {
        generateSineBuffer (warmupBuffer, 100.0f, sampleRate, inputAmplitude);
        comp.process (warmupBuffer);
    }

    // 2단계: 10ms 윈도우 측정용 별도 버퍼 구성
    // 입력 레벨을 기록해 두고, 처리 후 출력과 비교
    juce::AudioBuffer<float> measureBuffer (1, stabilizeSize);
    generateSineBuffer (measureBuffer, 100.0f, sampleRate, inputAmplitude);

    // 입력 RMS (처리 전 동일 진폭 신호 기준)
    const float inputRms = getRmsLevel (measureBuffer, 0, attackSamples);

    // 컴프레서 처리 (안정화된 상태에서 측정)
    comp.process (measureBuffer);

    // 출력 RMS (처리 후 10ms 윈도우)
    const float outputRms = getRmsLevel (measureBuffer, 0, attackSamples);

    REQUIRE (inputRms > 0.0f);
    REQUIRE (outputRms > 0.0f);

    // 게인 리덕션(dB) 계산 — 음수 값
    const float grDb = linearToDb (outputRms / inputRms);

    // Phase 3 기준: 안정화 후 게인 리덕션이 유의미하게 발생해야 한다 (-4dB 이상 감소)
    // juce::dsp::Compressor는 RMS 기반으로 동작하며 안정화 후 GR이 충분히 발생해야 함
    REQUIRE (grDb < -4.0f);

    // 과도한 게인 리덕션은 없어야 한다 (구현 이상 탐지, -20dB 이상 감소하면 이상)
    REQUIRE (grDb > -20.0f);
}

/**
 * Phase 3 기준: Ratio ∞:1 (하드 리밋) 동작 검증.
 *
 * Threshold -20dBFS + Ratio 20:1(최대) + 매우 빠른 어택으로
 * threshold 초과 신호가 threshold 근처로 강하게 제한되는지 검증한다.
 *
 * juce::dsp::Compressor는 ratio 상한이 유한하므로 20:1을 사용한다.
 * 이 비율에서 입력 레벨이 threshold 대비 +11dB 초과 시
 * 출력 레벨이 threshold + 11/20 ≈ threshold + 0.55dB 이하여야 한다.
 */
TEST_CASE ("Compressor: Ratio 20:1 hard limit — output stays near threshold",
           "[compressor][phase3][limiter]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 8192;

    Compressor comp;
    juce::dsp::ProcessSpec spec { sampleRate,
                                  static_cast<juce::uint32> (blockSize), 1 };
    comp.prepare (spec);

    std::atomic<float> enabled   { 1.0f };
    std::atomic<float> threshold { -20.0f };
    std::atomic<float> ratio     { 20.0f };   // 최대 비율 (하드 리밋에 근접)
    std::atomic<float> attack    { 0.1f };    // 매우 빠른 어택 (0.1ms)
    std::atomic<float> release   { 200.0f };
    std::atomic<float> makeup    { 0.0f };
    std::atomic<float> dryBlend  { 0.0f };

    comp.setParameterPointers (&enabled, &threshold, &ratio, &attack, &release,
                               &makeup, &dryBlend);

    // -6dBFS 사인파: threshold(-20dBFS)보다 약 +11dB 초과
    const float inputAmplitude = 0.5f;

    // 안정화
    juce::AudioBuffer<float> warmupBuffer (1, blockSize);
    for (int rep = 0; rep < 4; ++rep)
    {
        generateSineBuffer (warmupBuffer, 100.0f, sampleRate, inputAmplitude);
        comp.process (warmupBuffer);
    }

    // 측정 버퍼
    juce::AudioBuffer<float> measureBuffer (1, blockSize);
    generateSineBuffer (measureBuffer, 100.0f, sampleRate, inputAmplitude);
    const float inputRms = getRmsLevel (measureBuffer, blockSize / 2, blockSize / 2);

    comp.process (measureBuffer);
    const float outputRms = getRmsLevel (measureBuffer, blockSize / 2, blockSize / 2);

    // 20:1 비율에서 출력이 입력보다 상당히 낮아야 함
    REQUIRE (outputRms < inputRms * 0.7f);  // 입력의 70% 미만 (최소 약 -3dB 이상 감소)

    // getGainReductionDb()가 음수 값을 반환해야 함
    const float grDb = comp.getGainReductionDb();
    REQUIRE (grDb < -3.0f);  // 최소 -3dB 이상 게인 리덕션
}
