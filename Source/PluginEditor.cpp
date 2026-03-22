#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      // --- 노브 생성: 라벨, APVTS 파라미터 ID, APVTS 인스턴스 ---
      inputGainKnob ("Input Gain", "input_gain", p.apvts),  // Preamp 섹션
      volumeKnob    ("Volume",     "volume",     p.apvts),
      bassKnob      ("Bass",       "bass",       p.apvts),  // Tone Stack 섹션
      midKnob       ("Mid",        "mid",        p.apvts),
      trebleKnob    ("Treble",     "treble",     p.apvts),
      driveKnob     ("Drive",      "drive",      p.apvts),  // Power Amp 섹션
      presenceKnob  ("Presence",   "presence",   p.apvts)
{
    // --- 노브 표시 ---
    addAndMakeVisible (inputGainKnob);
    addAndMakeVisible (volumeKnob);
    addAndMakeVisible (bassKnob);
    addAndMakeVisible (midKnob);
    addAndMakeVisible (trebleKnob);
    addAndMakeVisible (driveKnob);
    addAndMakeVisible (presenceKnob);

    // --- Cabinet Bypass 토글 버튼 구성 ---
    cabBypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);  // 텍스트: 흰색
    cabBypassButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));  // 체크: 주황색
    addAndMakeVisible (cabBypassButton);

    // --- Cabinet Bypass 토글을 APVTS 파라미터에 바인딩 ---
    // ButtonAttachment가 자동으로 토글 ↔ APVTS 동기화
    cabBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "cab_bypass", cabBypassButton);

    // --- 에디터 윈도우 크기 설정 ---
    setSize (800, 500);
}

PluginEditor::~PluginEditor() = default;

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    // --- 다크 배경 ---
    // #1a1a2e: 진한 파란색/자주색 배경 (시각적 편의성, 밝은 텍스트와 대비)
    g.fillAll (juce::Colour (0xff1a1a2e));

    // --- 제목 "BassMusicGear" ---
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 10, getWidth(), 30,
                      juce::Justification::centred, 1);

    // --- 섹션 라벨 (주황색 강조) ---
    g.setFont (juce::FontOptions (14.0f));
    g.setColour (juce::Colour (0xffff8800));  // #ff8800: 주황색

    // Preamp 섹션 라벨
    g.drawFittedText ("PREAMP", 20, 50, 200, 20, juce::Justification::centred, 1);

    // Tone Stack 섹션 라벨
    g.drawFittedText ("TONE STACK", 240, 50, 300, 20, juce::Justification::centred, 1);

    // Power Amp 섹션 라벨
    g.drawFittedText ("POWER AMP", 560, 50, 220, 20, juce::Justification::centred, 1);

    // --- Phase 지시자 (하단) ---
    // 현재 개발 단계 표시 (향후 Phase 2, Phase 3 추가 기능 표시 예정)
    g.setColour (juce::Colour (0xff666688));  // 연한 회색
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 1 -- Core Signal Chain",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    // --- 레이아웃 파라미터 ---
    const int knobWidth  = 90;   // 노브 너비 (픽셀)
    const int knobHeight = 100;  // 노브 높이 (라벨 포함)
    const int topY       = 80;   // 노브 상단 Y 좌표
    const int gap        = 10;   // 노브 간격

    // --- Preamp 섹션: InputGain, Volume (좌측) ---
    inputGainKnob.setBounds (30, topY, knobWidth, knobHeight);
    volumeKnob.setBounds    (30 + knobWidth + gap, topY, knobWidth, knobHeight);

    // --- Tone Stack 섹션: Bass, Mid, Treble (중앙) ---
    int toneX = 260;  // Tone Stack 섹션 시작 X 좌표
    bassKnob.setBounds   (toneX, topY, knobWidth, knobHeight);
    midKnob.setBounds    (toneX + knobWidth + gap, topY, knobWidth, knobHeight);
    trebleKnob.setBounds (toneX + 2 * (knobWidth + gap), topY, knobWidth, knobHeight);

    // --- Power Amp 섹션: Drive, Presence (우측) ---
    int ampX = 580;  // Power Amp 섹션 시작 X 좌표
    driveKnob.setBounds    (ampX, topY, knobWidth, knobHeight);
    presenceKnob.setBounds (ampX + knobWidth + gap, topY, knobWidth, knobHeight);

    // --- Cabinet Bypass 토글 (노브 아래) ---
    // 노브들 하단(topY + knobHeight + 30) 위치에 150x25 크기로 배치
    cabBypassButton.setBounds (30, topY + knobHeight + 30, 150, 25);
}
