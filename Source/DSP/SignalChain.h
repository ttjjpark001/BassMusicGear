#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects/NoiseGate.h"
#include "Tuner.h"
#include "Effects/Compressor.h"
#include "Effects/Overdrive.h"
#include "Effects/Octaver.h"
#include "Effects/EnvelopeFilter.h"
#include "Preamp.h"
#include "ToneStack.h"
#include "PowerAmp.h"
#include "Cabinet.h"
#include "../Models/AmpModel.h"
#include "../Models/AmpModelLibrary.h"

/**
 * @brief 완전한 베이스 신호 처리 체인 조립 및 관리
 *
 * **신호 체인 전체 순서**:
 * 입력 → [NoiseGate] → [Tuner(YIN)] → [Compressor] → [BiAmp placeholder]
 *     → [Overdrive(Tube/JFET/Fuzz)] → [Octaver(YIN)] → [EnvelopeFilter(SVF)]
 *     → [Preamp(4xOS)] → [ToneStack(모델별)] → [PowerAmp(Drive/Presence/Sag)] → [Cabinet(IR)] → 출력
 *
 * **각 블록의 역할**:
 * 1. **NoiseGate**: 허밍 및 배경 노이즈 제거 (히스테리시스 게이트)
 * 2. **Preamp**: 입력 이득 스테이징 + 타입별 웨이브쉐이핑 (4배 오버샘플링)
 *    - Tube12AX7Cascade: 비대칭 tanh
 *    - JFETParallel: 평행 드라이/클린 혼합
 *    - ClassDLinear: 선형 증폭
 * 3. **ToneStack**: 톤 컨트롤 (모델별 고유한 토폴로지)
 *    - TMB, Baxandall, James, BaxandallGrunt, MarkbassFourBand
 * 4. **PowerAmp**: 포화 + 고음 강조 필터 + Sag(튜브만)
 * 5. **Cabinet**: 콘볼루션으로 캐비닛 스피커 시뮬레이션 (5종 내장 IR)
 *
 * **모델 전환 메커니즘** (setAmpModel):
 * - ToneStack: 토폴로지 재설정 (compute*Coefficients 로직 선택)
 * - Preamp: 타입 변경 (Tube/JFET/ClassD)
 * - PowerAmp: 타입 + Sag 활성화 여부 설정
 * - Cabinet: 기본 IR 로드
 *
 * **파라미터 동기화**:
 * - connectParameters(): 생성 시 APVTS 파라미터를 각 블록에 등록
 * - updateCoefficientsFromMainThread(): 매 프레임 메인 스레드에서 변경된 파라미터 감지 및 반영
 *   (오디오 스레드 안전성: 변경 시에만 호출)
 */
class SignalChain
{
public:
    SignalChain() = default;

    /**
     * @brief 신호 체인 전체를 DSP 초기화한다.
     *
     * 모든 블록(Gate/Tuner/Compressor/Overdrive/Octaver/EnvelopeFilter/Preamp/ToneStack/PowerAmp/Cabinet)을 순서대로 prepare한다.
     * 오버샘플링, 필터, 컨볼루션 등의 버퍼를 할당한다.
     *
     * @param spec  오디오 스펙 (sampleRate, samplesPerBlock)
     * @note [메인 스레드] PluginProcessor::prepareToPlay()에서 호출된다.
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 오디오 버퍼를 신호 체인 전체에 통과시킨다.
     *
     * Gate → Tuner → Compressor → Overdrive → Octaver → EnvelopeFilter
     * → Preamp → ToneStack → PowerAmp → Cabinet 순서로 처리.
     * 각 블록은 In-place로 버퍼를 수정한다.
     *
     * @param buffer  모노 오디오 버퍼
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 모든 필터 버퍼를 클리어한다.
     *
     * @note [오디오 스레드] 재생 중지 또는 모델 전환 시 호출.
     */
    void reset();

    /**
     * @brief 전체 신호 체인의 지연을 샘플 단위로 반환한다.
     *
     * Overdrive 오버샘플링 지연 + Preamp 오버샘플링 지연 + Cabinet 컨볼루션 지연 합산.
     * PluginProcessor가 이 값을 DAW에 보고 (PDC - Plugin Delay Compensation).
     *
     * @return  총 지연 샘플 수
     * @note    DAW가 다른 트랙과 시간 정렬에 사용.
     */
    int getTotalLatencyInSamples() const;

    /**
     * @brief APVTS 파라미터를 각 DSP 블록에 등록한다.
     *
     * 각 블록이 원자적 포인터를 통해 파라미터 변경을 폴링할 수 있도록 한다.
     * 모든 노브, 슬라이더, 토글의 ID를 연결.
     *
     * @param apvts  APVTS 인스턴스
     * @note [메인 스레드] 생성 시 호출된다.
     */
    void connectParameters (juce::AudioProcessorValueTreeState& apvts);

    /**
     * @brief 변경된 APVTS 파라미터를 각 블록에 반영한다.
     *
     * Bass/Mid/Treble/Presence/VPF/VLE/Grunt/Attack/MidPosition 등
     * 메인 스레드에서만 계산할 수 있는 계수 갱신을 수행한다.
     * 변경 감지(이전값 비교)로 불필요한 재계산을 회피.
     *
     * @param apvts  APVTS (파라미터값 읽기)
     * @note [메인 스레드] PluginProcessor::timerCallback()에서 주기적 호출.
     *       또는 APVTS 리스너로 필요시에만 호출 가능 (개선 여지).
     */
    void updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts);

    /**
     * @brief 앰프 모델을 전환한다.
     *
     * 선택된 AmpModel에 따라 신호 체인 각 블록의 토폴로지/타입을 변경:
     * - ToneStack::setType(toneStack) 및 계수 초기화
     * - Preamp::setPreampType(preamp)
     * - PowerAmp::setPowerAmpType(powerAmp, sagEnabled)
     * - Cabinet: 기본 IR 로드
     * - 모든 필터/버퍼 reset()
     *
     * @param modelId  AmpModelId (AmericanVintage, TweedBass, BritishStack, ModernMicro, ItalianClean)
     * @note [메인 스레드 전용] UI의 앰프 모델 ComboBox 변경 시 호출된다.
     */
    void setAmpModel (AmpModelId modelId);

    /**
     * @brief Cabinet에 접근하여 IR을 로드한다.
     *
     * CabinetSelector UI에서 IR 변경 시 사용.
     * Cabinet::loadIRFromBinaryData()를 호출할 수 있다.
     *
     * @return  Cabinet 인스턴스 참조
     * @note    [메인 스레드] IR 로드는 내부적으로 백그라운드 스레드에서 진행.
     */
    Cabinet& getCabinet() { return cabinet; }

    /** Tuner 접근자 — TunerDisplay에서 피치 정보를 읽을 때 사용 */
    Tuner& getTuner() { return tuner; }

    /** Compressor 접근자 — UI에서 게인 리덕션 값을 읽을 때 사용 */
    Compressor& getCompressor() { return compressor; }

private:
    /** IR 이름 문자열을 cab_ir 파라미터 인덱스(0~4)로 변환한다. */
    static int irNameToIndex (const juce::String& irName)
    {
        if (irName == "ir_8x10_svt_wav")     return 0;
        if (irName == "ir_4x10_jbl_wav")     return 1;
        if (irName == "ir_1x15_vintage_wav") return 2;
        if (irName == "ir_2x12_british_wav") return 3;
        if (irName == "ir_2x10_modern_wav")  return 4;
        return 0; // fallback
    }

    // --- 신호 체인 DSP 모듈 ---
    NoiseGate      noiseGate;       // 히스테리시스 게이트: 허밍 및 배경 노이즈 제거
    Tuner          tuner;           // YIN 피치 감지 (41Hz~330Hz, 크로매틱 표시용)
    Compressor     compressor;      // VCA 동적 범위 압축 (패러렐 드라이 블렌드)
    // [BiAmp placeholder — Phase 6: Linkwitz-Riley 크로스오버 구현 예정]
    Overdrive      overdrive;       // Pre-FX: Tube/JFET/Fuzz 웨이브쉐이핑 (4x/8x OS) + Dry Blend
    Octaver        octaver;         // Pre-FX: YIN 피치 추적 + F0/2, F0*2 사인파 합성
    EnvelopeFilter envelopeFilter;  // Pre-FX: SVF 필터 + 엔벨로프 팔로워 변조
    Preamp         preamp;          // 입력 이득 스테이징 + 타입별 웨이브쉐이핑 (4배 OS)
    ToneStack      toneStack;       // 모델별 톤 컨트롤 (TMB, Baxandall, James 등)
    PowerAmp       powerAmp;        // 포화 + Presence 피킹 필터 + Sag 엔벨로프(튜브만)
    Cabinet        cabinet;         // 컨볼루션 캐비닛 IR 적용

    AmpModelId currentModelId = AmpModelId::TweedBass;  // 현재 선택된 앰프 모델

    // --- 메인 스레드: 파라미터 변경 감지 (이전값 기억) ---
    // updateCoefficientsFromMainThread()에서 현재값과 비교하여
    // 변경된 파라미터만 각 블록에 반영 (불필요한 계산 회피)
    float prevBass      = -1.0f;    // 이전 Bass 값
    float prevMid       = -1.0f;    // 이전 Mid 값
    float prevTreble    = -1.0f;    // 이전 Treble 값
    float prevPresence  = -1.0f;    // 이전 Presence 값
    float prevVpf       = -1.0f;    // 이전 VPF 값 (Italian Clean)
    float prevVle       = -1.0f;    // 이전 VLE 값 (Italian Clean)
    float prevGrunt     = -1.0f;    // 이전 Grunt 값 (Modern Micro)
    float prevAttack    = -1.0f;    // 이전 Attack 값 (Modern Micro)
    int   prevMidPos    = -1;       // 이전 Mid Position (American Vintage)
    int   prevAmpModel  = -1;       // 이전 앰프 모델 인덱스

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalChain)
};
