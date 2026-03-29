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
