#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PluginProcessor;

/**
 * @brief 캐비닛 IR(임펄스 응답) 선택 및 활성화 UI
 *
 * **신호 체인 위치**: [전체 신호 체인] → Cabinet (Convolution) → 출력
 *
 * **기능**:
 * - IR 선택 ComboBox: 5종 내장 캐비닛 IR (8x10 SVT, 4x10 JBL, 1x15 Vintage, 2x12 British, 2x10 Modern)
 * - Bypass 토글: Cabinet의 Convolution을 우회 (Amp head만 사용)
 * - 자동 로드: IR 선택이 변경되면 MessageManager를 통해 Background 스레드에서 IR 파일 로드
 *
 * IR 로드는 juce::dsp::Convolution의 내부 처리로 백그라운드 스레드에서 안전하게 수행된다.
 * processBlock() 도중에는 원자적 swap으로 기존 IR이 계속 동작하고, 준비 완료 시 교체된다.
 */
class CabinetSelector : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    /**
     * @brief CabinetSelector 생성 및 초기화
     *
     * @param apvts      APVTS 참조 (cab_ir, cab_bypass 파라미터)
     * @param processor  PluginProcessor 참조 (SignalChain → Cabinet 접근)
     * @note [메인 스레드] PluginEditor 생성 시 호출된다.
     */
    CabinetSelector (juce::AudioProcessorValueTreeState& apvts,
                     PluginProcessor& processor);
    ~CabinetSelector() override;

    void paint (juce::Graphics& g) override;    // 배경 + "CABINET" 라벨 드로우
    void resized() override;                    // IR ComboBox와 Bypass 버튼 배치

private:
    /**
     * @brief APVTS 파라미터 변경 콜백
     *
     * "cab_ir" 파라미터가 바뀌면 호출되어, loadSelectedIR()을 메인 스레드에서 스케줄한다.
     *
     * @param parameterID  변경된 파라미터 ID ("cab_ir" 확인)
     * @param newValue     미사용 (ComboBox에서 직접 값을 읽음)
     */
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    /**
     * @brief 현재 선택된 IR을 BinaryData에서 로드하여 Cabinet에 전달한다.
     *
     * Cabinet::loadIRFromBinaryData()는 내부적으로 백그라운드 스레드에서
     * FFT 컨볼루션 준비를 하고, 준비 완료 시 오디오 스레드에 원자적으로 교체한다.
     *
     * @note [메인 스레드] MessageManager::callAsync로 호출된다.
     *       파일 I/O 없이 BinaryData에서 직접 로드하므로 안전하다.
     */
    void loadSelectedIR();
    void openIRFileChooser();   // 커스텀 IR 파일 브라우저 열기

    juce::AudioProcessorValueTreeState& apvtsRef;   // APVTS 참조
    PluginProcessor& processorRef;                  // SignalChain 접근용

    // --- IR 선택 ComboBox ---
    juce::ComboBox irCombo;     // 5종 내장 IR 선택
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> irAttachment;

    // --- 커스텀 IR 로드 버튼 ---
    juce::TextButton loadIRButton { "Load IR..." };
    std::unique_ptr<juce::FileChooser> fileChooser;

    // --- Bypass 토글 ---
    juce::ToggleButton bypassButton { "Bypass" };   // Cabinet Convolution 우회
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabinetSelector)
};
