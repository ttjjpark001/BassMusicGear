#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/ToneStack.h"
#include "Models/AmpModel.h"
#include <cmath>

//==============================================================================
namespace {

float measureGainAtFrequency (ToneStack& ts, float freqHz, double sampleRate,
                               int numSamples = 8192)
{
    juce::AudioBuffer<float> buffer (1, numSamples);

    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);

    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, std::sin (omega * static_cast<float> (i)));

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

float linearTodB (float linear)
{
    return 20.0f * std::log10 (std::max (linear, 1e-10f));
}

inline void prepareToneStack (ToneStack& ts, double sampleRate,
                               ToneStackType type,
                               float bass, float mid, float treble)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 8192;
    spec.numChannels      = 1;
    ts.setType (type);
    ts.prepare (spec);
    ts.updateCoefficients (bass, mid, treble);
}

} // namespace

//==============================================================================
// TMB Tests (existing, adapted)
//==============================================================================

TEST_CASE ("ToneStack TMB: initializes without crash", "[tonestack][tmb]")
{
    ToneStack ts;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels     = 1;
    ts.setType (ToneStackType::TMB);
    REQUIRE_NOTHROW (ts.prepare (spec));
}

TEST_CASE ("ToneStack TMB: flat setting produces signal", "[tonestack][tmb]")
{
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::TMB, 0.5f, 0.5f, 0.5f);

    float gainAt500Hz = measureGainAtFrequency (ts, 500.0f, sampleRate);
    REQUIRE (gainAt500Hz > 0.001f);
}

TEST_CASE ("ToneStack TMB: flat setting (0.5/0.5/0.5) signal in valid range",
           "[tonestack][tmb][phase1]")
{
    constexpr double sampleRate = 44100.0;
    constexpr float minGainDB   = -20.0f;
    constexpr float maxGainDB   =  +6.0f;

    const std::array<float, 4> checkFreqs = { 100.0f, 500.0f, 1000.0f, 5000.0f };

    for (float freq : checkFreqs)
    {
        ToneStack tsFreq;
        prepareToneStack (tsFreq, sampleRate, ToneStackType::TMB, 0.5f, 0.5f, 0.5f);
        float gainDB = linearTodB (measureGainAtFrequency (tsFreq, freq, sampleRate));

        INFO ("Frequency: " << freq << "Hz, gain: " << gainDB << "dB");
        REQUIRE (gainDB >= minGainDB);
        REQUIRE (gainDB <= maxGainDB);
    }
}

TEST_CASE ("ToneStack TMB: Bass=1.0 boosts 100Hz compared to Bass=0.5",
           "[tonestack][tmb][phase1]")
{
    constexpr double sampleRate = 44100.0;

    ToneStack tsRef, tsBoosted;
    prepareToneStack (tsRef,     sampleRate, ToneStackType::TMB, 0.5f, 0.5f, 0.5f);
    prepareToneStack (tsBoosted, sampleRate, ToneStackType::TMB, 1.0f, 0.5f, 0.5f);

    float refGain     = measureGainAtFrequency (tsRef,     100.0f, sampleRate);
    float boostedGain = measureGainAtFrequency (tsBoosted, 100.0f, sampleRate);

    REQUIRE (boostedGain > refGain);
}

TEST_CASE ("ToneStack TMB: treble boost increases high frequency gain", "[tonestack][tmb]")
{
    constexpr double sampleRate = 44100.0;

    ToneStack tsRef, tsBoosted;
    prepareToneStack (tsRef,     sampleRate, ToneStackType::TMB, 0.5f, 0.5f, 0.5f);
    prepareToneStack (tsBoosted, sampleRate, ToneStackType::TMB, 0.5f, 0.5f, 1.0f);

    float refGain     = measureGainAtFrequency (tsRef,     5000.0f, sampleRate);
    float boostedGain = measureGainAtFrequency (tsBoosted, 5000.0f, sampleRate);

    REQUIRE (boostedGain > refGain);
}

//==============================================================================
// Baxandall (American Vintage) Tests
//==============================================================================

TEST_CASE ("ToneStack Baxandall: initializes and produces signal", "[tonestack][baxandall]")
{
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::Baxandall, 0.5f, 0.5f, 0.5f);

    float gain = measureGainAtFrequency (ts, 500.0f, sampleRate);
    REQUIRE (gain > 0.001f);
}

TEST_CASE ("ToneStack Baxandall: Bass boost at 100Hz", "[tonestack][baxandall][phase2]")
{
    constexpr double sampleRate = 44100.0;

    ToneStack tsRef, tsBoosted;
    prepareToneStack (tsRef,     sampleRate, ToneStackType::Baxandall, 0.5f, 0.5f, 0.5f);
    prepareToneStack (tsBoosted, sampleRate, ToneStackType::Baxandall, 1.0f, 0.5f, 0.5f);

    float refGain     = measureGainAtFrequency (tsRef,     100.0f, sampleRate);
    float boostedGain = measureGainAtFrequency (tsBoosted, 100.0f, sampleRate);

    REQUIRE (boostedGain > refGain);
}

//==============================================================================
// James (British Stack) Tests
//==============================================================================

TEST_CASE ("ToneStack James: initializes and produces signal", "[tonestack][james]")
{
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::James, 0.5f, 0.5f, 0.5f);

    float gain = measureGainAtFrequency (ts, 500.0f, sampleRate);
    REQUIRE (gain > 0.001f);
}

TEST_CASE ("ToneStack James: Bass change does not affect 8kHz (independence)",
           "[tonestack][james][phase2]")
{
    // James topology: Bass and Treble are INDEPENDENT shelving filters
    // Changing Bass should not affect high frequencies significantly
    constexpr double sampleRate = 44100.0;

    ToneStack tsLowBass, tsHighBass;
    prepareToneStack (tsLowBass,  sampleRate, ToneStackType::James, 0.0f, 0.5f, 0.5f);
    prepareToneStack (tsHighBass, sampleRate, ToneStackType::James, 1.0f, 0.5f, 0.5f);

    float gainLow  = linearTodB (measureGainAtFrequency (tsLowBass,  8000.0f, sampleRate));
    float gainHigh = linearTodB (measureGainAtFrequency (tsHighBass, 8000.0f, sampleRate));

    // 8kHz response should be roughly the same regardless of Bass setting (within 2dB)
    INFO ("Bass=0 at 8kHz: " << gainLow << " dB, Bass=1 at 8kHz: " << gainHigh << " dB");
    REQUIRE (std::abs (gainHigh - gainLow) < 2.0f);
}

//==============================================================================
// BaxandallGrunt (Modern Micro) Tests
//==============================================================================

TEST_CASE ("ToneStack ModernMicro: initializes and produces signal", "[tonestack][modern]")
{
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::BaxandallGrunt, 0.5f, 0.5f, 0.5f);

    float gain = measureGainAtFrequency (ts, 500.0f, sampleRate);
    REQUIRE (gain > 0.001f);
}

//==============================================================================
// MarkbassFourBand (Italian Clean) Tests
//==============================================================================

TEST_CASE ("ToneStack ItalianClean: initializes and produces signal", "[tonestack][markbass]")
{
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::MarkbassFourBand, 0.5f, 0.5f, 0.5f);

    float gain = measureGainAtFrequency (ts, 500.0f, sampleRate);
    REQUIRE (gain > 0.001f);
}

TEST_CASE ("ToneStack ItalianClean VPF max: 380Hz notch -6dB or more relative to 1kHz",
           "[tonestack][markbass][vpf][phase2]")
{
    // VPF at max should create a notch at 380Hz.
    // Phase 2 requirement: 380Hz 노치 -6dB 이상 확인.
    // VPF 구현: filter[5] = makePeakFilter(380Hz, Q=2.0, gain=-vpfDepthDB)
    // vpfDepthDB = vpf(1.0) * 12.0 = 12dB → 이론 노치 깊이 최대 -12dB
    // 1kHz 대비 상대 감쇠가 6dB 이상이어야 한다 (Phase 2 기준).
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::MarkbassFourBand, 0.5f, 0.5f, 0.5f);

    // VPF max, VLE off
    ts.updateMarkbassExtras (1.0f, 0.0f);

    float gainAt380 = linearTodB (measureGainAtFrequency (ts, 380.0f, sampleRate));
    float gainAt1k  = linearTodB (measureGainAtFrequency (ts, 1000.0f, sampleRate));

    INFO ("380Hz gain: " << gainAt380 << " dB, 1kHz gain: " << gainAt1k << " dB");
    INFO ("Notch depth (1kHz - 380Hz): " << (gainAt1k - gainAt380) << " dB");

    // Phase 2 기준: 1kHz 대비 380Hz 감쇠가 최소 6dB 이상 (명세: "380Hz 노치 -6dB 이상")
    REQUIRE (gainAt380 < gainAt1k);
    REQUIRE ((gainAt1k - gainAt380) >= 6.0f);
}

TEST_CASE ("ToneStack ItalianClean VLE max: 8kHz rolloff -12dB or more relative to 500Hz",
           "[tonestack][markbass][vle][phase2]")
{
    // VLE at max should roll off high frequencies significantly.
    // Phase 2 requirement: 8kHz -12dB 이상 롤오프 확인.
    // VLE 구현: StateVariableTPTFilter LP, cutoff = 4kHz at max
    // 500Hz는 LP 통과대역 내에서 거의 감쇠 없음.
    // 8kHz는 cutoff(4kHz)보다 1 octave 위 → 6dB/oct 1차 필터로 약 -6dB,
    // 하지만 실제로는 4kHz 이후 급격히 감쇠되어 -12dB 이상이어야 한다.
    constexpr double sampleRate = 44100.0;
    ToneStack ts;
    prepareToneStack (ts, sampleRate, ToneStackType::MarkbassFourBand, 0.5f, 0.5f, 0.5f);

    // VPF off, VLE max (cutoff = 4kHz)
    ts.updateMarkbassExtras (0.0f, 1.0f);

    float gainAt500  = linearTodB (measureGainAtFrequency (ts, 500.0f, sampleRate));
    float gainAt8k   = linearTodB (measureGainAtFrequency (ts, 8000.0f, sampleRate));

    INFO ("500Hz gain: " << gainAt500 << " dB, 8kHz gain: " << gainAt8k << " dB");
    INFO ("Rolloff (500Hz - 8kHz): " << (gainAt500 - gainAt8k) << " dB");

    // Phase 2 기준: 500Hz 대비 8kHz 감쇠가 최소 12dB 이상 (명세: "8kHz -12dB 이상 롤오프")
    REQUIRE (gainAt8k < gainAt500);
    REQUIRE ((gainAt500 - gainAt8k) >= 12.0f);
}

//==============================================================================
// Cross-topology NaN/Inf safety
//==============================================================================

TEST_CASE ("ToneStack: all topologies produce no NaN/Inf", "[tonestack][safety]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 1024;

    const ToneStackType types[] = {
        ToneStackType::TMB,
        ToneStackType::Baxandall,
        ToneStackType::James,
        ToneStackType::BaxandallGrunt,
        ToneStackType::MarkbassFourBand
    };

    for (auto type : types)
    {
        ToneStack ts;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate      = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels     = 1;
        ts.setType (type);
        ts.prepare (spec);
        ts.updateCoefficients (1.0f, 1.0f, 1.0f);

        if (type == ToneStackType::MarkbassFourBand)
            ts.updateMarkbassExtras (1.0f, 1.0f);

        juce::AudioBuffer<float> buffer (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i,
                std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f
                          / static_cast<float> (sampleRate) * static_cast<float> (i)));

        ts.process (buffer);

        auto* data = buffer.getReadPointer (0);
        bool hasNaN = false, hasInf = false;
        for (int i = 0; i < numSamples; ++i)
        {
            hasNaN |= std::isnan (data[i]);
            hasInf |= std::isinf (data[i]);
        }

        INFO ("ToneStackType: " << static_cast<int> (type));
        REQUIRE_FALSE (hasNaN);
        REQUIRE_FALSE (hasInf);
    }
}
