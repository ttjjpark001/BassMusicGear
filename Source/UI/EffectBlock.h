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
     * @brief 이펙터 블록 UI 컴포넌트 생성
     *
     * 각 이펙터(Overdrive, Octaver, EnvelopeFilter, Chorus, Delay, Reverb)에서
     * 재사용 가능한 ON/OFF 토글 + 파라미터 노브 범용 UI.
     *
     * **레이아웃**: 좌측(80px) ON/OFF 토글, 우측 파라미터 노브 수평 배치
     *
     * @param name          이펙터 표시 이름 (ex: "Overdrive", "Octaver")
     * @param apvts         APVTS 인스턴스 (파라미터 바인딩용)
     * @param enabledParamId ON/OFF 토글에 연결할 파라미터 ID
     * @param paramIds      노브에 연결할 파라미터 ID 배열
     * @param paramLabels   각 노브 아래 표시할 라벨 배열
     * @note 최대 6개 파라미터 노브 지원 (공간상 제한)
     */
    EffectBlock (const juce::String& name,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& enabledParamId,
                 const juce::StringArray& paramIds,
                 const juce::StringArray& paramLabels);

    ~EffectBlock() override;

    /**
     * @brief 이펙터 블록 배경 및 테두리를 그린다
     *
     * 어두운 배경(#2a2a3e)에 서브틀한 테두리 (#444466)로 시각적 분리
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief 토글과 노브의 레이아웃을 계산한다
     *
     * 좌측 토글 고정(80px), 우측 공간을 노브 개수로 균등 분할
     */
    void resized() override;

private:
    // 이펙터 이름 (토글 버튼 텍스트로 표시)
    juce::String effectName;

    // ON/OFF 토글 버튼 (이펙터 활성화/바이패스)
    juce::ToggleButton enabledToggle;
    // APVTS와 토글 버튼 자동 바인딩 (사용자 조작 → APVTS 갱신, APVTS 변경 → UI 업데이트)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    // 파라미터 노브 배열 (최대 6개)
    juce::OwnedArray<Knob> knobs;
    // 각 노브를 APVTS 파라미터에 자동 바인딩
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;
    // 각 노브 아래에 표시할 라벨 배열
    juce::StringArray knobLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectBlock)
};
