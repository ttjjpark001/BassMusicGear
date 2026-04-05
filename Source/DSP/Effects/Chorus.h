#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 코러스: LFO 변조된 딜레이 라인
 *
 * **신호 체인 위치**: GraphicEQ → **Chorus** → Delay → Reverb → PowerAmp
 *
 * **특징**:
 * - 정현파 LFO로 딜레이 시간을 동적으로 변조하여 풍부한 코러스 음색 생성
 * - 여러 카피를 약간씩 다른 시간에 지연시켜 원본과 섞음
 * - Rate(LFO 주파수)를 높일수록 더 빠른 변조 움직임, Depth를 높일수록 더 깊은 변조
 *
 * **파라미터**:
 * - chorus_enabled: ON/OFF 토글
 * - chorus_rate: LFO 주파수 (0.1~10 Hz, 초당 변조 사이클 횟수)
 * - chorus_depth: 변조 깊이 (0~1, 0~10ms 딜레이 변화폭으로 매핑)
 * - chorus_mix: 드라이/웨트 믹스 (0=원본, 1=코러스 신호 100%)
 *
 * **구현**:
 * - 정현파 LFO로 딜레이 시간 변조
 * - 중심 딜레이: 7ms, 변조 범위: ±depth×5ms
 * - 소수 딜레이 샘플 위치 구현을 위해 선형 보간 사용
 */
class Chorus
{
public:
    Chorus() = default;
    ~Chorus() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* rate,
                               std::atomic<float>* depth,
                               std::atomic<float>* mix);

private:
    double currentSampleRate = 44100.0;

    // Delay buffer (circular)
    std::vector<float> delayBuffer;
    int delayWritePos = 0;
    static constexpr int maxDelayMs = 30;  // max delay buffer in ms

    // LFO
    double lfoPhase = 0.0;

    // Center delay in samples (7ms)
    float centerDelaySamples = 0.0f;

    // APVTS parameter pointers
    std::atomic<float>* enabledParam = nullptr;
    std::atomic<float>* rateParam    = nullptr;
    std::atomic<float>* depthParam   = nullptr;
    std::atomic<float>* mixParam     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Chorus)
};
