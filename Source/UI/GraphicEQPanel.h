#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * @brief 10밴드 그래픽 EQ 패널 UI
 *
 * 10개의 수직 슬라이더(31Hz~16kHz) + ON/OFF 토글 + FLAT 리셋 버튼.
 * 각 슬라이더는 해당 밴드의 ±12dB 이득을 제어.
 * 사용자가 직관적으로 EQ 곡선을 시각화하고 조정할 수 있도록 설계.
 *
 * **레이아웃**:
 * - 헤더 행: [ON/OFF 토글 "Graphic EQ"] [FLAT 버튼]
 * - 슬라이더 영역: 10개 수직 슬라이더를 균등 분할하여 배치
 * - 라벨 행: 각 슬라이더 아래 주파수 표시 (31, 63, 125, ... 16k)
 *
 * **색상 스킴**:
 * - 배경: 다크 네이비 (#2a2a3e)
 * - 토글: 주황색 (#ff8800)
 * - 슬라이더 트랙: 중간 회색 (#5a5a7a)
 * - 센터라인: 반투명 주황색 (0dB 기준선)
 */
class GraphicEQPanel : public juce::Component
{
public:
    static constexpr int panelHeight = 200;

    GraphicEQPanel (juce::AudioProcessorValueTreeState& apvts);
    ~GraphicEQPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;

    // ON/OFF toggle
    juce::ToggleButton enabledToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    // FLAT reset button
    juce::TextButton flatButton;

    // 10 band sliders
    static constexpr int numBands = 10;
    juce::Slider sliders[numBands];
    juce::Label  labels[numBands];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments[numBands];

    static constexpr const char* bandParamIds[numBands] = {
        "geq_31", "geq_63", "geq_125", "geq_250", "geq_500",
        "geq_1k", "geq_2k", "geq_4k", "geq_8k", "geq_16k"
    };

    static constexpr const char* bandLabels[numBands] = {
        "31", "63", "125", "250", "500",
        "1k", "2k", "4k", "8k", "16k"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicEQPanel)
};
