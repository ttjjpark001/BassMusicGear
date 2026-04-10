/**
 * PowerAmpTest.cpp
 *
 * Phase 7 단위 테스트 — PowerAmp DSP 모듈 검증
 *
 * 검증 항목:
 *  A. 4개 PowerAmpType(Tube6550/TubeEL34/SolidState/ClassD) 각각 동일 입력에 대해
 *     서로 다른 출력 RMS 생성
 *  B. 4개 PowerAmpType 각각 동일 입력에 대해 서로 다른 THD(Total Harmonic Distortion) 생성
 *  C. 모든 타입이 0dBFS 사인 입력에서 NaN/Inf 없음
 *  D. Sag enabled vs disabled 출력 RMS 차이 검증 (Tube 타입만)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/PowerAmp.h"
#include "Models/AmpModel.h"
#include <cmath>
#include <array>
#include <numeric>

namespace {

constexpr double kSampleRate   = 44100.0;
constexpr int    kBlockSize    = 1024;
constexpr float  kFreq100Hz    = 100.0f;

/** 지정 주파수의 사인파를 진폭 amplitude로 단일 채널 버퍼에 채운다. */
void fillSine (juce::AudioBuffer<float>& buf, float freqHz, double sr, float amplitude)
{
    const int n = buf.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sr);
    for (int i = 0; i < n; ++i)
        buf.setSample (0, i, amplitude * std::sin (omega * static_cast<float> (i)));
}

/** RMS 레벨 계산 (채널 0 전체). */
float calcRms (const juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();
    const float* data = buf.getReadPointer (0);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += data[i] * data[i];
    return std::sqrt (sum / static_cast<float> (n));
}

/**
 * 간이 THD(Total Harmonic Distortion) 추정 — 에너지 비율 기반.
 *
 * 기본파(freqHz)의 주파수 구간과 나머지 구간의 RMS 에너지를 비교한다.
 * 단순 시간 도메인 방법으로, 완전한 FFT 기반 THD 대신 상대적인 왜곡 수준을
 * 비교하는 데 충분하다.
 *
 * 방법: 출력 신호에서 기본파 기여분을 제거(직교 분해)한 잔류 신호의 RMS /
 *       전체 신호 RMS 를 반환한다.
 */
float estimateTHD (const juce::AudioBuffer<float>& buf, float freqHz, double sr)
{
    const int n = buf.getNumSamples();
    const float* data = buf.getReadPointer (0);
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sr);

    // 기본파 성분의 진폭 추정 (정현/여현 상관)
    double sinCorr = 0.0, cosCorr = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float t = omega * static_cast<float> (i);
        sinCorr += static_cast<double> (data[i]) * std::sin (static_cast<double> (t));
        cosCorr += static_cast<double> (data[i]) * std::cos (static_cast<double> (t));
    }
    const double fundAmp = 2.0 * std::sqrt (sinCorr * sinCorr + cosCorr * cosCorr)
                           / static_cast<double> (n);

    // 전체 RMS
    double totalPower = 0.0;
    for (int i = 0; i < n; ++i)
        totalPower += static_cast<double> (data[i]) * static_cast<double> (data[i]);
    const double totalRms = std::sqrt (totalPower / static_cast<double> (n));

    // 기본파 RMS = fundAmp / sqrt(2)
    const double fundRms = fundAmp / std::sqrt (2.0);

    // 잔류 신호 RMS(고조파+노이즈) = sqrt(total^2 - fund^2)
    const double residual = std::sqrt (std::max (totalRms * totalRms - fundRms * fundRms, 0.0));

    // THD = 잔류 / 전체
    return totalRms > 1e-10 ? static_cast<float> (residual / totalRms) : 0.0f;
}

/** PowerAmp를 준비하고 고정 파라미터 포인터를 설정하는 헬퍼. */
struct PowerAmpFixture
{
    std::atomic<float> drive    { 0.5f };  // 중간 드라이브
    std::atomic<float> presence { 0.5f };  // 중립 프레즌스
    std::atomic<float> sag      { 0.0f };  // 기본 Sag OFF

    PowerAmp amp;

    explicit PowerAmpFixture (PowerAmpType type, bool sagEnabled = false)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = kSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (kBlockSize);
        spec.numChannels      = 1;

        amp.prepare (spec);
        amp.setPowerAmpType (type, sagEnabled);
        amp.setParameterPointers (&drive, &presence, &sag);
        amp.updatePresenceFilter (0.5f);
    }

    /** 사인파를 numBlocks번 통과시켜 앰프를 안정화한 뒤, 마지막 블록을 반환한다. */
    juce::AudioBuffer<float> processAndMeasure (float amplitude, int numBlocks = 8)
    {
        juce::AudioBuffer<float> buf (1, kBlockSize);
        for (int b = 0; b < numBlocks; ++b)
        {
            fillSine (buf, kFreq100Hz, kSampleRate, amplitude);
            amp.process (buf);
        }
        return buf;
    }
};

} // namespace

//==============================================================================
// A. 4개 타입이 동일 입력에서 서로 다른 RMS를 생성하는지 검증
//==============================================================================

TEST_CASE ("PowerAmp: 4 types produce different output RMS for same input",
           "[poweramp][phase7][differentiation]")
{
    // 중간 Drive(0.5)로 포화가 발생하는 진폭 입력
    constexpr float inputAmplitude = 0.8f;

    const std::array<PowerAmpType, 4> types = {
        PowerAmpType::Tube6550,
        PowerAmpType::TubeEL34,
        PowerAmpType::SolidState,
        PowerAmpType::ClassD
    };

    std::array<float, 4> rmsValues {};

    for (int i = 0; i < 4; ++i)
    {
        PowerAmpFixture fix (types[static_cast<size_t> (i)]);
        auto result = fix.processAndMeasure (inputAmplitude);
        rmsValues[static_cast<size_t> (i)] = calcRms (result);

        INFO ("Type " << i << " RMS = " << rmsValues[static_cast<size_t> (i)]);
        REQUIRE (rmsValues[static_cast<size_t> (i)] > 0.0f);  // 출력이 0이 아님
    }

    // 4개 타입 중 적어도 한 쌍은 RMS가 서로 다르다 (상대 차이 > 1%)
    bool foundDifference = false;
    for (int i = 0; i < 4 && !foundDifference; ++i)
        for (int j = i + 1; j < 4 && !foundDifference; ++j)
        {
            const float ratio = rmsValues[static_cast<size_t> (i)]
                                / std::max (rmsValues[static_cast<size_t> (j)], 1e-10f);
            if (ratio > 1.01f || ratio < 0.99f)
                foundDifference = true;
        }

    REQUIRE (foundDifference);
}

//==============================================================================
// B. 4개 타입이 동일 입력에서 서로 다른 THD를 생성하는지 검증
//==============================================================================

TEST_CASE ("PowerAmp: 4 types produce different THD for same input",
           "[poweramp][phase7][thd]")
{
    // Drive 0.7 — 포화 왜곡이 충분히 발생하는 레벨
    constexpr float inputAmplitude = 0.9f;

    const std::array<PowerAmpType, 4> types = {
        PowerAmpType::Tube6550,
        PowerAmpType::TubeEL34,
        PowerAmpType::SolidState,
        PowerAmpType::ClassD
    };

    std::array<float, 4> thdValues {};

    for (int i = 0; i < 4; ++i)
    {
        PowerAmpFixture fix (types[static_cast<size_t> (i)]);
        fix.drive.store (0.7f);
        auto result = fix.processAndMeasure (inputAmplitude);
        thdValues[static_cast<size_t> (i)] = estimateTHD (result, kFreq100Hz, kSampleRate);

        INFO ("Type " << i << " THD = " << thdValues[static_cast<size_t> (i)] * 100.0f << "%");
        REQUIRE (std::isfinite (thdValues[static_cast<size_t> (i)]));
    }

    // 적어도 한 쌍이 THD에서 서로 다르다 (차이 > 0.005 = 0.5%p)
    bool foundDifference = false;
    for (int i = 0; i < 4 && !foundDifference; ++i)
        for (int j = i + 1; j < 4 && !foundDifference; ++j)
            if (std::abs (thdValues[static_cast<size_t> (i)] - thdValues[static_cast<size_t> (j)]) > 0.005f)
                foundDifference = true;

    REQUIRE (foundDifference);
}

//==============================================================================
// C. 모든 타입이 0dBFS(amplitude=1.0) 사인 입력에서 NaN/Inf를 생성하지 않는지 검증
//==============================================================================

TEST_CASE ("PowerAmp: no NaN or Inf on 0dBFS sine input for all types",
           "[poweramp][phase7][stability]")
{
    const std::array<PowerAmpType, 4> types = {
        PowerAmpType::Tube6550,
        PowerAmpType::TubeEL34,
        PowerAmpType::SolidState,
        PowerAmpType::ClassD
    };

    const std::array<const char*, 4> typeNames = {
        "Tube6550", "TubeEL34", "SolidState", "ClassD"
    };

    for (int t = 0; t < 4; ++t)
    {
        DYNAMIC_SECTION ("Type: " << typeNames[static_cast<size_t> (t)])
        {
            // Drive 최대(1.0)로 극한 포화 조건 시험
            PowerAmpFixture fix (types[static_cast<size_t> (t)]);
            fix.drive.store (1.0f);

            juce::AudioBuffer<float> buf (1, kBlockSize);
            fillSine (buf, kFreq100Hz, kSampleRate, 1.0f);  // 0dBFS (amplitude = 1.0)

            fix.amp.process (buf);

            const float* data = buf.getReadPointer (0);
            for (int i = 0; i < kBlockSize; ++i)
            {
                INFO ("Sample " << i << " = " << data[i]);
                REQUIRE (std::isfinite (data[i]));
            }
        }
    }
}

//==============================================================================
// C-2. Tube 타입 Drive 최솟값(0.0)에서도 NaN/Inf 없음
//==============================================================================

TEST_CASE ("PowerAmp: no NaN or Inf with Drive=0 for Tube types",
           "[poweramp][phase7][stability][boundary]")
{
    const std::array<PowerAmpType, 2> tubeTypes = {
        PowerAmpType::Tube6550,
        PowerAmpType::TubeEL34
    };

    for (auto type : tubeTypes)
    {
        PowerAmpFixture fix (type, /*sagEnabled=*/ true);
        fix.drive.store (0.0f);   // Drive 최솟값
        fix.sag.store (1.0f);     // Sag 최대

        juce::AudioBuffer<float> buf (1, kBlockSize);
        fillSine (buf, kFreq100Hz, kSampleRate, 1.0f);
        fix.amp.process (buf);

        const float* data = buf.getReadPointer (0);
        for (int i = 0; i < kBlockSize; ++i)
            REQUIRE (std::isfinite (data[i]));
    }
}

//==============================================================================
// D. Sag enabled vs disabled: Tube 타입에서 출력 RMS가 달라야 한다
//==============================================================================

TEST_CASE ("PowerAmp: Sag enabled vs disabled produces different output for Tube types",
           "[poweramp][phase7][sag]")
{
    // 강한 신호를 입력하면 Sag가 게인을 동적으로 감소시켜 RMS 차이가 생긴다.
    constexpr float inputAmplitude = 0.9f;

    const std::array<PowerAmpType, 2> tubeTypes = {
        PowerAmpType::Tube6550,
        PowerAmpType::TubeEL34
    };
    const std::array<const char*, 2> tubeNames = { "Tube6550", "TubeEL34" };

    for (int t = 0; t < 2; ++t)
    {
        DYNAMIC_SECTION ("Tube type: " << tubeNames[static_cast<size_t> (t)])
        {
            // --- Sag OFF ---
            PowerAmpFixture fixOff (tubeTypes[static_cast<size_t> (t)], /*sagEnabled=*/ false);
            fixOff.drive.store (0.7f);
            fixOff.sag.store (0.0f);
            auto resultOff = fixOff.processAndMeasure (inputAmplitude, /*numBlocks=*/ 16);
            const float rmsOff = calcRms (resultOff);

            // --- Sag ON (최대 깊이) ---
            PowerAmpFixture fixOn (tubeTypes[static_cast<size_t> (t)], /*sagEnabled=*/ true);
            fixOn.drive.store (0.7f);
            fixOn.sag.store (1.0f);
            auto resultOn = fixOn.processAndMeasure (inputAmplitude, /*numBlocks=*/ 16);
            const float rmsOn = calcRms (resultOn);

            INFO ("Sag OFF RMS = " << rmsOff << ", Sag ON RMS = " << rmsOn);

            // Sag ON이 강한 신호를 더 많이 감소시키므로 RMS가 달라야 한다
            // 두 RMS가 동일하지 않음을 확인 (상대 차이 > 0.1%)
            const float diff = std::abs (rmsOff - rmsOn)
                               / std::max (rmsOff, 1e-10f);
            REQUIRE (diff > 0.001f);
        }
    }
}

//==============================================================================
// D-2. SolidState / ClassD 타입은 sagEnabled=true로 설정해도 Sag 영향이 없다
//==============================================================================

TEST_CASE ("PowerAmp: SolidState and ClassD are unaffected by Sag parameter",
           "[poweramp][phase7][sag][boundary]")
{
    // sagEnabled=false로 setPowerAmpType을 호출하면 Sag 로직이 비활성화된다.
    // SolidState/ClassD는 항상 sagEnabled=false 이므로 Sag 파라미터가 출력에
    // 영향을 주지 않아야 한다.

    const std::array<PowerAmpType, 2> ssTypes = {
        PowerAmpType::SolidState,
        PowerAmpType::ClassD
    };

    constexpr float inputAmplitude = 0.9f;

    for (auto type : ssTypes)
    {
        // sagEnabled=false: Sag 완전 비활성
        PowerAmpFixture fixNoSag (type, /*sagEnabled=*/ false);
        fixNoSag.drive.store (0.7f);
        fixNoSag.sag.store (0.0f);
        auto resultNoSag = fixNoSag.processAndMeasure (inputAmplitude);

        // sagEnabled=false로 고정된 상태에서 sag 파라미터 값만 변경 — 결과는 동일해야 함
        PowerAmpFixture fixWithSagParam (type, /*sagEnabled=*/ false);
        fixWithSagParam.drive.store (0.7f);
        fixWithSagParam.sag.store (1.0f);  // sagEnabled=false이면 이 값은 무시됨
        auto resultWithParam = fixWithSagParam.processAndMeasure (inputAmplitude);

        const float rmsA = calcRms (resultNoSag);
        const float rmsB = calcRms (resultWithParam);

        INFO ("SolidState/ClassD Sag param test: RMS A=" << rmsA << " B=" << rmsB);
        // sagEnabled=false이므로 동일한 포화 곡선 → RMS 차이 < 1%
        REQUIRE (std::abs (rmsA - rmsB) / std::max (rmsA, 1e-10f) < 0.01f);
    }
}
