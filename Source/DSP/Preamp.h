#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 프리앰프: 4배 오버샘플링 + 비대칭 tanh 웨이브쉐이핑
 *
 * 신호 체인 위치: 노이즈 게이트 → [프리앰프] → 톤스택
 *
 * 역할:
 * - InputGain: 드라이브 양 조절 (게인 스테이징)
 * - Asymmetric tanh 소프트 클리핑: 짝수 고조파 강조 (튜브 캐릭터)
 * - 4배 오버샘플링(폴리페이즈 IIR): 비선형 웨이브쉐이핑 앨리어싱 억제
 * - Volume: 출력 레벨 조절
 *
 * 신호 흐름:
 *   입력 → [InputGain 승수] → [4x Upsampling] → [Asymmetric Tanh] → [4x Downsampling] → [Volume 승수] → 출력
 *
 * 오버샘플링 지연: ~200샘플 (샘플레이트와 필터 차수에 따라 변함)
 */
class Preamp
{
public:
    Preamp();
    ~Preamp() = default;

    /**
     * @brief DSP 처리 스펙 설정
     * @param spec 샘플레이트, 버퍼 크기 등 처리 정보
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 버퍼에 프리앰프(드라이브 + 웨이브쉐이핑) 적용
     * @note [오디오 스레드] prepareToPlay() 이후 매 버퍼마다 호출된다.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 오버샘플링 상태 초기화
     */
    void reset();

    /**
     * @brief 오버샘플링으로 인한 지연 시간(샘플 단위) 반환
     * @return 지연 샘플 수 (PDC 보고용)
     */
    int getLatencyInSamples() const;

    /**
     * @brief APVTS 파라미터 포인터를 캐시한다.
     *
     * @param inputGain   입력 게인 (dB) → 웨이브쉐이핑 드라이브량
     * @param volume      출력 볼륨 (dB)
     * @note [메인 스레드 전용]
     */
    void setParameterPointers (std::atomic<float>* inputGain,
                               std::atomic<float>* volume);

private:
    // --- 4배 오버샘플링 (2^2 = 4) ---
    // 폴리페이즈 IIR 하프밴드 필터 사용:
    // - 높은 필터 특성 (멈춤 대역에서 우수한 감쇠)
    // - 낮은 리플(평탄도)
    // - 오디오 신호에 최적화
    //
    // 비선형 처리(웨이브쉐이핑)는 많은 고조파를 생성하므로 4배 오버샘플링이 필요:
    // - 나이퀴스트 주파수가 4배 상승 → 고조파 앨리어싱 감소
    // - 최종 다운샘플링 필터가 나이퀴스트(SR/2) 위 고조파를 제거
    juce::dsp::Oversampling<float> oversampling { 1, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    std::atomic<float>* inputGainParam = nullptr;  // dB
    std::atomic<float>* volumeParam    = nullptr;  // dB

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Preamp)
};
