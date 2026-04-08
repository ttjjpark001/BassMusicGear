#include "DIBlendPanel.h"

DIBlendPanel::DIBlendPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // --- 접기/펼치기 버튼 ---
    // 초기 상태: 접혀있음 (">"), 클릭 시 "v"(펼침)로 토글
    expandButton.setButtonText (">");
    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    // --- DI Blend ON/OFF 토글 ---
    // OFF (false): processed 신호만 출력 (BiAmpCrossover LP 무시)
    // ON (true): Blend/Clean Level/Processed Level로 혼합 활성
    enabledToggle.setButtonText ("DI Blend");
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "di_blend_on", enabledToggle);
    addAndMakeVisible (enabledToggle);

    // --- Blend 노브 ---
    // 클린 DI(LP 출력)과 처리된 신호(앰프 체인) 혼합 비율
    // APVTS "di_blend" 파라미터 (0.0=클린 100%, 1.0=처리음 100%)
    // 초기: 숨김, setExpanded(true)에서만 표시
    blendKnob = std::make_unique<Knob> ("Blend", "di_blend", apvts);
    blendKnob->setVisible (false);
    addAndMakeVisible (*blendKnob);

    // --- Clean Level 노브 ---
    // 클린 신호 개별 레벨 트림 (±12dB)
    // APVTS "clean_level" 파라미터
    // dB → linear gain 변환 후 혼합식에 적용
    cleanLevelKnob = std::make_unique<Knob> ("Clean", "clean_level", apvts);
    cleanLevelKnob->setVisible (false);
    addAndMakeVisible (*cleanLevelKnob);

    // --- Processed Level 노브 ---
    // 처리된 신호(앰프 체인) 개별 레벨 트림 (±12dB)
    // APVTS "processed_level" 파라미터
    // 두 신호의 균형을 세밀하게 조절 가능
    processedLevelKnob = std::make_unique<Knob> ("Proc", "processed_level", apvts);
    processedLevelKnob->setVisible (false);
    addAndMakeVisible (*processedLevelKnob);

    // --- IR Position(Cabinet 위치) 토글 ---
    // Cabinet(Convolution IR) 삽입 위치 선택
    // OFF (false) → Post-IR: Cabinet이 처리된 신호 뒤에 배치
    //   (앰프 체인 → Cabinet → DIBlend → 출력)
    // ON (true) → Pre-IR: Cabinet이 혼합 신호에 적용
    //   (DIBlend → Cabinet → 출력)
    // APVTS "ir_position" 파라미터 (0 또는 1)
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

    // 버튼 아이콘 업데이트: 펼침 = "v", 접음 = ">"
    expandButton.setButtonText (expanded ? "v" : ">");

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
    // 패널 배경 렌더링: 짙은 회색 (0xff2a2a3e) 라운드 사각형
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 패널 테두리 렌더링: 더 밝은 회색 (0xff444466), 1px 선
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void DIBlendPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // --- 헤더 영역 (28px 높이) ---
    // 접기/펼치기 버튼 + DI Blend ON/OFF 토글
    auto header = area.removeFromTop (28);

    // 헤더 왼쪽: 접기/펼치기 버튼 (정사각형, 28x28)
    expandButton.setBounds (header.removeFromLeft (28));

    // 헤더 우측: DI Blend ON/OFF 토글 (수평 확장)
    enabledToggle.setBounds (header);

    // --- 본문 영역 (펼쳐졌을 때만 표시) ---
    // Blend 노브 + Clean 노브 + Processed 노브 + IR Position 토글을 수평 배치
    if (expanded)
    {
        auto knobArea = area.reduced (10, 0);  // 좌우 10px 여백
        const int knobW = 80;   // 노브 너비 (회전 + 텍스트 박스 + 라벨)
        const int knobH = 80;   // 노브 높이 (회전 44px + 텍스트 18px + 라벨 18px)

        // 1. Blend 노브 (좌측)
        blendKnob->setBounds (knobArea.removeFromLeft (knobW).withHeight (knobH));
        knobArea.removeFromLeft (5);  // 노브 간 여백

        // 2. Clean Level 노브
        cleanLevelKnob->setBounds (knobArea.removeFromLeft (knobW).withHeight (knobH));
        knobArea.removeFromLeft (5);  // 노브 간 여백

        // 3. Processed Level 노브
        processedLevelKnob->setBounds (knobArea.removeFromLeft (knobW).withHeight (knobH));
        knobArea.removeFromLeft (15);  // IR Position 토글과의 여유 간격

        // 4. IR Position 토글 (우측, 100px 너비, 높이 28px)
        irPositionToggle.setBounds (knobArea.removeFromLeft (100).withHeight (28));
    }
}
