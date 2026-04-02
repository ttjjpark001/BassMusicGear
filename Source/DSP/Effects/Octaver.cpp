#include "Octaver.h"

Octaver::Octaver() = default;

void Octaver::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // YIN internal buffer
    yinBuffer.resize (static_cast<size_t> (yinBufferSize / 2), 0.0f);

    // Ring buffer for accumulating input samples for YIN analysis
    inputRingBuffer.resize (static_cast<size_t> (yinBufferSize), 0.0f);
    contiguousBuffer.resize (static_cast<size_t> (yinBufferSize), 0.0f);
    ringWritePos = 0;
    ringSamplesAccumulated = 0;

    // Reset oscillator phases
    subPhase = 0.0;
    upPhase  = 0.0;

    currentFrequency = 0.0f;
    envelopeLevel = 0.0f;

    // Envelope follower coefficients (~5ms attack, ~50ms release)
    const float attackMs  = 5.0f;
    const float releaseMs = 50.0f;
    envelopeAttack  = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * attackMs  / 1000.0f));
    envelopeRelease = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * releaseMs / 1000.0f));
}

void Octaver::reset()
{
    subPhase = 0.0;
    upPhase  = 0.0;
    currentFrequency = 0.0f;
    envelopeLevel = 0.0f;
    ringWritePos = 0;
    ringSamplesAccumulated = 0;

    std::fill (inputRingBuffer.begin(), inputRingBuffer.end(), 0.0f);
}

void Octaver::setParameterPointers (std::atomic<float>* enabled,
                                     std::atomic<float>* subLevel,
                                     std::atomic<float>* upLevel,
                                     std::atomic<float>* dryLevel)
{
    enabledParam  = enabled;
    subLevelParam = subLevel;
    upLevelParam  = upLevel;
    dryLevelParam = dryLevel;
}

//==============================================================================
// YIN Pitch Detection
//==============================================================================
float Octaver::detectPitch (const float* data, int numSamples)
{
    // YIN algorithm steps:
    // 1. Difference function
    // 2. Cumulative mean normalized difference function
    // 3. Absolute threshold
    // 4. Parabolic interpolation

    const int halfN = numSamples / 2;
    if (halfN < 2) return 0.0f;

    auto& d = yinBuffer;
    if (static_cast<int> (d.size()) < halfN)
        return 0.0f; // safety

    // Step 1: Difference function
    d[0] = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfN; ++j)
        {
            float diff = data[j] - data[j + tau];
            sum += diff * diff;
        }
        d[static_cast<size_t> (tau)] = sum;
    }

    // Step 2: Cumulative mean normalized difference function
    d[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < halfN; ++tau)
    {
        runningSum += d[static_cast<size_t> (tau)];
        if (runningSum > 0.0f)
            d[static_cast<size_t> (tau)] = d[static_cast<size_t> (tau)] * static_cast<float> (tau) / runningSum;
        else
            d[static_cast<size_t> (tau)] = 1.0f;
    }

    // Step 3: Absolute threshold (find first dip below 0.15)
    constexpr float threshold = 0.15f;
    int tauEstimate = -1;

    // Minimum tau corresponds to max frequency (330Hz for bass)
    const int minTau = static_cast<int> (currentSampleRate / 330.0);
    // Maximum tau corresponds to min frequency (41Hz for bass)
    const int maxTau = std::min (halfN - 1, static_cast<int> (currentSampleRate / 41.0));

    for (int tau = minTau; tau <= maxTau; ++tau)
    {
        if (d[static_cast<size_t> (tau)] < threshold)
        {
            // Find the local minimum after threshold crossing
            while (tau + 1 <= maxTau &&
                   d[static_cast<size_t> (tau + 1)] < d[static_cast<size_t> (tau)])
                ++tau;
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate < 1)
        return 0.0f;  // No pitch detected

    // Step 4: Parabolic interpolation for sub-sample accuracy
    float betterTau = static_cast<float> (tauEstimate);
    if (tauEstimate > 0 && tauEstimate < halfN - 1)
    {
        float s0 = d[static_cast<size_t> (tauEstimate - 1)];
        float s1 = d[static_cast<size_t> (tauEstimate)];
        float s2 = d[static_cast<size_t> (tauEstimate + 1)];

        float denom = 2.0f * (2.0f * s1 - s0 - s2);
        if (std::abs (denom) > 1e-10f)
            betterTau = static_cast<float> (tauEstimate) + (s0 - s2) / denom;
    }

    return static_cast<float> (currentSampleRate) / betterTau;
}

//==============================================================================
// Process (audio thread)
//==============================================================================
void Octaver::process (juce::AudioBuffer<float>& buffer)
{
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    const float subLevel = subLevelParam != nullptr ? subLevelParam->load() : 0.0f;
    const float upLevel  = upLevelParam  != nullptr ? upLevelParam->load()  : 0.0f;
    const float dryLevel = dryLevelParam != nullptr ? dryLevelParam->load() : 1.0f;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);

    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // Accumulate input into ring buffer for YIN analysis
        inputRingBuffer[static_cast<size_t> (ringWritePos)] = inputSample;
        ringWritePos = (ringWritePos + 1) % yinBufferSize;
        ringSamplesAccumulated++;

        // Run YIN every yinBufferSize/4 samples (hop size)
        if (ringSamplesAccumulated >= yinBufferSize / 4)
        {
            ringSamplesAccumulated = 0;

            // Build contiguous buffer from ring buffer for YIN
            // contiguousBuffer allocated in prepare(), no allocation here
            for (int j = 0; j < yinBufferSize; ++j)
            {
                int idx = (ringWritePos + j) % yinBufferSize;
                contiguousBuffer[static_cast<size_t> (j)] = inputRingBuffer[static_cast<size_t> (idx)];
            }

            float detected = detectPitch (contiguousBuffer.data(), yinBufferSize);

            // Smooth frequency transition
            if (detected > 0.0f)
            {
                if (currentFrequency > 0.0f)
                    currentFrequency = currentFrequency * 0.7f + detected * 0.3f;
                else
                    currentFrequency = detected;
            }
        }

        // Envelope follower
        const float absInput = std::abs (inputSample);
        if (absInput > envelopeLevel)
            envelopeLevel += envelopeAttack * (absInput - envelopeLevel);
        else
            envelopeLevel += envelopeRelease * (absInput - envelopeLevel);

        // Synthesize sub-octave and octave-up sine waves
        float subSample = 0.0f;
        float upSample  = 0.0f;

        if (currentFrequency > 0.0f && envelopeLevel > 0.001f)
        {
            // Sub-octave: F0/2
            const double subFreq = static_cast<double> (currentFrequency) * 0.5;
            subPhase += subFreq / currentSampleRate;
            if (subPhase >= 1.0) subPhase -= 1.0;
            subSample = static_cast<float> (std::sin (twoPi * subPhase)) * envelopeLevel;

            // Octave-up: F0*2
            const double upFreq = static_cast<double> (currentFrequency) * 2.0;
            upPhase += upFreq / currentSampleRate;
            if (upPhase >= 1.0) upPhase -= 1.0;
            upSample = static_cast<float> (std::sin (twoPi * upPhase)) * envelopeLevel;
        }

        // Mix: Sub + OctUp + Dry
        data[i] = dryLevel * inputSample
                + subLevel * subSample
                + upLevel  * upSample;
    }
}
