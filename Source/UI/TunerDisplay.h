#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/Tuner.h"

/**
 * @brief 크로매틱 튜너 표시 UI — 에디터 상단 상시 표시
 *
 * **표시 정보**:
 * - 음이름 (C, C#, D, ..., B) — 큰 글자
 * - 센트 편차 바 (-50 ~ +50 cents) — 수평 인디케이터
 * - Mute 버튼 — 튜닝 중 출력 뮤트 토글
 *
 * **갱신**: juce::Timer 30Hz (~33ms 간격)
 * - Tuner DSP에서 atomic으로 전달된 detectedHz, centsDeviation, noteIndex를 읽는다.
 *
 * **레이아웃**: 에디터 상단 가로 전체, 높이 ~50px
 */
class TunerDisplay : public juce::Component,
                     private juce::Timer
{
public:
    TunerDisplay (Tuner& tuner,
                  juce::AudioProcessorValueTreeState& apvts);
    ~TunerDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    Tuner& tunerRef;

    // Mute 토글 버튼
    juce::ToggleButton muteButton { "MUTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;

    // 캐시된 표시 값 (타이머에서 갱신)
    juce::String noteName = "--";
    float cents = 0.0f;
    bool detected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerDisplay)
};
