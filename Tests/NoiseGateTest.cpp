#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Effects/NoiseGate.h"
#include <cmath>

//==============================================================================
namespace {

/**
 * @brief 테스트용 정현파 버퍼 채우기
 *
 * @param buffer      채울 오디오 버퍼
 * @param freqHz      파형 주파수 (Hz)
 * @param sampleRate  샘플 레이트 (Hz)
 * @param amplitude   진폭 (0~1 권장)
 *
 * 매 블록마다 위상 0부터 시작하는 단순 사인파 생성.
 */
void fillSine (juce::AudioBuffer<float>& buffer, float freqHz,
               double sampleRate, float amplitude)
{
    const int n = buffer.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < n; ++i)
        buffer.setSample (0, i, amplitude * std::sin (omega * static_cast<float> (i)));
}

/**
 * @brief 버퍼의 절대값 피크 레벨 측정
 *
 * @param buffer       오디오 버퍼 채널 0
 * @param startSample  측정 시작 샘플 인덱스
 * @param numSamples   측정할 샘플 수
 * @return             지정 구간의 최대 절대값
 *
 * 게이트 오픈/클로즈 여부 검증 및 개략적 신호 레벨 확인용.
 */
float peakAbs (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const float* d = buffer.getReadPointer (0);
    float p = 0.0f;
    for (int i = startSample; i < startSample + numSamples; ++i)
        p = std::max (p, std::abs (d[i]));
    return p;
}

/**
 * @brief 버퍼에 NaN(Not-a-Number) 또는 Inf(무한대) 확인
 *
 * @param buffer  오디오 버퍼 채널 0
 * @return        NaN/Inf 발견 시 true, 정상 유한값만 있으면 false
 *
 * DSP 안정성 검증 — 클리핑, 회귀, 수치 오류 감지용.
 */
bool hasNanOrInf (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const float* d = buffer.getReadPointer (0);
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (d[i]))
            return true;
    return false;
}

} // namespace

/**
 * @brief NoiseGate: enabled=false 시 신호 통과
 *
 * 게이트가 OFF(enabled=0)일 때 입력 신호가 그대로 통과해야 한다.
 * 임계값, 공격/홀드/릴리즈 시간은 무시된다.
 */
TEST_CASE ("NoiseGate: bypass passes signal unchanged when enabled=false",
           "[noisegate][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    NoiseGate gate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    gate.prepare (spec);

    // 파라미터 원자값 설정
    std::atomic<float> threshold { -40.0f };
    std::atomic<float> attack    { 5.0f };
    std::atomic<float> hold      { 50.0f };
    std::atomic<float> release   { 50.0f };
    std::atomic<float> enabled   { 0.0f };  // OFF (게이트 우회)

    gate.setParameterPointers (&threshold, &attack, &hold, &release, &enabled);

    // 테스트용 사인파 생성 (100Hz, 0.5 진폭)
    juce::AudioBuffer<float> buffer (1, blockSize);
    fillSine (buffer, 100.0f, sampleRate, 0.5f);

    // 원본 신호 백업
    juce::AudioBuffer<float> orig (1, blockSize);
    orig.copyFrom (0, 0, buffer, 0, 0, blockSize);

    // 게이트 처리
    gate.process (buffer);

    // 신호 동일성 확인 (오차 허용: 1e-5)
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (buffer.getSample (0, i)
                 == Catch::Approx (orig.getSample (0, i)).margin (1e-5f));

    // 수치 안정성 확인
    REQUIRE_FALSE (hasNanOrInf (buffer));
}

/**
 * @brief NoiseGate: 임계값 0dB에서 약한 신호 음소거
 *
 * 임계값을 0dB로 설정하면 ~-10dBFS 사인파(진폭 0.3)는 임계값 미만이므로
 * 게이트가 닫혀 신호가 음소거되어야 한다.
 * 어택/릴리즈 안정화를 위해 여러 블록 반복 처리.
 */
TEST_CASE ("NoiseGate: threshold at 0 dB mutes signal",
           "[noisegate][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    NoiseGate gate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    gate.prepare (spec);

    std::atomic<float> threshold { 0.0f };    // 0dB: 대부분 신호가 threshold 미만
    std::atomic<float> attack    { 5.0f };
    std::atomic<float> hold      { 10.0f };
    std::atomic<float> release   { 10.0f };
    std::atomic<float> enabled   { 1.0f };

    gate.setParameterPointers (&threshold, &attack, &hold, &release, &enabled);

    // 여러 블록 반복 처리로 게이트 상태(닫힘) 안정화
    juce::AudioBuffer<float> buffer (1, blockSize);
    for (int rep = 0; rep < 6; ++rep)
    {
        fillSine (buffer, 100.0f, sampleRate, 0.3f);  // 진폭 0.3 ≈ -10dBFS
        gate.process (buffer);
    }

    // 마지막 절반 구간 피크: 게이트가 닫혀있으므로 거의 0이어야 함
    const float p = peakAbs (buffer, blockSize / 2, blockSize / 2);
    REQUIRE (p < 0.05f);
    REQUIRE_FALSE (hasNanOrInf (buffer));
}

/**
 * @brief NoiseGate: 매우 낮은 임계값(-60dB)에서 큰 신호 통과
 *
 * 임계값 -60dB는 거의 모든 신호가 threshold 이상이므로
 * 게이트가 열려 입력 신호가 대부분 통과해야 한다.
 */
TEST_CASE ("NoiseGate: very low threshold passes loud signal",
           "[noisegate][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    NoiseGate gate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    gate.prepare (spec);

    std::atomic<float> threshold { -60.0f };  // 매우 낮음: 거의 모든 신호 통과
    std::atomic<float> attack    { 1.0f };
    std::atomic<float> hold      { 10.0f };
    std::atomic<float> release   { 10.0f };
    std::atomic<float> enabled   { 1.0f };

    gate.setParameterPointers (&threshold, &attack, &hold, &release, &enabled);

    // 여러 블록 반복하여 게이트 오픈 상태 안정화
    juce::AudioBuffer<float> buffer (1, blockSize);
    for (int rep = 0; rep < 4; ++rep)
    {
        fillSine (buffer, 100.0f, sampleRate, 0.5f);
        gate.process (buffer);
    }

    // 마지막 절반 구간 피크: 게이트가 열려있으므로 큰 신호 통과
    const float p = peakAbs (buffer, blockSize / 2, blockSize / 2);
    REQUIRE (p > 0.3f);  // 신호가 충분히 통과
    REQUIRE_FALSE (hasNanOrInf (buffer));
}

TEST_CASE ("NoiseGate: attack 1ms opens faster than 50ms",
           "[noisegate][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    auto measureEnergyAfterSilence = [&] (float attackMs)
    {
        NoiseGate gate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
        gate.prepare (spec);

        std::atomic<float> threshold { -40.0f };
        std::atomic<float> attack    { attackMs };
        std::atomic<float> hold      { 5.0f };
        std::atomic<float> release   { 5.0f };
        std::atomic<float> enabled   { 1.0f };
        gate.setParameterPointers (&threshold, &attack, &hold, &release, &enabled);

        // 1. 닫힘 상태로 진입 (무음)
        juce::AudioBuffer<float> silent (1, blockSize);
        silent.clear();
        for (int r = 0; r < 4; ++r)
            gate.process (silent);

        // 2. 큰 신호 인가 직후 첫 ~5ms 구간 에너지 측정 (221 샘플)
        juce::AudioBuffer<float> loud (1, blockSize);
        fillSine (loud, 100.0f, sampleRate, 0.5f);
        gate.process (loud);

        const int measureSamples = static_cast<int> (sampleRate * 0.005);  // 5ms
        float sum = 0.0f;
        const float* d = loud.getReadPointer (0);
        for (int i = 0; i < measureSamples; ++i)
            sum += d[i] * d[i];
        return sum;
    };

    const float fastEnergy = measureEnergyAfterSilence (1.0f);
    const float slowEnergy = measureEnergyAfterSilence (50.0f);

    // 빠른 어택이 느린 어택보다 초기 구간 에너지가 더 커야 한다
    REQUIRE (fastEnergy > slowEnergy);
}

TEST_CASE ("NoiseGate: no NaN/Inf with edge-case parameters",
           "[noisegate][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 1024;

    NoiseGate gate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    gate.prepare (spec);

    std::atomic<float> threshold { -30.0f };
    std::atomic<float> attack    { 0.1f };
    std::atomic<float> hold      { 1.0f };
    std::atomic<float> release   { 1.0f };
    std::atomic<float> enabled   { 1.0f };

    gate.setParameterPointers (&threshold, &attack, &hold, &release, &enabled);

    juce::AudioBuffer<float> buffer (1, blockSize);
    for (int rep = 0; rep < 4; ++rep)
    {
        fillSine (buffer, 100.0f, sampleRate, 0.5f);
        gate.process (buffer);
        REQUIRE_FALSE (hasNanOrInf (buffer));
    }

    // 무음으로 전환
    buffer.clear();
    gate.process (buffer);
    REQUIRE_FALSE (hasNanOrInf (buffer));
}
