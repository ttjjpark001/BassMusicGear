#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Effects/Overdrive.h"
#include "DSP/Preamp.h"
#include <cmath>
#include <vector>

//==============================================================================
// Test helpers
//==============================================================================
namespace {

void fillSine (juce::AudioBuffer<float>& buf, float freqHz,
               double sampleRate, float amplitude = 0.9f)
{
    const int n = buf.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < n; ++i)
        buf.setSample (0, i, amplitude * std::sin (omega * static_cast<float> (i)));
}

float computeRMS (const juce::AudioBuffer<float>& buf)
{
    float rms = 0.0f;
    const int n = buf.getNumSamples();
    const float* data = buf.getReadPointer (0);
    for (int i = 0; i < n; ++i)
        rms += data[i] * data[i];
    return std::sqrt (rms / static_cast<float> (n));
}

bool hasNaNOrInf (const juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();
    const float* data = buf.getReadPointer (0);
    for (int i = 0; i < n; ++i)
        if (std::isnan (data[i]) || std::isinf (data[i]))
            return true;
    return false;
}

float peakAbs (const juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();
    const float* data = buf.getReadPointer (0);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
        peak = std::max (peak, std::abs (data[i]));
    return peak;
}

} // namespace

//==============================================================================
// Overdrive Tests
//==============================================================================

TEST_CASE ("Overdrive: initializes without crash", "[overdrive][init]")
{
    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;

    REQUIRE_NOTHROW (od.prepare (spec));
}

TEST_CASE ("Overdrive: bypass passes signal unchanged", "[overdrive][bypass]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 1024;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    // enabledParam = nullptr -> bypass by default (false)

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.5f);

    // Copy input for comparison
    juce::AudioBuffer<float> inputCopy (1, numSamples);
    inputCopy.copyFrom (0, 0, buffer, 0, 0, numSamples);

    od.process (buffer);

    // Output should equal input when bypassed
    const float* out = buffer.getReadPointer (0);
    const float* inp = inputCopy.getReadPointer (0);
    float maxDiff = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - inp[i]));

    REQUIRE (maxDiff < 1e-6f);
}

TEST_CASE ("Overdrive Tube: produces saturated output", "[overdrive][tube]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 2048;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    // Set up parameters via atomic values
    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 0.0f };  // Tube
    std::atomic<float> drive { 0.7f };
    std::atomic<float> tone { 0.5f };
    std::atomic<float> dryBlend { 0.0f };  // full wet

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.8f);

    od.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
    REQUIRE (computeRMS (buffer) > 0.001f);
    // Saturated output should be bounded
    REQUIRE (peakAbs (buffer) < 5.0f);
}

TEST_CASE ("Overdrive JFET: produces non-zero bounded output", "[overdrive][jfet]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 2048;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 1.0f };  // JFET
    std::atomic<float> drive { 0.6f };
    std::atomic<float> tone { 0.5f };
    std::atomic<float> dryBlend { 0.0f };

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.8f);

    od.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
    REQUIRE (computeRMS (buffer) > 0.001f);
    REQUIRE (peakAbs (buffer) < 5.0f);
}

TEST_CASE ("Overdrive Fuzz: produces non-zero bounded output with 8x OS",
           "[overdrive][fuzz]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 2048;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 2.0f };  // Fuzz
    std::atomic<float> drive { 0.8f };
    std::atomic<float> tone { 0.5f };
    std::atomic<float> dryBlend { 0.0f };

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.8f);

    od.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
    REQUIRE (computeRMS (buffer) > 0.001f);
    // Fuzz hard clipping should bound the output
    REQUIRE (peakAbs (buffer) < 5.0f);
}

TEST_CASE ("Overdrive: dry blend at 1.0 passes clean signal",
           "[overdrive][dryblend]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 2048;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 0.0f };  // Tube
    std::atomic<float> drive { 1.0f }; // high drive
    std::atomic<float> tone { 1.0f };
    std::atomic<float> dryBlend { 1.0f };  // full dry

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.5f);

    juce::AudioBuffer<float> inputCopy (1, numSamples);
    inputCopy.copyFrom (0, 0, buffer, 0, 0, numSamples);

    od.process (buffer);

    // With full dry blend, output should closely match input
    const float* out = buffer.getReadPointer (0);
    const float* inp = inputCopy.getReadPointer (0);
    float maxDiff = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - inp[i]));

    INFO ("Max diff with full dry blend: " << maxDiff);
    // Allow small tolerance for oversampling filter artifacts
    REQUIRE (maxDiff < 0.1f);
}

TEST_CASE ("Overdrive: three types produce different outputs",
           "[overdrive][types]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 2048;

    auto processWithType = [&] (int odType) -> float
    {
        Overdrive od;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels      = 1;
        od.prepare (spec);

        std::atomic<float> enabled { 1.0f };
        std::atomic<float> type { static_cast<float> (odType) };
        std::atomic<float> drive { 0.7f };
        std::atomic<float> tone { 0.5f };
        std::atomic<float> dryBlend { 0.0f };

        od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

        juce::AudioBuffer<float> buffer (1, numSamples);
        fillSine (buffer, 440.0f, sampleRate, 0.8f);
        od.process (buffer);

        return computeRMS (buffer);
    };

    float rmsT = processWithType (0);  // Tube
    float rmsJ = processWithType (1);  // JFET
    float rmsF = processWithType (2);  // Fuzz

    INFO ("Tube RMS: " << rmsT << ", JFET RMS: " << rmsJ << ", Fuzz RMS: " << rmsF);

    // All three should produce non-zero output
    REQUIRE (rmsT > 0.001f);
    REQUIRE (rmsJ > 0.001f);
    REQUIRE (rmsF > 0.001f);

    // At least two of the three should differ (different saturation characteristics)
    bool tubeJfetDiffer = std::abs (rmsT - rmsJ) > 0.01f;
    bool tubeFuzzDiffer = std::abs (rmsT - rmsF) > 0.01f;
    bool jfetFuzzDiffer = std::abs (rmsJ - rmsF) > 0.01f;

    REQUIRE ((tubeJfetDiffer || tubeFuzzDiffer || jfetFuzzDiffer));
}

TEST_CASE ("Overdrive: getLatencyInSamples returns positive after prepare",
           "[overdrive][latency]")
{
    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    od.prepare (spec);

    REQUIRE (od.getLatencyInSamples() > 0);
}

//==============================================================================
// Preamp tests (kept from original OverdriveTest.cpp)
//==============================================================================

TEST_CASE ("Preamp: initializes without crash", "[preamp][init]")
{
    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels     = 1;

    REQUIRE_NOTHROW (preamp.prepare (spec));
}

TEST_CASE ("Preamp: getLatencyInSamples returns positive value after prepare",
           "[preamp][latency]")
{
    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels     = 1;
    preamp.prepare (spec);

    REQUIRE (preamp.getLatencyInSamples() > 0);
}

TEST_CASE ("Preamp: output is non-zero for non-zero input", "[preamp][basic]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples    = 1024;

    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels     = 1;
    preamp.prepare (spec);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.5f);

    preamp.process (buffer);

    REQUIRE (computeRMS (buffer) > 0.001f);
}

TEST_CASE ("Preamp: output does not contain NaN or Inf under high drive", "[preamp][safety]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples    = 1024;

    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels     = 1;
    preamp.prepare (spec);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 1000.0f, sampleRate, 1.0f);

    preamp.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
}
