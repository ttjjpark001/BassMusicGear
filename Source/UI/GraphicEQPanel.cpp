#include "GraphicEQPanel.h"

GraphicEQPanel::GraphicEQPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // --- ON/OFF toggle ---
    enabledToggle.setButtonText ("Graphic EQ");
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "geq_enabled", enabledToggle);

    // --- FLAT 리셋 버튼 ---
    flatButton.setButtonText ("FLAT");
    flatButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a3a5a));
    flatButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffff8800));
    flatButton.onClick = [this]
    {
        // 모든 밴드를 0dB(플랫)으로 리셋
        // setValueNotifyingHost(): 호스트(DAW)에 변경 알림 + 리스너 트리거
        // convertTo0to1(): 0.0dB를 0~1 정규화 범위로 변환
        for (int i = 0; i < numBands; ++i)
        {
            if (auto* param = apvtsRef.getParameter (bandParamIds[i]))
                param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
        }
    };
    addAndMakeVisible (flatButton);

    // --- Band sliders (vertical) ---
    for (int i = 0; i < numBands; ++i)
    {
        sliders[i].setSliderStyle (juce::Slider::LinearVertical);
        sliders[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sliders[i].setColour (juce::Slider::thumbColourId,        juce::Colour (0xffff8800));
        sliders[i].setColour (juce::Slider::trackColourId,        juce::Colour (0xff5a5a7a));
        sliders[i].setColour (juce::Slider::backgroundColourId,   juce::Colour (0xff2a2a3e));
        addAndMakeVisible (sliders[i]);

        sliderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, bandParamIds[i], sliders[i]);

        labels[i].setText (bandLabels[i], juce::dontSendNotification);
        labels[i].setJustificationType (juce::Justification::centred);
        labels[i].setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        labels[i].setFont (juce::FontOptions (10.0f));
        addAndMakeVisible (labels[i]);
    }
}

GraphicEQPanel::~GraphicEQPanel() = default;

void GraphicEQPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 배경 그리기
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 테두리
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // 0dB 센터라인: 사용자가 부스트/컷 기준점을 시각적으로 파악
    auto sliderArea = getLocalBounds().reduced (4);
    sliderArea.removeFromTop (30);  // 헤더 영역 제외
    sliderArea.removeFromBottom (16);  // 라벨 영역 제외
    const int centerY = sliderArea.getY() + sliderArea.getHeight() / 2;
    g.setColour (juce::Colour (0x44ff8800));  // 반투명 주황색
    g.drawHorizontalLine (centerY, static_cast<float> (sliderArea.getX()),
                          static_cast<float> (sliderArea.getRight()));
}

void GraphicEQPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // 헤더 행: [ON/OFF 토글 140px] ... [FLAT 버튼 60px]
    auto headerRow = area.removeFromTop (26);
    enabledToggle.setBounds (headerRow.removeFromLeft (140));
    flatButton.setBounds (headerRow.removeFromRight (60).reduced (2));

    area.removeFromTop (2);  // 헤더와 슬라이더 간 갭

    // 라벨 행: 하단에 주파수 표시
    auto labelRow = area.removeFromBottom (16);

    // 남은 영역을 10개 슬라이더에 균등 분할
    const int sliderWidth = area.getWidth() / numBands;

    for (int i = 0; i < numBands; ++i)
    {
        sliders[i].setBounds (area.getX() + i * sliderWidth, area.getY(),
                              sliderWidth, area.getHeight());
        labels[i].setBounds (labelRow.getX() + i * sliderWidth, labelRow.getY(),
                             sliderWidth, labelRow.getHeight());
    }
}
