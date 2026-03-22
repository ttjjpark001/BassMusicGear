#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Knob.h"

/**
 * @brief 플러그인 에디터(UI): 앰프 제어 인터페이스
 *
 * Phase 1 구성:
 * - Preamp: InputGain, Volume 노브
 * - Tone Stack: Bass, Mid, Treble 노브
 * - Power Amp: Drive, Presence 노브
 * - Cabinet: Bypass 토글 버튼
 *
 * UI 레이아웃: 800x500 픽셀
 * - 제목: "BassMusicGear" (상단)
 * - 섹션 라벨: "PREAMP", "TONE STACK", "POWER AMP" (색상 구분)
 * - 노브: Knob 컴포넌트로 일관된 모양 제공 (우클릭 기본값 리셋)
 * - 토글: Cabinet Bypass 버튼
 * - 상태 표시: "Phase 1 -- Core Signal Chain" (하단)
 *
 * 디자인: 다크 테마 (배경: #1a1a2e), 주황색 강조 (#ff8800)
 */
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    /**
     * @brief 플러그인 에디터 생성
     *
     * @param processor PluginProcessor 인스턴스 (apvts 접근용)
     */
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    /**
     * @brief UI 배경 및 텍스트 그리기
     *
     * @param g Graphics 객체 (배경, 제목, 라벨 그림)
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief 자식 컴포넌트 레이아웃 설정
     *
     * 노브와 버튼의 크기/위치를 윈도우 크기에 맞춰 배치.
     */
    void resized() override;

private:
    PluginProcessor& processorRef;  // apvts 접근용

    // --- Preamp 섹션 노브 ---
    Knob inputGainKnob;  // Input Gain (-20..+40 dB)
    Knob volumeKnob;     // Volume (-60..+12 dB)

    // --- Tone Stack 섹션 노브 ---
    Knob bassKnob;       // Bass (0..1 가변저항 위치)
    Knob midKnob;        // Mid (0..1 가변저항 위치)
    Knob trebleKnob;     // Treble (0..1 가변저항 위치)

    // --- Power Amp 섹션 노브 ---
    Knob driveKnob;      // Drive (0..1 → 포화량)
    Knob presenceKnob;   // Presence (0..1 → -6..+6dB 고주파 필터)

    // --- Cabinet 섹션 버튼 ---
    juce::ToggleButton cabBypassButton { "Cab Bypass" };  // IR 컨볼루션 Bypass
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabBypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
