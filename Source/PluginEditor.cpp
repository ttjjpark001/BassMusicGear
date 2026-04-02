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

    // Pre-FX effect blocks
    overdriveBlock = std::make_unique<EffectBlock> (
        "Overdrive", p.apvts, "od_enabled",
        juce::StringArray { "od_drive", "od_tone", "od_dry_blend" },
        juce::StringArray { "Drive", "Tone", "Blend" });
    addAndMakeVisible (*overdriveBlock);

    octaverBlock = std::make_unique<EffectBlock> (
        "Octaver", p.apvts, "oct_enabled",
        juce::StringArray { "oct_sub_level", "oct_up_level", "oct_dry_level" },
        juce::StringArray { "Sub", "Oct-Up", "Dry" });
    addAndMakeVisible (*octaverBlock);

    envelopeFilterBlock = std::make_unique<EffectBlock> (
        "Env Filter", p.apvts, "ef_enabled",
        juce::StringArray { "ef_sensitivity", "ef_resonance" },
        juce::StringArray { "Sens", "Reso" });
    addAndMakeVisible (*envelopeFilterBlock);

    setSize (800, 700);
}

PluginEditor::~PluginEditor() = default;

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 5, getWidth(), 30,
                      juce::Justification::centred, 1);

    // Phase indicator
    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 4 -- Pre-FX",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (5);
    area.removeFromTop (35);  // title space
    area.removeFromBottom (22);  // phase indicator

    // Tuner display at top (always visible)
    tunerDisplay.setBounds (area.removeFromTop (42));
    area.removeFromTop (4);

    // Amp panel takes most of the space
    ampPanel.setBounds (area.removeFromTop (260));

    area.removeFromTop (4);

    // Pre-FX section (3 effect blocks)
    overdriveBlock->setBounds (area.removeFromTop (55));
    area.removeFromTop (2);
    octaverBlock->setBounds (area.removeFromTop (55));
    area.removeFromTop (2);
    envelopeFilterBlock->setBounds (area.removeFromTop (55));

    area.removeFromTop (4);

    // Cabinet selector below
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}
