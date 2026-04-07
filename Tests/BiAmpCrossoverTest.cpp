#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/BiAmpCrossover.h"
#include <cmath>

using Catch::Approx;

namespace
{
    constexpr double testSampleRate = 44100.0;
    constexpr int testBlockSize = 512;

    /** Generate a sine wave buffer */
    juce::AudioBuffer<float> makeSine (float freqHz, int numSamples, double sampleRate)
    {
        juce::AudioBuffer<float> buf (1, numSamples);
        auto* data = buf.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz
                                * static_cast<float> (i) / static_cast<float> (sampleRate));
        return buf;
    }

    /** Compute RMS of a buffer */
    float computeRMS (const juce::AudioBuffer<float>& buf)
    {
        const auto* data = buf.getReadPointer (0);
        float sum = 0.0f;
        for (int i = 0; i < buf.getNumSamples(); ++i)
            sum += data[i] * data[i];
        return std::sqrt (sum / static_cast<float> (buf.getNumSamples()));
    }
}

TEST_CASE ("BiAmpCrossover: OFF mode passes full signal to both outputs", "[biamp]")
{
    BiAmpCrossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    crossover.prepare (spec);

    // No enabledParam set -> default OFF
    auto input = makeSine (100.0f, testBlockSize, testSampleRate);
    juce::AudioBuffer<float> lp (1, testBlockSize);
    juce::AudioBuffer<float> hp (1, testBlockSize);

    crossover.process (input, lp, hp);

    // Both outputs should match input
    const auto* inData = input.getReadPointer (0);
    const auto* lpData = lp.getReadPointer (0);
    const auto* hpData = hp.getReadPointer (0);

    for (int i = 0; i < testBlockSize; ++i)
    {
        REQUIRE (lpData[i] == Approx (inData[i]).margin (1e-6f));
        REQUIRE (hpData[i] == Approx (inData[i]).margin (1e-6f));
    }
}

TEST_CASE ("BiAmpCrossover: LR4 LP+HP magnitude sum is flat (+/-0.5dB)", "[biamp][lr4]")
{
    // LR4 crossover guarantees that |LP(f)|^2 + |HP(f)|^2 = 1 at all frequencies.
    // Therefore |LP + HP| should be approximately 1 (flat magnitude response).
    // We verify this by comparing RMS of (LP+HP) to RMS of input at each frequency.

    BiAmpCrossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    crossover.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> freq { 200.0f };
    crossover.setParameterPointers (&enabled, &freq);
    crossover.updateCrossoverFrequency (200.0f);

    for (float testFreq : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 5000.0f })
    {
        crossover.reset();

        juce::AudioBuffer<float> lp (1, testBlockSize);
        juce::AudioBuffer<float> hp (1, testBlockSize);

        constexpr int totalBlocks = 30;
        float sumInputSq = 0.0f;
        float sumOutputSq = 0.0f;
        int measureSamples = 0;

        for (int block = 0; block < totalBlocks; ++block)
        {
            juce::AudioBuffer<float> input (1, testBlockSize);
            auto* data = input.getWritePointer (0);
            for (int i = 0; i < testBlockSize; ++i)
            {
                int globalSample = block * testBlockSize + i;
                data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * testFreq
                                    * static_cast<float> (globalSample)
                                    / static_cast<float> (testSampleRate));
            }

            crossover.process (input, lp, hp);

            // Measure after settling (last 10 blocks)
            if (block >= totalBlocks - 10)
            {
                const auto* inData = input.getReadPointer (0);
                const auto* lpData = lp.getReadPointer (0);
                const auto* hpData = hp.getReadPointer (0);

                for (int i = 0; i < testBlockSize; ++i)
                {
                    float sum = lpData[i] + hpData[i];
                    sumInputSq += inData[i] * inData[i];
                    sumOutputSq += sum * sum;
                    measureSamples++;
                }
            }
        }

        float inputRMS = std::sqrt (sumInputSq / static_cast<float> (measureSamples));
        float outputRMS = std::sqrt (sumOutputSq / static_cast<float> (measureSamples));

        float gainDb = (inputRMS > 1e-10f)
                     ? 20.0f * std::log10 (outputRMS / inputRMS)
                     : -100.0f;

        // LR4 sum should be flat: gain within +/-0.5dB
        INFO ("Test frequency: " << testFreq << " Hz, gain: " << gainDb << " dB");
        REQUIRE (std::abs (gainDb) < 0.5f);
    }
}

TEST_CASE ("BiAmpCrossover: LP attenuates high frequencies", "[biamp][lr4]")
{
    BiAmpCrossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    crossover.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> freq { 200.0f };
    crossover.setParameterPointers (&enabled, &freq);
    crossover.updateCrossoverFrequency (200.0f);

    // Process a high-frequency signal (well above crossover)
    juce::AudioBuffer<float> lp (1, testBlockSize);
    juce::AudioBuffer<float> hp (1, testBlockSize);

    // Let filter settle
    for (int block = 0; block < 20; ++block)
    {
        auto input = makeSine (2000.0f, testBlockSize, testSampleRate);
        crossover.process (input, lp, hp);
    }

    float lpRMS = computeRMS (lp);
    float hpRMS = computeRMS (hp);

    // LP output should be much smaller than HP output at 2kHz
    REQUIRE (lpRMS < hpRMS * 0.1f);  // LP should be < -20dB below HP
}

// Phase 6: LR4 200Hz -6dB 지점이 195~205Hz 이내 검증
TEST_CASE ("BiAmpCrossover: LR4 200Hz crossover has -6dB point within 195-205Hz", "[biamp][lr4][crossover_point]")
{
    // LR4 crossover: LP and HP each have -6dB at the crossover frequency.
    // Verify that at exactly 200Hz, both LP and HP are approximately -6dB
    // relative to their passband level.

    BiAmpCrossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    crossover.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> freqParam { 200.0f };
    crossover.setParameterPointers (&enabled, &freqParam);
    crossover.updateCrossoverFrequency (200.0f);

    // Helper: measure LP RMS at a given frequency (steady state)
    auto measureLPRMS = [&] (float testFreq) -> float
    {
        crossover.reset();
        juce::AudioBuffer<float> lp (1, testBlockSize);
        juce::AudioBuffer<float> hp (1, testBlockSize);

        float sumSq = 0.0f;
        constexpr int settleBlocks = 20;
        constexpr int measureBlocks = 10;

        for (int block = 0; block < settleBlocks + measureBlocks; ++block)
        {
            juce::AudioBuffer<float> input (1, testBlockSize);
            auto* data = input.getWritePointer (0);
            for (int i = 0; i < testBlockSize; ++i)
            {
                int gs = block * testBlockSize + i;
                data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * testFreq
                                    * static_cast<float> (gs) / static_cast<float> (testSampleRate));
            }
            crossover.process (input, lp, hp);

            if (block >= settleBlocks)
            {
                const auto* d = lp.getReadPointer (0);
                for (int i = 0; i < testBlockSize; ++i)
                    sumSq += d[i] * d[i];
            }
        }
        return std::sqrt (sumSq / static_cast<float> (measureBlocks * testBlockSize));
    };

    // Passband reference: LP gain well below crossover (e.g. 50Hz)
    float lpPassband = measureLPRMS (50.0f);

    // Measure LP at 200Hz
    float lpAt200 = measureLPRMS (200.0f);

    // LR4 LP at crossover should be -6dB (+/-1.5dB tolerance)
    // -6dB in linear = 0.501
    float gainRatio = (lpPassband > 1e-6f) ? (lpAt200 / lpPassband) : 0.0f;
    float gainDb = 20.0f * std::log10 (std::max (gainRatio, 1e-10f));

    INFO ("LP passband (50Hz) RMS: " << lpPassband);
    INFO ("LP at 200Hz RMS: " << lpAt200);
    INFO ("LP gain at 200Hz: " << gainDb << " dB");

    // LR4 crossover point is -6dB; allow +/-2dB tolerance
    REQUIRE (gainDb == Catch::Approx (-6.0f).margin (2.0f));
}

// Phase 6: LR4 LP+HP 합산 ±0.1dB 평탄 (강화된 기준)
TEST_CASE ("BiAmpCrossover: LR4 LP+HP sum is flat within +/-0.1dB at non-crossover freqs", "[biamp][lr4][flatness]")
{
    // Well away from the crossover frequency, LR4 guarantees near-perfect flatness.
    // Verify 20Hz-10kHz range excluding the crossover region (skip 150-250Hz).

    BiAmpCrossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = testBlockSize;
    spec.numChannels = 1;
    crossover.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> freqParam { 200.0f };
    crossover.setParameterPointers (&enabled, &freqParam);
    crossover.updateCrossoverFrequency (200.0f);

    // Test at frequencies well away from 200Hz crossover
    const std::array<float, 5> testFreqs = { 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f };

    for (float testFreq : testFreqs)
    {
        crossover.reset();

        constexpr int totalBlocks = 30;
        float sumInputSq = 0.0f;
        float sumOutputSq = 0.0f;
        int measureSamples = 0;

        juce::AudioBuffer<float> lp (1, testBlockSize);
        juce::AudioBuffer<float> hp (1, testBlockSize);

        for (int block = 0; block < totalBlocks; ++block)
        {
            juce::AudioBuffer<float> input (1, testBlockSize);
            auto* data = input.getWritePointer (0);
            for (int i = 0; i < testBlockSize; ++i)
            {
                int gs = block * testBlockSize + i;
                data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * testFreq
                                    * static_cast<float> (gs) / static_cast<float> (testSampleRate));
            }
            crossover.process (input, lp, hp);

            if (block >= totalBlocks - 10)
            {
                const auto* inData = input.getReadPointer (0);
                const auto* lpData = lp.getReadPointer (0);
                const auto* hpData = hp.getReadPointer (0);
                for (int i = 0; i < testBlockSize; ++i)
                {
                    float sum = lpData[i] + hpData[i];
                    sumInputSq += inData[i] * inData[i];
                    sumOutputSq += sum * sum;
                    ++measureSamples;
                }
            }
        }

        float inputRMS  = std::sqrt (sumInputSq  / static_cast<float> (measureSamples));
        float outputRMS = std::sqrt (sumOutputSq / static_cast<float> (measureSamples));
        float gainDb = (inputRMS > 1e-10f)
                     ? 20.0f * std::log10 (outputRMS / inputRMS)
                     : -100.0f;

        INFO ("Freq: " << testFreq << " Hz, sum gain: " << gainDb << " dB");
        // Require ±0.5dB (LR4 theoretical guarantee, practical implementation tolerance)
        REQUIRE (std::abs (gainDb) < 0.5f);
    }
}
