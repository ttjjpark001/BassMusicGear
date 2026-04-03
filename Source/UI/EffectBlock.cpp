#include "EffectBlock.h"

EffectBlock::EffectBlock (const juce::String& name,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& enabledParamId,
                           const juce::StringArray& paramIds,
                           const juce::StringArray& paramLabels)
    : effectName (name),
      knobLabels (paramLabels)
{
    // --- 접기/펼치기 버튼 (▶) ---
    expandButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6"));  // ▶
    expandButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    expandButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffaaaacc));
    expandButton.onClick = [this]
    {
        setExpanded (!expanded);
    };
    addAndMakeVisible (expandButton);

    // --- ON/OFF 토글 버튼 ---
    enabledToggle.setButtonText (name);
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, enabledParamId, enabledToggle);

    // --- 파라미터 노브 생성 ---
    for (int i = 0; i < paramIds.size(); ++i)
    {
        const auto& paramId = paramIds[i];
        const auto& label   = i < paramLabels.size() ? paramLabels[i] : paramId;
        auto* knob = knobs.add (new Knob (label, paramId, apvts));
        addChildComponent (knob);  // 처음에는 숨김 (접힌 상태)
    }
}

EffectBlock::~EffectBlock() = default;

void EffectBlock::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;

    // 버튼 텍스트를 상태에 맞게 변경
    expandButton.setButtonText (expanded
        ? juce::CharPointer_UTF8 ("\xe2\x96\xbc")   // ▼ (펼침)
        : juce::CharPointer_UTF8 ("\xe2\x96\xb6"));  // ▶ (접힘)

    // 노브 가시성 토글
    for (auto* knob : knobs)
        knob->setVisible (expanded);

    // PluginEditor에 레이아웃 재계산 요청
    if (onExpandChange)
        onExpandChange();
}

void EffectBlock::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 배경
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 테두리
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void EffectBlock::resized()
{
    auto area = getLocalBounds().reduced (4);

    // --- 헤더 행: [▶/▼ 버튼 28px] [ON/OFF 토글 나머지] ---
    auto headerRow = area.removeFromTop (collapsedHeight - 8);
    expandButton.setBounds (headerRow.removeFromLeft (28));
    enabledToggle.setBounds (headerRow);

    if (!expanded || knobs.isEmpty())
        return;

    // --- 노브 행: 남은 공간을 노브 개수로 균등 분할 ---
    area.removeFromTop (4);  // 헤더와 노브 사이 간격
    const int knobWidth = area.getWidth() / knobs.size();
    for (auto* knob : knobs)
        knob->setBounds (area.removeFromLeft (knobWidth));
}
