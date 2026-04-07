#include "DIBlendPanel.h"

DIBlendPanel::DIBlendPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // --- 접기/펼치기 버튼 ---
    // 초기 아이콘: ▶ (오른쪽 삼각형, 접혀있음)
    expandButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6"));
    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    // --- 제목 라벨 ---
    // "DI Blend" 라벨 (14pt Bold, 흰색)
    titleLabel.setText ("DI Blend", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    addAndMakeVisible (titleLabel);

    // --- Blend 노브 ---
    // 클린 DI와 처리된 신호의 혼합 비율 (0% = 클린, 100% = 처리음)
    // APVTS 파라미터: "di_blend"
    // 초기: 숨김 상태, setExpanded(true)일 때만 표시
    blendKnob = std::make_unique<Knob> ("Blend", "di_blend", apvts);
    blendKnob->setVisible (false);
    addAndMakeVisible (*blendKnob);

    // --- Clean Level 노브 ---
    // 클린 신호의 개별 레벨 트림 (±12dB)
    // APVTS 파라미터: "clean_level"
    cleanLevelKnob = std::make_unique<Knob> ("Clean", "clean_level", apvts);
    cleanLevelKnob->setVisible (false);
    addAndMakeVisible (*cleanLevelKnob);

    // --- Processed Level 노브 ---
    // 처리된 신호(앰프 체인)의 레벨 트림 (±12dB)
    // APVTS 파라미터: "processed_level"
    processedLevelKnob = std::make_unique<Knob> ("Proc", "processed_level", apvts);
    processedLevelKnob->setVisible (false);
    addAndMakeVisible (*processedLevelKnob);

    // --- IR Position 토글 ---
    // Cabinet(컨볼루션 IR)의 신호 체인 위치 선택
    // OFF (toggle=false) → Post-IR: Cabinet이 앰프 체인 뒤에 배치
    // ON (toggle=true) → Pre-IR: Cabinet이 혼합 신호에 적용
    // APVTS 파라미터: "ir_position"
    irPositionToggle.setButtonText ("Pre-IR");
    irPositionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "ir_position", irPositionToggle);
    irPositionToggle.setVisible (false);
    addAndMakeVisible (irPositionToggle);
}

DIBlendPanel::~DIBlendPanel() = default;

void DIBlendPanel::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;

    // 버튼 아이콘 업데이트
    // 펼침(true) = ▼ (아래 삼각형), 접음(false) = ▶ (오른쪽 삼각형)
    expandButton.setButtonText (expanded ? juce::CharPointer_UTF8 ("\xe2\x96\xbc")
                                         : juce::CharPointer_UTF8 ("\xe2\x96\xb6"));

    // 펼쳐졌을 때만 컨트롤 표시
    // (3개 노브: Blend, Clean, Processed + IR Position 토글)
    blendKnob->setVisible (expanded);
    cleanLevelKnob->setVisible (expanded);
    processedLevelKnob->setVisible (expanded);
    irPositionToggle.setVisible (expanded);

    // 상위 컴포넌트(SignalChainView)에 크기 변화를 통지
    // (collapsedHeight ↔ expandedHeight 전환 수행)
    if (onExpandChange)
        onExpandChange();
}

void DIBlendPanel::paint (juce::Graphics& g)
{
    // 패널 배경 렌더링: 짙은 회색 (0xff222244) 라운드 사각형
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff222244));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 패널 테두리 렌더링: 더 밝은 회색 (0xff444466), 1px 선
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void DIBlendPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // --- 헤더 영역 (28px 높이) ---
    // 접기/펼치기 버튼과 "DI Blend" 제목 라벨이 배치됨
    auto header = area.removeFromTop (28);

    // 헤더 왼쪽: 접기/펼치기 버튼 (28x28 정사각형)
    expandButton.setBounds (header.removeFromLeft (28));

    // 헤더 나머지: "DI Blend" 제목 라벨 (수평 확장)
    titleLabel.setBounds (header);

    // --- 본문 영역 (펼쳐졌을 때만) ---
    // 3개 노브(Blend, Clean, Processed) + IR Position 토글을 수평 배치
    if (expanded)
    {
        auto knobArea = area.reduced (10, 0);  // 좌우 10px 마진
        const int knobW = 80;   // 노브 너비
        const int knobH = 60;   // 노브 높이

        // Blend 노브 (좌측)
        blendKnob->setBounds (knobArea.removeFromLeft (knobW).withHeight (knobH));
        knobArea.removeFromLeft (5);  // 노브 간 간격

        // Clean Level 노브
        cleanLevelKnob->setBounds (knobArea.removeFromLeft (knobW).withHeight (knobH));
        knobArea.removeFromLeft (5);  // 노브 간 간격

        // Processed Level 노브
        processedLevelKnob->setBounds (knobArea.removeFromLeft (knobW).withHeight (knobH));
        knobArea.removeFromLeft (15);  // 토글과의 여유 간격

        // IR Position 토글 (우측, 100px 너비)
        irPositionToggle.setBounds (knobArea.removeFromLeft (100).withHeight (28));
    }
}
