#include "EffectBlock.h"

EffectBlock::EffectBlock (const juce::String& name,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& enabledParamId,
                           const juce::StringArray& paramIds,
                           const juce::StringArray& paramLabels,
                           const juce::StringArray& comboParamIds,
                           const juce::StringArray& comboLabelsArray)
    : effectName (name),
      knobLabels (paramLabels)
{
    // --- 접기/펼치기 버튼 (> / v) ---
    expandButton.setButtonText (">");
    expandButton.setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff3a3a5a));
    expandButton.setColour (juce::TextButton::buttonOnColourId,  juce::Colour (0xff5a5a7a));
    expandButton.setColour (juce::TextButton::textColourOffId,   juce::Colour (0xffffffff));
    expandButton.setColour (juce::TextButton::textColourOnId,    juce::Colour (0xffff8800));
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

    // --- ComboBox 생성 (선택 파라미터 지원) ---
    for (int i = 0; i < comboParamIds.size(); ++i)
    {
        const auto& comboId = comboParamIds[i];
        const auto& comboLabel = i < comboLabelsArray.size() ? comboLabelsArray[i] : comboId;

        auto* combo = combos.add (new juce::ComboBox());
        combo->setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xff3a3a5a));
        combo->setColour (juce::ComboBox::textColourId,        juce::Colours::white);
        combo->setColour (juce::ComboBox::outlineColourId,     juce::Colour (0xff444466));
        combo->setColour (juce::ComboBox::arrowColourId,       juce::Colour (0xffff8800));
        addChildComponent (combo);

        auto* lbl = comboLabelComponents.add (new juce::Label());
        lbl->setText (comboLabel, juce::dontSendNotification);
        lbl->setJustificationType (juce::Justification::centred);
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setFont (juce::FontOptions (10.0f));
        addChildComponent (lbl);

        auto* att = comboAttachments.add (
            new juce::AudioProcessorValueTreeState::ComboBoxAttachment (apvts, comboId, *combo));
        juce::ignoreUnused (att);
    }
}

EffectBlock::~EffectBlock() = default;

void EffectBlock::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;

    // 확장 버튼 텍스트 변경: ">" (접힘) ↔ "v" (펼침)
    expandButton.setButtonText (expanded ? "v" : ">");

    // 모든 파라미터 노브의 가시성 토글
    for (auto* knob : knobs)
        knob->setVisible (expanded);

    // ComboBox와 그 라벨의 가시성 토글
    for (auto* combo : combos)
        combo->setVisible (expanded);
    for (auto* lbl : comboLabelComponents)
        lbl->setVisible (expanded);

    // PluginEditor에 알림: 전체 창 높이 재계산 필요
    // calculateNeededHeight()는 각 블록의 getExpanded() 상태를 읽어
    // 새로운 창 높이를 계산한 후 setSize()를 호출
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

    // 헤더 행: [▼/▶ 버튼(28px)] [ON/OFF 토글(나머지 공간)]
    auto headerRow = area.removeFromTop (collapsedHeight - 8);
    expandButton.setBounds (headerRow.removeFromLeft (28));
    enabledToggle.setBounds (headerRow);

    // 접혀 있거나 파라미터가 없으면 헤더만 표시 (early return)
    if (!expanded || knobs.isEmpty())
        return;

    // 펼친 상태: 노브와 ComboBox를 수평으로 균등하게 배치
    area.removeFromTop (4);  // 헤더와 컨트롤 사이 갭

    const int totalControls = knobs.size() + combos.size();
    if (totalControls == 0) return;

    // 각 컨트롤에 할당할 폭 (균등 분할)
    const int controlWidth = area.getWidth() / totalControls;

    // 파라미터 노브 배치
    for (auto* knob : knobs)
        knob->setBounds (area.removeFromLeft (controlWidth));

    // ComboBox 배치: 드롭다운은 중앙, 라벨은 하단
    for (int i = 0; i < combos.size(); ++i)
    {
        auto comboArea = area.removeFromLeft (controlWidth);
        auto labelArea = comboArea.removeFromBottom (16);  // 라벨용 16px
        comboArea.removeFromTop (comboArea.getHeight() / 3);  // 세로 중앙 정렬
        combos[i]->setBounds (comboArea.reduced (4, 0).removeFromTop (24));  // 콤보박스 높이 24px
        comboLabelComponents[i]->setBounds (labelArea);
    }
}
