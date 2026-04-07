#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/DIBlend.h"
#include <cmath>

using Catch::Approx;

namespace
{
    constexpr int blockSize = 256;

    juce::AudioBuffer<float> makeConstant (float value, int numSamples)
    {
        juce::AudioBuffer<float> buf (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buf.setSample (0, i, value);
        return buf;
    }
}

TEST_CASE ("DIBlend: blend=0 outputs clean DI only", "[diblend]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal { 0.0f };
    std::atomic<float> cleanLevel { 0.0f };
    std::atomic<float> procLevel { 0.0f };
    std::atomic<float> irPos { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI = makeConstant (0.5f, blockSize);
    auto processed = makeConstant (1.0f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // blend=0: output = cleanDI * 1.0 * (1-0) + processed * 1.0 * 0 = cleanDI
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Approx (0.5f).margin (1e-6f));
}

TEST_CASE ("DIBlend: blend=1 outputs processed only", "[diblend]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal { 1.0f };
    std::atomic<float> cleanLevel { 0.0f };
    std::atomic<float> procLevel { 0.0f };
    std::atomic<float> irPos { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI = makeConstant (0.5f, blockSize);
    auto processed = makeConstant (0.8f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // blend=1: output = cleanDI * 1.0 * 0 + processed * 1.0 * 1 = processed
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Approx (0.8f).margin (1e-6f));
}

TEST_CASE ("DIBlend: blend=0.5 mixes equally", "[diblend]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal { 0.5f };
    std::atomic<float> cleanLevel { 0.0f };
    std::atomic<float> procLevel { 0.0f };
    std::atomic<float> irPos { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI = makeConstant (1.0f, blockSize);
    auto processed = makeConstant (0.0f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // blend=0.5: output = 1.0 * 1.0 * 0.5 + 0.0 * 1.0 * 0.5 = 0.5
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Approx (0.5f).margin (1e-6f));
}

TEST_CASE ("DIBlend: level trim applies correctly", "[diblend]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    // Clean level +6dB, processed level -6dB, blend=0.5
    std::atomic<float> blendVal { 0.5f };
    std::atomic<float> cleanLevel { 6.0f };   // gain = ~2.0
    std::atomic<float> procLevel { -6.0f };   // gain = ~0.5
    std::atomic<float> irPos { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI = makeConstant (1.0f, blockSize);
    auto processed = makeConstant (1.0f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // Expected: 1.0 * 10^(6/20) * 0.5 + 1.0 * 10^(-6/20) * 0.5
    float cleanGain = std::pow (10.0f, 6.0f / 20.0f);
    float procGain = std::pow (10.0f, -6.0f / 20.0f);
    float expected = cleanGain * 0.5f + procGain * 0.5f;

    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Approx (expected).margin (1e-4f));
}

TEST_CASE ("DIBlend: IR position returns correct value", "[diblend]")
{
    DIBlend blend;

    // Default (no param): Post-IR (0)
    REQUIRE (blend.getIRPosition() == 0);

    std::atomic<float> irPos { 1.0f };
    blend.setParameterPointers (nullptr, nullptr, nullptr, &irPos);
    REQUIRE (blend.getIRPosition() == 1);

    irPos.store (0.0f);
    REQUIRE (blend.getIRPosition() == 0);
}

// Phase 6: cleanDI only (blend=0) 오차 1e-6 이하 검증
TEST_CASE ("DIBlend: blend=0 clean-only output matches cleanDI with 1e-6 tolerance", "[diblend][boundary]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal { 0.0f };
    std::atomic<float> cleanLevel { 0.0f };   // 0dB = gain 1.0
    std::atomic<float> procLevel  { 0.0f };
    std::atomic<float> irPos      { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    // Use non-trivial input values to make the test meaningful
    auto cleanDI   = makeConstant (0.7f, blockSize);
    auto processed = makeConstant (0.3f, blockSize);   // should not appear in output
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // Expected: cleanDI * 1.0 * (1-0) + processed * 1.0 * 0 = 0.7
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Catch::Approx (0.7f).margin (1e-6f));
}

// Phase 6: processed only (blend=1) 오차 1e-6 이하 검증
TEST_CASE ("DIBlend: blend=1 processed-only output matches processed with 1e-6 tolerance", "[diblend][boundary]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal { 1.0f };
    std::atomic<float> cleanLevel { 0.0f };
    std::atomic<float> procLevel  { 0.0f };   // 0dB = gain 1.0
    std::atomic<float> irPos      { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI   = makeConstant (0.3f, blockSize);   // should not appear
    auto processed = makeConstant (0.9f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // Expected: cleanDI * 1.0 * 0 + processed * 1.0 * 1 = 0.9
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Catch::Approx (0.9f).margin (1e-6f));
}

// Phase 6: IR Position 전환 시 getIRPosition() 값 변화 검증
// (DIBlend 자체는 출력을 바꾸지 않음 — IR Position은 SignalChain에서 Cabinet 위치를 결정.
//  여기서는 파라미터 값이 올바르게 노출되는지만 검증한다.)
TEST_CASE ("DIBlend: IR position toggle changes reported position", "[diblend][irposition]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal  { 0.5f };
    std::atomic<float> cleanLevel { 0.0f };
    std::atomic<float> procLevel  { 0.0f };
    std::atomic<float> irPos      { 0.0f };  // Post-IR
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    REQUIRE (blend.getIRPosition() == 0);   // Post-IR

    // Switch to Pre-IR
    irPos.store (1.0f);
    REQUIRE (blend.getIRPosition() == 1);   // Pre-IR

    // Back to Post-IR
    irPos.store (0.0f);
    REQUIRE (blend.getIRPosition() == 0);   // Post-IR
}

// Phase 6: cleanDI level trim 정확도 검증 (±1e-4 허용)
TEST_CASE ("DIBlend: clean level trim +12dB applies correct gain", "[diblend][level]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal  { 0.0f };   // clean only
    std::atomic<float> cleanLevel { 12.0f }; // +12dB
    std::atomic<float> procLevel  { 0.0f };
    std::atomic<float> irPos      { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI   = makeConstant (0.5f, blockSize);
    auto processed = makeConstant (0.0f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // Expected: 0.5 * 10^(12/20) = 0.5 * ~3.981 = ~1.990
    float expectedGain = std::pow (10.0f, 12.0f / 20.0f);
    float expected = 0.5f * expectedGain;

    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Catch::Approx (expected).margin (1e-4f));
}

// Phase 6: processed level trim 정확도 검증 (±1e-4 허용)
TEST_CASE ("DIBlend: processed level trim -12dB applies correct gain", "[diblend][level]")
{
    DIBlend blend;
    juce::dsp::ProcessSpec spec { 44100.0, (juce::uint32) blockSize, 1 };
    blend.prepare (spec);

    std::atomic<float> blendVal  { 1.0f };   // processed only
    std::atomic<float> cleanLevel { 0.0f };
    std::atomic<float> procLevel  { -12.0f }; // -12dB
    std::atomic<float> irPos      { 0.0f };
    blend.setParameterPointers (&blendVal, &cleanLevel, &procLevel, &irPos);

    auto cleanDI   = makeConstant (0.0f, blockSize);
    auto processed = makeConstant (1.0f, blockSize);
    juce::AudioBuffer<float> output (1, blockSize);

    blend.process (cleanDI, processed, output);

    // Expected: 1.0 * 10^(-12/20) = ~0.2512
    float expected = std::pow (10.0f, -12.0f / 20.0f);

    for (int i = 0; i < blockSize; ++i)
        REQUIRE (output.getSample (0, i) == Catch::Approx (expected).margin (1e-4f));
}
