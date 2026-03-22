#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/SignalChain.h"

/**
 * @brief BassMusicGear 플러그인 오디오 프로세서
 *
 * 역할:
 * - JUCE AudioProcessor 인터페이스 구현
 * - 모든 DSP 모듈 조립 및 신호 체인 관리 (SignalChain)
 * - APVTS 파라미터 정의 및 관리
 * - PDC(Plugin Delay Compensation) 지연 시간 보고
 * - 메인 스레드 타이머: 필터 계수 주기적 갱신 (30Hz)
 *
 * 신호 흐름:
 *   AudioInput → processBlock() → SignalChain.process() → AudioOutput
 *
 * Phase 1 신호 체인:
 *   NoiseGate → Preamp → ToneStack → PowerAmp → Cabinet
 *
 * APVTS 파라미터:
 *   - NoiseGate: gate_threshold, gate_attack, gate_hold, gate_release, gate_enabled
 *   - Preamp: input_gain, volume
 *   - ToneStack: bass, mid, treble
 *   - PowerAmp: drive, presence
 *   - Cabinet: cab_bypass
 */
class PluginProcessor final : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // --- JUCE AudioProcessor 오버라이드 ---

    /**
     * @brief 재생 시작 시 호출: 샘플레이트, 버퍼 크기 설정
     *
     * @param sampleRate    샘플 레이트 (Hz, e.g., 44100, 48000)
     * @param samplesPerBlock 버퍼 크기 (샘플 수, e.g., 512)
     *
     * 여기서:
     * - SignalChain.prepare() 호출로 모든 DSP 모듈 초기화
     * - 오버샘플링 필터 및 컨볼루션 버퍼 할당
     * - setLatencySamples()로 DAW에 지연 시간 보고
     * @note [오디오 스레드]
     */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /**
     * @brief 재생 중지 시 호출: 리소스 해제
     */
    void releaseResources() override;

    /**
     * @brief 버스 레이아웃 지원 여부 확인
     *
     * 지원: 모노 입력 + 스테레오 출력 (또는 모노 출력)
     */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    /**
     * @brief 매 버퍼마다 오디오 처리 실행
     *
     * @param buffer       입력/출력 오디오 버퍼 (채널별 샘플 배열)
     * @param midiMessages MIDI 메시지 (이 플러그인에서는 사용 안 함)
     *
     * 처리 순서:
     *   1. 여분의 출력 채널 클리어
     *   2. 스테레오 입력 → 모노 채널 0 선택
     *   3. SignalChain.process() 호출
     *   4. 모노 결과 → 스테레오 출력 복제
     * @note [오디오 스레드]
     */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    /**
     * @brief 에디터(UI) 생성
     * @return PluginEditor 인스턴스
     */
    juce::AudioProcessorEditor* createEditor() override;

    /**
     * @brief 에디터 보유 여부
     * @return true (PluginEditor 구현됨)
     */
    bool hasEditor() const override;

    // --- 플러그인 메타데이터 ---
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // --- 프로그램(프리셋) 관리 (Phase 2 이후 확장) ---
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    // --- 상태 저장/복원 (프리셋, 복구 등) ---
    /**
     * @brief APVTS 상태를 바이너리로 직렬화해 저장
     *
     * DAW가 플러그인 설정을 저장할 때 호출.
     * APVTS.copyState() → XML → 바이너리 변환.
     */
    void getStateInformation (juce::MemoryBlock& destData) override;

    /**
     * @brief 바이너리 상태 데이터를 로드해 APVTS에 복원
     *
     * DAW가 저장된 설정을 불러올 때 호출.
     * 바이너리 → XML → APVTS.replaceState() 변환.
     */
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- APVTS 인스턴스 (UI 및 DSP 공유) ---
    /**
     * 모든 파라미터의 진실의 원천(SSOT).
     * - 정의: createParameterLayout()
     * - UI: SliderAttachment / ButtonAttachment로 바인딩
     * - DSP: getRawParameterValue() → atomic load로 락프리 읽기
     */
    juce::AudioProcessorValueTreeState apvts;

private:
    /**
     * @brief 타이머 콜백 (메인 스레드, ~30Hz)
     *
     * 역할:
     * - ToneStack 계수 재계산 (Bass/Mid/Treble 변화 감지)
     * - PowerAmp Presence 필터 계수 재계산
     *
     * 이 함수 내에서만 주기적으로 계수를 갱신하므로,
     * 메인 스레드의 느린 계산이 오디오 스레드를 블로킹하지 않는다.
     * @note [메인 스레드]
     */
    void timerCallback() override;

    /**
     * @brief APVTS 파라미터 레이아웃 정의
     *
     * 모든 파라미터(노브, 버튼 등)의 ID, 범위, 기본값 등을 정의.
     * 생성자에서 호출되어 apvts 초기화에 사용됨.
     *
     * @return ParameterLayout (생성자 전달 인수)
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // --- DSP 신호 체인 ---
    SignalChain signalChain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
