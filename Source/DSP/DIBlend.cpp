#include "DIBlend.h"

void DIBlend::prepare (const juce::dsp::ProcessSpec& /*spec*/)
{
    // 내부 상태 없음 (클린 DI와 처리 신호는 외부에서 버퍼로 전달됨)
}

void DIBlend::reset()
{
    // 내부 상태 없음
}

void DIBlend::setParameterPointers (std::atomic<float>* blend,
                                     std::atomic<float>* cleanLevel,
                                     std::atomic<float>* processedLevel,
                                     std::atomic<float>* irPosition,
                                     std::atomic<float>* enabled)
{
    blendParam          = blend;
    cleanLevelParam     = cleanLevel;
    processedLevelParam = processedLevel;
    irPositionParam     = irPosition;
    enabledParam        = enabled;
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

    // 비활성화 상태: processed 신호만 그대로 출력
    // (DI Blend 모듈 전체를 우회하고 앰프 체인만 통과)
    const bool enabled = (enabledParam == nullptr) || (enabledParam->load() > 0.5f);
    if (! enabled)
    {
        output.copyFrom (0, 0, processed, 0, 0, numSamples);
        return;
    }

    // atomic load: 오디오 스레드에서 파라미터 값을 락프리로 읽음
    const float blend = (blendParam != nullptr) ? blendParam->load() : 0.5f;

    // dB → linear gain 변환 (-12 ~ +12 dB 범위)
    // 각 신호에 개별 레벨 트림을 적용한 후 블렌드
    const float cleanDB   = (cleanLevelParam     != nullptr) ? cleanLevelParam->load()     : 0.0f;
    const float procDB    = (processedLevelParam != nullptr) ? processedLevelParam->load() : 0.0f;
    const float cleanGain = juce::Decibels::decibelsToGain (cleanDB);
    const float procGain  = juce::Decibels::decibelsToGain (procDB);

    const auto* cleanData = cleanDI.getReadPointer (0);
    const auto* procData  = processed.getReadPointer (0);
    auto* outData         = output.getWritePointer (0);

    // 혼합 공식: mixed = cleanDI * cleanGain * (1 - blend) + processed * procGain * blend
    for (int i = 0; i < numSamples; ++i)
        outData[i] = (cleanData[i] * cleanGain * (1.0f - blend))
                   + (procData[i]  * procGain  * blend);
}
