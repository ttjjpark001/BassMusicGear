#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief Linkwitz-Riley 4차(LR4) 크로스오버 필터
 *
 * 입력 신호를 두 경로로 분할한다:
 * - LP(저역통과): cleanDI 버퍼로 라우팅 (클린 DI 신호)
 * - HP(고역통과): 앰프 체인으로 라우팅 (처리된 신호)
 *
 * LR4 구현: 2차 Butterworth LP/HP를 직렬로 2번 캐스케이드.
 * LP + HP 합산 시 전 주파수에서 +/-0.1dB 이내 평탄하다.
 *
 * OFF 상태에서는 입력 신호를 그대로 양쪽 출력에 복사한다.
 *
 * 크로스오버 주파수 범위: 60-500 Hz (APVTS 파라미터 "crossover_freq")
 */
class BiAmpCrossover
{
public:
    BiAmpCrossover() = default;

    /**
     * @brief DSP 처리 준비. 샘플레이트 및 버퍼 크기 설정.
     *
     * @note [메인 스레드] 일반적으로 prepareToPlay()에서 호출.
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    void reset();

    /**
     * @brief 입력 신호를 LP, HP로 분할 처리한다.
     *
     * @param inputBuffer   입력 신호 (처리 중 읽기 전용)
     * @param lpOutput      LP 출력 버퍼 (cleanDI 목적지)
     * @param hpOutput      HP 출력 버퍼 (앰프 체인 목적지)
     * @note [오디오 스레드] 메모리 할당, 파일 I/O 금지.
     */
    void process (const juce::AudioBuffer<float>& inputBuffer,
                  juce::AudioBuffer<float>& lpOutput,
                  juce::AudioBuffer<float>& hpOutput);

    /**
     * @brief APVTS 파라미터 포인터를 설정한다.
     *
     * @param enabled       Bi-Amp ON/OFF 상태 (atomic, 오디오 스레드에서 읽음)
     * @param crossoverFreq 크로스오버 주파수 (Hz, 메인 스레드에서 변경)
     */
    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* crossoverFreq);

    /**
     * @brief 크로스오버 주파수 계수를 업데이트한다.
     *
     * @param freqHz  크로스오버 주파수 (Hz, 60-500 범위로 클램핑됨)
     * @note [메인 스레드 전용] 계수는 atomic으로 오디오 스레드에 전달된다.
     */
    void updateCrossoverFrequency (float freqHz);

private:
    // LR4 저역통과: 2차 Butterworth LP 필터 2개 직렬
    juce::dsp::IIR::Filter<float> lp1, lp2;
    // LR4 고역통과: 2차 Butterworth HP 필터 2개 직렬
    juce::dsp::IIR::Filter<float> hp1, hp2;

    double currentSampleRate = 44100.0;

    std::atomic<float>* enabledParam = nullptr;
    std::atomic<float>* crossoverFreqParam = nullptr;

    // 메인 스레드에서 오디오 스레드로의 원자적 계수 전달
    std::atomic<bool> coeffsNeedUpdate { false };
    juce::dsp::IIR::Coefficients<float>::Ptr pendingLPCoeffs;
    juce::dsp::IIR::Coefficients<float>::Ptr pendingHPCoeffs;
    juce::SpinLock coeffLock;  // Ptr 복사에만 사용, 계수 갱신 시에만 잠금 (매 버퍼 아님)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BiAmpCrossover)
};
