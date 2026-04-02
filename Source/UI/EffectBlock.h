#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Knob.h"

/**
 * @brief 이펙터 블록 범용 UI 컴포넌트 (Pre-FX/Post-FX 공용)
 *
 * ON/OFF 토글 + 최대 6개 파라미터 노브를 수평 배치하는 범용 컴포넌트.
 * 각 이펙터(Overdrive, Octaver, EnvelopeFilter, Chorus, Delay, Reverb 등)가
 * 이 컴포넌트를 사용하여 ON/OFF + 파라미터 조절 UI를 제공한다.
 *
 * **레이아웃**:
 * - 좌측: ON/OFF 토글 버튼 + 이펙터 이름
 * - 우측: 파라미터 노브 수평 배치
 */
class EffectBlock : public juce::Component
{
public:
    /**
     * @brief 이펙터 블록 생성
     *
     * @param name          이펙터 표시 이름 (ex: "Overdrive", "Octaver")
     * @param apvts         APVTS 참조
     * @param enabledParamId ON/OFF 파라미터 ID
     * @param paramIds      노브에 연결할 파라미터 ID 목록
     * @param paramLabels   각 노브의 표시 라벨 목록
     */
    EffectBlock (const juce::String& name,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& enabledParamId,
                 const juce::StringArray& paramIds,
                 const juce::StringArray& paramLabels);

    ~EffectBlock() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::String effectName;

    // ON/OFF toggle
    juce::ToggleButton enabledToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    // Parameter knobs (up to 6)
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;
    juce::StringArray knobLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectBlock)
};
