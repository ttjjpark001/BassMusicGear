#include "BiAmpPanel.h"

BiAmpPanel::BiAmpPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // --- 접기/펼치기 버튼 ---
    // 초기 아이콘: ▶ (오른쪽 삼각형, 접혀있음)
    expandButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6"));
    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    // --- Bi-Amp ON/OFF 토글 버튼 ---
    // APVTS 파라미터 "biamp_on"에 바인딩
    // OFF (toggle=false) → 크로스오버 필터링 비활성화 (양쪽 전대역 통과)
    // ON (toggle=true) → LR4 크로스오버 활성화 (LP + HP 분할)
    enabledToggle.setButtonText ("Bi-Amp");
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "biamp_on", enabledToggle);
    addAndMakeVisible (enabledToggle);

    // --- 크로스오버 주파수 노브 ---
    // 초기: 숨김 상태, setExpanded(true) 호출 시에만 표시
    // 파라미터: "crossover_freq" (60-500 Hz, 기본값 200Hz)
    freqKnob = std::make_unique<Knob> ("Freq", "crossover_freq", apvts);
    freqKnob->setVisible (false);
    addAndMakeVisible (*freqKnob);
}

BiAmpPanel::~BiAmpPanel() = default;

void BiAmpPanel::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;

    // 버튼 아이콘 업데이트
    // 펼침(true) = ▼ (아래 삼각형), 접음(false) = ▶ (오른쪽 삼각형)
    expandButton.setButtonText (expanded ? juce::CharPointer_UTF8 ("\xe2\x96\xbc")
                                         : juce::CharPointer_UTF8 ("\xe2\x96\xb6"));

    // freqKnob는 펼쳤을 때만 표시
    freqKnob->setVisible (expanded);

    // 상위 컴포넌트(SignalChainView)에 크기 변화를 통지
    // (collapsedHeight ↔ expandedHeight 전환 수행)
    if (onExpandChange)
        onExpandChange();
}

void BiAmpPanel::paint (juce::Graphics& g)
{
    // 패널 배경 렌더링: 짙은 회색 (0xff222244) 라운드 사각형
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff222244));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 패널 테두리 렌더링: 더 밝은 회색 (0xff444466), 1px 선
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void BiAmpPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // --- 헤더 영역 (28px 높이) ---
    // 접기/펼치기 버튼과 Bi-Amp 토글 버튼이 배치됨
    auto header = area.removeFromTop (28);

    // 헤더 왼쪽: 접기/펼치기 버튼 (28x28 정사각형)
    expandButton.setBounds (header.removeFromLeft (28));

    // 헤더 나머지: Bi-Amp 토글 버튼 (수평 확장)
    enabledToggle.setBounds (header);

    // --- 본문 영역 (펼쳐졌을 때만) ---
    // 크로스오버 주파수 노브 배치
    if (expanded)
    {
        auto knobArea = area.reduced (10, 0);  // 좌우 10px 마진
        freqKnob->setBounds (knobArea.removeFromLeft (80).withHeight (60));  // 80x60 사이즈
    }
}
