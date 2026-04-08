#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Knob.h"
#include "../Models/AmpModel.h"
#include "../Models/AmpModelLibrary.h"

/**
 * @brief 앰프 모델 선택 + 모델별 톤스택 노브 레이아웃 패널
 *
 * **신호 체인 위치**: 프리앰프(입력 게인) → 톤스택(Bass/Mid/Treble) → 파워앰프(Drive/Presence/Sag)
 *
 * 모델별로 보이는 노브가 동적으로 변한다:
 * - **공통**: Input Gain, Volume, Bass, Mid, Treble, Drive, Presence
 * - **American Vintage** (Baxandall 톤스택): Mid Position 주파수 선택 콤보박스
 * - **Tweed Bass** (TMB 톤스택): 특화 노브 없음 (공통만)
 * - **British Stack** (James 톤스택): 특화 노브 없음
 * - **Modern Micro** (BaxandallGrunt 톤스택): Grunt(깊이), Attack(빠르기) 노브
 * - **Italian Clean** (MarkbassFourBand 톤스택): VPF(미드 스쿱), VLE(고역 롤오프) 노브
 * - **모든 튜브 모델**: Sag 노브 (출력 트랜스포머 새깅 시뮬레이션)
 *
 * APVTS 파라미터 변경 리스너로 동작하여, 모델이 바뀔 때 레이아웃을 자동 갱신한다.
 */
class AmpPanel : public juce::Component,
                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    /**
     * @brief AmpPanel 생성 및 초기화
     *
     * 모든 노브와 콤보박스를 생성하고, Tweed Bass(기본값)로 설정한다.
     *
     * @param apvts  APVTS 참조 (파라미터 읽기/쓰기)
     * @note [메인 스레드] PluginEditor 생성 시 호출된다.
     */
    AmpPanel (juce::AudioProcessorValueTreeState& apvts);
    ~AmpPanel() override;

    void paint (juce::Graphics& g) override;    // 배경 + 섹션 라벨 ("PREAMP", "TONE STACK", "POWER AMP")
    void resized() override;                    // 모델별 노브 배치

private:
    /**
     * @brief APVTS 파라미터 변경 콜백 (MessageManager 큐잉)
     *
     * 앰프 모델이 바뀌면 모든 노브 가시성을 갱신하고 레이아웃을 재계산한다.
     *
     * @param parameterID  변경된 파라미터 ID ("amp_model" 확인)
     * @param newValue     새 모델 인덱스 (0~5)
     * @note [메인 스레드] MessageManager::callAsync로 큐잉되어 UI 스레드에서 실행된다.
     */
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    /**
     * @brief 현재 앰프 모델에 따라 노브 가시성을 갱신한다.
     *
     * Sag는 튜브 모델만, VPF/VLE는 Italian Clean만, Grunt/Attack은 Modern Micro만 보인다.
     *
     * @note [메인 스레드]
     */
    void updateVisibility();

    juce::AudioProcessorValueTreeState& apvtsRef;  // APVTS 참조 (파라미터 ID로 접근)

    // --- 앰프 모델 선택 ---
    juce::ComboBox modelCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;

    // --- 공통 노브 (모든 모델에서 표시) ---
    Knob inputGainKnob;        // 프리앰프 입력 게인 (0 ~ +24dB)
    Knob volumeKnob;           // 마스터 출력 볼륨 (-∞ ~ 0dB)
    Knob bassKnob;             // 톤스택 저음 (모델별 다름)
    Knob midKnob;              // 톤스택 중음 (모델별 다름)
    Knob trebleKnob;           // 톤스택 고음 (모델별 다름)
    Knob driveKnob;            // 파워앰프 포화도 (0 ~ 100%)
    Knob presenceKnob;         // 파워앰프 프레즌스 (+0 ~ +12dB @ 5kHz)

    // --- 튜브 모델만 표시 (sagEnabled=true) ---
    Knob sagKnob;              // 출력 트랜스포머 새깅 시뮬레이션 (0 ~ 100%)

    // --- American Vintage (Baxandall) 전용 ---
    juce::ComboBox midPositionCombo;            // 미드 밴드 중심주파수 (250Hz ~ 3kHz)
    juce::Label midPositionLabel;               // "Mid Freq" 라벨
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midPositionAttachment;

    // --- Italian Clean (MarkbassFourBand) 전용 ---
    Knob vpfKnob;              // VPF (Variable Pre-shape Filter) 미드 스쿱 깊이 (0 ~ 100%)
    Knob vleKnob;              // VLE (Vintage Loudspeaker Emulator) 고역 롤오프 깊이 (0 ~ 100%)

    // --- Modern Micro (BaxandallGrunt) 전용 ---
    Knob gruntKnob;            // 왜곡 깊이 (0 ~ 100%)
    Knob attackKnob;           // 반응 속도 (0 ~ 100%)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpPanel)
};
