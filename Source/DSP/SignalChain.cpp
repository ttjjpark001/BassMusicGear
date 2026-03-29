#include "SignalChain.h"
#include <BinaryData.h>

void SignalChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    noiseGate.prepare (monoSpec);
    tuner.prepare (monoSpec);
    compressor.prepare (monoSpec);
    preamp.prepare (monoSpec);
    toneStack.prepare (monoSpec);
    powerAmp.prepare (monoSpec);
    cabinet.prepare (monoSpec);
}

void SignalChain::reset()
{
    noiseGate.reset();
    tuner.reset();
    compressor.reset();
    preamp.reset();
    toneStack.reset();
    powerAmp.reset();
    cabinet.reset();
}

int SignalChain::getTotalLatencyInSamples() const
{
    return preamp.getLatencyInSamples() + cabinet.getLatencyInSamples();
}

void SignalChain::setAmpModel (AmpModelId modelId)
{
    currentModelId = modelId;
    const auto& model = AmpModelLibrary::getModel (modelId);

    // Update DSP modules for the selected model
    toneStack.setType (model.toneStack);
    preamp.setPreampType (model.preamp);
    powerAmp.setPowerAmpType (model.powerAmp, model.sagEnabled);

    // Load default IR for this model
    // Map IR name to BinaryData
    if (model.defaultIRName == "ir_8x10_svt_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_8x10_svt_wav,
                                       BinaryData::ir_8x10_svt_wavSize);
    else if (model.defaultIRName == "ir_4x10_jbl_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_4x10_jbl_wav,
                                       BinaryData::ir_4x10_jbl_wavSize);
    else if (model.defaultIRName == "ir_1x15_vintage_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_1x15_vintage_wav,
                                       BinaryData::ir_1x15_vintage_wavSize);
    else if (model.defaultIRName == "ir_2x12_british_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_2x12_british_wav,
                                       BinaryData::ir_2x12_british_wavSize);
    else if (model.defaultIRName == "ir_2x10_modern_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_2x10_modern_wav,
                                       BinaryData::ir_2x10_modern_wavSize);
    else
        cabinet.loadDefaultIR();

    // Reset all DSP state to prevent artifacts from previous model's filter memory
    toneStack.reset();
    powerAmp.reset();

    // Force coefficient recalculation
    prevBass = prevMid = prevTreble = prevPresence = -1.0f;
    prevVpf = prevVle = prevGrunt = prevAttack = -1.0f;
    prevMidPos = -1;
}

void SignalChain::connectParameters (juce::AudioProcessorValueTreeState& apvts)
{
    noiseGate.setParameterPointers (
        apvts.getRawParameterValue ("gate_threshold"),
        apvts.getRawParameterValue ("gate_attack"),
        apvts.getRawParameterValue ("gate_hold"),
        apvts.getRawParameterValue ("gate_release"),
        apvts.getRawParameterValue ("gate_enabled"));

    preamp.setParameterPointers (
        apvts.getRawParameterValue ("input_gain"),
        apvts.getRawParameterValue ("volume"));

    toneStack.setParameterPointers (
        apvts.getRawParameterValue ("bass"),
        apvts.getRawParameterValue ("mid"),
        apvts.getRawParameterValue ("treble"),
        nullptr);

    powerAmp.setParameterPointers (
        apvts.getRawParameterValue ("drive"),
        apvts.getRawParameterValue ("presence"),
        apvts.getRawParameterValue ("sag"));

    cabinet.setParameterPointers (
        apvts.getRawParameterValue ("cab_bypass"));

    tuner.setParameterPointers (
        apvts.getRawParameterValue ("tuner_reference_a"),
        apvts.getRawParameterValue ("tuner_mute"));

    compressor.setParameterPointers (
        apvts.getRawParameterValue ("comp_enabled"),
        apvts.getRawParameterValue ("comp_threshold"),
        apvts.getRawParameterValue ("comp_ratio"),
        apvts.getRawParameterValue ("comp_attack"),
        apvts.getRawParameterValue ("comp_release"),
        apvts.getRawParameterValue ("comp_makeup"),
        apvts.getRawParameterValue ("comp_dry_blend"));
}

void SignalChain::updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts)
{
    // --- Amp model switch ---
    const int ampModelIndex = static_cast<int> (apvts.getRawParameterValue ("amp_model")->load());
    if (ampModelIndex != prevAmpModel)
    {
        setAmpModel (static_cast<AmpModelId> (ampModelIndex));
        prevAmpModel = ampModelIndex;

        // 모델 기본 IR에 맞춰 cab_ir 파라미터도 갱신 → CabinetSelector ComboBox 동기화
        const auto& newModel = AmpModelLibrary::getModel (static_cast<AmpModelId> (ampModelIndex));
        const int irIndex = irNameToIndex (newModel.defaultIRName);
        if (auto* p = apvts.getParameter ("cab_ir"))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (irIndex)));
    }

    // --- ToneStack coefficients ---
    const float bass   = apvts.getRawParameterValue ("bass")->load();
    const float mid    = apvts.getRawParameterValue ("mid")->load();
    const float treble = apvts.getRawParameterValue ("treble")->load();

    if (bass != prevBass || mid != prevMid || treble != prevTreble)
    {
        toneStack.updateCoefficients (bass, mid, treble);
        prevBass   = bass;
        prevMid    = mid;
        prevTreble = treble;
    }

    // --- PowerAmp Presence filter ---
    const float presence = apvts.getRawParameterValue ("presence")->load();
    if (presence != prevPresence)
    {
        powerAmp.updatePresenceFilter (presence);
        prevPresence = presence;
    }

    // --- Model-specific parameters ---
    const auto& model = AmpModelLibrary::getModel (currentModelId);

    // Italian Clean: VPF / VLE
    if (model.toneStack == ToneStackType::MarkbassFourBand)
    {
        const float vpf = apvts.getRawParameterValue ("vpf")->load();
        const float vle = apvts.getRawParameterValue ("vle")->load();
        if (vpf != prevVpf || vle != prevVle)
        {
            toneStack.updateMarkbassExtras (vpf, vle);
            prevVpf = vpf;
            prevVle = vle;
        }
    }

    // Modern Micro: Grunt / Attack
    if (model.toneStack == ToneStackType::BaxandallGrunt)
    {
        const float grunt  = apvts.getRawParameterValue ("grunt")->load();
        const float attack = apvts.getRawParameterValue ("attack")->load();
        if (grunt != prevGrunt || attack != prevAttack)
        {
            toneStack.updateModernExtras (grunt, attack);
            // Re-trigger coefficient computation with grunt/attack applied
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevGrunt  = grunt;
            prevAttack = attack;
        }
    }

    // American Vintage: Mid Position
    if (model.toneStack == ToneStackType::Baxandall)
    {
        const int midPos = static_cast<int> (apvts.getRawParameterValue ("mid_position")->load());
        if (midPos != prevMidPos)
        {
            toneStack.updateMidPosition (midPos);
            // Re-trigger coefficient computation
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevMidPos = midPos;
        }
    }
}

void SignalChain::process (juce::AudioBuffer<float>& buffer)
{
    // Signal chain order: Gate -> Tuner -> Compressor -> [BiAmp] -> Preamp -> ToneStack -> PowerAmp -> Cabinet
    noiseGate.process (buffer);
    tuner.process (buffer);
    compressor.process (buffer);
    // [BiAmp placeholder — Phase 6]
    preamp.process (buffer);
    toneStack.process (buffer);
    powerAmp.process (buffer);
    cabinet.process (buffer);
}
