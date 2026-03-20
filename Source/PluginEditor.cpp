#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // 기본 창 크기: 800 x 500
    // Phase 8에서 리사이즈 지원(setResizable)과 최소/최대 크기 제한이 추가된다.
    setSize (800, 500);
}

PluginEditor::~PluginEditor()
{
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    // --- 배경 ---
    // 다크 네이비 배경 (#1a1a2e). 전체 다크 테마는 Phase 8에서 완성된다.
    g.fillAll (juce::Colour (0xff1a1a2e));

    // --- 타이틀 텍스트 ---
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear",
                      0, 160, getWidth(), 50,
                      juce::Justification::centred, 1);

    // --- 부제 텍스트 ---
    // 회색 계열로 개발 진행 중임을 표시한다
    g.setColour (juce::Colour (0xffaaaaaa));
    g.setFont (juce::FontOptions (16.0f));
    g.drawFittedText ("Work in Progress",
                      0, 210, getWidth(), 30,
                      juce::Justification::centred, 1);

    // --- 빌드 단계 표시 ---
    // 현재 Phase를 표시해 개발 중 어느 단계인지 한눈에 파악할 수 있게 한다
    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText ("Phase 0 — Project Skeleton",
                      0, 250, getWidth(), 20,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    // Phase 0: 배치할 자식 컴포넌트가 없으므로 비어 있다.
    // Phase 1 이후 각 패널(AmpPanel, EffectBlock 등)의 setBounds() 호출이 추가된다.
}
