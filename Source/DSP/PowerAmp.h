#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Models/AmpModel.h"

/**
 * @brief 파워앰프: Drive(포화) + Presence(고음 강조) + Sag(동적 압축)
 *
 * **신호 체인 위치**: ToneStack → [PowerAmp] → Cabinet
 *
 * **3가지 파라미터**:
 * 1. **Drive** (0 ~ 100%): 포화 정도 조절
 *    - tanh soft clipping: output = tanh(input * drive_gain)
 *    - 0: 선형 증폭만
 *    - 100%: 강한 포화 (고조파 풍부)
 *
 * 2. **Presence** (0 ~ +12dB @ 5kHz): 고주파 셸빙 필터
 *    - NFB(Negative Feedback) 루프를 모사하는 고음 강조
 *    - 파워앰프 출력부의 음감 개선 (bright 톤)
 *
 * 3. **Sag** (0 ~ 100%, 튜브 모델만)
 *    - 출력 트랜스포머 전압 새깅 시뮬레이션
 *    - 강한 신호 시 가상 전원 전압이 일시적으로 떨어짐
 *    - 엔벨로프 팔로워: 신호 레벨을 빠르게 추적하여 게인을 다이나믹하게 감소
 *    - 효과: 치는 순간 강하고, 뒤로 갈수록 살짝 눌리는 느낌 (부드러운 응답)
 *
 * **파워앰프 타입별 특성**:
 * - Tube6550: 부드러운 포화 곡선 (high headroom) → Ampeg SVT
 * - TubeEL34: 빠른 포화 (낮은 headroom) → Orange AD200
 * - SolidState: 경하드 클리핑 → Markbass 현대식
 * - ClassD: 최소 왜곡 (선형) → Italian Clean
 */
class PowerAmp
{
public:
    PowerAmp() = default;

    /**
     * @brief DSP 초기화: Presence 필터와 Sag 엔벨로프 팔로워를 준비한다.
     *
     * @param spec  오디오 스펙 (sampleRate, samplesPerBlock)
     * @note [메인 스레드] prepareToPlay()에서 호출된다.
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 버퍼에 파워앰프 효과(Drive/Presence/Sag)를 적용한다.
     *
     * @param buffer  오디오 버퍼 (In-place 처리)
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
     *       Drive(tanh), Presence(필터), Sag(엔벨로프 팔로워)를 순서대로 적용.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief Presence 필터와 Sag 엔벨로프 상태를 클리어한다.
     *
     * @note [오디오 스레드] 모델 전환 또는 재생 중지 시 호출.
     */
    void reset();

    /**
     * @brief 파워앰프 타입 및 Sag 활성 여부를 설정한다.
     *
     * @param type        PowerAmpType (Tube6550, TubeEL34, SolidState, ClassD)
     * @param sagEnabled  true: 튜브 모델(Sag 활성), false: 솔리드스테이트/ClassD
     * @note [메인 스레드 전용] 앰프 모델 전환 시 호출된다.
     */
    void setPowerAmpType (PowerAmpType type, bool sagEnabled);

    /**
     * @brief APVTS 파라미터 포인터(Drive/Presence/Sag)를 저장한다.
     *
     * @param drive     Drive 파라미터 atomic<float>* 포인터 (0.0 ~ 1.0)
     * @param presence  Presence 파라미터 atomic<float>* 포인터 (0.0 ~ 1.0 → 0 ~ 12dB)
     * @param sag       Sag 파라미터 atomic<float>* 포인터 (0.0 ~ 1.0)
     * @note [메인 스레드] 생성 시 호출.
     */
    void setParameterPointers (std::atomic<float>* drive,
                               std::atomic<float>* presence,
                               std::atomic<float>* sag);

    /**
     * @brief Presence 고주파 셸빙 필터 계수를 재계산한다.
     *
     * Presence 값이 변경되었을 때 필터 계수를 갱신하여
     * 5kHz 고음 강조의 깊이를 조절한다.
     * pendingCoeffValues에 저장하고 presenceNeedsUpdate 플래그를 set.
     *
     * @param presenceValue  Presence 파라미터 (0.0 ~ 1.0 → 0 ~ 12dB)
     * @note [메인 스레드 전용] UI 변경 또는 프리셋 로드 시 호출.
     */
    void updatePresenceFilter (float presenceValue);

private:
    double sampleRate = 44100.0;                    // 오디오 샘플레이트

    PowerAmpType currentType = PowerAmpType::Tube6550;  // 현재 파워앰프 타입
    bool sagActive = false;                             // Sag 활성화 여부 (튜브만)

    // --- Presence 필터: 고주파 셸빙 (NFB loop emulation) ---
    juce::dsp::IIR::Filter<float> presenceFilter;   // 5kHz 고음 강조 바이쿼드 필터
    static constexpr int maxCoeffs = 5;             // 바이쿼드 계수: b0, b1, b2, a1, a2
    float pendingCoeffValues[maxCoeffs] = {};       // 메인 스레드가 계산한 계수 (임시)
    std::atomic<bool> presenceNeedsUpdate { false }; // Presence 필터 업데이트 플래그

    // --- Sag 엔벨로프 팔로워: 신호 레벨 추적하여 동적 게인 감소 ---
    // Sag 시뮬레이션: 강한 신호에서 전원 전압이 떨어지는 효과
    // - 신호 레벨을 빠르게 추적 (attack, 치는 순간 감지)
    // - 천천히 복귀 (release, 신호가 줄어들 때 서서히 회복)
    float sagEnvelope = 0.0f;        // 현재 엔벨로프 레벨 (0..1)
    float sagAttackCoeff = 0.0f;     // attack 평활화 계수 (빠름, 약 1ms)
    float sagReleaseCoeff = 0.0f;    // release 평활화 계수 (느림, 약 300ms)

    // --- APVTS 파라미터 포인터 (실시간 폴링용) ---
    std::atomic<float>* driveParam    = nullptr;  // Drive 노브 (0.0 ~ 1.0)
    std::atomic<float>* presenceParam = nullptr;  // Presence 노브 (0.0 ~ 1.0 → 0 ~ 12dB)
    std::atomic<float>* sagParam      = nullptr;  // Sag 노브 (0.0 ~ 1.0)

    float prevPresence = -1.0f;  // 이전 Presence 값 (변경 감지용)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PowerAmp)
};
