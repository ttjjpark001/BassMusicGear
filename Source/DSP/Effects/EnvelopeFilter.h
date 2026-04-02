#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 엔벨로프 필터: StateVariableTPTFilter + 엔벨로프 팔로워
 *
 * **신호 체인 위치**: Octaver -> **EnvelopeFilter** -> Preamp
 *
 * **동작 원리**:
 * 1. 입력 신호의 엔벨로프(진폭)를 추적
 * 2. 엔벨로프 값으로 SVF(State Variable Filter)의 컷오프 주파수를 변조
 * 3. 피킹 어택 시 필터가 열리고, 소리가 줄면 닫힘 (또는 반대 - Direction)
 *
 * **파라미터**:
 * - ef_enabled: ON/OFF
 * - ef_sensitivity: 엔벨로프 감도 (0~1) — 필터 스윕 범위 조절
 * - ef_freq_min: 최소 컷오프 주파수 (100~500 Hz)
 * - ef_freq_max: 최대 컷오프 주파수 (1000~8000 Hz)
 * - ef_resonance: 레조넌스/Q (0.5~10)
 * - ef_direction: Up(0) / Down(1) — 엔벨로프 증가 시 컷오프 상승/하강
 */
class EnvelopeFilter
{
public:
    EnvelopeFilter();
    ~EnvelopeFilter() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* sensitivity,
                               std::atomic<float>* freqMin,
                               std::atomic<float>* freqMax,
                               std::atomic<float>* resonance,
                               std::atomic<float>* direction);

private:
    juce::dsp::StateVariableTPTFilter<float> svfFilter;

    // Envelope follower state
    float envelopeLevel = 0.0f;
    float envAttackCoeff  = 0.0f;  // prepare에서 계산
    float envReleaseCoeff = 0.0f;

    double currentSampleRate = 44100.0;

    // APVTS 파라미터 포인터
    std::atomic<float>* enabledParam     = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* freqMinParam     = nullptr;
    std::atomic<float>* freqMaxParam     = nullptr;
    std::atomic<float>* resonanceParam   = nullptr;
    std::atomic<float>* directionParam   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeFilter)
};
