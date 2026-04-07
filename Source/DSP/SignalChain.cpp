#include "SignalChain.h"
#include <BinaryData.h>

void SignalChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // --- Signal chain order ---
    noiseGate.prepare (monoSpec);
    tuner.prepare (monoSpec);
    compressor.prepare (monoSpec);
    biAmpCrossover.prepare (monoSpec);
    overdrive.prepare (monoSpec);
    octaver.prepare (monoSpec);
    envelopeFilter.prepare (monoSpec);
    preamp.prepare (monoSpec);
    ampVoicing.prepare (monoSpec);
    toneStack.prepare (monoSpec);
    graphicEQ.prepare (monoSpec);
    chorus.prepare (monoSpec);
    delay.prepare (monoSpec);
    reverb.prepare (monoSpec);
    powerAmp.prepare (monoSpec);
    cabinet.prepare (monoSpec);
    diBlend.prepare (monoSpec);

    // --- Allocate signal split buffers (prepareToPlay only, never in processBlock) ---
    const int maxSamples = static_cast<int> (spec.maximumBlockSize);
    cleanDIBuffer.setSize (1, maxSamples);
    ampChainBuffer.setSize (1, maxSamples);
    mixedBuffer.setSize (1, maxSamples);
}

void SignalChain::reset()
{
    noiseGate.reset();
    tuner.reset();
    compressor.reset();
    biAmpCrossover.reset();
    overdrive.reset();
    octaver.reset();
    envelopeFilter.reset();
    preamp.reset();
    ampVoicing.reset();
    toneStack.reset();
    graphicEQ.reset();
    chorus.reset();
    delay.reset();
    reverb.reset();
    powerAmp.reset();
    cabinet.reset();
}

int SignalChain::getTotalLatencyInSamples() const
{
    return overdrive.getLatencyInSamples()
         + preamp.getLatencyInSamples()
         + cabinet.getLatencyInSamples();
}

void SignalChain::setAmpModel (AmpModelId modelId)
{
    currentModelId = modelId;
    const auto& model = AmpModelLibrary::getModel (modelId);

    // --- DSP modules reconfiguration ---
    toneStack.setType (model.toneStack);
    preamp.setPreampType (model.preamp);
    powerAmp.setPowerAmpType (model.powerAmp, model.sagEnabled);

    // --- AmpVoicing: load model-specific voicing filter coefficients ---
    ampVoicing.setModel (modelId);

    // --- Cabinet default IR ---
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

    // --- Clear filter memory ---
    toneStack.reset();
    powerAmp.reset();
    ampVoicing.reset();

    // --- Force coefficient recalculation on next timer tick ---
    prevBass = prevMid = prevTreble = prevPresence = -1.0f;
    prevVpf = prevVle = prevGrunt = prevAttack = -1.0f;
    prevMidPos = -1;
}

void SignalChain::connectParameters (juce::AudioProcessorValueTreeState& apvts)
{
    // --- NoiseGate ---
    noiseGate.setParameterPointers (
        apvts.getRawParameterValue ("gate_threshold"),
        apvts.getRawParameterValue ("gate_attack"),
        apvts.getRawParameterValue ("gate_hold"),
        apvts.getRawParameterValue ("gate_release"),
        apvts.getRawParameterValue ("gate_enabled"));

    // --- Preamp ---
    preamp.setParameterPointers (
        apvts.getRawParameterValue ("input_gain"),
        apvts.getRawParameterValue ("volume"));

    // --- ToneStack ---
    toneStack.setParameterPointers (
        apvts.getRawParameterValue ("bass"),
        apvts.getRawParameterValue ("mid"),
        apvts.getRawParameterValue ("treble"),
        nullptr);

    // --- PowerAmp ---
    powerAmp.setParameterPointers (
        apvts.getRawParameterValue ("drive"),
        apvts.getRawParameterValue ("presence"),
        apvts.getRawParameterValue ("sag"));

    // --- Cabinet ---
    cabinet.setParameterPointers (
        apvts.getRawParameterValue ("cab_bypass"));

    // --- Tuner ---
    tuner.setParameterPointers (
        apvts.getRawParameterValue ("tuner_reference_a"),
        apvts.getRawParameterValue ("tuner_mute"));

    // --- Compressor ---
    compressor.setParameterPointers (
        apvts.getRawParameterValue ("comp_enabled"),
        apvts.getRawParameterValue ("comp_threshold"),
        apvts.getRawParameterValue ("comp_ratio"),
        apvts.getRawParameterValue ("comp_attack"),
        apvts.getRawParameterValue ("comp_release"),
        apvts.getRawParameterValue ("comp_makeup"),
        apvts.getRawParameterValue ("comp_dry_blend"));

    // --- Overdrive ---
    overdrive.setParameterPointers (
        apvts.getRawParameterValue ("od_enabled"),
        apvts.getRawParameterValue ("od_type"),
        apvts.getRawParameterValue ("od_drive"),
        apvts.getRawParameterValue ("od_tone"),
        apvts.getRawParameterValue ("od_dry_blend"));

    // --- Octaver ---
    octaver.setParameterPointers (
        apvts.getRawParameterValue ("oct_enabled"),
        apvts.getRawParameterValue ("oct_sub_level"),
        apvts.getRawParameterValue ("oct_up_level"),
        apvts.getRawParameterValue ("oct_dry_level"));

    // --- EnvelopeFilter ---
    envelopeFilter.setParameterPointers (
        apvts.getRawParameterValue ("ef_enabled"),
        apvts.getRawParameterValue ("ef_sensitivity"),
        apvts.getRawParameterValue ("ef_freq_min"),
        apvts.getRawParameterValue ("ef_freq_max"),
        apvts.getRawParameterValue ("ef_resonance"),
        apvts.getRawParameterValue ("ef_direction"));

    // --- GraphicEQ ---
    {
        std::atomic<float>* geqGains[GraphicEQ::numBands] = {
            apvts.getRawParameterValue ("geq_31"),
            apvts.getRawParameterValue ("geq_63"),
            apvts.getRawParameterValue ("geq_125"),
            apvts.getRawParameterValue ("geq_250"),
            apvts.getRawParameterValue ("geq_500"),
            apvts.getRawParameterValue ("geq_1k"),
            apvts.getRawParameterValue ("geq_2k"),
            apvts.getRawParameterValue ("geq_4k"),
            apvts.getRawParameterValue ("geq_8k"),
            apvts.getRawParameterValue ("geq_16k")
        };
        graphicEQ.setParameterPointers (
            apvts.getRawParameterValue ("geq_enabled"),
            geqGains);
    }

    // --- Chorus ---
    chorus.setParameterPointers (
        apvts.getRawParameterValue ("chorus_enabled"),
        apvts.getRawParameterValue ("chorus_rate"),
        apvts.getRawParameterValue ("chorus_depth"),
        apvts.getRawParameterValue ("chorus_mix"));

    // --- Delay ---
    delay.setParameterPointers (
        apvts.getRawParameterValue ("delay_enabled"),
        apvts.getRawParameterValue ("delay_time"),
        apvts.getRawParameterValue ("delay_feedback"),
        apvts.getRawParameterValue ("delay_damping"),
        apvts.getRawParameterValue ("delay_mix"));

    // --- Reverb ---
    reverb.setParameterPointers (
        apvts.getRawParameterValue ("reverb_enabled"),
        apvts.getRawParameterValue ("reverb_type"),
        apvts.getRawParameterValue ("reverb_size"),
        apvts.getRawParameterValue ("reverb_decay"),
        apvts.getRawParameterValue ("reverb_mix"));

    // --- BiAmpCrossover ---
    biAmpCrossover.setParameterPointers (
        apvts.getRawParameterValue ("biamp_on"),
        apvts.getRawParameterValue ("crossover_freq"));

    // --- DIBlend ---
    diBlend.setParameterPointers (
        apvts.getRawParameterValue ("di_blend"),
        apvts.getRawParameterValue ("clean_level"),
        apvts.getRawParameterValue ("processed_level"),
        apvts.getRawParameterValue ("ir_position"));

    // Cache IR position param for use in process()
    irPositionParam = apvts.getRawParameterValue ("ir_position");
}

void SignalChain::updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts)
{
    // --- Amp model switch ---
    const int ampModelIndex = static_cast<int> (apvts.getRawParameterValue ("amp_model")->load());
    if (ampModelIndex != prevAmpModel)
    {
        setAmpModel (static_cast<AmpModelId> (ampModelIndex));
        prevAmpModel = ampModelIndex;

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

    // --- GraphicEQ coefficients ---
    {
        static const char* geqIds[GraphicEQ::numBands] = {
            "geq_31", "geq_63", "geq_125", "geq_250", "geq_500",
            "geq_1k", "geq_2k", "geq_4k", "geq_8k", "geq_16k"
        };
        float currentGains[GraphicEQ::numBands];
        bool changed = false;
        for (int i = 0; i < GraphicEQ::numBands; ++i)
        {
            currentGains[i] = apvts.getRawParameterValue (geqIds[i])->load();
            if (currentGains[i] != prevGEQGains[i])
                changed = true;
        }
        if (changed)
        {
            graphicEQ.updateCoefficients (currentGains);
            for (int i = 0; i < GraphicEQ::numBands; ++i)
                prevGEQGains[i] = currentGains[i];
        }
    }

    // --- BiAmpCrossover frequency ---
    const float crossoverFreq = apvts.getRawParameterValue ("crossover_freq")->load();
    if (crossoverFreq != prevCrossoverFreq)
    {
        biAmpCrossover.updateCrossoverFrequency (crossoverFreq);
        prevCrossoverFreq = crossoverFreq;
    }

    // --- Model-specific parameters ---
    const auto& model = AmpModelLibrary::getModel (currentModelId);

    // Italian Clean: VPF/VLE
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

    // Modern Micro: Grunt/Attack
    if (model.toneStack == ToneStackType::BaxandallGrunt)
    {
        const float grunt  = apvts.getRawParameterValue ("grunt")->load();
        const float attack = apvts.getRawParameterValue ("attack")->load();
        if (grunt != prevGrunt || attack != prevAttack)
        {
            toneStack.updateModernExtras (grunt, attack);
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevGrunt  = grunt;
            prevAttack = attack;
        }
    }

    // American Vintage / Origin Pure: Mid Position (Baxandall models)
    if (model.toneStack == ToneStackType::Baxandall)
    {
        const int midPos = static_cast<int> (apvts.getRawParameterValue ("mid_position")->load());
        if (midPos != prevMidPos)
        {
            toneStack.updateMidPosition (midPos);
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevMidPos = midPos;
        }
    }
}

/**
 * @brief Process audio through the entire signal chain.
 *
 * Signal routing depends on IR Position parameter:
 *
 * Post-IR mode (ir_position=0):
 *   Input -> Gate -> Tuner -> Compressor -> BiAmpCrossover
 *   HP -> Pre-FX -> Preamp -> AmpVoicing -> ToneStack -> GEQ -> Post-FX -> PowerAmp -> Cabinet
 *   DIBlend(cleanDI, processedWithCabinet) -> output
 *
 * Pre-IR mode (ir_position=1):
 *   Input -> Gate -> Tuner -> Compressor -> BiAmpCrossover
 *   HP -> Pre-FX -> Preamp -> AmpVoicing -> ToneStack -> GEQ -> Post-FX -> PowerAmp
 *   DIBlend(cleanDI, processedNoCabinet) -> Cabinet -> output
 */
void SignalChain::process (juce::AudioBuffer<float>& buffer)
{
    // --- Pre-crossover processing ---
    noiseGate.process (buffer);
    tuner.process (buffer);
    compressor.process (buffer);

    // --- BiAmpCrossover: split into LP (cleanDI) and HP (amp chain) ---
    biAmpCrossover.process (buffer, cleanDIBuffer, ampChainBuffer);

    // --- Amp chain processing (on HP output) ---
    overdrive.process (ampChainBuffer);
    octaver.process (ampChainBuffer);
    envelopeFilter.process (ampChainBuffer);
    preamp.process (ampChainBuffer);
    ampVoicing.process (ampChainBuffer);      // Preamp -> AmpVoicing -> ToneStack
    toneStack.process (ampChainBuffer);
    graphicEQ.process (ampChainBuffer);
    chorus.process (ampChainBuffer);
    delay.process (ampChainBuffer);
    reverb.process (ampChainBuffer);
    powerAmp.process (ampChainBuffer);

    // --- IR Position routing ---
    const int irPos = (irPositionParam != nullptr)
                    ? static_cast<int> (irPositionParam->load())
                    : 0;

    if (irPos == 0)
    {
        // Post-IR: Cabinet on processed path, then DIBlend
        cabinet.process (ampChainBuffer);
        diBlend.process (cleanDIBuffer, ampChainBuffer, buffer);
    }
    else
    {
        // Pre-IR: DIBlend first, then Cabinet on mixed result
        diBlend.process (cleanDIBuffer, ampChainBuffer, buffer);
        cabinet.process (buffer);
    }
}
