#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 딜레이: Time/Feedback/Damping/Mix
 *
 * **신호 체인 위치**: Chorus → **Delay** → Reverb → PowerAmp
 *
 * **특징**:
 * - 클래식 에코/반향 이펙트로 신호를 시간 지연시켜 재생
 * - Feedback: 딜레이된 신호를 입력에 섞어서 반복 에코 생성
 * - Damping: 피드백 경로에 로우패스 필터 적용하여 고음 감쇠 (자연스러운 에코)
 *
 * **파라미터**:
 * - delay_enabled: ON/OFF 토글
 * - delay_time: 딜레이 시간 (10~2000ms, 최대 2초)
 * - delay_feedback: 피드백 양 (0~0.95, 1.0 이상 시 발산)
 * - delay_damping: 피드백 경로 로우패스 컷오프 (0=밝음(20kHz), 1=어두움(1kHz))
 * - delay_mix: 드라이/웨트 블렌드 (0=원본, 1=딜레이 신호 100%)
 *
 * **개선 (P1)**: BPM 싱크 추가 예정 (Phase 8)
 */
class Delay
{
public:
    Delay() = default;
    ~Delay() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* time,
                               std::atomic<float>* feedback,
                               std::atomic<float>* damping,
                               std::atomic<float>* mix);

private:
    double currentSampleRate = 44100.0;

    // Circular delay buffer (max 2 seconds)
    std::vector<float> delayBuffer;
    int writePos = 0;

    // One-pole low-pass for damping on feedback path
    float dampingFilterState = 0.0f;

    // APVTS parameter pointers
    std::atomic<float>* enabledParam  = nullptr;
    std::atomic<float>* timeParam     = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* dampingParam  = nullptr;
    std::atomic<float>* mixParam      = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Delay)
};
