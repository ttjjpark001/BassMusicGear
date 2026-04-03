#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Knob.h"

/**
 * @brief 접기/펼치기 가능한 이펙터 블록 범용 UI 컴포넌트 (Pre-FX/Post-FX 공용)
 *
 * ON/OFF 토글 + 임의 개수의 파라미터 노브를 수평 배치하는 범용 컴포넌트.
 * 접힌 상태(collapsed)에서는 헤더만 표시하고,
 * 펼친 상태(expanded)에서는 노브 전체를 표시한다.
 *
 * **높이**:
 * - 접힌 상태: collapsedHeight (36px)
 * - 펼친 상태: expandedHeight (130px)
 *
 * **레이아웃 (펼친 상태)**:
 * - 헤더 행: [▼/▶ 버튼] [ON/OFF 토글]
 * - 노브 행: 파라미터 노브 수평 배치
 */
class EffectBlock : public juce::Component
{
public:
    static constexpr int collapsedHeight = 36;
    static constexpr int expandedHeight  = 130;

    /**
     * @brief 이펙터 블록 UI 컴포넌트 생성
     *
     * @param name           이펙터 표시 이름 (ex: "Overdrive", "Octaver")
     * @param apvts          APVTS 인스턴스 (파라미터 바인딩용)
     * @param enabledParamId ON/OFF 토글에 연결할 파라미터 ID
     * @param paramIds       노브에 연결할 파라미터 ID 배열
     * @param paramLabels    각 노브 아래 표시할 라벨 배열
     * @note 파라미터 노브 수는 paramIds 배열 크기로 결정된다
     */
    EffectBlock (const juce::String& name,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& enabledParamId,
                 const juce::StringArray& paramIds,
                 const juce::StringArray& paramLabels);

    ~EffectBlock() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** @brief 현재 접힘/펼침 상태 반환 */
    bool getExpanded() const { return expanded; }

    /**
     * @brief 접기/펼치기 상태 설정
     * @param shouldBeExpanded true = 펼침, false = 접힘
     * 상태 변경 시 onExpandChange 콜백을 호출한다.
     */
    void setExpanded (bool shouldBeExpanded);

    /**
     * @brief 접기/펼치기 상태 변경 시 호출되는 콜백
     * PluginEditor에서 레이아웃 재계산(resized())을 트리거하는 데 사용한다.
     */
    std::function<void()> onExpandChange;

private:
    juce::String effectName;

    // 접기/펼치기 버튼 (▶/▼)
    juce::TextButton expandButton;

    // ON/OFF 토글 버튼
    juce::ToggleButton enabledToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    // 파라미터 노브 배열
    juce::OwnedArray<Knob> knobs;
    juce::StringArray knobLabels;

    // 현재 상태: false = 접힘(기본), true = 펼침
    bool expanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectBlock)
};
