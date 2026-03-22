#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

/**
 * @brief 3밴드 액티브 EQ 톤스택 (Phase 1 임시 구현)
 *
 * Phase 1에서는 간단하고 확실히 들리는 3밴드 액티브 EQ를 사용한다.
 * - Bass : 80Hz 저역 셸빙 ±15dB
 * - Mid  : 500Hz 피킹  ±12dB  (Q=1.5)
 * - Treble: 3kHz 고역 셸빙 ±15dB
 *
 * Phase 2 예정: 모델별 게인 스테이징 구현 후 각 앰프 모델의 실제 토폴로지로 교체.
 * - Tweed Bass   : Fender TMB 패시브 RC (Yeh 2006 bilinear transform)
 * - American Vintage : Active Baxandall
 * - British Stack    : James 토폴로지
 * - Modern Micro     : Baxandall + Grunt/Attack
 * - Italian Clean    : 4-band + VPF/VLE
 *
 * 계수 계산: 메인 스레드 (updateCoefficients)
 * 계수 적용: 오디오 스레드 (process 내 RT-safe 교체)
 */
class ToneStack
{
public:
    ToneStack() = default;

    /** @brief DSP 처리 스펙 설정 */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 버퍼에 3밴드 EQ 적용
     * @note [오디오 스레드]
     */
    void process (juce::AudioBuffer<float>& buffer);

    /** @brief 필터 상태(지연 라인) 초기화 */
    void reset();

    /**
     * @brief Bass/Mid/Treble 파라미터로부터 IIR 계수를 재계산한다.
     *
     * 0..1 노브 값을 dB 게인으로 매핑한 후 JUCE IIR 계수를 생성.
     * 메인 스레드에서만 호출해야 한다.
     *
     * @param bass   0..1  → 80Hz 셸빙 -15..+15dB
     * @param mid    0..1  → 500Hz 피킹  -12..+12dB
     * @param treble 0..1  → 3kHz  셸빙 -15..+15dB
     * @note [메인 스레드 전용]
     */
    void updateCoefficients (float bass, float mid, float treble);

    /**
     * @brief APVTS 파라미터 포인터를 캐시한다.
     * @note [메인 스레드 전용]
     */
    void setParameterPointers (std::atomic<float>* bass,
                               std::atomic<float>* mid,
                               std::atomic<float>* treble,
                               std::atomic<float>* enabled);

private:
    /** @brief 오디오 스레드에서 대기 중인 계수를 적용한다. */
    void applyPendingCoefficients();

    // --- 3개 독립 바이쿼드 필터 ---
    juce::dsp::IIR::Filter<float> bassFilter;
    juce::dsp::IIR::Filter<float> midFilter;
    juce::dsp::IIR::Filter<float> trebleFilter;

    // --- RT-safe 계수 전달 (raw float 배열, Ptr swap 없음) ---
    // JUCE IIR biquad는 [b0, b1, b2, a1, a2] 5개 계수를 저장한다.
    // a0은 1.0으로 정규화되어 배열에 포함되지 않는다.
    static constexpr int maxCoeffs = 5;
    float pendingBassCoeffs   [maxCoeffs] = {};
    float pendingMidCoeffs    [maxCoeffs] = {};
    float pendingTrebleCoeffs [maxCoeffs] = {};
    std::atomic<bool> coeffsNeedUpdate { false };

    double sampleRate = 44100.0;

    // APVTS 파라미터 포인터
    std::atomic<float>* bassParam    = nullptr;
    std::atomic<float>* midParam     = nullptr;
    std::atomic<float>* trebleParam  = nullptr;
    std::atomic<float>* enabledParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStack)
};
