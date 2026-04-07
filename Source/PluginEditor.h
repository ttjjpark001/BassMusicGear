#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/AmpPanel.h"
#include "UI/CabinetSelector.h"
#include "UI/TunerDisplay.h"
#include "UI/EffectBlock.h"
#include "UI/GraphicEQPanel.h"
#include "UI/BiAmpPanel.h"
#include "UI/DIBlendPanel.h"

/**
 * @brief BassMusicGear 플러그인 에디터 (UI, Phase 6)
 *
 * **레이아웃** (위에서 아래 순서):
 * - TunerDisplay (42px)
 * - Pre-FX 이펙터 블록 (Overdrive, Octaver, EnvelopeFilter)
 * - BiAmpPanel (Bi-Amp crossover ON/OFF + Freq)
 * - AmpPanel (290px)
 * - GraphicEQPanel (접기/펼치기)
 * - Post-FX 이펙터 블록 (Chorus, Delay, Reverb)
 * - DIBlendPanel (Blend/Clean/Processed + IR Position)
 * - CabinetSelector (95px)
 */
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int calculateNeededHeight() const;

    PluginProcessor& processorRef;

    TunerDisplay    tunerDisplay;
    AmpPanel        ampPanel;
    CabinetSelector cabinetSelector;

    // Pre-FX effect blocks
    std::unique_ptr<EffectBlock> overdriveBlock;
    std::unique_ptr<EffectBlock> octaverBlock;
    std::unique_ptr<EffectBlock> envelopeFilterBlock;

    // BiAmp crossover panel
    BiAmpPanel biAmpPanel;

    // Graphic EQ panel (10-band)
    GraphicEQPanel graphicEQPanel;

    // Post-FX effect blocks
    std::unique_ptr<EffectBlock> chorusBlock;
    std::unique_ptr<EffectBlock> delayBlock;
    std::unique_ptr<EffectBlock> reverbBlock;

    // DI Blend panel
    DIBlendPanel diBlendPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
