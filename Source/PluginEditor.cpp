#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      tunerDisplay (p.getSignalChain().getTuner(), p.apvts),
      ampPanel (p.apvts),
      cabinetSelector (p.apvts, p),
      biAmpPanel (p.apvts),
      graphicEQPanel (p.apvts),
      diBlendPanel (p.apvts)
{
    addAndMakeVisible (tunerDisplay);
    addAndMakeVisible (ampPanel);
    addAndMakeVisible (cabinetSelector);

    // GraphicEQ panel
    graphicEQPanel.onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (graphicEQPanel);

    // BiAmp panel
    biAmpPanel.onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (biAmpPanel);

    // DIBlend panel
    diBlendPanel.onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (diBlendPanel);

    // --- Pre-FX effect blocks ---
    overdriveBlock = std::make_unique<EffectBlock> (
        "Overdrive", p.apvts, "od_enabled",
        juce::StringArray { "od_drive", "od_tone", "od_dry_blend" },
        juce::StringArray { "Drive", "Tone", "Blend" },
        juce::StringArray { "od_type" },
        juce::StringArray { "Type" });
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
        juce::StringArray { "ef_sensitivity", "ef_freq_min", "ef_freq_max", "ef_resonance" },
        juce::StringArray { "Sens", "Freq Min", "Freq Max", "Reso" },
        juce::StringArray { "ef_direction" },
        juce::StringArray { "Direction" });
    envelopeFilterBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*envelopeFilterBlock);

    // --- Post-FX effect blocks ---
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

    setSize (800, calculateNeededHeight());
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 5, getWidth(), 30,
                      juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 6 -- BiAmp + DIBlend + AmpVoicing",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

int PluginEditor::calculateNeededHeight() const
{
    auto fxH = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    auto biAmpH = biAmpPanel.getExpanded() ? BiAmpPanel::expandedHeight : BiAmpPanel::collapsedHeight;
    auto diBlendH = diBlendPanel.getExpanded() ? DIBlendPanel::expandedHeight : DIBlendPanel::collapsedHeight;
    auto geqH = graphicEQPanel.getExpanded() ? GraphicEQPanel::expandedHeight : GraphicEQPanel::collapsedHeight;

    return 35                              // Title
         + 22                              // Footer
         + 42 + 4                          // TunerDisplay + gap
         + fxH (*overdriveBlock)     + 2   // Overdrive
         + fxH (*octaverBlock)       + 2   // Octaver
         + fxH (*envelopeFilterBlock) + 4  // EnvelopeFilter + gap
         + biAmpH + 4                      // BiAmpPanel + gap
         + 290 + 4                         // AmpPanel + gap
         + geqH + 4                        // GraphicEQ + gap
         + fxH (*chorusBlock)   + 2        // Chorus
         + fxH (*delayBlock)    + 2        // Delay
         + fxH (*reverbBlock)   + 4        // Reverb + gap
         + diBlendH + 4                    // DIBlendPanel + gap
         + 95                              // CabinetSelector
         + 10;                             // Margin
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (5);
    area.removeFromTop (35);   // Title
    area.removeFromBottom (22); // Footer

    // Tuner
    tunerDisplay.setBounds (area.removeFromTop (42));
    area.removeFromTop (4);

    // Pre-FX
    auto fxH = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    overdriveBlock->setBounds (area.removeFromTop (fxH (*overdriveBlock)));
    area.removeFromTop (2);
    octaverBlock->setBounds (area.removeFromTop (fxH (*octaverBlock)));
    area.removeFromTop (2);
    envelopeFilterBlock->setBounds (area.removeFromTop (fxH (*envelopeFilterBlock)));
    area.removeFromTop (4);

    // BiAmp panel
    const int biAmpH = biAmpPanel.getExpanded() ? BiAmpPanel::expandedHeight : BiAmpPanel::collapsedHeight;
    biAmpPanel.setBounds (area.removeFromTop (biAmpH));
    area.removeFromTop (4);

    // Amp panel
    ampPanel.setBounds (area.removeFromTop (290));
    area.removeFromTop (4);

    // GraphicEQ
    const int geqH = graphicEQPanel.getExpanded() ? GraphicEQPanel::expandedHeight : GraphicEQPanel::collapsedHeight;
    graphicEQPanel.setBounds (area.removeFromTop (geqH));
    area.removeFromTop (4);

    // Post-FX
    chorusBlock->setBounds (area.removeFromTop (fxH (*chorusBlock)));
    area.removeFromTop (2);
    delayBlock->setBounds (area.removeFromTop (fxH (*delayBlock)));
    area.removeFromTop (2);
    reverbBlock->setBounds (area.removeFromTop (fxH (*reverbBlock)));
    area.removeFromTop (4);

    // DIBlend panel
    const int diBlendH = diBlendPanel.getExpanded() ? DIBlendPanel::expandedHeight : DIBlendPanel::collapsedHeight;
    diBlendPanel.setBounds (area.removeFromTop (diBlendH));
    area.removeFromTop (4);

    // Cabinet selector
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}
