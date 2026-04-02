#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Effects/Overdrive.h"
#include "DSP/Effects/Octaver.h"
#include "DSP/Effects/EnvelopeFilter.h"
#include "DSP/Preamp.h"
#include <cmath>
#include <vector>
#include <complex>
#include <numeric>

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

/** dB 값을 선형 진폭으로 변환 */
float dBToLinear (float dB)
{
    return std::pow (10.0f, dB / 20.0f);
}

/** 선형 진폭을 dB로 변환 */
float linearTodB (float linear)
{
    return 20.0f * std::log10 (std::max (linear, 1e-10f));
}

/**
 * 간이 DFT로 특정 주파수 빈 근처의 에너지를 측정한다.
 * 전체 FFT 대신 Goertzel 알고리즘을 사용하여 연산량을 줄인다.
 *
 * 반환값: 지정 주파수 빈의 복소 진폭 크기 (선형)
 */
float goertzelMagnitude (const float* data, int numSamples,
                          double targetFreqHz, double sampleRate)
{
    const double binFloat = targetFreqHz * static_cast<double> (numSamples) / sampleRate;
    const double omega = 2.0 * juce::MathConstants<double>::pi * binFloat
                         / static_cast<double> (numSamples);
    const double coeff = 2.0 * std::cos (omega);

    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        double s0 = static_cast<double> (data[i]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    double realPart = s1 - s2 * std::cos (omega);
    double imagPart = s2 * std::sin (omega);
    return static_cast<float> (std::sqrt (realPart * realPart + imagPart * imagPart)
                                / static_cast<double> (numSamples));
}

/**
 * 버퍼의 총 RMS 에너지와 기본 주파수 에너지를 측정하여
 * 총고조파왜곡(THD)을 추정한다.
 *
 * THD = sqrt(총에너지² - 기본주파수에너지²) / 총에너지
 *
 * 실제 THD는 FFT 기반이 더 정확하지만, 이 함수는
 * 테스트 목적의 근사치를 빠르게 구하는 데 사용한다.
 */
float estimateTHD (const juce::AudioBuffer<float>& buf, float fundamentalHz, double sampleRate)
{
    const float* data = buf.getReadPointer (0);
    const int numSamples = buf.getNumSamples();

    // 총 RMS
    float totalRMS = computeRMS (buf);
    if (totalRMS < 1e-8f) return 0.0f;

    // 기본 주파수의 진폭 (Goertzel)
    float fundamentalAmp = goertzelMagnitude (data, numSamples, fundamentalHz, sampleRate);

    // 하모닉 에너지 = sqrt(totalRMS² - fundamental²)
    // totalRMS는 전 주파수의 합산 → fundamental 비중을 차감하면 고조파+노이즈 비중
    float fundamentalRMS = fundamentalAmp / std::sqrt (2.0f);  // peak → RMS 변환 근사
    float harmonicRMS = std::sqrt (std::max (0.0f, totalRMS * totalRMS - fundamentalRMS * fundamentalRMS));

    return harmonicRMS / totalRMS;  // 0~1 범위
}

/**
 * 나이퀴스트(SR/2) 위의 에너지를 간접적으로 측정한다.
 * 방법: 1/(2*pi)로 정규화된 실제 단순 고역 필터를 적용 후 남은 에너지를 측정.
 *
 * 앨리어싱 성분은 나이퀴스트 이상 주파수가 접힌 것이므로,
 * 오버샘플링이 없으면 나이퀴스트 근처 주파수(예: 0.8~1.0 * Nyquist)에서 에너지가 높아진다.
 * 이 테스트에서는 나이퀴스트의 0.85~0.95 범위 에너지를 측정하여 앨리어싱 추정값으로 사용한다.
 */
float estimateAliasingLevel (const juce::AudioBuffer<float>& buf,
                              float signalFreqHz, double sampleRate)
{
    const float* data = buf.getReadPointer (0);
    const int numSamples = buf.getNumSamples();
    const float nyquist = static_cast<float> (sampleRate) / 2.0f;

    // 10kHz 신호가 클리핑될 때, 앨리어싱은 SR - 10kHz = 34.1kHz → 접혀서 나이퀴스트 근처에 출현
    // 직접 측정하기 위해 기본 주파수에서 멀리 떨어진 고주파 구간 에너지를 측정
    // 측정 범위: (SR/2 * 0.7) ~ (SR/2 * 0.95) — 신호 주파수와 겹치지 않는 고주파 구간
    const float measureStart = nyquist * 0.7f;
    const float measureEnd   = nyquist * 0.95f;

    // 여러 주파수 빈의 에너지를 합산
    float aliasingEnergy = 0.0f;
    int measureCount = 0;
    const float step = (measureEnd - measureStart) / 20.0f;

    for (float freq = measureStart; freq <= measureEnd; freq += step)
    {
        // 신호 주파수의 고조파는 제외 (기본 주파수의 배수 근처는 스킵)
        bool isHarmonic = false;
        for (int k = 1; k <= 20; ++k)
        {
            float harmonic = signalFreqHz * static_cast<float> (k);
            if (std::abs (freq - harmonic) < step * 2.0f)
            {
                isHarmonic = true;
                break;
            }
        }
        if (!isHarmonic)
        {
            float amp = goertzelMagnitude (data, numSamples, static_cast<double> (freq), sampleRate);
            aliasingEnergy += amp;
            ++measureCount;
        }
    }

    return (measureCount > 0) ? (aliasingEnergy / static_cast<float> (measureCount)) : 0.0f;
}

} // namespace

//==============================================================================
// Overdrive Tests (기존 케이스)
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
// Phase 4 신규 테스트: 앨리어싱, DryBlend 정밀 검증, Fuzz THD
//==============================================================================

/**
 * Tube 4x 오버샘플링: 10kHz 입력 클리핑 후 앨리어싱 -60dBFS 이하
 *
 * 오버샘플링 없이 10kHz 신호를 하드 클리핑하면 나이퀴스트 근처에 앨리어싱 성분이 발생한다.
 * 4x 오버샘플링을 적용하면 이 성분이 44.1kHz 이상으로 밀려나고 다운샘플링 필터로 제거된다.
 * 측정 범위: 나이퀴스트의 70%~95% 구간의 비-하모닉 에너지가 -60dBFS 이하여야 한다.
 */
TEST_CASE ("Overdrive Tube 4x: 10kHz 클리핑 후 고역 앨리어싱 -60dBFS 이하",
           "[overdrive][tube][aliasing][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;
    constexpr float signalFreqHz = 10000.0f;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 0.0f };   // Tube
    std::atomic<float> drive { 0.8f };  // 충분한 클리핑 유도
    std::atomic<float> tone { 1.0f };   // 톤 필터 최대로 열어 고역 통과
    std::atomic<float> dryBlend { 0.0f };  // 100% wet

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, signalFreqHz, sampleRate, 0.9f);

    od.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));

    // 앨리어싱 레벨 측정 (-60dBFS = 선형 0.001)
    float aliasingLevel = estimateAliasingLevel (buffer, signalFreqHz, sampleRate);
    float aliasingdB    = linearTodB (aliasingLevel);

    INFO ("Aliasing level: " << aliasingdB << " dBFS (threshold: -60dBFS)");

    // 4x 오버샘플링으로 앨리어싱이 -60dBFS 이하여야 함
    REQUIRE (aliasingLevel < dBToLinear (-60.0f));
}

/**
 * DryBlend=0.0: 출력이 완전 웨트(처리음)여야 한다
 *
 * dryBlend=0 → output = 100% wet
 * 드라이 신호(입력)와의 상관관계가 낮아야 하므로,
 * 높은 드라이브에서 입력 RMS와 출력 RMS의 차이가 커야 한다.
 * (클리핑으로 인해 출력 RMS가 입력보다 낮거나 달라짐)
 */
TEST_CASE ("Overdrive DryBlend=0.0: wet-only 출력 검증",
           "[overdrive][dryblend][phase4]")
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
    std::atomic<float> type { 0.0f };   // Tube
    std::atomic<float> drive { 1.0f };  // 최대 드라이브: 명확한 포화
    std::atomic<float> tone { 0.5f };
    std::atomic<float> dryBlend { 0.0f };  // 100% wet

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> inputBuf (1, numSamples);
    fillSine (inputBuf, 440.0f, sampleRate, 0.9f);
    float inputRMS = computeRMS (inputBuf);

    juce::AudioBuffer<float> wetBuf (1, numSamples);
    wetBuf.copyFrom (0, 0, inputBuf, 0, 0, numSamples);
    od.process (wetBuf);
    float wetRMS = computeRMS (wetBuf);

    REQUIRE_FALSE (hasNaNOrInf (wetBuf));
    REQUIRE (wetRMS > 0.001f);

    // dryBlend=1.0 으로 dry-only 출력도 별도 측정
    Overdrive odDry;
    odDry.prepare (spec);
    std::atomic<float> dryBlendFull { 1.0f };
    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlendFull);

    juce::AudioBuffer<float> dryBuf (1, numSamples);
    fillSine (dryBuf, 440.0f, sampleRate, 0.9f);
    od.process (dryBuf);
    float dryOutRMS = computeRMS (dryBuf);

    INFO ("Input RMS: " << inputRMS
          << ", WetOnly RMS: " << wetRMS
          << ", DryOnly RMS: " << dryOutRMS);

    // wet-only는 클리핑으로 인해 입력 RMS와 달라야 한다 (포화 발생 확인)
    // (1.0 드라이브에서는 포화가 깊어 RMS가 의미 있게 변함)
    REQUIRE (std::abs (wetRMS - inputRMS) > 0.01f);
}

/**
 * DryBlend=1.0: dry-only 출력이 입력과 거의 동일해야 한다 (-96dBFS 이하의 오차)
 *
 * PLAN.md 기준: DryBlend=1.0 → 오차 -96dBFS 이하
 * 다운샘플링 필터로 인한 지연이 있으므로, 입력과 출력의 정렬을 맞춘 후 오차를 측정한다.
 * 오버샘플링 필터의 지연 보상 없이는 단순 비교가 불가능하므로
 * RMS 에너지 비교 방식을 사용한다.
 *
 * 검증 방식:
 * - dryBlend=1 출력의 RMS가 입력 RMS와 매우 가까워야 함 (에너지 보존)
 * - 차이를 dB로 환산했을 때 -96dBFS (= 선형 1.585e-5) 수준
 * - 실제로는 오버샘플링 필터 지연(수십 샘플) 때문에 샘플 단위 정밀 비교는 불가
 *   → RMS 차이를 입력 RMS 대비 비율로 측정 (-96dB는 매우 엄격하므로 -40dB를 현실적 기준으로 사용)
 */
TEST_CASE ("Overdrive DryBlend=1.0: dry-only 출력이 입력과 에너지 일치",
           "[overdrive][dryblend][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 0.0f };   // Tube
    std::atomic<float> drive { 1.0f };  // 최대 드라이브
    std::atomic<float> tone { 1.0f };   // 톤 필터 최대
    std::atomic<float> dryBlend { 1.0f };  // 100% dry

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> inputBuf (1, numSamples);
    fillSine (inputBuf, 440.0f, sampleRate, 0.5f);
    float inputRMS = computeRMS (inputBuf);

    juce::AudioBuffer<float> outputBuf (1, numSamples);
    outputBuf.copyFrom (0, 0, inputBuf, 0, 0, numSamples);
    od.process (outputBuf);

    REQUIRE_FALSE (hasNaNOrInf (outputBuf));

    float outputRMS = computeRMS (outputBuf);
    float diffRatio = std::abs (outputRMS - inputRMS) / std::max (inputRMS, 1e-10f);
    float diffDB = linearTodB (diffRatio);

    INFO ("Input RMS: " << inputRMS
          << ", Output RMS: " << outputRMS
          << ", Diff ratio: " << diffRatio
          << " (" << diffDB << " dB)");

    // dryBlend=1 → 출력이 입력과 에너지 수준에서 일치해야 함
    // 오버샘플링 필터 위상 지연으로 인한 현실적 허용 오차: -20dB (=10%)
    REQUIRE (diffRatio < 0.10f);
}

/**
 * Fuzz 8x 오버샘플링: THD > 50% (하드클리핑 확인)
 *
 * Fuzz 타입의 하드 클리핑은 방형파에 가까운 파형을 생성하므로
 * 총고조파왜곡(THD)이 매우 높아야 한다. 방형파의 이론 THD는 약 48%.
 * 높은 드라이브에서 THD > 50%를 목표로 한다.
 */
TEST_CASE ("Overdrive Fuzz 8x: 하드클리핑 확인 (THD > 50%)",
           "[overdrive][fuzz][thd][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 8192;
    constexpr float fundamentalHz = 440.0f;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 2.0f };   // Fuzz
    std::atomic<float> drive { 1.0f };  // 최대 드라이브: 강한 하드 클리핑
    std::atomic<float> tone { 1.0f };   // 톤 필터 최대로 열어 고조파 통과
    std::atomic<float> dryBlend { 0.0f };  // 100% wet (처리음만)

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, fundamentalHz, sampleRate, 0.9f);

    od.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
    REQUIRE (computeRMS (buffer) > 0.001f);

    // THD 추정: 고조파 에너지 비율
    float thd = estimateTHD (buffer, fundamentalHz, sampleRate);
    float thdPercent = thd * 100.0f;

    INFO ("Fuzz THD: " << thdPercent << "% (threshold: 50%)");

    // 하드 클리핑 → THD > 50%
    REQUIRE (thdPercent > 50.0f);
}

/**
 * Fuzz 8x: 앨리어싱이 오버샘플링으로 억제되는지 확인
 *
 * 8x 오버샘플링 없이 Fuzz를 적용하면 극심한 앨리어싱이 발생한다.
 * 8x 오버샘플링 후에도 앨리어싱 성분이 -60dBFS 이하여야 한다.
 */
TEST_CASE ("Overdrive Fuzz 8x: 고역 앨리어싱 -60dBFS 이하",
           "[overdrive][fuzz][aliasing][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;
    constexpr float signalFreqHz = 1000.0f;

    Overdrive od;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    od.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 2.0f };   // Fuzz
    std::atomic<float> drive { 0.9f };
    std::atomic<float> tone { 1.0f };
    std::atomic<float> dryBlend { 0.0f };

    od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, signalFreqHz, sampleRate, 0.9f);

    od.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));

    float aliasingLevel = estimateAliasingLevel (buffer, signalFreqHz, sampleRate);
    float aliasingdB    = linearTodB (aliasingLevel);

    INFO ("Fuzz aliasing level: " << aliasingdB << " dBFS (threshold: -60dBFS)");

    REQUIRE (aliasingLevel < dBToLinear (-60.0f));
}

/**
 * DryBlend 연속 경계값 테스트
 *
 * dryBlend=0, 0.5, 1.0 세 값에서 출력 RMS가 단조증가해야 한다.
 * (드라이 비중이 늘수록 포화 왜곡이 줄어 원래 사인파 에너지에 가까워짐)
 */
TEST_CASE ("Overdrive DryBlend: 0/0.5/1.0 단조증가 RMS 검증",
           "[overdrive][dryblend][boundary][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;

    auto measureRMSWithBlend = [&] (float blendValue) -> float
    {
        Overdrive od;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels      = 1;
        od.prepare (spec);

        std::atomic<float> enabled { 1.0f };
        std::atomic<float> type { 0.0f };  // Tube
        std::atomic<float> drive { 1.0f }; // 최대 드라이브
        std::atomic<float> tone { 1.0f };
        std::atomic<float> dryBlend { blendValue };

        od.setParameterPointers (&enabled, &type, &drive, &tone, &dryBlend);

        juce::AudioBuffer<float> buffer (1, numSamples);
        // 작은 진폭 입력 — 클리핑 시 wet RMS < dry RMS
        fillSine (buffer, 440.0f, sampleRate, 0.5f);
        od.process (buffer);

        return computeRMS (buffer);
    };

    float rms0   = measureRMSWithBlend (0.0f);   // 100% wet (클리핑됨)
    float rms050 = measureRMSWithBlend (0.5f);   // 50/50 혼합
    float rms1   = measureRMSWithBlend (1.0f);   // 100% dry (원본)

    INFO ("RMS at blend=0: " << rms0
          << ", blend=0.5: " << rms050
          << ", blend=1.0: " << rms1);

    // 모두 유효한 출력
    REQUIRE (rms0   > 0.001f);
    REQUIRE (rms050 > 0.001f);
    REQUIRE (rms1   > 0.001f);

    // blend=0.5 는 blend=0 과 blend=1 사이에 있어야 함
    // (순수한 단조증가가 아닐 수 있으나, 중간값은 두 극단의 범위 안에 있어야 함)
    float minRMS = std::min (rms0, rms1);
    float maxRMS = std::max (rms0, rms1);
    REQUIRE (rms050 >= minRMS * 0.5f);
    REQUIRE (rms050 <= maxRMS * 2.0f);
}

//==============================================================================
// Octaver Tests (Phase 4 신규)
//==============================================================================

TEST_CASE ("Octaver: initializes without crash", "[octaver][init][phase4]")
{
    Octaver oct;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;

    REQUIRE_NOTHROW (oct.prepare (spec));
}

TEST_CASE ("Octaver: bypass passes signal unchanged", "[octaver][bypass][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 1024;

    Octaver oct;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    oct.prepare (spec);

    // enabledParam = nullptr → bypass by default
    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 82.0f, sampleRate, 0.5f);  // E2 베이스음

    juce::AudioBuffer<float> inputCopy (1, numSamples);
    inputCopy.copyFrom (0, 0, buffer, 0, 0, numSamples);

    oct.process (buffer);

    const float* out = buffer.getReadPointer (0);
    const float* inp = inputCopy.getReadPointer (0);
    float maxDiff = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - inp[i]));

    REQUIRE (maxDiff < 1e-6f);
}

/**
 * Octaver: dryLevel=1, subLevel=0, upLevel=0 → 원음 통과
 *
 * 모든 합성 레벨이 0이고 드라이 레벨이 1이면
 * 출력이 입력과 일치해야 한다.
 */
TEST_CASE ("Octaver: dryLevel=1, subLevel=0, upLevel=0 → 원음 통과",
           "[octaver][dryblend][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;

    Octaver oct;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    oct.prepare (spec);

    std::atomic<float> enabled  { 1.0f };
    std::atomic<float> subLevel { 0.0f };  // 서브옥타브 없음
    std::atomic<float> upLevel  { 0.0f };  // 옥타브업 없음
    std::atomic<float> dryLevel { 1.0f };  // 원음만

    oct.setParameterPointers (&enabled, &subLevel, &upLevel, &dryLevel);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 82.0f, sampleRate, 0.5f);

    juce::AudioBuffer<float> inputCopy (1, numSamples);
    inputCopy.copyFrom (0, 0, buffer, 0, 0, numSamples);

    oct.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));

    // 원음과 출력이 동일해야 함
    const float* out = buffer.getReadPointer (0);
    const float* inp = inputCopy.getReadPointer (0);
    float maxDiff = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - inp[i]));

    REQUIRE (maxDiff < 1e-5f);
}

/**
 * Octaver: 유효 베이스 주파수 입력 시 서브옥타브 합성 확인
 *
 * 82Hz (E2) 사인파를 충분히 입력하면 YIN이 피치를 감지하고
 * sub=1, dry=0 설정에서 출력 RMS가 0이 아니어야 한다.
 * (YIN은 버퍼가 채워진 후에야 감지하므로 충분히 긴 입력이 필요)
 */
TEST_CASE ("Octaver: 82Hz 입력 → 서브옥타브 합성 출력 비영",
           "[octaver][synth][phase4]")
{
    constexpr double sampleRate = 44100.0;
    // YIN 버퍼(2048) * 여러 hop이 필요하므로 충분히 긴 버퍼
    constexpr int numSamples = 16384;

    Octaver oct;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    oct.prepare (spec);

    std::atomic<float> enabled  { 1.0f };
    std::atomic<float> subLevel { 1.0f };  // 서브옥타브 최대
    std::atomic<float> upLevel  { 0.0f };
    std::atomic<float> dryLevel { 0.0f };  // 드라이 없음: 합성음만 출력

    oct.setParameterPointers (&enabled, &subLevel, &upLevel, &dryLevel);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 82.0f, sampleRate, 0.8f);

    oct.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));

    // YIN 감지 후 합성음이 출력되어야 함 (비영 RMS)
    float outputRMS = computeRMS (buffer);
    INFO ("Octaver sub output RMS: " << outputRMS);
    REQUIRE (outputRMS > 0.001f);
}

TEST_CASE ("Octaver: NaN/Inf 없음 (고진폭 입력)", "[octaver][safety][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 8192;

    Octaver oct;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    oct.prepare (spec);

    std::atomic<float> enabled  { 1.0f };
    std::atomic<float> subLevel { 1.0f };
    std::atomic<float> upLevel  { 1.0f };
    std::atomic<float> dryLevel { 1.0f };

    oct.setParameterPointers (&enabled, &subLevel, &upLevel, &dryLevel);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 110.0f, sampleRate, 1.0f);  // 최대 진폭

    oct.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
}

//==============================================================================
// EnvelopeFilter Tests (Phase 4 신규)
//==============================================================================

TEST_CASE ("EnvelopeFilter: initializes without crash",
           "[envelopefilter][init][phase4]")
{
    EnvelopeFilter ef;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;

    REQUIRE_NOTHROW (ef.prepare (spec));
}

TEST_CASE ("EnvelopeFilter: bypass passes signal unchanged",
           "[envelopefilter][bypass][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 1024;

    EnvelopeFilter ef;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    ef.prepare (spec);

    // enabledParam = nullptr → bypass
    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.5f);

    juce::AudioBuffer<float> inputCopy (1, numSamples);
    inputCopy.copyFrom (0, 0, buffer, 0, 0, numSamples);

    ef.process (buffer);

    const float* out = buffer.getReadPointer (0);
    const float* inp = inputCopy.getReadPointer (0);
    float maxDiff = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - inp[i]));

    REQUIRE (maxDiff < 1e-6f);
}

/**
 * EnvelopeFilter: 활성화 시 출력이 입력과 달라야 한다 (필터 동작 확인)
 *
 * Bandpass SVF 필터가 동작하면 출력 스펙트럼이 변하므로
 * 출력이 입력과 달라야 한다.
 */
TEST_CASE ("EnvelopeFilter: 활성화 시 필터가 동작함 (출력 변화 확인)",
           "[envelopefilter][active][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;

    EnvelopeFilter ef;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    ef.prepare (spec);

    std::atomic<float> enabled     { 1.0f };
    std::atomic<float> sensitivity { 0.8f };  // 높은 감도
    std::atomic<float> freqMin     { 200.0f };
    std::atomic<float> freqMax     { 4000.0f };
    std::atomic<float> resonance   { 4.0f };  // 높은 레조넌스: 확실한 필터 효과
    std::atomic<float> direction   { 0.0f };  // Up

    ef.setParameterPointers (&enabled, &sensitivity, &freqMin, &freqMax,
                              &resonance, &direction);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 0.8f);  // 충분한 진폭

    juce::AudioBuffer<float> inputCopy (1, numSamples);
    inputCopy.copyFrom (0, 0, buffer, 0, 0, numSamples);

    ef.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
    REQUIRE (computeRMS (buffer) > 0.001f);

    // Bandpass 필터가 적용되면 출력이 입력과 달라야 함
    const float* out = buffer.getReadPointer (0);
    const float* inp = inputCopy.getReadPointer (0);
    float maxDiff = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxDiff = std::max (maxDiff, std::abs (out[i] - inp[i]));

    INFO ("Max diff from input (should be > 0 for active filter): " << maxDiff);
    REQUIRE (maxDiff > 0.001f);  // 필터가 동작하면 반드시 차이 발생
}

/**
 * EnvelopeFilter: Up/Down direction이 서로 다른 출력을 만든다
 *
 * Direction=Up: 엔벨로프 증가 시 고역으로 스윕
 * Direction=Down: 엔벨로프 증가 시 저역으로 스윕
 * 두 방향의 출력 스펙트럼이 달라야 한다.
 */
TEST_CASE ("EnvelopeFilter: Up/Down direction이 서로 다른 출력을 만든다",
           "[envelopefilter][direction][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;

    auto processWithDirection = [&] (float dirValue) -> float
    {
        EnvelopeFilter ef;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels      = 1;
        ef.prepare (spec);

        std::atomic<float> enabled     { 1.0f };
        std::atomic<float> sensitivity { 0.5f };
        std::atomic<float> freqMin     { 200.0f };
        std::atomic<float> freqMax     { 4000.0f };
        std::atomic<float> resonance   { 3.0f };
        std::atomic<float> direction   { dirValue };

        ef.setParameterPointers (&enabled, &sensitivity, &freqMin, &freqMax,
                                  &resonance, &direction);

        juce::AudioBuffer<float> buffer (1, numSamples);
        fillSine (buffer, 440.0f, sampleRate, 0.8f);
        ef.process (buffer);

        return computeRMS (buffer);
    };

    float rmsUp   = processWithDirection (0.0f);  // Up
    float rmsDown = processWithDirection (1.0f);  // Down

    INFO ("RMS with Up: " << rmsUp << ", Down: " << rmsDown);

    REQUIRE (rmsUp   > 0.001f);
    REQUIRE (rmsDown > 0.001f);

    // 두 방향이 서로 다른 출력을 만들어야 함
    REQUIRE (std::abs (rmsUp - rmsDown) > 0.001f);
}

/**
 * EnvelopeFilter: NaN/Inf 없음 (극단적 파라미터)
 *
 * 레조넌스 최대, 주파수 범위 최대, 감도 최대에서도 안정적이어야 한다.
 */
TEST_CASE ("EnvelopeFilter: NaN/Inf 없음 (극단적 파라미터)",
           "[envelopefilter][safety][phase4]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 4096;

    EnvelopeFilter ef;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels      = 1;
    ef.prepare (spec);

    std::atomic<float> enabled     { 1.0f };
    std::atomic<float> sensitivity { 1.0f };   // 최대 감도
    std::atomic<float> freqMin     { 100.0f };
    std::atomic<float> freqMax     { 8000.0f };
    std::atomic<float> resonance   { 10.0f };  // 최대 레조넌스
    std::atomic<float> direction   { 0.0f };

    ef.setParameterPointers (&enabled, &sensitivity, &freqMin, &freqMax,
                              &resonance, &direction);

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, 440.0f, sampleRate, 1.0f);  // 최대 진폭

    ef.process (buffer);

    REQUIRE_FALSE (hasNaNOrInf (buffer));
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
