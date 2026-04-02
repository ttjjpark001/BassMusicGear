#include "EffectBlock.h"

EffectBlock::EffectBlock (const juce::String& name,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& enabledParamId,
                           const juce::StringArray& paramIds,
                           const juce::StringArray& paramLabels)
    : effectName (name),
      knobLabels (paramLabels)
{
    // ON/OFF toggle
    enabledToggle.setButtonText (name);
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, enabledParamId, enabledToggle);

    // Parameter knobs
    for (int i = 0; i < paramIds.size(); ++i)
    {
        const auto& paramId = paramIds[i];
        const auto& label = i < paramLabels.size() ? paramLabels[i] : paramId;

        auto* knob = knobs.add (new Knob (label, paramId, apvts));
        addAndMakeVisible (knob);
    }
}

EffectBlock::~EffectBlock() = default;

void EffectBlock::paint (juce::Graphics& g)
{
    // Background with subtle border
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void EffectBlock::resized()
{
    auto area = getLocalBounds().reduced (4);

    // Toggle on the left (80px wide)
    auto toggleArea = area.removeFromLeft (80);
    enabledToggle.setBounds (toggleArea.reduced (2));

    // Knobs fill the remaining space
    if (!knobs.isEmpty())
    {
        const int knobWidth = area.getWidth() / knobs.size();
        for (auto* knob : knobs)
        {
            knob->setBounds (area.removeFromLeft (knobWidth));
        }
    }
}
