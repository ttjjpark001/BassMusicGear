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

    setSize (800, 550);
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
    g.drawFittedText ("Phase 3 -- Tuner + Compressor",
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
    ampPanel.setBounds (area.removeFromTop (300));

    area.removeFromTop (5);

    // Cabinet selector below (최소 95px: 패딩20 + 라벨20 + ComboBox25 + 간격5 + 버튼22 = 92px)
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}
