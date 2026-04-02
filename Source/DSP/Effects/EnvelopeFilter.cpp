#include "EnvelopeFilter.h"

EnvelopeFilter::EnvelopeFilter() = default;

void EnvelopeFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    svfFilter.prepare (spec);
    svfFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

    envelopeLevel = 0.0f;

    // Envelope follower coefficients
    // Attack ~1ms (fast response to picking)
    // Release ~30ms (smooth filter closing)
    const float attackMs  = 1.0f;
    const float releaseMs = 30.0f;
    envAttackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * attackMs  / 1000.0f));
    envReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * releaseMs / 1000.0f));
}

void EnvelopeFilter::reset()
{
    svfFilter.reset();
    envelopeLevel = 0.0f;
}

void EnvelopeFilter::setParameterPointers (std::atomic<float>* enabled,
                                             std::atomic<float>* sensitivity,
                                             std::atomic<float>* freqMin,
                                             std::atomic<float>* freqMax,
                                             std::atomic<float>* resonance,
                                             std::atomic<float>* direction)
{
    enabledParam     = enabled;
    sensitivityParam = sensitivity;
    freqMinParam     = freqMin;
    freqMaxParam     = freqMax;
    resonanceParam   = resonance;
    directionParam   = direction;
}

//==============================================================================
// Process (audio thread)
//==============================================================================
void EnvelopeFilter::process (juce::AudioBuffer<float>& buffer)
{
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    const float sensitivity = sensitivityParam != nullptr ? sensitivityParam->load() : 0.5f;
    const float freqMin     = freqMinParam     != nullptr ? freqMinParam->load()     : 200.0f;
    const float freqMax     = freqMaxParam     != nullptr ? freqMaxParam->load()     : 4000.0f;
    const float resonance   = resonanceParam   != nullptr ? resonanceParam->load()   : 3.0f;
    const bool  directionUp = directionParam   != nullptr ? directionParam->load() < 0.5f : true;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);

    // Clamp resonance to valid range for SVF
    const float clampedQ = juce::jlimit (0.5f, 10.0f, resonance);
    svfFilter.setResonance (clampedQ);

    // Process sample by sample for per-sample envelope modulation
    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // Envelope follower
        const float absInput = std::abs (inputSample);
        if (absInput > envelopeLevel)
            envelopeLevel += envAttackCoeff * (absInput - envelopeLevel);
        else
            envelopeLevel += envReleaseCoeff * (absInput - envelopeLevel);

        // Map envelope to cutoff frequency
        // Sensitivity scales the envelope influence
        float envValue = juce::jlimit (0.0f, 1.0f, envelopeLevel * sensitivity * 10.0f);

        // Direction: Up = envelope increases cutoff, Down = envelope decreases cutoff
        float cutoff;
        if (directionUp)
            cutoff = freqMin + envValue * (freqMax - freqMin);
        else
            cutoff = freqMax - envValue * (freqMax - freqMin);

        // Clamp cutoff to valid range
        cutoff = juce::jlimit (20.0f, static_cast<float> (currentSampleRate) * 0.45f, cutoff);

        svfFilter.setCutoffFrequency (cutoff);

        // Process single sample through SVF
        data[i] = svfFilter.processSample (0, inputSample);
    }
}
