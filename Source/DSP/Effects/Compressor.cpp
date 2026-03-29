#include "Compressor.h"
#include <cmath>

void Compressor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int> (spec.maximumBlockSize);

    compressor.prepare (spec);

    // Dry 버퍼를 prepareToPlay에서 미리 할당 (processBlock에서 재할당 금지)
    dryBuffer.setSize (1, maxBlockSize);
    dryBuffer.clear();

    reset();
}

void Compressor::reset()
{
    compressor.reset();
    gainReductionDb.store (0.0f);
}

void Compressor::setParameterPointers (std::atomic<float>* enabled,
                                        std::atomic<float>* threshold,
                                        std::atomic<float>* ratio,
                                        std::atomic<float>* attack,
                                        std::atomic<float>* release,
                                        std::atomic<float>* makeup,
                                        std::atomic<float>* dryBlend)
{
    enabledParam   = enabled;
    thresholdParam = threshold;
    ratioParam     = ratio;
    attackParam    = attack;
    releaseParam   = release;
    makeupParam    = makeup;
    dryBlendParam  = dryBlend;
}

void Compressor::process (juce::AudioBuffer<float>& buffer)
{
    // 바이패스 체크
    if (enabledParam != nullptr && enabledParam->load() < 0.5f)
    {
        gainReductionDb.store (0.0f);
        return;
    }

    const int numSamples = buffer.getNumSamples();

    // 파라미터 읽기 (atomic, 락프리)
    const float threshold = (thresholdParam != nullptr) ? thresholdParam->load() : -20.0f;
    const float ratio     = (ratioParam     != nullptr) ? ratioParam->load()     : 4.0f;
    const float attackMs  = (attackParam    != nullptr) ? attackParam->load()    : 10.0f;
    const float releaseMs = (releaseParam   != nullptr) ? releaseParam->load()   : 100.0f;
    const float makeupDb  = (makeupParam    != nullptr) ? makeupParam->load()    : 0.0f;
    const float dryBlend  = (dryBlendParam  != nullptr) ? dryBlendParam->load()  : 0.0f;

    // 컴프레서 파라미터 설정
    compressor.setThreshold (threshold);
    compressor.setRatio (ratio);
    compressor.setAttack (attackMs);
    compressor.setRelease (releaseMs);

    // 입력 RMS 측정 (게인 리덕션 계산용)
    const float inputRms = buffer.getRMSLevel (0, 0, numSamples);

    // Dry 버퍼 복사 (패러렐 컴프레션용)
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

    // 컴프레서 처리
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    compressor.process (context);

    // 메이크업 게인 적용
    if (std::abs (makeupDb) > 0.01f)
    {
        const float makeupLinear = juce::Decibels::decibelsToGain (makeupDb);
        buffer.applyGain (0, 0, numSamples, makeupLinear);
    }

    // 게인 리덕션 계산 (출력 RMS vs 입력 RMS)
    const float outputRms = buffer.getRMSLevel (0, 0, numSamples);
    if (inputRms > 1e-6f)
    {
        float grDb = juce::Decibels::gainToDecibels (outputRms / inputRms);
        // makeup을 제외한 순수 게인 리덕션
        grDb -= makeupDb;
        gainReductionDb.store (std::min (0.0f, grDb));
    }
    else
    {
        gainReductionDb.store (0.0f);
    }

    // Dry Blend 혼합 (패러렐 컴프레션)
    // dryBlend=0 -> 100% wet (compressed), dryBlend=1 -> 100% dry
    if (dryBlend > 0.001f)
    {
        const float* dry = dryBuffer.getReadPointer (0);
        float* wet = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = dryBlend * dry[i] + (1.0f - dryBlend) * wet[i];
    }
}
