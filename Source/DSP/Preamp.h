#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Models/AmpModel.h"

/**
 * @brief 프리앰프: 모델별 입력 게인 스테이징 + 4배 오버샘플링 웨이브쉐이핑
 *
 * **신호 체인 위치**: 입력 → [Preamp(4x OS + 웨이브쉐이핑)] → ToneStack → PowerAmp
 *
 * **3가지 프리앰프 타입**:
 * 1. **Tube12AX7Cascade** (American Vintage, Tweed Bass, British Stack)
 *    - 비대칭 tanh 클리핑: 양의 반파보다 음의 반파를 약하게 누름
 *    - 짝수 고조파 강조 (tubelike warm 톤)
 *    - 4x 오버샘플링으로 앨리어싱 방지
 *
 * 2. **JFETParallel** (Modern Micro)
 *    - 병렬 구조: dry(클린) + wet(드라이브) 신호 혼합
 *    - 드라이브 노브로 dry/wet 비율 조절
 *    - 클린 톤 유지하면서 고조파 추가
 *
 * 3. **ClassDLinear** (Italian Clean)
 *    - 선형 증폭, 웨이브쉐이핑 없음
 *    - 입력 게인과 출력 볼륨만 적용
 *    - 최소 왜곡, 깨끗한 톤
 *
 * **오버샘플링**:
 * - 4x 오버샘플링: 비선형 처리(tanh, clipping) 고조파로 인한 앨리어싱 제거
 * - JUCE dsp::Oversampling 사용 (IIR 하프밴드 필터)
 * - Tube/JFET만 오버샘플링, ClassDLinear는 불필요
 *
 * **지연**:
 * - 오버샘플링으로 인한 지연: getLatencyInSamples()로 반환
 * - PluginProcessor에서 총 지연 합산 후 setLatencySamples(total)로 DAW 보고
 */
class Preamp
{
public:
    Preamp();
    ~Preamp() = default;

    /**
     * @brief DSP 초기화: 오버샘플링과 필터를 준비한다.
     *
     * @param spec  오디오 스펙 (sampleRate, samplesPerBlock)
     * @note [메인 스레드] prepareToPlay()에서 호출된다.
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 입력 버퍼에 프리앰프 웨이브쉐이핑을 적용한다.
     *
     * @param buffer  오디오 버퍼 (In-place 처리)
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
     *       inputGain 및 volume 파라미터를 폴링하여 적용.
     *       Oversampling up/down + 웨이브쉐이핑(타입별)
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 오버샘플러와 필터 버퍼를 클리어한다.
     *
     * @note [오디오 스레드] 모델 전환 또는 재생 중지 시 호출.
     */
    void reset();

    /**
     * @brief 오버샘플링으로 인한 지연을 샘플 단위로 반환한다.
     *
     * @return  지연 샘플 수 (4x 오버샘플링: 약 1~2ms @ 44.1kHz)
     * @note    PluginProcessor가 이 값을 포함하여 총 PDC 지연을 계산.
     */
    int getLatencyInSamples() const;

    /**
     * @brief 프리앰프 타입을 설정한다.
     *
     * @param type  PreampType (Tube12AX7Cascade, JFETParallel, ClassDLinear)
     * @note [메인 스레드 전용] 앰프 모델 전환 시 호출된다.
     */
    void setPreampType (PreampType type);

    /**
     * @brief APVTS 파라미터 포인터(inputGain, volume)를 저장한다.
     *
     * 오디오 스레드가 이 포인터들을 폴링하여 실시간 파라미터 변경을 감지.
     *
     * @param inputGain  입력 게인 atomic<float>* 포인터
     * @param volume     마스터 볼륨 atomic<float>* 포인터
     * @note [메인 스레드] 생성 시 호출.
     */
    void setParameterPointers (std::atomic<float>* inputGain,
                               std::atomic<float>* volume);

private:
    // --- 타입별 웨이브쉐이핑 처리 (오버샘플된 버퍼 내에서 실행) ---

    /**
     * @brief Tube12AX7 비대칭 tanh 클리핑
     *
     * output = tanh(input * inGain + asymmetry_offset) * outGain
     * asymmetry: 양의 반파보다 음의 반파를 약하게 누름 (짝수 고조파 강조)
     */
    void processTube12AX7 (float* data, size_t numSamples, float inGain, float outGain);

    /**
     * @brief JFET 병렬 드라이브/클린 혼합
     *
     * output = (1-blend) * input + blend * tanh(input * inGain) * outGain
     * blend: drive 노브로 제어되는 dry/wet 비율
     */
    void processJFETParallel (float* data, size_t numSamples, float inGain, float outGain);

    /**
     * @brief ClassD 선형 증폭 (웨이브쉐이핑 없음)
     *
     * output = input * inGain * outGain
     * 순수 게인만 적용
     */
    void processClassDLinear (float* data, size_t numSamples, float inGain, float outGain);

    // --- 4배 오버샘플링: IIR 하프밴드 필터를 사용한 다중 레이트 처리 ---
    // processSamplesUp(): 원본 SR → 4xSR (업샘플링 + LPF)
    // processSamplesDown(): 4xSR → 원본 SR (웨이브쉐이핑 후 다운샘플링 + LPF)
    juce::dsp::Oversampling<float> oversampling { 1,  // 1 채널 (모노)
                                                  2,  // 2배 스테이지 = 4배 오버샘플링 (2^2)
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    PreampType currentType = PreampType::Tube12AX7Cascade;  // 현재 프리앰프 타입

    // --- DC 블로킹 필터: 비대칭 웨이브쉐이핑으로 발생하는 DC 오프셋 제거 ---
    // Tube12AX7, JFETParallel의 비대칭 클리핑은 DC 오프셋을 생성하므로
    // 후처리로 1차 하이패스(~5Hz)를 적용하여 DC를 제거한다.
    // y[n] = x[n] - x[n-1] + R * y[n-1]
    float dcPrevInput  = 0.0f;       // x[n-1]
    float dcPrevOutput = 0.0f;       // y[n-1]
    float dcBlockerCoeff = 0.9999f;  // R 계수 (~5Hz @ 44.1kHz, prepare에서 재계산)

    // --- APVTS 파라미터 포인터 (실시간 폴링용) ---
    std::atomic<float>* inputGainParam = nullptr;  // 입력 게인 (0 ~ +24dB)
    std::atomic<float>* volumeParam    = nullptr;  // 마스터 볼륨 (-∞ ~ 0dB)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Preamp)
};
