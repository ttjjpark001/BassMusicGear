#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 오버드라이브 이펙터: Tube/JFET/Fuzz 3종 웨이브쉐이핑 + Dry Blend
 *
 * **신호 체인 위치**: Compressor -> [BiAmp] -> **Overdrive** -> Octaver -> EnvelopeFilter -> Preamp
 *
 * **3가지 타입**:
 * 1. **Tube** (비대칭 tanh 소프트 클리핑) — 4x 오버샘플링
 *    - 워밍하고 자연스러운 포화. 짝수 고조파 강조.
 * 2. **JFET** (병렬 클린+드라이브 혼합) — 4x 오버샘플링
 *    - 클린 톤 유지하면서 그릿 추가. 모던 베이스 톤.
 * 3. **Fuzz** (하드 클리핑) — 8x 오버샘플링
 *    - 극도의 포화. 고조파 왜곡이 심해 8x 오버샘플링 필수.
 *
 * **Dry Blend**: 모든 타입에서 필수.
 *   output = dryBlend * input + (1 - dryBlend) * clipped
 *
 * **파라미터**:
 * - od_enabled: ON/OFF 바이패스
 * - od_type: Tube(0) / JFET(1) / Fuzz(2)
 * - od_drive: 드라이브 양 (0~1)
 * - od_tone: 톤 필터 (0~1, 로우패스 컷오프)
 * - od_dry_blend: 드라이 블렌드 (0=full wet, 1=full dry)
 */
class Overdrive
{
public:
    Overdrive();
    ~Overdrive() = default;

    /**
     * @brief DSP 초기화: 오버샘플링, 톤 필터, 드라이 버퍼를 준비한다
     *
     * @param spec  오디오 스펙 (sampleRate, maximumBlockSize)
     * @note [메인 스레드] prepareToPlay()에서 호출된다
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 오디오 버퍼를 웨이브쉐이핑으로 처리한다
     *
     * 선택된 타입(Tube/JFET/Fuzz)에 따라 다른 클리핑 곡선을 적용하고,
     * 드라이 신호와 웨트 신호를 Dry Blend 파라미터로 혼합한다.
     *
     * @param buffer  모노 오디오 버퍼
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 오버샘플링 필터 및 톤 필터 상태를 초기화한다
     *
     * @note [오디오 스레드] 재생 중지 시 호출
     */
    void reset();

    /**
     * @brief 오버샘플링(4x/8x)로 인한 지연 샘플 수를 반환한다
     *
     * worst-case로 8x 오버샘플링 지연을 항상 보고하여 DAW PDC 일관성 유지.
     *
     * @return 지연 샘플 수
     */
    int getLatencyInSamples() const;

    /**
     * @brief APVTS 파라미터 포인터를 등록한다
     *
     * 오디오 스레드에서 atomic load로 파라미터 변경을 폴링할 수 있도록 한다.
     *
     * @param enabled   ON/OFF 상태 (0.0 = OFF, > 0.5 = ON)
     * @param type      오버드라이브 타입 (0=Tube, 1=JFET, 2=Fuzz)
     * @param drive     드라이브 양 (0~1)
     * @param tone      톤 로우패스 컷오프 (0~1, 500Hz~12kHz)
     * @param dryBlend  드라이 블렌드 (0=모두 웨트, 1=모두 드라이)
     * @note [메인 스레드] 생성 시 호출된다
     */
    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* type,
                               std::atomic<float>* drive,
                               std::atomic<float>* tone,
                               std::atomic<float>* dryBlend);

private:
    void processTube (float* data, size_t numSamples, float driveGain);
    void processJFET (float* data, size_t numSamples, float driveGain);
    void processFuzz (float* data, size_t numSamples, float driveGain);

    // 4배 오버샘플링 (Tube/JFET): 웨이브쉐이핑 앨리어싱 방지
    // numStages=2 → 2^2 = 4x. Polyphase IIR 필터로 고속 다운샘플링
    juce::dsp::Oversampling<float> oversampling4x { 1, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    // 8배 오버샘플링 (Fuzz): 극도의 하드 클리핑으로 고조파가 심하므로 8x 필요
    // numStages=3 → 2^3 = 8x. 고주파 에일리어싱 방지
    juce::dsp::Oversampling<float> oversampling8x { 1, 3,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    // 톤 필터: StateVariable 타입 로우패스. Overdrive 출력의 고역 밝기 조절
    juce::dsp::StateVariableTPTFilter<float> toneFilter;

    // 드라이 신호 저장용 버퍼 (Dry Blend 계산에 사용)
    // prepareToPlay에서 한 번 할당 후, processBlock마다 재할당 하지 않음 (RT-safe)
    juce::AudioBuffer<float> dryBuffer;

    // DC 블로커: 비대칭 웨이브쉐이핑 후 발생하는 DC 오프셋 제거
    // 차분 필터: y[n] = x[n] - x[n-1] + R*y[n-1], R = 1 - 2*pi*5/SR (~5Hz 하이패스)
    float dcPrevInput  = 0.0f;
    float dcPrevOutput = 0.0f;
    float dcBlockerCoeff = 0.9999f;

    double currentSampleRate = 44100.0;

    // APVTS 파라미터 포인터
    std::atomic<float>* enabledParam  = nullptr;
    std::atomic<float>* typeParam     = nullptr;
    std::atomic<float>* driveParam    = nullptr;
    std::atomic<float>* toneParam     = nullptr;
    std::atomic<float>* dryBlendParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Overdrive)
};
