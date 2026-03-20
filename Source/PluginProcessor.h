#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief BassMusicGear 플러그인의 오디오 처리 핵심 클래스
 *
 * JUCE AudioProcessor를 상속하며, 플러그인의 DSP 진입점 역할을 한다.
 * DAW(또는 Standalone 호스트)가 오디오 버퍼를 이 클래스에 전달하면
 * processBlock()이 매 버퍼마다 호출되어 신호를 처리한다.
 *
 * 신호 체인 위치: [오디오 입력] → PluginProcessor → [오디오 출력]
 *
 * - Phase 0: 빈 프로세서. APVTS 빈 레이아웃 포함. processBlock은 무음 출력.
 * - Phase 1 이후: SignalChain이 추가되어 실제 DSP가 동작한다.
 */
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    //==========================================================================
    // AudioProcessor 인터페이스 — 호스트가 호출하는 생명주기 함수들
    //==========================================================================

    /**
     * @brief DSP 모듈을 초기화하고 처리 준비를 완료한다.
     *
     * 재생 시작 전 호스트가 한 번 호출한다. 샘플레이트·버퍼 크기가 확정된
     * 시점이므로, 오버샘플링·컨볼루션 등 모든 DSP 모듈의 prepare()를 여기서 실행한다.
     * 오버샘플링·컨볼루션 지연 합산 후 setLatencySamples()를 반드시 호출해야
     * DAW의 PDC(Plugin Delay Compensation)가 정확하게 동작한다.
     *
     * @param sampleRate      호스트의 샘플레이트 (Hz)
     * @param samplesPerBlock 한 버퍼당 최대 샘플 수
     * @note [메인 스레드] processBlock() 호출 전에 실행이 보장된다.
     */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /**
     * @brief 재생 중지 시 DSP 모듈 리소스를 해제한다.
     *
     * @note [메인 스레드] prepareToPlay()와 쌍으로 호출된다.
     */
    void releaseResources() override;

    /**
     * @brief 요청된 버스 레이아웃(입출력 채널 구성)을 지원하는지 검사한다.
     *
     * BassMusicGear는 모노 입력 + 스테레오(또는 모노) 출력만 허용한다.
     * 베이스 기타는 단일 채널이므로 멀티채널 입력은 지원하지 않는다.
     *
     * @param layouts 호스트가 요청하는 버스 레이아웃
     * @return 지원 가능하면 true, 그 외 false
     */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    /**
     * @brief 매 오디오 버퍼마다 호출되는 DSP 처리 함수.
     *
     * 신호 체인(SignalChain)을 통해 입력 버퍼를 처리하고 결과를 동일 버퍼에 쓴다.
     * 이 함수는 오디오 스레드에서 실시간으로 호출되므로, 함수 내부에서
     * 메모리 할당(new/delete), 파일 I/O, mutex, 시스템 콜을 절대 사용하면 안 된다.
     *
     * @param buffer      입출력 오디오 버퍼 (in-place 처리)
     * @note [오디오 스레드] 리얼타임 안전(real-time safe) 함수여야 한다.
     */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    //==========================================================================
    // Editor
    //==========================================================================

    /**
     * @brief UI 에디터 인스턴스를 생성해 반환한다.
     *
     * 호스트가 플러그인 창을 열 때 호출된다. 생성된 PluginEditor의 소유권은
     * 호스트(또는 JUCE 인프라)로 넘어간다.
     *
     * @return 새로 생성된 PluginEditor 포인터
     * @note [메인 스레드]
     */
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    // Plugin 정보
    //==========================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    // 프로그램 (프리셋) — Phase 0에서는 단일 프로그램만 사용
    // Phase 7 이후 PresetManager로 교체된다.
    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    // 상태 저장/복원 — DAW 세션 저장 및 프리셋 직렬화에 사용
    //==========================================================================

    /**
     * @brief 현재 APVTS 상태를 바이너리 블록으로 직렬화한다.
     *
     * DAW가 프로젝트를 저장할 때 호출한다. APVTS 전체 파라미터를 XML로
     * 변환한 후 바이너리로 인코딩해 destData에 기록한다.
     *
     * @param destData 직렬화된 상태가 저장될 메모리 블록
     */
    void getStateInformation (juce::MemoryBlock& destData) override;

    /**
     * @brief 바이너리 블록에서 APVTS 상태를 복원한다.
     *
     * DAW가 프로젝트를 로드하거나 프리셋을 적용할 때 호출한다.
     * XML 태그 이름이 apvts.state의 타입과 일치하는 경우에만 복원한다.
     *
     * @param data        직렬화된 상태 데이터 포인터
     * @param sizeInBytes 데이터 크기 (바이트)
     */
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // APVTS — 파라미터 트리
    //
    // 모든 노브·슬라이더·버튼 파라미터는 이 객체에 등록된다.
    // UI 컴포넌트는 SliderAttachment / ButtonAttachment로 바인딩하고,
    // 오디오 스레드에서는 getRawParameterValue("id")->load() 로 락 없이 읽는다.
    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    /**
     * @brief APVTS에 등록할 파라미터 레이아웃을 생성한다.
     *
     * Phase 0: 파라미터가 없는 빈 레이아웃을 반환한다.
     * 이후 Phase에서 게인, 톤스택, 이펙터 파라미터가 추가된다.
     *
     * @return 파라미터 레이아웃 객체
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
