#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 클린 DI와 처리된 신호 혼합기 (IR Position 제어 포함)
 *
 * BiAmpCrossover의 LP 출력(클린 DI)과 앰프 체인 신호를 다음 공식에 따라 혼합한다:
 *
 *   mixed = (cleanDI * cleanGain * (1 - blend)) + (processed * processedGain * blend)
 *
 * IR Position은 Cabinet의 위치를 결정한다:
 * - Post-IR (0): Cabinet이 processed 경로에 있음. mixed 신호가 직접 출력됨.
 * - Pre-IR  (1): Cabinet이 mixed 신호에 적용됨.
 *
 * APVTS 파라미터:
 * - di_blend:        0.0-1.0 (0% 클린 ... 100% 처리음)
 * - clean_level:     -12 ~ +12 dB
 * - processed_level: -12 ~ +12 dB
 * - ir_position:     0=Post-IR, 1=Pre-IR
 */
class DIBlend
{
public:
    DIBlend() = default;

    /**
     * @brief DSP 처리 준비 (내부 상태 없음).
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    void reset();

    /**
     * @brief 클린 DI와 처리 신호를 혼합하여 출력 버퍼에 쓴다.
     *
     * 혼합 공식:
     *   output = cleanDI * cleanGain * (1 - blend) + processed * processedGain * blend
     *
     * @param cleanDI    클린 DI 신호 (BiAmpCrossover LP 출력)
     * @param processed  처리된 신호 (앰프 체인 출력)
     * @param output     혼합 결과를 쓸 출력 버퍼
     * @note [오디오 스레드] 메모리 할당 금지. atomic으로 파라미터 읽음.
     */
    void process (const juce::AudioBuffer<float>& cleanDI,
                  const juce::AudioBuffer<float>& processed,
                  juce::AudioBuffer<float>& output);

    /**
     * @brief APVTS 파라미터 포인터를 설정한다.
     *
     * @param blend             Blend 비율 (0.0=클린 100%, 1.0=처리음 100%)
     * @param cleanLevel        클린 신호 레벨 트림 (-12 ~ +12 dB)
     * @param processedLevel    처리 신호 레벨 트림 (-12 ~ +12 dB)
     * @param irPosition        Cabinet 위치 (0=Post-IR, 1=Pre-IR)
     */
    void setParameterPointers (std::atomic<float>* blend,
                               std::atomic<float>* cleanLevel,
                               std::atomic<float>* processedLevel,
                               std::atomic<float>* irPosition);

    /**
     * @brief 현재 IR Position(Cabinet 위치)를 반환한다.
     *
     * @return 0=Post-IR(Cabinet이 processed 경로), 1=Pre-IR(Cabinet이 mixed 신호에 적용).
     *         SignalChain에서 Cabinet 배치를 결정할 때 사용.
     */
    int getIRPosition() const;

private:
    std::atomic<float>* blendParam         = nullptr;
    std::atomic<float>* cleanLevelParam    = nullptr;
    std::atomic<float>* processedLevelParam = nullptr;
    std::atomic<float>* irPositionParam    = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DIBlend)
};
