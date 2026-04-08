/**
 * @brief Knob 생성자: 회전 노브 + 라벨 + 리셋 기능 초기화
 *
 * 커스텀 Knob 래퍼는 JUCE Slider(RotaryHorizontalVerticalDrag)를 래핑하여
 * 이미지 커스터마이징, 텍스트 박스, 우클릭 리셋 기능을 제공한다.
 *
 * @param labelText  노브 라벨 텍스트 (예: "Bass", "Drive")
 * @param paramId    APVTS 파라미터 ID (예: "bass", "drive")
 * @param apvts      AudioProcessorValueTreeState 참조
 * @note [메인 스레드] UI 초기화 시 호출됨
 */
#include "Knob.h"

Knob::Knob (const juce::String& labelText, const juce::String& paramId,
            juce::AudioProcessorValueTreeState& apvts)
{
    // --- Slider를 회전 노브로 구성 ---
    // RotaryHorizontalVerticalDrag: 상하좌우 마우스 드래그로 노브 회전
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);

    // 텍스트 박스: 노브 아래 배치, 60px 너비, 18px 높이
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);

    // 마우스 휠 비활성화 (Viewport 스크롤 우선)
    slider.setScrollWheelEnabled (false);

    // --- 회전 노브 색상 설정 ---
    // 채우기: 주황색 (활성), 테두리: 진한 회색, 텍스트: 흰색
    slider.setColour (juce::Slider::rotarySliderFillColourId,      juce::Colour (0xffff8800));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId,   juce::Colour (0xff444466));
    slider.setColour (juce::Slider::textBoxTextColourId,           juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId,        juce::Colours::transparentBlack);

    addAndMakeVisible (slider);

    // --- 라벨 구성 ---
    // 노브 아래에 파라미터 이름(예: "Bass", "Drive") 표시
    label.setText (labelText, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    // --- APVTS 파라미터에 Slider 바인딩 ---
    // SliderAttachment: 양방향 동기화
    //   - Slider 값 변경 → APVTS 파라미터 업데이트
    //   - APVTS 파라미터 변경 → Slider 표시 업데이트
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, paramId, slider);

    // --- 우클릭 리셋 핸들러 설정 ---
    // apvts.getParameter()로 파라미터 객체 조회
    // ResetMouseListener: 우클릭 감지 → getDefaultValue() 호출 → 기본값으로 리셋
    auto* param = apvts.getParameter (paramId);
    resetListener = std::make_unique<ResetMouseListener> (slider, param);
    slider.addMouseListener (resetListener.get(), false);  // false = 자식 컴포넌트 이벤트 처리 안 함
}

Knob::~Knob()
{
    // --- 마우스 리스너 제거 (메모리 누수 방지) ---
    // JUCE 표준 패턴: addMouseListener()로 추가한 리스너는 명시적 제거 필수
    slider.removeMouseListener (resetListener.get());
}

void Knob::resized()
{
    // --- 자식 컴포넌트 레이아웃 ---
    // 로컬 바운드를 아래(라벨)와 위(슬라이더)로 분리
    auto bounds = getLocalBounds();

    // 라벨: 아래쪽 18px (파라미터 이름 표시)
    label.setBounds (bounds.removeFromBottom (18));

    // 슬라이더: 나머지 영역(위쪽) (회전 노브 + 텍스트 박스)
    slider.setBounds (bounds);
}
