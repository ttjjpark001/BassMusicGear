#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "DSP/GraphicEQ.h"

// ---------------------------------------------------------------------------
// 헬퍼: 특정 주파수의 EQ 이득을 측정하는 함수
//
// 정현파를 생성하여 DSP를 통과시키고 입출력 RMS 비율을 계산한다.
// skipSamples 구간만큼 초기 과도응답(transient)을 제외하여 정상 상태 게인을 측정한다.
// ---------------------------------------------------------------------------
static float measureGainAtFrequency (GraphicEQ& eq, float freqHz, double sampleRate,
                                     int numSamples = 8192, int skipSamples = 2048)
{
    juce::AudioBuffer<float> buffer (1, numSamples);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // 정현파 생성: sin(2π × f × t)
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, std::sin (twoPi * freqHz / static_cast<float> (sampleRate) * static_cast<float> (i)));

    // 입력 RMS 측정 (과도응답 제외)
    float inputRms = 0.0f;
    for (int i = skipSamples; i < numSamples; ++i)
    {
        float v = buffer.getSample (0, i);
        inputRms += v * v;
    }
    inputRms = std::sqrt (inputRms / static_cast<float> (numSamples - skipSamples));

    // EQ를 통해 처리
    eq.process (buffer);

    // 출력 RMS 측정 (과도응답 제외)
    float outputRms = 0.0f;
    for (int i = skipSamples; i < numSamples; ++i)
    {
        float v = buffer.getSample (0, i);
        outputRms += v * v;
    }
    outputRms = std::sqrt (outputRms / static_cast<float> (numSamples - skipSamples));

    if (inputRms < 1e-10f) return 0.0f;
    return outputRms / inputRms;  // 이득 = 출력 RMS / 입력 RMS
}

// 선형 이득을 dB로 변환: 20×log10(gain)
static float gainToDb (float gain)
{
    return 20.0f * std::log10 (std::max (gain, 1e-10f));
}

// ---------------------------------------------------------------------------
// 헬퍼: GraphicEQ 를 단일 밴드 부스트 상태로 초기화
//
// 지정한 bandIndex 하나만 gainDb 로 설정하고 나머지는 0dB.
// enabled=1.0(활성) 상태의 파라미터 포인터를 연결한다.
// ---------------------------------------------------------------------------
struct EQTestState
{
    std::atomic<float> enabled { 1.0f };
    std::atomic<float> bandGains[GraphicEQ::numBands];
    std::atomic<float>* bandPtrs[GraphicEQ::numBands];

    EQTestState()
    {
        for (int i = 0; i < GraphicEQ::numBands; ++i)
        {
            bandGains[i].store (0.0f);
            bandPtrs[i] = &bandGains[i];
        }
    }
};

// ---------------------------------------------------------------------------
// Phase 5 테스트 1: 각 밴드 +12dB 설정 시 해당 주파수 측정값 +11.5~+12.5dB
//
// 10개 밴드(31/63/125/250/500/1k/2k/4k/8k/16kHz) 각각에 대해
// 해당 중심 주파수에서의 RMS 이득이 +12dB ±0.5dB 범위에 있는지 검증한다.
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ Phase5: each band +12dB boost accuracy (±0.5dB)", "[graphiceq][phase5][frequency]")
{
    constexpr double sampleRate = 44100.0;

    const std::array<float, GraphicEQ::numBands> bands = {
        31.0f, 63.0f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };

    // 각 밴드를 독립적으로 +12dB 부스트하고 해당 주파수에서의 이득을 확인
    for (int bandIdx = 0; bandIdx < GraphicEQ::numBands; ++bandIdx)
    {
        GraphicEQ eq;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate      = sampleRate;
        spec.maximumBlockSize = 8192;
        spec.numChannels     = 1;
        eq.prepare (spec);

        // 해당 밴드만 +12dB, 나머지 0dB
        float gains[GraphicEQ::numBands] = {};
        gains[bandIdx] = 12.0f;
        eq.updateCoefficients (gains);

        EQTestState state;
        state.bandGains[bandIdx].store (12.0f);
        eq.setParameterPointers (&state.enabled, state.bandPtrs);

        eq.reset();
        const float freq   = bands[static_cast<size_t> (bandIdx)];
        const float gain   = measureGainAtFrequency (eq, freq, sampleRate);
        const float gainDb = gainToDb (gain);

        // Phase 5 기준: +11.5dB ~ +12.5dB
        INFO ("Band " << bandIdx << " (" << freq << "Hz): measured " << gainDb << " dB");
        REQUIRE (gainDb >= 11.5f);
        REQUIRE (gainDb <= 12.5f);
    }
}

// ---------------------------------------------------------------------------
// Phase 5 테스트 2: 전 밴드 0dB 시 20Hz~20kHz ±0.5dB 평탄
//
// 전 대역을 0dB(플랫)으로 설정하고 측정 주파수를 8개 포인트로 분산하여
// 모두 ±0.5dB 이내의 평탄한 응답을 보이는지 검증한다.
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ Phase5: all bands 0dB flat response 20Hz-20kHz (±0.5dB)", "[graphiceq][phase5][flat]")
{
    constexpr double sampleRate = 44100.0;

    GraphicEQ eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = 8192;
    spec.numChannels     = 1;
    eq.prepare (spec);

    // 전 밴드 0dB
    float gains[GraphicEQ::numBands] = {};
    eq.updateCoefficients (gains);

    EQTestState state;
    eq.setParameterPointers (&state.enabled, state.bandPtrs);

    // 측정 주파수: 20Hz ~ 20kHz 범위를 로그 스케일로 분산
    const std::array<float, 12> testFreqs = {
        20.0f, 40.0f, 100.0f, 300.0f, 800.0f,
        2000.0f, 5000.0f, 10000.0f, 14000.0f, 18000.0f, 19000.0f, 20000.0f
    };

    for (float freq : testFreqs)
    {
        eq.reset();
        float gainDb = gainToDb (measureGainAtFrequency (eq, freq, sampleRate));

        INFO ("Frequency " << freq << "Hz: measured " << gainDb << " dB");
        REQUIRE (gainDb == Catch::Approx (0.0f).margin (0.5f));
    }
}

// ---------------------------------------------------------------------------
// Phase 5 테스트 3: 바이패스 ON 시 입력 = 출력 (수치 일치)
//
// enabled=0(비활성) 상태에서 극단적인 EQ 설정(±12dB)이 있어도
// 출력이 입력과 ±0.1dB 이내로 동일해야 한다.
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ Phase5: bypass ON — output equals input numerically", "[graphiceq][phase5][bypass]")
{
    constexpr double sampleRate = 44100.0;

    GraphicEQ eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = 8192;
    spec.numChannels     = 1;
    eq.prepare (spec);

    // 극단 EQ 설정 — 바이패스이므로 무관해야 함
    float gains[GraphicEQ::numBands];
    for (int i = 0; i < GraphicEQ::numBands; ++i)
        gains[i] = (i % 2 == 0) ? 12.0f : -12.0f;
    eq.updateCoefficients (gains);

    // enabled = 0.0 (바이패스)
    EQTestState state;
    state.enabled.store (0.0f);
    for (int i = 0; i < GraphicEQ::numBands; ++i)
        state.bandGains[i].store (gains[i]);
    eq.setParameterPointers (&state.enabled, state.bandPtrs);

    // 여러 주파수에서 바이패스 확인
    const std::array<float, 5> freqs = { 100.0f, 500.0f, 1000.0f, 4000.0f, 10000.0f };
    for (float freq : freqs)
    {
        eq.reset();
        float gainDb = gainToDb (measureGainAtFrequency (eq, freq, sampleRate));

        INFO ("Bypass check at " << freq << "Hz: measured " << gainDb << " dB");
        // 바이패스 = 0dB (±0.1dB 오차 허용)
        REQUIRE (gainDb == Catch::Approx (0.0f).margin (0.1f));
    }
}

// ---------------------------------------------------------------------------
// Phase 5 테스트 4 (이전 호환): 1kHz 밴드 부스트 시 31Hz 영향 미미
//
// 특정 밴드 부스트가 멀리 떨어진 밴드에 누설되지 않는지 검증한다.
// 1kHz 밴드 +12dB 설정 시 31Hz에서의 영향이 +3dB 미만이어야 한다.
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ: boosting 1kHz band increases gain at 1kHz", "[graphiceq]")
{
    constexpr double sampleRate = 44100.0;
    GraphicEQ eq;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 4096;
    spec.numChannels      = 1;
    eq.prepare (spec);

    float gains[GraphicEQ::numBands] = {};
    gains[5] = 12.0f;  // 밴드 인덱스 5 = 1kHz
    eq.updateCoefficients (gains);

    EQTestState state;
    state.bandGains[5].store (12.0f);
    eq.setParameterPointers (&state.enabled, state.bandPtrs);

    eq.reset();
    float gainAt1k  = gainToDb (measureGainAtFrequency (eq, 1000.0f, sampleRate));
    REQUIRE (gainAt1k > 6.0f);

    eq.reset();
    float gainAt31  = gainToDb (measureGainAtFrequency (eq, 31.0f, sampleRate));
    REQUIRE (gainAt31 < 3.0f);
}

// ---------------------------------------------------------------------------
// Phase 5 테스트 5 (이전 호환): 250Hz 밴드 컷 시 해당 주파수 이득 감소
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ: cutting 250Hz band decreases gain at 250Hz", "[graphiceq]")
{
    constexpr double sampleRate = 44100.0;
    GraphicEQ eq;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 4096;
    spec.numChannels      = 1;
    eq.prepare (spec);

    float gains[GraphicEQ::numBands] = {};
    gains[3] = -12.0f;  // 밴드 인덱스 3 = 250Hz
    eq.updateCoefficients (gains);

    EQTestState state;
    state.bandGains[3].store (-12.0f);
    eq.setParameterPointers (&state.enabled, state.bandPtrs);

    eq.reset();
    float gainAt250 = gainToDb (measureGainAtFrequency (eq, 250.0f, sampleRate));
    REQUIRE (gainAt250 < -6.0f);
}

// ---------------------------------------------------------------------------
// Phase 5 테스트 6 (이전 호환): 전 밴드 0dB 플랫 응답 — 구 테스트 호환
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ: flat response when all bands at 0dB", "[graphiceq]")
{
    constexpr double sampleRate = 44100.0;
    GraphicEQ eq;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 4096;
    spec.numChannels      = 1;
    eq.prepare (spec);

    float gains[GraphicEQ::numBands] = {};
    eq.updateCoefficients (gains);

    EQTestState state;
    eq.setParameterPointers (&state.enabled, state.bandPtrs);

    const float testFreqs[] = { 100.0f, 500.0f, 1000.0f, 4000.0f, 10000.0f };
    for (float freq : testFreqs)
    {
        eq.reset();
        float gainDb = gainToDb (measureGainAtFrequency (eq, freq, sampleRate));
        REQUIRE (gainDb == Catch::Approx (0.0f).margin (0.5f));
    }
}

// ---------------------------------------------------------------------------
// Phase 5 테스트 7 (이전 호환): 바이패스 구 테스트 호환
// ---------------------------------------------------------------------------
TEST_CASE ("GraphicEQ: bypass passes signal unchanged", "[graphiceq]")
{
    constexpr double sampleRate = 44100.0;
    GraphicEQ eq;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 4096;
    spec.numChannels      = 1;
    eq.prepare (spec);

    float gains[GraphicEQ::numBands];
    for (int i = 0; i < GraphicEQ::numBands; ++i)
        gains[i] = 12.0f;
    eq.updateCoefficients (gains);

    EQTestState state;
    state.enabled.store (0.0f);  // DISABLED
    for (int i = 0; i < GraphicEQ::numBands; ++i)
        state.bandGains[i].store (gains[i]);
    eq.setParameterPointers (&state.enabled, state.bandPtrs);

    eq.reset();
    float gainDb = gainToDb (measureGainAtFrequency (eq, 1000.0f, sampleRate));
    REQUIRE (gainDb == Catch::Approx (0.0f).margin (0.5f));
}
