#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/AmpPanel.h"
#include "UI/CabinetSelector.h"
#include "UI/TunerDisplay.h"
#include "UI/EffectBlock.h"
#include "UI/GraphicEQPanel.h"
#include "UI/BiAmpPanel.h"
#include "UI/DIBlendPanel.h"

// ─────────────────────────────────────────────────────────────────────────────
// ContentComponent — 스크롤 가능한 신호 체인 UI 전체를 담는 컨테이너
// Viewport 안에 배치되며, 높이는 블록 펼침 상태에 따라 동적으로 변한다.
// ─────────────────────────────────────────────────────────────────────────────
class ContentComponent : public juce::Component
{
public:
    ContentComponent (PluginProcessor& p);
    ~ContentComponent() override = default;

    void resized() override;

    /**
     * @brief 현재 모든 블록의 높이 합산값을 반환한다.
     *
     * @return Viewport 콘텐츠 높이 (TunerDisplay + 신호 체인 블록들 합산)
     *         블록이 펼쳐졌을 때는 expandedHeight, 접혀있을 때는 collapsedHeight 사용
     */
    int calculateNeededHeight() const;

    /**
     * @brief 블록 펼침/접힘 시 호출되는 콜백.
     *
     * Viewport에 크기 변경을 알려 레이아웃을 재조정하도록 함.
     * PluginEditor::updateContentSize()에서 설정된다.
     */
    std::function<void()> onHeightChanged;

    TunerDisplay    tunerDisplay;
    AmpPanel        ampPanel;
    CabinetSelector cabinetSelector;

    std::unique_ptr<EffectBlock> noiseGateBlock;
    std::unique_ptr<EffectBlock> compressorBlock;

    std::unique_ptr<EffectBlock> overdriveBlock;
    std::unique_ptr<EffectBlock> octaverBlock;
    std::unique_ptr<EffectBlock> envelopeFilterBlock;

    BiAmpPanel     biAmpPanel;
    GraphicEQPanel graphicEQPanel;

    std::unique_ptr<EffectBlock> chorusBlock;
    std::unique_ptr<EffectBlock> delayBlock;
    std::unique_ptr<EffectBlock> reverbBlock;

    DIBlendPanel diBlendPanel;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentComponent)
};

// ─────────────────────────────────────────────────────────────────────────────
// PluginEditor — AudioProcessorEditor 루트
// 상단 타이틀 + Viewport(스크롤 가능 신호 체인) + 하단 푸터로 구성
// ─────────────────────────────────────────────────────────────────────────────
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int titleHeight  = 35;
    static constexpr int footerHeight = 22;
    static constexpr int fixedHeight  = 1000; // 창 고정 높이 (스크롤로 콘텐츠 조회)

    /**
     * @brief ContentComponent 높이를 재계산하고 Viewport 콘텐츠 크기를 갱신한다.
     *
     * 블록 펼침/접힘 시 ContentComponent::onHeightChanged 콜백으로 호출되어
     * 신호 체인 전체 높이를 다시 계산한다.
     * Viewport는 이 값에 따라 스크롤 영역을 조정한다.
     */
    void updateContentSize();

    PluginProcessor& processorRef;

    ContentComponent content;
    juce::Viewport   viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
