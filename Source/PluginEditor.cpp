#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // 기본 창 크기: 800 x 500
    // Phase 8 에서 리사이즈 지원이 추가된다
    setSize (800, 500);
}

PluginEditor::~PluginEditor()
{
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    // 어두운 배경 (다크 테마는 Phase 8 에서 완성)
    g.fillAll (juce::Colour (0xff1a1a2e));

    // 제목 텍스트
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear",
                      0, 160, getWidth(), 50,
                      juce::Justification::centred, 1);

    // 부제 텍스트
    g.setColour (juce::Colour (0xffaaaaaa));
    g.setFont (juce::FontOptions (16.0f));
    g.drawFittedText ("Work in Progress",
                      0, 210, getWidth(), 30,
                      juce::Justification::centred, 1);

    // 버전 텍스트
    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText ("Phase 0 — Project Skeleton",
                      0, 250, getWidth(), 20,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    // Phase 0: 레이아웃 요소 없음
    // 이후 Phase 에서 컴포넌트 배치 코드가 추가된다
}
