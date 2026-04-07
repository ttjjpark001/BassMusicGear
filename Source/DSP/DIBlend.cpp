#include "DIBlend.h"

void DIBlend::prepare (const juce::dsp::ProcessSpec& /*spec*/)
{
    // No internal state to initialize
}

void DIBlend::reset()
{
    // No internal state to reset
}

void DIBlend::setParameterPointers (std::atomic<float>* blend,
                                     std::atomic<float>* cleanLevel,
                                     std::atomic<float>* processedLevel,
                                     std::atomic<float>* irPosition)
{
    blendParam          = blend;
    cleanLevelParam     = cleanLevel;
    processedLevelParam = processedLevel;
    irPositionParam     = irPosition;
}

int DIBlend::getIRPosition() const
{
    if (irPositionParam == nullptr)
        return 0;  // Default: Post-IR
    return static_cast<int> (irPositionParam->load());
}

void DIBlend::process (const juce::AudioBuffer<float>& cleanDI,
                        const juce::AudioBuffer<float>& processed,
                        juce::AudioBuffer<float>& output)
{
    const int numSamples = output.getNumSamples();

    // APVTS 파라미터를 atomic load로 읽음 (오디오 스레드 안전, 락프리)
    const float blend = (blendParam != nullptr) ? blendParam->load() : 0.5f;

    // 클린/처리 신호의 레벨 트림을 dB에서 선형 이득으로 변환
    // cleanDB, procDB: -12 ~ +12 dB 범위
    const float cleanDB  = (cleanLevelParam != nullptr)     ? cleanLevelParam->load()     : 0.0f;
    const float procDB   = (processedLevelParam != nullptr) ? processedLevelParam->load() : 0.0f;
    const float cleanGain = juce::Decibels::decibelsToGain (cleanDB);
    const float procGain  = juce::Decibels::decibelsToGain (procDB);

    const auto* cleanData = cleanDI.getReadPointer (0);
    const auto* procData  = processed.getReadPointer (0);
    auto* outData         = output.getWritePointer (0);

    // 혼합: blend가 0이면 cleanGain*cleanData, 1이면 procGain*procData 전달
    // blend 중간값: 양쪽의 크로스페이드
    // 형식: output[i] = cleanData[i] * cleanGain * (1 - blend) + procData[i] * procGain * blend
    for (int i = 0; i < numSamples; ++i)
    {
        outData[i] = (cleanData[i] * cleanGain * (1.0f - blend))
                   + (procData[i] * procGain * blend);
    }
}
