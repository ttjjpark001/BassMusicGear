#include "Preamp.h"

Preamp::Preamp() = default;

void Preamp::prepare (const juce::dsp::ProcessSpec& spec)
{
    oversampling.initProcessing (spec.maximumBlockSize);

    // DC 블로킹 필터 계수 계산: 1차 하이패스 ~5Hz
    // R = 1 - (2*pi*fc / sampleRate), fc = 5Hz
    dcBlockerCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f
                             / static_cast<float> (spec.sampleRate));
    dcPrevInput  = 0.0f;
    dcPrevOutput = 0.0f;
}

void Preamp::reset()
{
    oversampling.reset();
    dcPrevInput  = 0.0f;
    dcPrevOutput = 0.0f;
}

int Preamp::getLatencyInSamples() const
{
    return static_cast<int> (oversampling.getLatencyInSamples());
}

void Preamp::setPreampType (PreampType type)
{
    currentType = type;
}

void Preamp::setParameterPointers (std::atomic<float>* inputGain,
                                    std::atomic<float>* volume)
{
    inputGainParam = inputGain;
    volumeParam    = volume;
}

//==============================================================================
// Tube 12AX7 Cascade: asymmetric tanh soft clipping (even harmonics)
//==============================================================================

void Preamp::processTube12AX7 (float* data, size_t numSamples,
                                float inGain, float outGain)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        float x = inGain * data[i];
        // Asymmetric tanh: x*x term introduces even harmonics (tube character)
        data[i] = std::tanh (x + 0.1f * x * x) * outGain;
    }
}

//==============================================================================
// JFET Parallel: parallel clean + driven paths (Modern Micro character)
//==============================================================================

void Preamp::processJFETParallel (float* data, size_t numSamples,
                                   float inGain, float outGain)
{
    // Parallel structure: clean path + driven path mixed together
    // This creates a distinctive modern bass tone with clarity + grit
    const float driveAmount = std::min (inGain / 10.0f, 1.0f);  // normalize drive level
    const float cleanMix = 1.0f - driveAmount * 0.5f;           // clean path stays present
    const float driveMix = driveAmount;

    for (size_t i = 0; i < numSamples; ++i)
    {
        float x = inGain * data[i];
        float clean = data[i];  // undriven signal

        // JFET asymmetric clipping: softer than tube, sharper knee
        float driven = x;
        if (driven > 0.0f)
            driven = std::tanh (driven * 1.5f);          // positive: softer clip
        else
            driven = std::tanh (driven * 2.0f) * 0.8f;   // negative: harder clip, lower headroom

        // Parallel mix
        data[i] = (cleanMix * clean + driveMix * driven) * outGain;
    }
}

//==============================================================================
// Class D Linear: pure gain, no clipping (Italian Clean headroom)
//==============================================================================

void Preamp::processClassDLinear (float* data, size_t numSamples,
                                   float inGain, float outGain)
{
    const float totalGain = inGain * outGain;
    for (size_t i = 0; i < numSamples; ++i)
        data[i] *= totalGain;
}

//==============================================================================
// Process (audio thread)
//==============================================================================

void Preamp::process (juce::AudioBuffer<float>& buffer)
{
    const float gainDB = inputGainParam != nullptr ? inputGainParam->load() : 0.0f;
    const float volDB  = volumeParam    != nullptr ? volumeParam->load()    : 0.0f;
    const float inGain = juce::Decibels::decibelsToGain (gainDB);
    const float outGain = juce::Decibels::decibelsToGain (volDB);

    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);

    // Class D Linear and SolidState Linear don't need oversampling (no nonlinearity)
    if (currentType == PreampType::ClassDLinear || currentType == PreampType::SolidStateLinear)
    {
        processClassDLinear (block.getChannelPointer (0),
                             static_cast<size_t> (block.getNumSamples()),
                             inGain, outGain);
        return;
    }

    // 4x oversampling for nonlinear processing
    auto oversampledBlock = oversampling.processSamplesUp (block);

    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer (ch);
        auto numSamples = oversampledBlock.getNumSamples();

        switch (currentType)
        {
            case PreampType::Tube12AX7Cascade:
                processTube12AX7 (data, numSamples, inGain, outGain);
                break;
            case PreampType::JFETParallel:
                processJFETParallel (data, numSamples, inGain, outGain);
                break;
            default:
                break;
        }
    }

    oversampling.processSamplesDown (block);

    // --- DC 블로킹 필터 적용 ---
    // 비대칭 웨이브쉐이핑(Tube12AX7, JFET)이 DC 오프셋을 생성하므로 제거한다.
    // 1차 하이패스: y[n] = x[n] - x[n-1] + R * y[n-1]
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
}
