#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      ampPanel (p.apvts),
      cabinetSelector (p.apvts, p)
{
    addAndMakeVisible (ampPanel);
    addAndMakeVisible (cabinetSelector);

    setSize (800, 500);
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
    g.drawFittedText ("Phase 2 -- All 5 Amp Models",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (5);
    area.removeFromTop (35);  // title space
    area.removeFromBottom (22);  // phase indicator

    // Amp panel takes most of the space
    ampPanel.setBounds (area.removeFromTop (300));

    area.removeFromTop (5);

    // Cabinet selector below
    cabinetSelector.setBounds (area.removeFromTop (80).reduced (200, 0));
}
