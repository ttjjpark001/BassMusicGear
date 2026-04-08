#include "BiAmpPanel.h"

BiAmpPanel::BiAmpPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // --- 접기/펼치기 버튼 ---
    // 초기 상태: 접혀있음 (">"), 클릭 시 "v"(펼침)로 토글
    expandButton.setButtonText (">");
    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    // --- Bi-Amp ON/OFF 토글 버튼 ---
    // APVTS "biamp_on" 파라미터 바인딩
    // OFF (false): 크로스오버 필터링 비활성화 (양쪽 전대역 통과, 신호 분할 없음)
    // ON (true): LR4 크로스오버 활성화 (LP 클린DI + HP 앰프 체인 분할)
    enabledToggle.setButtonText ("Bi-Amp");
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "biamp_on", enabledToggle);
    addAndMakeVisible (enabledToggle);

    // --- 크로스오버 주파수 노브 ---
    // APVTS "crossover_freq" 파라미터 (60-500 Hz, 기본값 200Hz)
    // 초기: 숨김 상태, setExpanded(true)에서만 표시
    // LP(저역)과 HP(고역)의 분할점 주파수
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

    // 버튼 아이콘 업데이트: 펼침 = "v", 접음 = ">"
    expandButton.setButtonText (expanded ? "v" : ">");

    // freqKnob는 펼쳤을 때만 표시
    freqKnob->setVisible (expanded);

    // 상위 컴포넌트(SignalChainView)에 크기 변화를 통지
    // (collapsedHeight ↔ expandedHeight 전환 수행)
    if (onExpandChange)
        onExpandChange();
}

void BiAmpPanel::paint (juce::Graphics& g)
{
    // 패널 배경 렌더링: 짙은 회색 (0xff2a2a3e) 라운드 사각형
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 패널 테두리 렌더링: 더 밝은 회색 (0xff444466), 1px 선
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void BiAmpPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // --- 헤더 영역 (28px 높이) ---
    // 접기/펼치기 버튼 + Bi-Amp ON/OFF 토글
    auto header = area.removeFromTop (28);

    // 헤더 왼쪽: 접기/펼치기 버튼 (정사각형, 28x28)
    expandButton.setBounds (header.removeFromLeft (28));

    // 헤더 우측: Bi-Amp 토글 버튼 (수평 확장)
    enabledToggle.setBounds (header);

    // --- 본문 영역 (펼쳐졌을 때만 표시) ---
    // Freq 노브: 80x80 픽셀 (회전 노브 44px + 텍스트 박스 18px + 라벨 18px)
    if (expanded)
    {
        auto knobArea = area.reduced (10, 0);  // 좌우 10px 여백
        freqKnob->setBounds (knobArea.removeFromLeft (80).withHeight (80));
    }
}
