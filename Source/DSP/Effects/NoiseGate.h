#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 히스테리시스 노이즈 게이트
 *
 * 신호 체인 위치: 체인의 가장 앞(프리앰프 전). 배경 노이즈를 억제하는 첫 단계.
 *
 * 히스테리시스 기능: 게이트가 임계값에서 열리고, 임계값 - 6dB에서 닫혀
 * 신호가 임계값 근처에서 떨릴 때 반복 여닫힘을 방지한다.
 *
 * Attack/Hold/Release 엔벨로프: 부드러운 상태 전환으로 클릭음 제거.
 * - Attack: 열릴 때 시간상수 (ms)
 * - Hold: 임계값 아래 유지 시간 (ms)
 * - Release: 닫힐 때 시간상수 (ms)
 */
class NoiseGate
{
public:
    NoiseGate() = default;

    /**
     * @brief DSP 처리 스펙 설정
     * @param spec 샘플레이트, 버퍼 크기 등 처리 정보
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 버퍼에 노이즈 게이트 적용
     * @note [오디오 스레드] prepareToPlay() 이후 매 버퍼마다 호출된다.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 엔벨로프 상태 초기화
     */
    void reset();

    /**
     * @brief APVTS 파라미터 포인터를 캐시한다.
     *
     * 메인 스레드에서 한 번 호출하면, process()에서 락프리로 읽을 수 있다.
     *
     * @param threshold 열림 임계값 (dB)
     * @param attack    열릴 때 공격 시간 (ms)
     * @param hold      열린 후 유지 시간 (ms)
     * @param release   닫힐 때 해제 시간 (ms)
     * @param enabled   게이트 활성화 플래그
     * @note [메인 스레드 전용]
     */
    void setParameterPointers (std::atomic<float>* threshold,
                               std::atomic<float>* attack,
                               std::atomic<float>* hold,
                               std::atomic<float>* release,
                               std::atomic<float>* enabled);

private:
    // 게이트 상태 머신
    enum class State { Closed, Attack, Open, Hold, Release };
    State state = State::Closed;

    float envelope = 0.0f;         // 현재 엔벨로프 레벨 (0..1)
    int holdCounter = 0;           // Hold 상태의 남은 샘플 수

    double sampleRate = 44100.0;

    // 히스테리시스: 게이트는 임계값에서 열리고, 임계값 - 6dB에서 닫힌다.
    // 이를 통해 신호가 임계값 근처에서 떨릴 때 불필요한 반복 여닫힘을 방지한다.
    static constexpr float hysteresisDB = 6.0f;

    // 파라미터 포인터 (atomic, 오디오 스레드에서 락프리 읽기)
    std::atomic<float>* thresholdParam = nullptr;  // dB
    std::atomic<float>* attackParam    = nullptr;   // ms
    std::atomic<float>* holdParam      = nullptr;   // ms
    std::atomic<float>* releaseParam   = nullptr;   // ms
    std::atomic<float>* enabledParam   = nullptr;   // bool (0 = OFF, 1 = ON)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseGate)
};
