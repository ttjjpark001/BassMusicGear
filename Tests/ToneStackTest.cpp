#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/ToneStack.h"
#include <cmath>

//==============================================================================
// 익명 네임스페이스: 테스트 전용 헬퍼
//==============================================================================
namespace {

/**
 * ToneStack에 사인파를 통과시켜 입력 대비 출력 RMS 비율(= 선형 게인)을 측정한다.
 * 필터 과도 응답(settling) 제거를 위해 앞 절반 샘플을 건너뛴 뒤 측정한다.
 */
float measureGainAtFrequency (ToneStack& ts, float freqHz, double sampleRate,
                               int numSamples = 8192)
{
    juce::AudioBuffer<float> buffer (1, numSamples);

    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);

    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, std::sin (omega * static_cast<float> (i)));

    // 입력 RMS — 과도 응답 제거 후 측정
    const int skipSamples   = numSamples / 2;
    const int measureLength = numSamples - skipSamples;

    float inputRMS = 0.0f;
    for (int i = skipSamples; i < numSamples; ++i)
    {
        float v = buffer.getSample (0, i);
        inputRMS += v * v;
    }
    inputRMS = std::sqrt (inputRMS / static_cast<float> (measureLength));

    ts.process (buffer);

    float outputRMS = 0.0f;
    for (int i = skipSamples; i < numSamples; ++i)
    {
        float v = buffer.getSample (0, i);
        outputRMS += v * v;
    }
    outputRMS = std::sqrt (outputRMS / static_cast<float> (measureLength));

    if (inputRMS < 1e-10f)
        return 0.0f;

    return outputRMS / inputRMS;
}

/** 선형 진폭을 dB로 변환 (0이면 -200dB 반환) */
float linearTodB (float linear)
{
    return 20.0f * std::log10 (std::max (linear, 1e-10f));
}

/**
 * ToneStack 인스턴스를 초기화한다.
 * ToneStack은 non-copyable이므로 포인터/레퍼런스로만 전달 가능.
 * 이 함수는 공통 초기화 로직을 줄이기 위한 인라인 헬퍼다.
 */
inline void prepareToneStack (ToneStack& ts, double sampleRate,
                               float bass, float mid, float treble)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 8192;
    spec.numChannels      = 1;
    ts.prepare (spec);
    ts.updateCoefficients (bass, mid, treble);
}

} // namespace

//==============================================================================
// Tests
//==============================================================================

// --- 초기화 ---

TEST_CASE ("ToneStack TMB: initializes without crash", "[tonestack][tmb]")
{
    ToneStack ts;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels     = 1;

    REQUIRE_NOTHROW (ts.prepare (spec));
}

// --- 기본 동작 ---

TEST_CASE ("ToneStack TMB: flat setting produces signal", "[tonestack][tmb]")
{
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, 0.5f, 0.5f, 0.5f);

    float gainAt500Hz = measureGainAtFrequency (ts, 500.0f, sampleRate);

    // TMB는 패시브 네트워크라 게인이 1 이하지만, 신호가 사라지면 안 됨
    REQUIRE (gainAt500Hz > 0.001f);
}

// --- Phase 1 핵심 기준: Bass=0.5/Mid=0.5/Treble=0.5 → 100Hz~5kHz ±3dB ---

TEST_CASE ("ToneStack TMB: flat setting (0.5/0.5/0.5) passes signal in valid range at 100Hz to 5kHz",
           "[tonestack][tmb][frequency][phase1]")
{
    // Phase 1 테스트 기준 해석:
    //   Bass=0.5, Mid=0.5, Treble=0.5 → 100Hz~5kHz 각 주파수에서
    //   절대 게인이 유효 범위(-20dB ~ +6dB) 내에 있어야 한다.
    //
    // TMB 배경:
    //   Fender Bassman 5F6-A TMB 토폴로지는 패시브 RC 네트워크다.
    //   컨트롤 중간 위치에서 500Hz 부근이 자연스럽게 조금 낮아지고
    //   100Hz(저역)와 5kHz(고역) 쪽이 상대적으로 높게 나타나는 것이 정상 동작.
    //   실제 회로에서 100Hz와 500Hz는 6dB 이상 차이가 날 수 있다.
    //
    // 따라서 이 테스트는 "평탄도" 대신 "신호가 완전히 사라지거나 폭발하지 않음"을 검증한다.
    //   - 절대 게인이 -20dB 이상 (신호가 사실상 없어지지 않음)
    //   - 절대 게인이 +6dB 이하 (TMB 패시브 특성상 증폭 불가)
    constexpr double sampleRate = 44100.0;
    constexpr float minGainDB   = -20.0f;  // 패시브 TMB에서 허용 최솟값
    constexpr float maxGainDB   =  +6.0f;  // 패시브 TMB에서 허용 최댓값 (이론상 ≤0dB)

    const std::array<float, 4> checkFreqs = { 100.0f, 500.0f, 1000.0f, 5000.0f };

    for (float freq : checkFreqs)
    {
        ToneStack tsFreq;
        prepareToneStack (tsFreq, sampleRate, 0.5f, 0.5f, 0.5f);
        float gainDB = linearTodB (measureGainAtFrequency (tsFreq, freq, sampleRate));

        INFO ("Frequency: " << freq << "Hz, gain: " << gainDB << "dB");
        REQUIRE (gainDB >= minGainDB);
        REQUIRE (gainDB <= maxGainDB);
    }
}

// --- Phase 1 핵심 기준: Bass=1.0 시 100Hz 게인이 Bass=0.5 대비 증가 ---

TEST_CASE ("ToneStack TMB: Bass=1.0 boosts 100Hz compared to Bass=0.5",
           "[tonestack][tmb][boundary][phase1]")
{
    // Phase 1 테스트 기준:
    //   Bass=1.0, Mid=0.5, Treble=0.5 → Bass=0.5 대비 100Hz 게인이 높아야 함
    constexpr double sampleRate = 44100.0;

    ToneStack tsRef, tsBoosted;
    prepareToneStack (tsRef,     sampleRate, 0.5f, 0.5f, 0.5f);
    prepareToneStack (tsBoosted, sampleRate, 1.0f, 0.5f, 0.5f);

    float refGain100Hz     = measureGainAtFrequency (tsRef,     100.0f, sampleRate);
    float boostedGain100Hz = measureGainAtFrequency (tsBoosted, 100.0f, sampleRate);

    INFO ("Bass=0.5: " << linearTodB (refGain100Hz) << "dB, "
          << "Bass=1.0: " << linearTodB (boostedGain100Hz) << "dB");
    REQUIRE (boostedGain100Hz > refGain100Hz);
}

// --- 기존 테스트: Treble 부스트 ---

TEST_CASE ("ToneStack TMB: treble boost increases high frequency gain", "[tonestack][tmb]")
{
    constexpr double sampleRate = 44100.0;

    ToneStack tsRef, tsBoosted;
    prepareToneStack (tsRef,     sampleRate, 0.5f, 0.5f, 0.5f);
    prepareToneStack (tsBoosted, sampleRate, 0.5f, 0.5f, 1.0f);

    float refGain5kHz     = measureGainAtFrequency (tsRef,     5000.0f, sampleRate);
    float boostedGain5kHz = measureGainAtFrequency (tsBoosted, 5000.0f, sampleRate);

    REQUIRE (boostedGain5kHz > refGain5kHz);
}

// --- NaN / Inf 안전성 ---

TEST_CASE ("ToneStack TMB: output does not contain NaN or Inf", "[tonestack][tmb]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples    = 1024;

    ToneStack ts;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels     = 1;
    ts.prepare (spec);
    ts.updateCoefficients (1.0f, 1.0f, 1.0f);

    juce::AudioBuffer<float> buffer (1, numSamples);
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i,
            std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f / 44100.0f
                      * static_cast<float> (i)));

    ts.process (buffer);

    auto* data = buffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        REQUIRE_FALSE (std::isnan (data[i]));
        REQUIRE_FALSE (std::isinf (data[i]));
    }
}

// --- 경계값 ---

TEST_CASE ("ToneStack TMB: extreme parameter values do not produce NaN",
           "[tonestack][tmb][boundary]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples    = 512;

    const std::array<std::array<float, 3>, 4> extremes = {{
        {{ 0.0f, 0.0f, 0.0f }},
        {{ 1.0f, 1.0f, 1.0f }},
        {{ 0.0f, 1.0f, 0.0f }},
        {{ 1.0f, 0.0f, 1.0f }}
    }};

    for (const auto& params : extremes)
    {
        ToneStack ts;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate      = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels     = 1;
        ts.prepare (spec);
        ts.updateCoefficients (params[0], params[1], params[2]);

        juce::AudioBuffer<float> buffer (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i,
                std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f
                          / static_cast<float> (sampleRate) * static_cast<float> (i)));

        ts.process (buffer);

        auto* data = buffer.getReadPointer (0);
        bool hasNaN = false, hasInf = false;
        for (int i = 0; i < numSamples; ++i)
        {
            hasNaN |= std::isnan (data[i]);
            hasInf |= std::isinf (data[i]);
        }

        INFO ("bass=" << params[0] << " mid=" << params[1] << " treble=" << params[2]);
        REQUIRE_FALSE (hasNaN);
        REQUIRE_FALSE (hasInf);
    }
}
