#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/AmpVoicing.h"
#include "Models/AmpModelLibrary.h"
#include <cmath>

using Catch::Approx;

namespace
{
    constexpr double testSampleRate = 44100.0;
    constexpr int testBlockSize = 1024;

    /** Measure gain at a specific frequency by processing a sine wave */
    float measureGainAtFrequency (AmpVoicing& voicing, float freqHz, double sampleRate)
    {
        constexpr int warmupBlocks = 10;
        constexpr int numSamples = 1024;

        juce::AudioBuffer<float> buffer (1, numSamples);

        // Warmup: let filter settle
        for (int b = 0; b < warmupBlocks; ++b)
        {
            auto* data = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz
                                    * static_cast<float> (b * numSamples + i)
                                    / static_cast<float> (sampleRate));
            voicing.process (buffer);
        }

        // Measure block
        auto* data = buffer.getWritePointer (0);
        float inputRMS = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz
                                     * static_cast<float> (warmupBlocks * numSamples + i)
                                     / static_cast<float> (sampleRate));
            data[i] = sample;
            inputRMS += sample * sample;
        }
        inputRMS = std::sqrt (inputRMS / static_cast<float> (numSamples));

        voicing.process (buffer);

        float outputRMS = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            outputRMS += data[i] * data[i];
        outputRMS = std::sqrt (outputRMS / static_cast<float> (numSamples));

        if (inputRMS < 1e-10f)
            return 0.0f;
        return outputRMS / inputRMS;
    }

    float gainToDb (float gain)
    {
        if (gain < 1e-10f)
            return -100.0f;
        return 20.0f * std::log10 (gain);
    }
}

TEST_CASE ("AmpVoicing: Origin Pure is flat (all bands bypassed)", "[voicing][origin]")
{
    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::OriginPure);

    // Check gain at multiple frequencies - should all be ~0dB (unity gain)
    for (float freq : { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f })
    {
        voicing.reset();
        float gain = measureGainAtFrequency (voicing, freq, testSampleRate);
        float gainDb = gainToDb (gain);

        INFO ("Frequency: " << freq << " Hz, gain: " << gainDb << " dB");
        REQUIRE (gainDb == Approx (0.0f).margin (0.5f));
    }
}

TEST_CASE ("AmpVoicing: American Vintage has low shelf boost at 80Hz", "[voicing][american]")
{
    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::AmericanVintage);

    float gainAt80 = measureGainAtFrequency (voicing, 80.0f, testSampleRate);
    float gainDb = gainToDb (gainAt80);

    // V1: Low Shelf 80Hz +3dB
    INFO ("80Hz gain: " << gainDb << " dB");
    REQUIRE (gainDb > 1.5f);  // Should be boosted (at least +1.5dB)
}

TEST_CASE ("AmpVoicing: British Stack has HP at 60Hz", "[voicing][british]")
{
    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::BritishStack);

    // Below the HP cutoff, signal should be attenuated
    float gainAt30 = measureGainAtFrequency (voicing, 30.0f, testSampleRate);
    float gainAt500 = measureGainAtFrequency (voicing, 500.0f, testSampleRate);

    float dbAt30 = gainToDb (gainAt30);
    float dbAt500 = gainToDb (gainAt500);

    INFO ("30Hz gain: " << dbAt30 << " dB, 500Hz gain: " << dbAt500 << " dB");
    // 30Hz should be significantly lower than 500Hz (HP + peak boost at 500Hz)
    REQUIRE (dbAt30 < dbAt500 - 6.0f);
}

TEST_CASE ("AmpVoicing: Modern Micro has peak boost at 3kHz", "[voicing][modern]")
{
    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::ModernMicro);

    float gainAt3k = measureGainAtFrequency (voicing, 3000.0f, testSampleRate);
    float dbAt3k = gainToDb (gainAt3k);

    // V3: Peak 3000Hz +4dB Q=1.5
    INFO ("3kHz gain: " << dbAt3k << " dB");
    REQUIRE (dbAt3k > 2.0f);  // Should be noticeably boosted
}

TEST_CASE ("AmpVoicing: different models produce different responses", "[voicing][differentiation]")
{
    // When Cabinet is bypassed and ToneStack is flat, each amp model should
    // still sound different due to voicing filters

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;

    // Measure gain at 500Hz for each model
    std::array<float, 6> gainsAt500;

    for (int i = 0; i < 6; ++i)
    {
        AmpVoicing voicing;
        voicing.prepare (spec);
        voicing.setModel (static_cast<AmpModelId> (i));
        gainsAt500[(size_t) i] = gainToDb (
            measureGainAtFrequency (voicing, 500.0f, testSampleRate));
    }

    // At least some models should differ from each other by > 1dB at 500Hz
    bool foundDifference = false;
    for (int i = 0; i < 6 && ! foundDifference; ++i)
        for (int j = i + 1; j < 6 && ! foundDifference; ++j)
            if (std::abs (gainsAt500[(size_t) i] - gainsAt500[(size_t) j]) > 1.0f)
                foundDifference = true;

    REQUIRE (foundDifference);
}

// Phase 6: American Vintage 1.5kHz 감쇠 확인 (-2dB Peak 검증)
TEST_CASE ("AmpVoicing: American Vintage has upper-mid cut at 1.5kHz", "[voicing][american]")
{
    // AmpModelLibrary defines AmericanVintage voicing as:
    //   Band 3: Peak 1500Hz -2dB Q=1.2 (upper-mid scoop)
    // So gain at 1.5kHz should be below gain at 500Hz (which is boosted at 300Hz)

    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::AmericanVintage);

    float gainAt1500 = measureGainAtFrequency (voicing, 1500.0f, testSampleRate);
    float dbAt1500   = gainToDb (gainAt1500);

    // The 1500Hz band has -2dB peak; total gain should be measurably below 0dB
    // (combined with 80Hz shelf +3dB and 300Hz +2dB, but at 1500Hz the cut dominates)
    INFO ("1500Hz gain: " << dbAt1500 << " dB");
    REQUIRE (dbAt1500 < 0.0f);   // Should be attenuated (below unity)
}

// Phase 6: British Stack 500Hz 부스트 확인
TEST_CASE ("AmpVoicing: British Stack has mid boost at 500Hz", "[voicing][british]")
{
    // AmpModelLibrary: BritishStack voicing has Peak 500Hz +3dB Q=1.0
    // Gain at 500Hz should be clearly above 0dB

    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::BritishStack);

    float gainAt500 = measureGainAtFrequency (voicing, 500.0f, testSampleRate);
    float dbAt500   = gainToDb (gainAt500);

    INFO ("500Hz gain: " << dbAt500 << " dB");
    // Peak +3dB at 500Hz; allow for HP rolloff influence but expect net boost > +1dB
    REQUIRE (dbAt500 > 1.0f);
}

// Phase 6: Tweed Bass 주파수 응답 확인 (60Hz 저역 부스트, 600Hz 미드 스쿱)
TEST_CASE ("AmpVoicing: Tweed Bass has low boost at 60Hz and mid cut at 600Hz", "[voicing][tweed]")
{
    // AmpModelLibrary: TweedBass voicing:
    //   Band 1: LowShelf 60Hz +2dB
    //   Band 2: Peak 600Hz -3dB Q=0.7 (mid scoop)
    //   Band 3: HighShelf 5000Hz -2dB

    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::TweedBass);

    voicing.reset();
    float gainAt60  = measureGainAtFrequency (voicing, 60.0f,  testSampleRate);
    float dbAt60    = gainToDb (gainAt60);

    voicing.reset();
    float gainAt600 = measureGainAtFrequency (voicing, 600.0f, testSampleRate);
    float dbAt600   = gainToDb (gainAt600);

    INFO ("60Hz gain: " << dbAt60 << " dB, 600Hz gain: " << dbAt600 << " dB");

    // 60Hz: LowShelf +2dB — should be above 0dB
    REQUIRE (dbAt60 > 0.5f);

    // 600Hz should be lower than 60Hz (mid scoop vs low boost)
    REQUIRE (dbAt600 < dbAt60);
}

// Phase 6: Italian Clean Voicing가 거의 평탄함을 확인 (6kHz 약간의 부스트만 허용)
TEST_CASE ("AmpVoicing: Italian Clean Voicing is nearly flat (only slight 6kHz clarity)", "[voicing][italian]")
{
    // AmpModelLibrary: ItalianClean voicing is minimal:
    //   Band 1: Flat
    //   Band 2: Peak 6000Hz +1.5dB Q=1.5 (mild clarity)
    //   Band 3: Flat
    // Most frequencies should be very close to 0dB.

    AmpVoicing voicing;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    voicing.prepare (spec);
    voicing.setModel (AmpModelId::ItalianClean);

    // Low and mid frequencies should be flat (within ±0.5dB)
    for (float freq : { 100.0f, 300.0f, 500.0f, 1000.0f, 2000.0f })
    {
        voicing.reset();
        float gainDb = gainToDb (measureGainAtFrequency (voicing, freq, testSampleRate));

        INFO ("Italian Clean at " << freq << " Hz: " << gainDb << " dB");
        REQUIRE (gainDb == Catch::Approx (0.0f).margin (0.5f));
    }
}

// Phase 6: 6종 앰프 Voicing이 모두 서로 다른 응답을 가짐을 다중 주파수에서 확인
TEST_CASE ("AmpVoicing: all 6 models produce distinct responses", "[voicing][differentiation][phase6]")
{
    // Verify that no two models produce identical gain profiles across a set of test frequencies.
    // This is the Cabinet-bypass + ToneStack-flat scenario.

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;

    constexpr int numModels = 6;
    // Include 6000Hz to capture ItalianClean's 6kHz clarity boost vs OriginPure flat
    const std::array<float, 5> testFreqs = { 80.0f, 500.0f, 1500.0f, 3000.0f, 6000.0f };

    // Measure gain fingerprint for each model
    std::array<std::array<float, 5>, numModels> profiles;

    for (int m = 0; m < numModels; ++m)
    {
        AmpVoicing voicing;
        voicing.prepare (spec);
        voicing.setModel (static_cast<AmpModelId> (m));

        for (int f = 0; f < (int) testFreqs.size(); ++f)
        {
            voicing.reset();
            profiles[(size_t) m][(size_t) f] =
                gainToDb (measureGainAtFrequency (voicing, testFreqs[(size_t) f], testSampleRate));
        }
    }

    // For each pair of models, check that they differ by at least 1dB at some frequency
    for (int i = 0; i < numModels; ++i)
    {
        for (int j = i + 1; j < numModels; ++j)
        {
            bool differs = false;
            for (int f = 0; f < (int) testFreqs.size(); ++f)
            {
                if (std::abs (profiles[(size_t) i][(size_t) f] - profiles[(size_t) j][(size_t) f]) > 1.0f)
                {
                    differs = true;
                    break;
                }
            }
            INFO ("Models " << i << " and " << j << " must differ by >1dB at some frequency");
            REQUIRE (differs);
        }
    }
}
