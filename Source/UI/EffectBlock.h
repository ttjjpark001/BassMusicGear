#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Knob.h"

/**
 * @brief 접기/펼치기 가능한 이펙터 블록 범용 UI 컴포넌트
 *
 * Pre-FX(Overdrive, Octaver, EnvelopeFilter)와 Post-FX(Chorus, Delay, Reverb)
 * 모두에서 재사용 가능한 범용 이펙터 UI 컴포넌트.
 *
 * **구성**:
 * - ON/OFF 토글 + 접기/펼치기 버튼
 * - 임의 개수의 파라미터 노브 (수평 배치)
 * - 선택 파라미터용 ComboBox (예: Reverb Type, Env Filter Direction)
 *
 * **상태**:
 * - 접힌 상태(collapsed): 36px 높이, 헤더(토글+버튼)만 표시
 * - 펼친 상태(expanded): 130px 높이, 헤더 + 노브/콤보박스 전체 표시
 *
 * **레이아웃 (펼친 상태)**:
 * ```
 * 헤더행: [▼ 버튼(28px)] [ON/OFF 토글 "Overdrive" ...]
 * 노브행: [노브1: Drive] [노브2: Tone] [노브3: Blend] ...
 * ```
 *
 * **동적 레이아웃**: 접기/펼치기 상태 변경 시 onExpandChange 콜백으로
 * PluginEditor에 알림 → 전체 창 높이 자동 재계산
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
     * @param comboParamIds  ComboBox에 연결할 파라미터 ID 배열 (선택 사항)
     * @param comboLabels    각 ComboBox 아래 표시할 라벨 배열 (선택 사항)
     * @param toggleParamIds 추가 토글 버튼에 연결할 파라미터 ID 배열 (선택 사항)
     * @param toggleLabels   각 토글 버튼에 표시할 라벨 배열 (선택 사항)
     * @note 파라미터 노브 수는 paramIds 배열 크기로 결정된다
     */
    EffectBlock (const juce::String& name,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& enabledParamId,
                 const juce::StringArray& paramIds,
                 const juce::StringArray& paramLabels,
                 const juce::StringArray& comboParamIds = {},
                 const juce::StringArray& comboLabels = {},
                 const juce::StringArray& toggleParamIds = {},
                 const juce::StringArray& toggleLabels = {});

    ~EffectBlock() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** @brief 현재 접힘/펼침 상태 반환 */
    bool getExpanded() const { return expanded; }

    /**
     * @brief 이펙터 블록 접기/펼치기 상태를 설정한다.
     *
     * @param shouldBeExpanded  true = 펼침(파라미터 노브 전체 표시), false = 접힘(헤더만 표시)
     *
     * **상태 변화 효과**:
     * - 상태 변경 시 모든 노브/ComboBox/토글의 가시성을 일괄 토글
     * - 확장 버튼 텍스트 변경: ">" (접힘) ↔ "v" (펼침)
     * - onExpandChange 콜백 호출 → PluginEditor의 전체 창 높이 자동 재계산
     *
     * @note [메인 스레드] UI 클릭 등으로 호출. resized() 레이아웃 재계산 트리거함.
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

    // ComboBox 배열 (선택 파라미터 지원)
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> comboLabelComponents;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;

    // 추가 토글 버튼 배열 (예: Delay BPM Sync)
    juce::OwnedArray<juce::ToggleButton> extraToggles;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> extraToggleAttachments;

    // 현재 상태: false = 접힘(기본), true = 펼침
    bool expanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectBlock)
};
