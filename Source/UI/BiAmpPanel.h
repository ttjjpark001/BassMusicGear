#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Knob.h"

/**
 * @brief Bi-Amp 크로스오버 UI 패널
 *
 * LR4 크로스오버의 ON/OFF와 분할 주파수를 제어한다.
 * 신호 체인 구조: 입력 → BiAmpCrossover(LP+HP 분할) → DIBlend(혼합)
 *
 * 포함 요소:
 * - expandButton: 접기/펼치기 토글 (▶/▼ 아이콘)
 * - enabledToggle: Bi-Amp ON/OFF (biamp_on 파라미터 바인딩)
 * - freqKnob: 크로스오버 주파수 (crossover_freq, 60-500 Hz)
 *
 * 기능:
 * - expandButton 클릭 → setExpanded(boolean) 호출
 * - freqKnob은 펼쳐진 상태에서만 표시
 * - 확장 상태 변화 시 onExpandChange 콜백 실행 (상위 레이아웃 재조정용)
 */
class BiAmpPanel : public juce::Component
{
public:
    static constexpr int collapsedHeight = 36;  // 헤더만: 버튼 + 토글
    static constexpr int expandedHeight  = 100; // 헤더 + 노브 공간

    BiAmpPanel (juce::AudioProcessorValueTreeState& apvts);
    ~BiAmpPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /**
     * @brief 현재 확장 상태를 반환한다.
     *
     * @return true = 펼쳐짐 (freqKnob 표시), false = 접혀짐
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
    juce::TextButton expandButton;      // ▶/▼ 아이콘 (토글)
    juce::ToggleButton enabledToggle;   // "Bi-Amp" ON/OFF 버튼
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    std::unique_ptr<Knob> freqKnob;     // 크로스오버 주파수 노브 (펼쳐졌을 때만 표시)

    bool expanded = false;              // 내부 확장 상태

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BiAmpPanel)
};
