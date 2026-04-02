#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Models/AmpModelLibrary.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    signalChain.connectParameters (apvts);
    startTimerHz (30);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
double PluginProcessor::getTailLengthSeconds() const { return 0.5; }

const juce::String PluginProcessor::getProgramName (int)           { return {}; }
void PluginProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;

    signalChain.prepare (spec);
    setLatencySamples (signalChain.getTotalLatencyInSamples());
}

void PluginProcessor::releaseResources()
{
    signalChain.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    signalChain.process (buffer);

    if (buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
void PluginProcessor::timerCallback()
{
    signalChain.updateCoefficientsFromMainThread (apvts);
}

//==============================================================================
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

bool PluginProcessor::hasEditor() const { return true; }

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    //--------------------------------------------------------------------------
    // Amp Model Selection
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "amp_model", 1 }, "Amp Model",
        AmpModelLibrary::getModelNames(), 1));  // default: Tweed Bass (index 1)

    //--------------------------------------------------------------------------
    // Noise Gate
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_threshold", 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_attack", 1 }, "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.1f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_hold", 1 }, "Gate Hold",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_release", 1 }, "Gate Release",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gate_enabled", 1 }, "Gate Enabled", true));

    //--------------------------------------------------------------------------
    // Preamp
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input_gain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-20.0f, 40.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    //--------------------------------------------------------------------------
    // ToneStack (shared Bass/Mid/Treble)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bass", 1 }, "Bass",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mid", 1 }, "Mid",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "treble", 1 }, "Treble",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    //--------------------------------------------------------------------------
    // American Vintage: Mid Position (5-position switch)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mid_position", 1 }, "Mid Position",
        juce::StringArray { "250 Hz", "500 Hz", "800 Hz", "1.5 kHz", "3 kHz" }, 1));

    //--------------------------------------------------------------------------
    // Italian Clean: VPF / VLE
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "vpf", 1 }, "VPF",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "vle", 1 }, "VLE",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    //--------------------------------------------------------------------------
    // Modern Micro: Grunt / Attack
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "grunt", 1 }, "Grunt",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    //--------------------------------------------------------------------------
    // PowerAmp
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "presence", 1 }, "Presence",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sag", 1 }, "Sag",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    //--------------------------------------------------------------------------
    // Cabinet
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "cab_bypass", 1 }, "Cabinet Bypass", false));

    //--------------------------------------------------------------------------
    // Cabinet IR Selection
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "cab_ir", 1 }, "Cabinet IR",
        juce::StringArray { "8x10 SVT", "4x10 JBL", "1x15 Vintage",
                            "2x12 British", "2x10 Modern" }, 0));

    //--------------------------------------------------------------------------
    // Tuner
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tuner_reference_a", 1 }, "Reference A",
        juce::NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tuner_mute", 1 }, "Tuner Mute", false));

    //--------------------------------------------------------------------------
    // Compressor
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "comp_enabled", 1 }, "Compressor", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_threshold", 1 }, "Comp Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_ratio", 1 }, "Comp Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel (":1")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_attack", 1 }, "Comp Attack",
        juce::NormalisableRange<float> (0.1f, 200.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_release", 1 }, "Comp Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_makeup", 1 }, "Comp Makeup",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_dry_blend", 1 }, "Comp Dry Blend",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    //--------------------------------------------------------------------------
    // Overdrive (Pre-FX)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "od_enabled", 1 }, "Overdrive", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "od_type", 1 }, "OD Type",
        juce::StringArray { "Tube", "JFET", "Fuzz" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "od_drive", 1 }, "OD Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "od_tone", 1 }, "OD Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "od_dry_blend", 1 }, "OD Dry Blend",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    //--------------------------------------------------------------------------
    // Octaver (Pre-FX)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "oct_enabled", 1 }, "Octaver", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "oct_sub_level", 1 }, "Sub Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "oct_up_level", 1 }, "Oct-Up Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "oct_dry_level", 1 }, "Oct Dry Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    //--------------------------------------------------------------------------
    // Envelope Filter (Pre-FX)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "ef_enabled", 1 }, "Envelope Filter", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_sensitivity", 1 }, "EF Sensitivity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_freq_min", 1 }, "EF Freq Min",
        juce::NormalisableRange<float> (100.0f, 500.0f, 1.0f), 200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_freq_max", 1 }, "EF Freq Max",
        juce::NormalisableRange<float> (1000.0f, 8000.0f, 1.0f), 4000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_resonance", 1 }, "EF Resonance",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.1f), 3.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "ef_direction", 1 }, "EF Direction",
        juce::StringArray { "Up", "Down" }, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
