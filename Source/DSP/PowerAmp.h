#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 파워앰프: Drive 포화 + Presence 음질 조절
 *
 * 신호 체인 위치: 톤스택 → [파워앰프] → 캐비닛
 *
 * 역할:
 * - Drive: 포화 정도 조절 (0..1 → 1..10배 게인 → tanh 소프트 클리핑)
 *   높을수록 신호가 포화되어 따뜻한 톤, 동적 압축 효과
 * - Presence: 고주파 셸빙 필터 (3~5kHz, ±6dB)
 *   실제 튜브 앰프의 음의 궤환(NFB) 루프 톤 보이싱을 모사
 *
 * 신호 흐름:
 *   입력 → [Drive Gain] → [tanh 포화] → [출력 보정] → [Presence 필터] → 출력
 *
 * Phase 2 예정 기능:
 * - Sag: 출력 트랜스포머 전압 새추레이션 에뮬레이션
 *   (강한 신호에서 전원 전압이 일시적으로 내려가는 튜브 효과)
 */
class PowerAmp
{
public:
    PowerAmp() = default;

    /**
     * @brief DSP 처리 스펙 설정
     * @param spec 샘플레이트, 버퍼 크기 등 처리 정보
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 버퍼에 파워앰프(포화 + Presence) 적용
     * @note [오디오 스레드] prepareToPlay() 이후 매 버퍼마다 호출된다.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 필터 상태 초기화
     */
    void reset();

    /**
     * @brief APVTS 파라미터 포인터를 캐시한다.
     *
     * @param drive    드라이브 양 (0..1)
     * @param presence Presence 필터 게인 (0..1 → -6..+6dB)
     * @note [메인 스레드 전용]
     */
    void setParameterPointers (std::atomic<float>* drive,
                               std::atomic<float>* presence);

    /**
     * @brief Presence 고주파 셸빙 필터 계수를 재계산한다.
     *
     * Presence 노브 변화가 감지되면 메인 스레드에서 호출하여
     * 필터 계수를 갱신. 계수는 원자적으로 오디오 스레드로 전달된다.
     *
     * @param presenceValue Presence 노브 위치 (0..1)
     *                      0 → -6dB (어둡게), 0.5 → 0dB (평탄), 1 → +6dB (밝게)
     * @note [메인 스레드 전용]
     */
    void updatePresenceFilter (float presenceValue);

private:
    double sampleRate = 44100.0;

    // --- Presence 필터: 고주파 셸빙 필터 ---
    // 약 3~5kHz에서 부스트/컷해 앰프 음질을 조절
    // (실제 튜브 앰프의 음의 궤환 루프 네트워크 특성을 에뮬레이트)
    juce::dsp::IIR::Filter<float> presenceFilter;

    // 대기 중인 계수 (raw 배열로 저장해 RT-safe 교체 구현)
    // juce::dsp::IIR::Coefficients::Ptr는 참조 카운트 기반이므로
    // 오디오 스레드에서 Ptr를 swap하면 이전 객체의 refcount가 0이 되어
    // delete가 발생할 수 있다 (RT 안전성 위반).
    // 따라서 계수 값만 float 배열로 복사하고, 오디오 스레드에서는
    // 기존 Coefficients 객체의 값을 덮어쓰는 방식으로 RT-safe하게 교체한다.
    static constexpr int maxCoeffs = 5;  // IIR biquad: b0, b1, b2, a1, a2 (a0=1로 정규화, 배열 미포함)
    float pendingCoeffValues[maxCoeffs] = {};
    std::atomic<bool> presenceNeedsUpdate { false };

    std::atomic<float>* driveParam    = nullptr;    // 0..1
    std::atomic<float>* presenceParam = nullptr;    // 0..1

    float prevPresence = -1.0f;  // 메인 스레드에서 변화 감지용

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PowerAmp)
};
