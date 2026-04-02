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

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    int getLatencyInSamples() const;

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* type,
                               std::atomic<float>* drive,
                               std::atomic<float>* tone,
                               std::atomic<float>* dryBlend);

private:
    void processTube (float* data, size_t numSamples, float driveGain);
    void processJFET (float* data, size_t numSamples, float driveGain);
    void processFuzz (float* data, size_t numSamples, float driveGain);

    // 4x 오버샘플링 (Tube/JFET용): 2^2 = 4x
    juce::dsp::Oversampling<float> oversampling4x { 1, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    // 8x 오버샘플링 (Fuzz용): 2^3 = 8x
    juce::dsp::Oversampling<float> oversampling8x { 1, 3,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };

    // 톤 필터: 1차 로우패스 (Overdrive 출력의 고역 조절)
    juce::dsp::StateVariableTPTFilter<float> toneFilter;

    // Dry 버퍼: prepareToPlay에서 할당, processBlock에서 재할당 금지
    juce::AudioBuffer<float> dryBuffer;

    // DC 블로커
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
