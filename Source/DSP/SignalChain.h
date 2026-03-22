#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects/NoiseGate.h"
#include "Preamp.h"
#include "ToneStack.h"
#include "PowerAmp.h"
#include "Cabinet.h"

/**
 * @brief 완전한 신호 처리 체인을 조립하고 관리
 *
 * Phase 1 신호 순서: NoiseGate → Preamp → ToneStack → PowerAmp → Cabinet
 *
 * 신호 체인 설명:
 *   1. NoiseGate: 배경 노이즈 억제 (히스테리시스 게이트)
 *   2. Preamp: 드라이브 + 4배 오버샘플링 웨이브쉐이핑 (튜브 캐릭터)
 *   3. ToneStack: Fender TMB 톤스택 (Bass/Mid/Treble 음질 조절)
 *   4. PowerAmp: 포화 + Presence 고주파 필터 (음력 있는 톤)
 *   5. Cabinet: 스피커/캐비닛 IR 컨볼루션 (음향 특성)
 *
 * 이후 Phase에서 추가:
 *   - Tuner: YIN 피치 트래킹 (UI 튜너 디스플레이용)
 *   - Compressor: VCA/광학 컴프레서
 *   - BiAmpCrossover: Linkwitz-Riley 4차 크로스오버
 *   - Pre-FX / Post-FX 섹션
 *   - GraphicEQ: 10밴드 고정주파수 EQ
 *   - DIBlend: 클린 DI + 프로세스드 신호 혼합
 *
 * 멀티 스레드 설계:
 *   - prepare() / reset() / process(): 오디오 스레드
 *   - connectParameters() / updateCoefficientsFromMainThread(): 메인 스레드
 */
class SignalChain
{
public:
    SignalChain() = default;

    /**
     * @brief 모든 DSP 모듈의 처리 스펙 설정
     *
     * @param spec 샘플레이트, 버퍼 크기 등 처리 정보
     * @note [오디오 스레드] prepareToPlay()에서 호출됨
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 버퍼에 신호 체인 전체 적용
     *
     * 순서: Gate → Preamp → ToneStack → PowerAmp → Cabinet
     *
     * @param buffer 모노 입력 버퍼 (채널 0)
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출됨
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 모든 DSP 모듈 상태 초기화
     */
    void reset();

    /**
     * @brief 신호 체인 전체 지연 시간(샘플 단위) 반환
     *
     * 오버샘플링(Preamp) + 컨볼루션(Cabinet) 지연의 합.
     * PluginProcessor의 setLatencySamples()로 DAW에 보고되어
     * Plugin Delay Compensation(PDC)을 정확하게 한다.
     *
     * @return 전체 지연 샘플 수
     */
    int getTotalLatencyInSamples() const;

    /**
     * @brief APVTS 파라미터 포인터를 모든 DSP 모듈에 연결한다.
     *
     * 메인 스레드에서 PluginProcessor 생성자 또는 초기화 시점에 호출.
     * 이후 각 모듈은 락프리로 파라미터 값을 읽을 수 있다.
     *
     * @param apvts PluginProcessor의 APVTS 인스턴스
     * @note [메인 스레드 전용]
     */
    void connectParameters (juce::AudioProcessorValueTreeState& apvts);

    /**
     * @brief 메인 스레드에서만 계산 가능한 필터 계수를 갱신한다.
     *
     * ToneStack(TMB 계수) 및 PowerAmp(Presence 필터)의 계수는
     * 복잡한 수식 계산이 필요하므로 메인 스레드에서만 업데이트.
     * 계수는 원자적으로 오디오 스레드로 전달된다.
     *
     * PluginProcessor의 timerCallback()에서 30Hz로 호출되어
     * 파라미터 변화에 빠르게 반응한다.
     *
     * @param apvts PluginProcessor의 APVTS 인스턴스
     * @note [메인 스레드 전용]
     */
    void updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts);

private:
    // Phase 1 DSP 모듈들
    NoiseGate noiseGate;   // 노이즈 게이트
    Preamp    preamp;      // 프리앰프 + 드라이브
    ToneStack toneStack;   // TMB 톤스택
    PowerAmp  powerAmp;    // 파워앰프 포화 + Presence
    Cabinet   cabinet;     // IR 컨볼루션

    // 메인 스레드 변화 감지용 이전 파라미터 값
    // 30Hz 타이머에서 매번 재계산하는 대신, 값이 바뀔 때만 계수를 갱신한다.
    float prevBass     = -1.0f;
    float prevMid      = -1.0f;
    float prevTreble   = -1.0f;
    float prevPresence = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalChain)
};
