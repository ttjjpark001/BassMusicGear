#include "PowerAmp.h"

void PowerAmp::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    presenceFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 2000.0f, 0.707f, 1.0f);
    presenceFilter.prepare (spec);

    // Sag envelope follower coefficients
    // Attack: fast (~2ms) to respond to transients
    // Release: slow (~200ms) for natural sag feel
    sagAttackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.002f));
    sagReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.200f));
    sagEnvelope = 0.0f;

    updatePresenceFilter (0.5f);
    presenceNeedsUpdate.store (false);
}

void PowerAmp::reset()
{
    presenceFilter.reset();
    sagEnvelope = 0.0f;
    prevPresence = -1.0f;
}

void PowerAmp::setPowerAmpType (PowerAmpType type, bool sagEnabled)
{
    currentType = type;
    sagActive = sagEnabled;
}

void PowerAmp::setParameterPointers (std::atomic<float>* drive,
                                      std::atomic<float>* presence,
                                      std::atomic<float>* sag)
{
    driveParam    = drive;
    presenceParam = presence;
    sagParam      = sag;
}

void PowerAmp::updatePresenceFilter (float presenceValue)
{
    const float gainDB = (presenceValue - 0.5f) * 12.0f;

    auto tempCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 2000.0f, 0.707f, juce::Decibels::decibelsToGain (gainDB));
    auto* raw = tempCoeffs->getRawCoefficients();
    for (int i = 0; i < maxCoeffs; ++i)
        pendingCoeffValues[i] = raw[i];
    presenceNeedsUpdate.store (true);
}

void PowerAmp::process (juce::AudioBuffer<float>& buffer)
{
    // --- Presence coefficient swap (RT-safe) ---
    if (presenceNeedsUpdate.exchange (false) && presenceFilter.coefficients != nullptr)
    {
        auto* c = presenceFilter.coefficients->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            c[i] = pendingCoeffValues[i];
    }

    const float driveAmount = driveParam != nullptr ? driveParam->load() : 0.5f;
    const float sagAmount   = (sagParam != nullptr && sagActive) ? sagParam->load() : 0.0f;

    auto* data = buffer.getWritePointer (0);
    const int numSamples = buffer.getNumSamples();

    // Drive gain: exponential curve 1..40x
    const float driveGain = std::pow (40.0f, driveAmount);
    const float compensation = 0.7f;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = data[i];

        // --- Sag simulation ---
        // Track envelope of the signal. When signal is loud, sag reduces gain.
        // This emulates power tube plate voltage sagging under heavy load.
        float sagGainReduction = 1.0f;
        if (sagActive && sagAmount > 0.01f)
        {
            float absVal = std::abs (x * driveGain);

            // Envelope follower (asymmetric: fast attack, slow release)
            if (absVal > sagEnvelope)
                sagEnvelope += sagAttackCoeff * (absVal - sagEnvelope);
            else
                sagEnvelope += sagReleaseCoeff * (absVal - sagEnvelope);

            // Sag: higher envelope = lower gain (compressive, dynamic feel)
            // sagAmount controls the depth of the effect
            float sagDepth = sagAmount * 0.5f;  // max 50% gain reduction
            sagGainReduction = 1.0f - sagDepth * juce::jlimit (0.0f, 1.0f, sagEnvelope);
        }

        // Drive + Sag + tanh saturation
        float driven = x * driveGain * sagGainReduction;
        data[i] = std::tanh (driven) * compensation;
    }

    // --- Presence filter ---
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    presenceFilter.process (context);
}
