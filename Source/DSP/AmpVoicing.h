#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Models/AmpModel.h"
#include <array>

/**
 * @brief 앰프별 고정 Voicing 필터 체인 (앰프당 2~3개 바이쿼드)
 *
 * 각 앰프의 고유 주파수 특성을 재현한다. 이는 사용자가 조작하는 톤스택과 무관한,
 * 회로 자체의 고정된 음색이다. 앰프 모델이 바뀔 때 자동으로 적용된다.
 *
 * 신호 체인 위치: Preamp 출력 → [AmpVoicing] → ToneStack
 *
 * 밴드별 필터 타입:
 * - LowShelf:  makeLowShelf() — 저역 셸빙 (20Hz ~ 설정 주파수)
 * - HighShelf: makeHighShelf() — 고역 셸빙 (설정 주파수 ~ 20kHz)
 * - Peak:      makePeakFilter() — 피킹/벨 필터 (좁은 대역, Q 제어)
 * - HighPass:  makeHighPass() — 2차 고역통과 (서브베이스 제거용)
 * - Flat:      처리 없음 (통과)
 *
 * 모델 전환: setModel()은 메인 스레드에서만 호출.
 * 계수는 atomic 플래그 + SpinLock을 통해 오디오 스레드로 전달된다.
 */
class AmpVoicing
{
public:
    static constexpr int maxBands = 3;

    AmpVoicing() = default;

    /**
     * @brief DSP 처리 준비. 3개의 바이쿼드 필터를 초기화한다.
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 앰프 모델의 Voicing 필터 체인을 처리한다.
     *
     * 메인 스레드로부터 전달된 pending 계수를 atomic exchange로 감지하면 적용하고,
     * 활성화된 밴드의 바이쿼드 필터를 순차적으로 처리한다.
     *
     * @note [오디오 스레드] 메모리 할당, 파일 I/O 금지.
     */
    void process (juce::AudioBuffer<float>& buffer);

    void reset();

    /**
     * @brief 새로운 앰프 모델의 Voicing 프로필로 전환한다.
     *
     * AmpModelLibrary에서 해당 모델의 voicingBands 데이터를 읽어
     * 모든 바이쿼드 필터 계수를 재계산한다. 계산된 계수는 atomic 메커니즘으로
     * 다음 processBlock()에서 오디오 스레드에 안전하게 적용된다.
     *
     * @param modelId  적용할 앰프 모델 ID (AmpModelId enum)
     * @note [메인 스레드 전용] processBlock() 내에서 직접 호출 금지.
     *       UI에서 콤보박스 선택 시 호출된다.
     */
    void setModel (AmpModelId modelId);

private:
    void computeCoefficients (AmpModelId modelId);

    // Voicing용 3개의 IIR 바이쿼드 필터
    std::array<juce::dsp::IIR::Filter<float>, maxBands> filters;

    // 밴드별 활성화 플래그 (false = 해당 밴드 평탄/바이패스)
    std::array<bool, maxBands> bandActive { false, false, false };

    double currentSampleRate = 44100.0;

    // 메인 스레드에서 오디오 스레드로의 원자적 계수 전달
    struct PendingCoeffs
    {
        std::array<juce::dsp::IIR::Coefficients<float>::Ptr, maxBands> coeffs;
        std::array<bool, maxBands> active { false, false, false };
    };

    PendingCoeffs pending;
    juce::SpinLock coeffLock;
    std::atomic<bool> coeffsNeedUpdate { false };  // 오디오 스레드가 폴링하는 신호

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpVoicing)
};
