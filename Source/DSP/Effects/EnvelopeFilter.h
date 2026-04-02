#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 엔벨로프 필터: StateVariableTPTFilter + 엔벨로프 팔로워
 *
 * **신호 체인 위치**: Octaver -> **EnvelopeFilter** -> Preamp
 *
 * **동작 원리**:
 * 1. 입력 신호의 엔벨로프(진폭)를 추적
 * 2. 엔벨로프 값으로 SVF(State Variable Filter)의 컷오프 주파수를 변조
 * 3. 피킹 어택 시 필터가 열리고, 소리가 줄면 닫힘 (또는 반대 - Direction)
 *
 * **파라미터**:
 * - ef_enabled: ON/OFF
 * - ef_sensitivity: 엔벨로프 감도 (0~1) — 필터 스윕 범위 조절
 * - ef_freq_min: 최소 컷오프 주파수 (100~500 Hz)
 * - ef_freq_max: 최대 컷오프 주파수 (1000~8000 Hz)
 * - ef_resonance: 레조넌스/Q (0.5~10)
 * - ef_direction: Up(0) / Down(1) — 엔벨로프 증가 시 컷오프 상승/하강
 */
class EnvelopeFilter
{
public:
    EnvelopeFilter();
    ~EnvelopeFilter() = default;

    /**
     * @brief DSP 초기화: SVF 필터 및 엔벨로프 팔로워 계수를 준비한다
     *
     * @param spec  오디오 스펙
     * @note [메인 스레드] prepareToPlay()에서 호출된다
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 오디오 버퍼를 처리한다
     *
     * 입력 신호의 엔벨로프를 추적하여 SVF의 컷오프 주파수를 실시간 변조한다.
     * 엔벨로프가 높을수록 필터가 열리거나 닫히며, 피킹에 반응한다.
     *
     * @param buffer  모노 오디오 버퍼
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
     *       이 함수는 매 샘플마다 필터 계수를 갱신하므로 계산량이 높다.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief SVF 필터 상태를 초기화한다
     *
     * @note [오디오 스레드] 재생 중지 시 호출
     */
    void reset();

    /**
     * @brief APVTS 파라미터 포인터를 등록한다
     *
     * @param enabled     ON/OFF
     * @param sensitivity 엔벨로프 감도 (0~1) — 필터 스윕 범위에 영향
     * @param freqMin     최소 컷오프 주파수 (Hz)
     * @param freqMax     최대 컷오프 주파수 (Hz)
     * @param resonance   레조넌스/Q (0.5~10)
     * @param direction   Up(0) = 엔벨로프 증가 시 컷오프 상승, Down(1) = 하강
     * @note [메인 스레드] 생성 시 호출된다
     */
    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* sensitivity,
                               std::atomic<float>* freqMin,
                               std::atomic<float>* freqMax,
                               std::atomic<float>* resonance,
                               std::atomic<float>* direction);

private:
    // State Variable TPT 필터: Bandpass 타입으로 사용
    // 클린한 주파수 응답과 안정적 리소넌스 제어 특성
    juce::dsp::StateVariableTPTFilter<float> svfFilter;

    // 엔벨로프 팔로워: 입력 신호의 진폭 추적
    float envelopeLevel = 0.0f;  // 현재 추적 엔벨로프 값
    float envAttackCoeff  = 0.0f;  // prepare에서 계산 (~1ms, 빠른 응답)
    float envReleaseCoeff = 0.0f;  // prepare에서 계산 (~30ms, 부드러운 감쇠)

    double currentSampleRate = 44100.0;

    // APVTS 파라미터 포인터
    std::atomic<float>* enabledParam     = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* freqMinParam     = nullptr;
    std::atomic<float>* freqMaxParam     = nullptr;
    std::atomic<float>* resonanceParam   = nullptr;
    std::atomic<float>* directionParam   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeFilter)
};
