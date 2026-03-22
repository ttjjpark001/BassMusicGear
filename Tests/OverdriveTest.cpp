#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Preamp.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

//==============================================================================
// 익명 네임스페이스: 테스트 전용 헬퍼
//==============================================================================
namespace {

/** dB 값을 선형 진폭으로 변환 */
float dBToLinear (float dB) { return std::pow (10.0f, dB / 20.0f); }

/** 선형 진폭을 dB로 변환 (0 이하는 -200dB 클램프) */
float linearTodB (float linear) { return 20.0f * std::log10 (std::max (linear, 1e-10f)); }

/**
 * 버퍼에 주어진 주파수의 사인파를 채운다.
 * amplitude: 피크 진폭 (0.0~1.0)
 */
void fillSine (juce::AudioBuffer<float>& buf, float freqHz,
               double sampleRate, float amplitude = 0.9f)
{
    const int n = buf.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < n; ++i)
        buf.setSample (0, i, amplitude * std::sin (omega * static_cast<float> (i)));
}

/**
 * 실수 배열에 단순 DFT를 수행해 magnitude 스펙트럼을 반환한다.
 *
 * 이 함수는 테스트 목적의 O(N^2) naive DFT를 사용한다.
 * 빌드에 FFTW/JUCE FFT 의존성을 추가하지 않고 앨리어싱 레벨을 측정하기 위해
 * 배수만 검사하는 간소화 방식으로 구현된다.
 *
 * @param data   시간 도메인 신호 (N 샘플)
 * @param N      FFT 점수 (2의 거듭제곱 권장)
 * @return magnitude[k] (k=0..N/2)
 */
std::vector<float> computeMagnitudeSpectrum (const float* data, int N)
{
    const int halfN = N / 2 + 1;
    std::vector<float> mag (static_cast<size_t> (halfN), 0.0f);

    for (int k = 0; k < halfN; ++k)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n)
        {
            double angle = -2.0 * juce::MathConstants<double>::pi * k * n / N;
            re += data[n] * std::cos (angle);
            im += data[n] * std::sin (angle);
        }
        mag[static_cast<size_t> (k)] = static_cast<float> (std::sqrt (re * re + im * im) / N);
    }
    return mag;
}

/**
 * 스펙트럼에서 나이퀴스트(SR/2) 이상 대역의 최대 에너지를 dBFS로 반환한다.
 *
 * Preamp 4x 오버샘플링 검증에 사용:
 *   - 나이퀴스트 = SR/2 = 22050Hz @ 44.1kHz
 *   - 앨리어싱은 다운샘플링 후 SR/2 이상 주파수가 접혀 들어오는 현상
 *   - 다운샘플링 후 최종 신호는 SR/2 이상의 주파수를 포함하지 않으므로
 *     이를 검증하려면 입력 신호 고조파가 SR/2 이하로 폴딩되는지 확인해야 한다.
 *
 * 여기서는 다른 접근: 10kHz 입력을 강하게 클리핑할 때 생기는
 * 고조파(20kHz, 30kHz, ...)가 다운샘플 필터로 충분히 제거됐는지
 * 주파수 빈 인덱스 기준으로 확인한다.
 *
 * @param mag        magnitude 스펙트럼 (N/2+1 빈)
 * @param sampleRate 샘플레이트
 * @param N          원본 FFT 점수
 * @param cutoffHz   이 주파수 이상의 빈을 검사
 * @return 해당 대역 최대 magnitude (선형)
 */
float maxMagnitudeAbove (const std::vector<float>& mag,
                         double sampleRate, int N, float cutoffHz)
{
    // 주파수 해상도: sampleRate / N [Hz/bin]
    // 빈 k는 k * sampleRate / N Hz에 해당
    const float binWidth = static_cast<float> (sampleRate) / static_cast<float> (N);
    const int   startBin = static_cast<int> (std::ceil (static_cast<double> (cutoffHz / binWidth)));
    const int   maxBin   = static_cast<int> (mag.size()) - 1;

    float peak = 0.0f;
    for (int k = startBin; k <= maxBin; ++k)
        peak = std::max (peak, mag[static_cast<size_t> (k)]);

    return peak;
}

/**
 * 기음(fundamental) 부근의 magnitude 최댓값을 반환한다 (±1 빈 범위).
 */
float fundamentalMagnitude (const std::vector<float>& mag,
                             double sampleRate, int N, float freqHz)
{
    const float binWidth  = static_cast<float> (sampleRate) / static_cast<float> (N);
    const int   centerBin = static_cast<int> (std::round (static_cast<double> (freqHz / binWidth)));
    const int   startBin  = std::max (0, centerBin - 1);
    const int   endBin    = std::min (static_cast<int> (mag.size()) - 1, centerBin + 1);

    float peak = 0.0f;
    for (int k = startBin; k <= endBin; ++k)
        peak = std::max (peak, mag[static_cast<size_t> (k)]);

    return peak;
}

} // namespace

//==============================================================================
// Tests
//==============================================================================

// --- 초기화 ---

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

    // 4배 오버샘플링 폴리페이즈 IIR 필터는 반드시 양의 지연을 가진다.
    REQUIRE (preamp.getLatencyInSamples() > 0);
}

// --- Phase 1 핵심 기준: 4x 오버샘플 후 10kHz 입력 앨리어싱 -60dBFS 이하 ---

TEST_CASE ("Preamp 4x oversample: aliasing above Nyquist is below -60dBFS after hard drive",
           "[preamp][aliasing][oversampling][phase1]")
{
    // Phase 1 테스트 기준:
    //   10kHz 사인파를 Preamp(4x 오버샘플링)에 통과시켰을 때
    //   나이퀴스트(22050Hz) 이상 주파수 성분의 에너지가 -60dBFS 이하여야 한다.
    //
    // 검증 방식:
    //   Preamp 출력의 고조파를 DFT로 분석.
    //   10kHz의 2차 고조파(20kHz)는 나이퀴스트(22.05kHz) 이하이므로 허용.
    //   3차 고조파(30kHz)는 22.05kHz를 넘어 앨리어싱 대상.
    //   오버샘플링 없이 처리하면 이 성분이 폴딩되어 나타나지만,
    //   4x 오버샘플링 + 다운샘플링 필터가 이를 충분히 억제해야 한다.
    //
    // 주의: Preamp 출력은 다운샘플링 후 신호이므로
    //   최종 샘플레이트(44.1kHz)에서 22.05kHz 이상은 물리적으로 표현 불가.
    //   따라서 앨리어싱 검증은 '특정 주파수 범위의 스펙트럼 에너지'가 아닌
    //   '기음 대비 전체 고조파 에너지 비율(THD 관점)' 또는
    //   '입력과 다른 주파수 성분 에너지'로 측정한다.
    //
    // 실용적 검증: 기음(10kHz) magnitude 대비 5kHz~10kHz 사이 다른 성분이
    //   -60dBFS 이하인지 확인 (앨리어싱이 있다면 이 구간에 폴딩된 성분이 나타남).

    constexpr double sampleRate = 44100.0;
    // DFT는 O(N^2)이므로 테스트 시간을 줄이기 위해 N=2048 사용
    constexpr int numSamples = 2048;
    constexpr float inputFreqHz = 10000.0f;

    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels     = 1;
    preamp.prepare (spec);

    // 파라미터 없이 기본값(gainDB=0, volDB=0) 사용 시 inGain=1.0, outGain=1.0
    // 웨이브쉐이핑이 동작하도록 진폭 0.9로 설정 (tanh 비선형 구간 진입)

    juce::AudioBuffer<float> buffer (1, numSamples);
    fillSine (buffer, inputFreqHz, sampleRate, 0.9f);

    preamp.process (buffer);

    // DFT 분석 (과도 응답 이후 절반 구간)
    const int analyzeStart  = numSamples / 2;
    const int analyzeLength = numSamples - analyzeStart;
    const float* data = buffer.getReadPointer (0) + analyzeStart;

    auto mag = computeMagnitudeSpectrum (data, analyzeLength);

    // 기음 10kHz의 magnitude
    float fundMag = fundamentalMagnitude (mag, sampleRate, analyzeLength, inputFreqHz);

    // 앨리어싱 검증: 200Hz~5kHz 구간의 최대 magnitude를 측정한다.
    // 오버샘플링이 부족하면 고조파(30kHz, 50kHz...)가 이 구간으로 폴딩되어 나타난다.
    const float binWidth = static_cast<float> (sampleRate) / static_cast<float> (analyzeLength);
    const int   maxAliasBin = static_cast<int> (5000.0f / binWidth);
    const int   minAliasBin = static_cast<int> (200.0f  / binWidth);

    float aliasPeak = 0.0f;
    for (int k = minAliasBin; k <= maxAliasBin && k < static_cast<int> (mag.size()); ++k)
        aliasPeak = std::max (aliasPeak, mag[static_cast<size_t> (k)]);

    const float aliasdB = linearTodB (aliasPeak);
    const float funddB  = linearTodB (fundMag);

    INFO ("Fundamental (10kHz): " << funddB << " dBFS");
    INFO ("Alias peak (200Hz–5kHz): " << aliasdB << " dBFS");
    INFO ("Alias relative to fundamental: " << (aliasdB - funddB) << " dB");

    // 앨리어싱 성분(200Hz~5kHz)이 기음 대비 -60dB 이하이거나
    // 절대 레벨이 -60dBFS 이하여야 한다.
    const bool absCondition = (aliasdB < -60.0f);
    const bool relCondition = (aliasdB - funddB < -60.0f);

    REQUIRE ((absCondition || relCondition));
}

// --- Preamp 출력 신호 유효성 ---

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

    // 출력이 모두 0이 되면 안 됨
    float rms = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        rms += buffer.getSample (0, i) * buffer.getSample (0, i);
    rms = std::sqrt (rms / numSamples);

    REQUIRE (rms > 0.001f);
}

TEST_CASE ("Preamp: output does not contain NaN or Inf under high drive", "[preamp][safety]")
{
    // 강한 드라이브(진폭 1.0)에서도 NaN/Inf가 발생하지 않아야 한다.
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples    = 1024;

    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels     = 1;
    preamp.prepare (spec);

    juce::AudioBuffer<float> buffer (1, numSamples);
    // 최대 진폭(포화 확실) 사인파
    fillSine (buffer, 1000.0f, sampleRate, 1.0f);

    preamp.process (buffer);

    const float* data = buffer.getReadPointer (0);
    bool hasNaN = false, hasInf = false;
    for (int i = 0; i < numSamples; ++i)
    {
        hasNaN |= std::isnan  (data[i]);
        hasInf |= std::isinf  (data[i]);
    }

    REQUIRE_FALSE (hasNaN);
    REQUIRE_FALSE (hasInf);
}

TEST_CASE ("Preamp: output amplitude is bounded (tanh soft-clipping)",
           "[preamp][saturation]")
{
    // tanh 웨이브쉐이핑이므로 출력은 이론적으로 유한 범위 내에 있어야 한다.
    // 파라미터가 null(기본값 0dB)인 경우 inGain=1, outGain=1 이므로
    // tanh(x + 0.1*x^2) 출력의 절대값 최대 ≈ 1.1 수준.
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples    = 2048;

    Preamp preamp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate      = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels     = 1;
    preamp.prepare (spec);

    juce::AudioBuffer<float> buffer (1, numSamples);
    // 과포화 입력
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, 10.0f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                   * 440.0f / static_cast<float> (sampleRate)
                                                   * static_cast<float> (i)));

    preamp.process (buffer);

    const float* data = buffer.getReadPointer (0);
    float maxAbs = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxAbs = std::max (maxAbs, std::abs (data[i]));

    // tanh 소프트 클리핑이므로 출력 피크는 입력 피크(10)보다 훨씬 작아야 함
    // 파라미터 없을 때 outGain=1이므로 출력 최대 ≈ 2 이하 예상
    REQUIRE (maxAbs < 5.0f);
}
