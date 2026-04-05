#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      tunerDisplay (p.getSignalChain().getTuner(), p.apvts),
      ampPanel (p.apvts),
      cabinetSelector (p.apvts, p),
      graphicEQPanel (p.apvts)
{
    // 메인 UI 컴포넌트 추가 (위에서 아래 순서로 배치)
    addAndMakeVisible (tunerDisplay);      // 크로매틱 튜너 (상단 고정)
    addAndMakeVisible (ampPanel);          // 앰프 모델 및 톤 컨트롤
    addAndMakeVisible (cabinetSelector);   // 캐비닛 IR 선택
    addAndMakeVisible (graphicEQPanel);    // 10밴드 그래픽 EQ

    // --- Pre-FX 이펙터 블록 (앰프 앞단, 접기/펼치기 가능) ---
    // 신호 흐름: Overdrive → Octaver → EnvelopeFilter → Preamp
    // 각 블록의 펼침 상태 변경 시 레이아웃 재계산을 트리거하여 창 높이 자동 조정
    overdriveBlock = std::make_unique<EffectBlock> (
        "Overdrive", p.apvts, "od_enabled",
        juce::StringArray { "od_drive", "od_tone", "od_dry_blend" },
        juce::StringArray { "Drive", "Tone", "Blend" });
    overdriveBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*overdriveBlock);

    octaverBlock = std::make_unique<EffectBlock> (
        "Octaver", p.apvts, "oct_enabled",
        juce::StringArray { "oct_sub_level", "oct_up_level", "oct_dry_level" },
        juce::StringArray { "Sub", "Oct-Up", "Dry" });
    octaverBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*octaverBlock);

    // EnvelopeFilter: 파라미터 노브 + Direction ComboBox
    envelopeFilterBlock = std::make_unique<EffectBlock> (
        "Env Filter", p.apvts, "ef_enabled",
        juce::StringArray { "ef_sensitivity", "ef_freq_min", "ef_freq_max", "ef_resonance" },
        juce::StringArray { "Sens", "Freq Min", "Freq Max", "Reso" },
        juce::StringArray { "ef_direction" },
        juce::StringArray { "Direction" });
    envelopeFilterBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*envelopeFilterBlock);

    // --- Post-FX 이펙터 블록 (앰프 뒷단, 접기/펼치기 가능) ---
    // 신호 흐름: GraphicEQ → Chorus → Delay → Reverb → PowerAmp
    chorusBlock = std::make_unique<EffectBlock> (
        "Chorus", p.apvts, "chorus_enabled",
        juce::StringArray { "chorus_rate", "chorus_depth", "chorus_mix" },
        juce::StringArray { "Rate", "Depth", "Mix" });
    chorusBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*chorusBlock);

    delayBlock = std::make_unique<EffectBlock> (
        "Delay", p.apvts, "delay_enabled",
        juce::StringArray { "delay_time", "delay_feedback", "delay_damping", "delay_mix" },
        juce::StringArray { "Time", "Feedback", "Damp", "Mix" });
    delayBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*delayBlock);

    reverbBlock = std::make_unique<EffectBlock> (
        "Reverb", p.apvts, "reverb_enabled",
        juce::StringArray { "reverb_size", "reverb_decay", "reverb_mix" },
        juce::StringArray { "Size", "Decay", "Mix" },
        juce::StringArray { "reverb_type" },
        juce::StringArray { "Type" });
    reverbBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*reverbBlock);

    // 초기 창 크기: 800px 너비 × 필요 높이 (모든 블록의 현재 상태에 따라 계산)
    setSize (800, calculateNeededHeight());
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    // 배경 색상: 다크 네이비
    g.fillAll (juce::Colour (0xff1a1a2e));

    // 타이틀: "BassMusicGear" (상단, 하얀색, 굵은체)
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 5, getWidth(), 30,
                      juce::Justification::centred, 1);

    // 하단 텍스트: Phase 표시 (회색)
    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 5 -- GEQ + Post-FX",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

int PluginEditor::calculateNeededHeight() const
{
    // 각 EffectBlock의 현재 상태(접힘/펼침)에 따른 높이 계산
    auto fxH = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    // 전체 창 높이 = 고정 요소 + 동적 Pre-FX 블록 + 고정 중앙부 + 동적 Post-FX 블록 + 하단 패딩
    return 35                              // 타이틀 (BassMusicGear, 35px)
         + 22                              // 하단 Phase 표시 (22px)
         + 42 + 4                          // TunerDisplay(42px) + 갭(4px)
         + fxH (*overdriveBlock)     + 2   // Overdrive (접힘36/펼침130px) + 갭(2px)
         + fxH (*octaverBlock)       + 2   // Octaver + 갭(2px)
         + fxH (*envelopeFilterBlock) + 6  // EnvelopeFilter + 갭(6px)
         + 290 + 4                         // AmpPanel(290px 고정) + 갭(4px)
         + GraphicEQPanel::panelHeight + 4 // GraphicEQ(200px 고정) + 갭(4px)
         + fxH (*chorusBlock)   + 2        // Chorus (접힘36/펼침130px) + 갭(2px)
         + fxH (*delayBlock)    + 2        // Delay + 갭(2px)
         + fxH (*reverbBlock)   + 6        // Reverb + 갭(6px)
         + 95                              // CabinetSelector(95px 고정)
         + 10;                             // 상하 마진(10px)
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (5);
    area.removeFromTop (35);   // 타이틀
    area.removeFromBottom (22); // 페이즈 표시

    // 튜너: 상단 고정
    tunerDisplay.setBounds (area.removeFromTop (42));
    area.removeFromTop (4);

    // Pre-FX 블록 (앰프 앞단) — 접힘/펼침 상태에 따라 높이 동적 계산
    auto preFxHeight = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    overdriveBlock->setBounds (area.removeFromTop (preFxHeight (*overdriveBlock)));
    area.removeFromTop (2);
    octaverBlock->setBounds (area.removeFromTop (preFxHeight (*octaverBlock)));
    area.removeFromTop (2);
    envelopeFilterBlock->setBounds (area.removeFromTop (preFxHeight (*envelopeFilterBlock)));
    area.removeFromTop (6);

    // 앰프 패널
    ampPanel.setBounds (area.removeFromTop (290));
    area.removeFromTop (4);

    // GraphicEQ 패널
    graphicEQPanel.setBounds (area.removeFromTop (GraphicEQPanel::panelHeight));
    area.removeFromTop (4);

    // Post-FX 블록
    auto postFxHeight = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    chorusBlock->setBounds (area.removeFromTop (postFxHeight (*chorusBlock)));
    area.removeFromTop (2);
    delayBlock->setBounds (area.removeFromTop (postFxHeight (*delayBlock)));
    area.removeFromTop (2);
    reverbBlock->setBounds (area.removeFromTop (postFxHeight (*reverbBlock)));
    area.removeFromTop (6);

    // 캐비닛 선택 (IR)
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}
