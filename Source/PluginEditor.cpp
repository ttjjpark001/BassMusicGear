#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      tunerDisplay (p.getSignalChain().getTuner(), p.apvts),
      ampPanel (p.apvts),
      cabinetSelector (p.apvts, p)
{
    addAndMakeVisible (tunerDisplay);
    addAndMakeVisible (ampPanel);
    addAndMakeVisible (cabinetSelector);

    // --- Pre-FX 이펙터 블록 (앰프 앞단) ---
    // 신호 흐름: Overdrive → Octaver → EnvelopeFilter → Preamp
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

    envelopeFilterBlock = std::make_unique<EffectBlock> (
        "Env Filter", p.apvts, "ef_enabled",
        juce::StringArray { "ef_sensitivity", "ef_resonance" },
        juce::StringArray { "Sens", "Reso" });
    envelopeFilterBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*envelopeFilterBlock);

    setSize (800, calculateNeededHeight());
}

PluginEditor::~PluginEditor() = default;

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 5, getWidth(), 30,
                      juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 4 -- Pre-FX",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

int PluginEditor::calculateNeededHeight() const
{
    auto fxH = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    return 35                              // 타이틀
         + 22                             // 하단 Phase 표시
         + 42 + 4                         // TunerDisplay + 갭
         + fxH (*overdriveBlock)     + 2  // Pre-FX 1
         + fxH (*octaverBlock)       + 2  // Pre-FX 2
         + fxH (*envelopeFilterBlock) + 6 // Pre-FX 3 + 갭
         + 290 + 4                        // AmpPanel + 갭
         + 95                             // CabinetSelector
         + 10;                            // 상하 패딩 (reduced 5)
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

    // 앰프 패널 (2행 노브까지 포함: row2Y + knobH = 175 + 95 = 270 + 여유 20)
    ampPanel.setBounds (area.removeFromTop (290));
    area.removeFromTop (4);

    // 캐비닛 선택 (IR)
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}
