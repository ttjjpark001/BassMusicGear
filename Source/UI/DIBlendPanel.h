#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Knob.h"

/**
 * @brief 클린 DI + 앰프 체인 신호 혼합 UI 패널
 *
 * BiAmpCrossover의 LP 출력(클린DI)과 앰프 체인 신호를 다양한 비율로 혼합한다.
 * 또한 Cabinet IR의 삽입 위치(Pre-IR vs Post-IR)를 제어한다.
 *
 * 포함 요소:
 * - expandButton: 접기/펼치기 토글 (>/v 아이콘)
 * - enabledToggle: DI Blend ON/OFF (di_blend_on 파라미터 바인딩)
 * - blendKnob: 혼합 비율 (di_blend, 0-100%): 클린(0%) ← → 처리음(100%)
 * - cleanLevelKnob: 클린 신호 레벨 (clean_level, -12~+12 dB)
 * - processedLevelKnob: 처리 신호 레벨 (processed_level, -12~+12 dB)
 * - irPositionToggle: Cabinet 위치 (ir_position, OFF=Post-IR, ON=Pre-IR)
 *
 * 기능:
 * - expandButton 클릭 → setExpanded(boolean) 호출
 * - 펼쳐진 상태에서만 3개 노브 + IR 토글 표시
 * - 확장 상태 변화 시 onExpandChange 콜백 실행
 */
class DIBlendPanel : public juce::Component
{
public:
    static constexpr int collapsedHeight = 36;  // 헤더만: 버튼 + 라벨
    static constexpr int expandedHeight  = 130; // 헤더(36) + 노브(80) + 여백(14)

    DIBlendPanel (juce::AudioProcessorValueTreeState& apvts);
    ~DIBlendPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /**
     * @brief 현재 확장 상태를 반환한다.
     *
     * @return true = 펼쳐짐 (노브+토글 표시), false = 접혀짐
     */
    bool getExpanded() const { return expanded; }

    /**
     * @brief 패널 확장/축소 상태를 설정한다.
     *
     * expandButton 클릭 또는 외부에서 호출되어 펼침/접음을 제어한다.
     *
     * @param shouldBeExpanded  true = 펼침, false = 접음
     */
    void setExpanded (bool shouldBeExpanded);

    /**
     * @brief 확장 상태 변화 시 호출되는 콜백.
     *
     * 상위 컴포넌트(SignalChainView 등)가 이를 감지하여
     * 레이아웃(collapsedHeight ↔ expandedHeight)을 재조정할 때 사용.
     */
    std::function<void()> onExpandChange;

private:
    juce::TextButton expandButton;      // >/v 아이콘 (토글)
    juce::ToggleButton enabledToggle;   // DI Blend ON/OFF
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    std::unique_ptr<Knob> blendKnob;              // 클린/처리 혼합 비율 (0~100%)
    std::unique_ptr<Knob> cleanLevelKnob;        // 클린 레벨 트림 (±12dB)
    std::unique_ptr<Knob> processedLevelKnob;    // 처리 레벨 트림 (±12dB)

    juce::ToggleButton irPositionToggle;  // "Pre-IR" 토글 (Post/Pre 선택)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> irPositionAttachment;

    bool expanded = false;  // 내부 확장 상태

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DIBlendPanel)
};
