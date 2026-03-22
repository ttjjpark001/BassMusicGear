#include "Knob.h"

Knob::Knob (const juce::String& labelText, const juce::String& paramId,
            juce::AudioProcessorValueTreeState& apvts)
{
    // --- 슬라이더를 회전 노브로 구성 ---
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);  // 상하좌우 드래그로 회전
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);  // 텍스트 박스: 노브 아래, 60x18 픽셀

    // --- 회전 노브 색상 지정 ---
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff8800));       // 노브 채우기: 주황색
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff444466));   // 노브 테두리: 진한 회색
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);                // 텍스트: 흰색
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);  // 텍스트 박스 테두리: 투명

    addAndMakeVisible (slider);

    // --- 라벨 구성 ---
    label.setText (labelText, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));  // 라벨 텍스트: 밝은 회색
    label.setJustificationType (juce::Justification::centred);               // 중앙 정렬
    addAndMakeVisible (label);

    // --- APVTS 파라미터에 슬라이더 바인딩 ---
    // SliderAttachment가 자동으로 양방향 동기화 처리:
    // - 슬라이더 값 변경 → APVTS 파라미터 업데이트
    // - 파라미터 변경 → 슬라이더 표시 업데이트
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, paramId, slider);

    // --- 우클릭 리셋 핸들러 설정 ---
    // getParameter()로 파라미터 객체 조회하여 ResetMouseListener에 전달
    // 우클릭 시 param->getDefaultValue()를 읽어 기본값으로 리셋
    auto* param = apvts.getParameter (paramId);
    resetListener = std::make_unique<ResetMouseListener> (slider, param);
    slider.addMouseListener (resetListener.get(), false);  // false = 마우스 리스너가 자식 컴포넌트 이벤트 처리 안 함
}

Knob::~Knob()
{
    // 리스너 제거 (메모리 누수 방지, JUCE 표준 패턴)
    slider.removeMouseListener (resetListener.get());
}

void Knob::resized()
{
    // --- 자식 컴포넌트 레이아웃 ---
    // 로컬 바운드를 아래에서부터 18픽셀(라벨 높이)을 떼어 라벨에 할당
    auto bounds = getLocalBounds();
    label.setBounds (bounds.removeFromBottom (18));  // 라벨: 아래쪽 18픽셀
    slider.setBounds (bounds);                        // 슬라이더: 나머지 영역 (위쪽)
}
