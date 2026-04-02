#include "Overdrive.h"

Overdrive::Overdrive() = default;

void Overdrive::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    oversampling4x.initProcessing (spec.maximumBlockSize);
    oversampling8x.initProcessing (spec.maximumBlockSize);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setCutoffFrequency (8000.0f);

    dryBuffer.setSize (1, static_cast<int> (spec.maximumBlockSize), false, true);

    // DC blocker coefficient: ~5Hz highpass
    dcBlockerCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f
                             / static_cast<float> (spec.sampleRate));
    dcPrevInput  = 0.0f;
    dcPrevOutput = 0.0f;
}

void Overdrive::reset()
{
    oversampling4x.reset();
    oversampling8x.reset();
    toneFilter.reset();
    dcPrevInput  = 0.0f;
    dcPrevOutput = 0.0f;
}

int Overdrive::getLatencyInSamples() const
{
    // Return the maximum latency (8x has more latency than 4x)
    // The actual latency depends on which type is active, but we report
    // the worst case for consistent PDC
    return static_cast<int> (oversampling8x.getLatencyInSamples());
}

void Overdrive::setParameterPointers (std::atomic<float>* enabled,
                                       std::atomic<float>* type,
                                       std::atomic<float>* drive,
                                       std::atomic<float>* tone,
                                       std::atomic<float>* dryBlend)
{
    enabledParam  = enabled;
    typeParam     = type;
    driveParam    = drive;
    toneParam     = tone;
    dryBlendParam = dryBlend;
}

//==============================================================================
// Tube: asymmetric tanh soft clipping (even harmonics, warm saturation)
//==============================================================================
void Overdrive::processTube (float* data, size_t numSamples, float driveGain)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        float x = driveGain * data[i];
        // Asymmetric tanh: even harmonic emphasis (tube character)
        data[i] = std::tanh (x + 0.15f * x * x);
    }
}

//==============================================================================
// JFET: parallel clean + driven path (modern bass character)
//==============================================================================
void Overdrive::processJFET (float* data, size_t numSamples, float driveGain)
{
    const float blend = std::min (driveGain / 5.0f, 1.0f);
    const float cleanMix = 1.0f - blend * 0.6f;
    const float driveMix = blend;

    for (size_t i = 0; i < numSamples; ++i)
    {
        float clean = data[i];
        float x = driveGain * data[i];

        // JFET asymmetric clipping
        float driven;
        if (x > 0.0f)
            driven = std::tanh (x * 1.5f);
        else
            driven = std::tanh (x * 2.0f) * 0.8f;

        data[i] = cleanMix * clean + driveMix * driven;
    }
}

//==============================================================================
// Fuzz: hard clipping (extreme saturation, needs 8x oversampling)
//==============================================================================
void Overdrive::processFuzz (float* data, size_t numSamples, float driveGain)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        float x = driveGain * data[i];
        // Hard clip with slight asymmetry
        float clipped;
        if (x > 0.7f)
            clipped = 0.7f + 0.1f * std::tanh ((x - 0.7f) * 5.0f);
        else if (x < -0.6f)
            clipped = -0.6f - 0.1f * std::tanh ((-x - 0.6f) * 5.0f);
        else
            clipped = x;

        data[i] = clipped;
    }
}

//==============================================================================
// Process (audio thread)
//==============================================================================
void Overdrive::process (juce::AudioBuffer<float>& buffer)
{
    // Bypass check
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    const int odType      = typeParam     != nullptr ? static_cast<int> (typeParam->load())     : 0;
    const float drive     = driveParam    != nullptr ? driveParam->load()    : 0.5f;
    const float tone      = toneParam     != nullptr ? toneParam->load()     : 0.5f;
    const float dryBlend  = dryBlendParam != nullptr ? dryBlendParam->load() : 0.0f;

    const int numSamples = buffer.getNumSamples();

    // Save dry signal for blending (dryBuffer allocated in prepare, no allocation here)
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

    // Drive gain mapping: 0~1 -> 1~20
    const float driveGain = 1.0f + drive * 19.0f;

    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);

    // Type-dependent oversampling and processing
    if (odType == 2) // Fuzz: 8x oversampling
    {
        auto oversampledBlock = oversampling8x.processSamplesUp (block);
        for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
        {
            processFuzz (oversampledBlock.getChannelPointer (ch),
                         oversampledBlock.getNumSamples(), driveGain);
        }
        oversampling8x.processSamplesDown (block);
    }
    else // Tube(0) or JFET(1): 4x oversampling
    {
        auto oversampledBlock = oversampling4x.processSamplesUp (block);
        for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
        {
            auto* data = oversampledBlock.getChannelPointer (ch);
            auto nSamples = oversampledBlock.getNumSamples();

            if (odType == 1)
                processJFET (data, nSamples, driveGain);
            else
                processTube (data, nSamples, driveGain);
        }
        oversampling4x.processSamplesDown (block);
    }

    // DC blocker
    {
        auto* blockData = block.getChannelPointer (0);
        auto numBlockSamples = block.getNumSamples();
        const float R = dcBlockerCoeff;
        float xPrev = dcPrevInput;
        float yPrev = dcPrevOutput;

        for (size_t i = 0; i < numBlockSamples; ++i)
        {
            float x = blockData[i];
            float y = x - xPrev + R * yPrev;
            xPrev = x;
            yPrev = y;
            blockData[i] = y;
        }

        dcPrevInput  = xPrev;
        dcPrevOutput = yPrev;
    }

    // Tone filter: cutoff 500Hz(tone=0) to 12kHz(tone=1)
    const float cutoff = 500.0f + tone * 11500.0f;
    toneFilter.setCutoffFrequency (cutoff);
    {
        auto singleBlock = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
        auto context = juce::dsp::ProcessContextReplacing<float> (singleBlock);
        toneFilter.process (context);
    }

    // Dry Blend: output = dryBlend * dry + (1 - dryBlend) * wet
    {
        const float* dry = dryBuffer.getReadPointer (0);
        float* wet = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = dryBlend * dry[i] + (1.0f - dryBlend) * wet[i];
    }
}
