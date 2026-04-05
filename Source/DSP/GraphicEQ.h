#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 10밴드 Constant-Q 그래픽 EQ
 *
 * 10개의 고정 주파수 피킹 바이쿼드 필터를 직렬로 배치한 10밴드 그래픽 EQ.
 * 각 밴드는 해당 주파수 중심에서 ±12dB 이득 조절 가능.
 * Constant-Q 특성으로 모든 밴드가 동일한 상대 대역폭(옥타브 기준)을 유지.
 *
 * **신호 체인 위치**: ToneStack → **GraphicEQ** → Chorus
 *
 * **Constant-Q 피킹 바이쿼드 계수**:
 * - omega = 2π × freq / SR (정규화 각속도)
 * - alpha = sin(omega) / (2Q) (대역폭 제어)
 * - A = 10^(gainDB/40) (선형 진폭, 양방향 dB 변환)
 * - b0 = 1 + alpha×A, b1 = -2×cos(omega), b2 = 1 - alpha×A (분자 계수)
 * - a0 = 1 + alpha/A, a1 = -2×cos(omega), a2 = 1 - alpha/A (분모 계수)
 *
 * **중요**: JUCE IIR::Coefficients는 내부적으로 atomic ReferenceCountedObjectPtr를 사용하므로,
 * 메인 스레드에서 계수 재할당 시 오디오 스레드가 안전하게 읽을 수 있다.
 */
class GraphicEQ
{
public:
    static constexpr int numBands = 10;
    static constexpr float bandFrequencies[numBands] = {
        31.0f, 63.0f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };

    GraphicEQ() = default;
    ~GraphicEQ() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    /**
     * @brief APVTS 파라미터 포인터를 모든 밴드 + 활성화 토글에 연결한다.
     *
     * @param enabled    ON/OFF 토글 (enabled != nullptr && *enabled > 0.5f 시 처리)
     * @param bandGains  10개 밴드의 원자적 포인터 배열 (geq_31~geq_16k, 각 ±12dB)
     * @note [메인 스레드] PluginProcessor 생성 시 SignalChain::connectParameters()에서 호출.
     *       오디오 스레드는 이 포인터들을 폴링하여 실시간 파라미터 변화를 감지한다.
     */
    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* bandGains[numBands]);

    /**
     * @brief 모든 밴드의 바이쿼드 계수를 재계산한다.
     *
     * 메인 스레드에서 호출되며, 각 밴드의 이득이 변경될 때 필터 계수를 갱신.
     * JUCE의 IIR::Coefficients atomic swap으로 안전하게 오디오 스레드에 반영된다.
     *
     * @param gains  10개 밴드의 이득값 배열 (dB, ±12 범위)
     * @note [메인 스레드] SignalChain::updateCoefficientsFromMainThread()에서 호출.
     *       GraphicEQ 파라미터 변경 감지 시 반복 계산 회피를 위해 변경된 밴드만 처리 가능 (P1).
     */
    void updateCoefficients (const float gains[numBands]);

private:
    double currentSampleRate = 44100.0;

    // 10 cascaded IIR biquad filters (Constant-Q peaking)
    juce::dsp::IIR::Filter<float> filters[numBands];

    // APVTS parameter pointers
    std::atomic<float>* enabledParam = nullptr;
    std::atomic<float>* bandGainParams[numBands] = {};

    // Q values for each band (Constant-Q: determined by bandwidth relative to neighbors)
    static float computeQ (int bandIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicEQ)
};
