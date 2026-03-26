#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/SignalChain.h"

/**
 * @brief BassMusicGear 플러그인 오디오 프로세서
 *
 * **신호 체인**: NoiseGate → Preamp(4xOS) → ToneStack → PowerAmp(Drive/Presence/Sag) → Cabinet(IR)
 *
 * **5종 앰프 모델**: American Vintage / Tweed Bass / British Stack / Modern Micro / Italian Clean
 *
 * **APVTS 파라미터**:
 * - 앰프 모델 선택 (ComboBox)
 * - 입력 게인, 마스터 볼륨
 * - Bass/Mid/Treble (톤스택)
 * - Drive/Presence (파워앰프)
 * - Sag (튜브 앰프만)
 * - 모델별 특화 파라미터 (VPF/VLE, Grunt/Attack, Mid Position)
 * - 캐비닛 IR 선택, Bypass
 *
 * **JUCE 패턴**:
 * - prepareToPlay: 샘플레이트, 버퍼 크기 초기화, 총 PDC 지연 보고
 * - processBlock: 매 버퍼마다 오디오 스레드에서 호출 (실시간 제약: malloc/파일I/O 금지)
 * - timerCallback (Timer): 메인 스레드 ~100ms 간격으로 호출, 톤 컨트롤 계수 갱신
 * - getStateInformation / setStateInformation: 프리셋 저장/로드 (APVTS 직렬화)
 */
class PluginProcessor final : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    /**
     * @brief 오디오 처리 준비: DSP 초기화 및 PDC 보고
     *
     * @param sampleRate      오디오 샘플레이트 (Hz)
     * @param samplesPerBlock  버퍼 크기 (샘플 수)
     * @note [메인 스레드] DAW에서 재생 직전에 호출된다.
     *       - SignalChain::prepare() 호출
     *       - 모든 필터/오버샘플러/컨볼루션 초기화
     *       - setLatencySamples(totalPDC) 호출: Preamp 오버샘플링 + Cabinet 지연 합산
     */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /**
     * @brief 오디오 처리 종료: 버퍼 해제
     *
     * @note [메인 스레드] 재생 중지 또는 플러그인 언로드 시 호출.
     */
    void releaseResources() override;

    /**
     * @brief 지원 오디오 채널 구성 확인
     *
     * @param layouts  DAW가 제공하는 버스 레이아웃
     * @return        입력 1채널 모노 + 출력 2채널 스테레오만 지원하면 true
     * @note          베이스는 모노 입력, 처리 후 스테레오 확장.
     */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    /**
     * @brief 매 버퍼마다 오디오를 처리한다 (오디오 스레드, 실시간 제약).
     *
     * @param buffer      입출력 오디오 버퍼 (In-place 처리)
     * @param midiMessages  MIDI 메시지 (미사용)
     * @note [오디오 스레드] 절대 금지: malloc/new, 파일 I/O, 뮤텍스, 시스템 콜
     *       모든 DSP 블록의 process() 메서드를 순서대로 호출하여
     *       신호를 게이트 → 프리앰프 → 톤스택 → 파워앰프 → 캐비닛으로 통과시킨다.
     */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    /**
     * @brief UI 에디터 팩토리 (nullptr 반환 = 헤드리스 모드 가능)
     *
     * @return  PluginEditor 인스턴스 (또는 nullptr)
     */
    juce::AudioProcessorEditor* createEditor() override;

    /**
     * @brief UI 에디터 지원 여부
     *
     * @return  true (Phase 2에서 UI 제공)
     */
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override { juce::ignoreUnused (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    /**
     * @brief APVTS 상태를 바이너리로 직렬화하여 프리셋 저장
     *
     * @param destData  메모리 블록 (플러그인이 채움)
     * @note [메인 스레드] DAW의 Save Preset 또는 프리셋 매니저에서 호출.
     */
    void getStateInformation (juce::MemoryBlock& destData) override;

    /**
     * @brief 저장된 프리셋 바이너리를 역직렬화하여 APVTS 상태 복원
     *
     * @param data         프리셋 바이너리 데이터
     * @param sizeInBytes  데이터 크기
     * @note [메인 스레드] DAW의 Load Preset 또는 프리셋 로드 시 호출.
     */
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- APVTS: 모든 파라미터 관리 (노브, 슬라이더, ComboBox, 토글) ---
    /**
     * @brief JUCE AudioProcessorValueTreeState
     *
     * 모든 UI 파라미터의 중앙 저장소.
     * - getValue(id): 오디오 스레드 안전 (atomic)
     * - getParameter(id)->setValueNotifyingHost(): UI 변경 → DAW 보고
     * - State 전체 직렬화: getStateInformation() / setStateInformation()
     */
    juce::AudioProcessorValueTreeState apvts;

    /**
     * @brief SignalChain 접근자
     *
     * PluginEditor 또는 외부 컴포넌트에서 SignalChain의 DSP 블록에 접근.
     * 예: getSignalChain().getCabinet().loadIRFromBinaryData()
     *
     * @return  SignalChain 인스턴스 참조
     */
    SignalChain& getSignalChain() { return signalChain; }

private:
    /**
     * @brief 타이머 콜백: 메인 스레드에서 ~100ms마다 호출
     *
     * Bass/Mid/Treble/Presence 등 메인 스레드에서만 계산할 수 있는
     * 톤 컨트롤 계수를 갱신하기 위해 SignalChain::updateCoefficientsFromMainThread() 호출.
     *
     * @note [메인 스레드] JUCE::Timer 구동, 변경 감지로 불필요한 재계산 회피.
     */
    void timerCallback() override;

    /**
     * @brief APVTS 파라미터 레이아웃 생성 (정적 메서드)
     *
     * 모든 파라미터를 정의하고 기본값, 범위, 스텝을 설정한다.
     * - amp_model: ComboBox (0~4)
     * - input_gain, volume: 데시벨 슬라이더
     * - bass, mid, treble: 0~1 연속값
     * - drive, presence, sag: 0~1 연속값
     * - vpf, vle, grunt, attack: 0~1 연속값 (모델별)
     * - mid_position: ComboBox (0~4, American Vintage만)
     * - cab_ir: ComboBox (0~4, 내장 IR)
     * - cab_bypass: 토글 버튼
     *
     * @return  ParameterLayout 인스턴스
     * @note    생성자 → createParameterLayout() 호출로 apvts 초기화.
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    SignalChain signalChain;    // 모든 DSP 블록을 관리하는 신호 체인

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
