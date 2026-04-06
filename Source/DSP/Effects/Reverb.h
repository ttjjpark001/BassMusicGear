#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 리버브: JUCE 빌트인 리버브 래퍼 (Spring/Room/Hall/Plate 4종 타입)
 *
 * **신호 체인 위치**: Delay → **Reverb** → PowerAmp
 *
 * **특징**:
 * - JUCE의 고품질 Freeverb 알고리즘 기반 리버브
 * - 4가지 타입: Spring, Room, Hall, Plate
 * - 드라이 버퍼 미리 할당으로 오디오 스레드에서 RT-safe 처리
 *
 * **파라미터**:
 * - reverb_enabled: ON/OFF 토글
 * - reverb_type: Spring(0) / Room(1) / Hall(2) / Plate(3) 선택
 * - reverb_size: 룸 크기 (0~1, 에코 간격 조절)
 * - reverb_decay: 감쇠/댐핑 (0~1, 높을수록 고음 보존 = 밝음)
 * - reverb_mix: 드라이/웨트 블렌드 (0=원본, 1=리버브 신호 100%)
 *
 * **음향 특성**:
 * - Spring 타입: 짧은 감쇠, 밝은 음색, 좁은 스테레오 (빈티지 스프링 탱크 모사)
 * - Room 타입: 자연스러운 공간감, 따뜻한 음색 (소규모 연습실/스튜디오)
 * - Hall 타입: 넓고 긴 잔향, 풍부한 공간감 (콘서트홀 모사)
 * - Plate 타입: 밀도 높고 빠른 초기 반사, 선명한 음색 (금속판 리버브 모사)
 */
class Reverb
{
public:
    Reverb() = default;
    ~Reverb() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* type,
                               std::atomic<float>* size,
                               std::atomic<float>* decay,
                               std::atomic<float>* mix);

private:
    juce::dsp::Reverb reverb;

    // Dry buffer for wet/dry mixing (allocated in prepare, no RT allocation)
    juce::AudioBuffer<float> dryBuffer;

    // APVTS parameter pointers
    std::atomic<float>* enabledParam = nullptr;
    std::atomic<float>* typeParam    = nullptr;
    std::atomic<float>* sizeParam    = nullptr;
    std::atomic<float>* decayParam   = nullptr;
    std::atomic<float>* mixParam     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Reverb)
};
