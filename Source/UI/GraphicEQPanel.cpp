#include "GraphicEQPanel.h"

/**
 * @brief 10밴드 그래픽 EQ 패널 UI 생성
 *
 * 슬라이더, 토글, 프리셋 드롭다운, FLAT 버튼을 초기화하고
 * APVTS 파라미터들에 바인딩한다. 초기 상태는 펼침(expanded=true).
 *
 * @param apvts AudioProcessorValueTreeState 참조
 *            (geq_enabled, geq_31~16k, 파라미터들을 포함해야 함)
 */
GraphicEQPanel::GraphicEQPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // --- 접기/펼치기 버튼 (EffectBlock과 동일한 색상/스타일) ---
    // 초기값은 "v" (펼침 상태). setExpanded()로 상태 변경 시 ">" / "v" 토글
    expandButton.setButtonText ("v");
    expandButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a3a5a));
    expandButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5a5a7a));
    expandButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffffffff));
    expandButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffff8800));
    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    // --- ON/OFF toggle ---
    enabledToggle.setButtonText ("Graphic EQ");
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "geq_enabled", enabledToggle);

    // --- EQ 프리셋 드롭다운 ---
    // 선택지: (Custom), Flat, Bass Boost, Scoop Mid, Presence, Vintage Warmth, Hi-Fi
    // ID: 1 (Custom, 읽기 전용 — 사용자 수동 조정 시 자동 표시), 2~7 (프리셋 ID)
    // onChange: ID >= 2 (실제 프리셋)이면 applyPreset()을 호출하여 10밴드 값 일괄 적용
    presetCombo.addItem ("(Custom)",       1);
    presetCombo.addItem ("Flat",           2);
    presetCombo.addItem ("Bass Boost",     3);
    presetCombo.addItem ("Scoop Mid",      4);
    presetCombo.addItem ("Presence",       5);
    presetCombo.addItem ("Vintage Warmth", 6);
    presetCombo.addItem ("Hi-Fi",          7);
    presetCombo.setSelectedId (1, juce::dontSendNotification);
    presetCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff3a3a5a));
    presetCombo.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    presetCombo.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff444466));
    presetCombo.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xffff8800));
    presetCombo.onChange = [this]
    {
        const int id = presetCombo.getSelectedId();
        if (id >= 2)
            applyPreset (id);
    };
    addAndMakeVisible (presetCombo);

    // --- FLAT 리셋 버튼 ---
    // 클릭 시 모든 10밴드를 0dB로 설정하고 presetCombo를 "Flat"(ID=2)으로 선택
    // setValueNotifyingHost()로 파라미터를 변경하므로 슬라이더의 onValueChange가 발동되어
    // 일시적으로 "(Custom)"으로 바뀌지만, 마지막에 presetCombo를 "Flat"으로 강제 설정한다.
    flatButton.setButtonText ("FLAT");
    flatButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a3a5a));
    flatButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffff8800));
    flatButton.onClick = [this]
    {
        for (int i = 0; i < numBands; ++i)
        {
            if (auto* param = apvtsRef.getParameter (bandParamIds[i]))
                param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
        }
        presetCombo.setSelectedId (2, juce::dontSendNotification); // "Flat"
    };
    addAndMakeVisible (flatButton);

    // --- 10밴드 수직 슬라이더 초기화 ---
    // 각 슬라이더:
    // - LinearVertical 스타일 (위로 드래그 = 게인 증가)
    // - dB 값을 슬라이더 위에 텍스트박스로 표시 (높이 16px, 1소수점)
    // - 색상: 엄지손가락(thumb)은 주황색, 트랙은 중간 회색, 배경은 다크 네이비
    // - APVTS 파라미터에 동기화됨 (SliderAttachment)
    // - onValueChange: 사용자가 수동으로 슬라이더를 조정하면 presetCombo를 "(Custom)"으로 표시
    //   (단, isApplyingPreset 플래그가 true일 때는 무시하여 프리셋 적용 중 "(Custom)"으로 되지 않음)
    for (int i = 0; i < numBands; ++i)
    {
        sliders[i].setSliderStyle (juce::Slider::LinearVertical);
        sliders[i].setTextBoxStyle (juce::Slider::TextBoxAbove, true, 40, 16);
        sliders[i].setNumDecimalPlacesToDisplay (1);
        sliders[i].setTextValueSuffix (" dB");
        sliders[i].setColour (juce::Slider::thumbColourId,        juce::Colour (0xffff8800));
        sliders[i].setColour (juce::Slider::trackColourId,        juce::Colour (0xff5a5a7a));
        sliders[i].setColour (juce::Slider::backgroundColourId,   juce::Colour (0xff2a2a3e));
        sliders[i].setColour (juce::Slider::textBoxTextColourId,  juce::Colour (0xffdddddd));
        sliders[i].setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a1a2e));
        sliders[i].setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        addAndMakeVisible (sliders[i]);

        sliderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, bandParamIds[i], sliders[i]);

        // 슬라이더 수동 조정 시 프리셋 표시 변경 로직
        // isApplyingPreset=false이면 "(Custom)"으로 표시. true이면 무시하여 프리셋 적용 중 되돌리지 않음.
        sliders[i].onValueChange = [this]
        {
            if (! isApplyingPreset)
                presetCombo.setSelectedId (1, juce::dontSendNotification);
        };

        // 각 슬라이더 아래 주파수 라벨 (31, 63, 125, ... 16k)
        labels[i].setText (bandLabels[i], juce::dontSendNotification);
        labels[i].setJustificationType (juce::Justification::centred);
        labels[i].setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        labels[i].setFont (juce::FontOptions (10.0f));
        addAndMakeVisible (labels[i]);
    }
}

/**
 * @brief GraphicEQPanel의 펼침/접힘 상태를 변경하고 UI 업데이트
 *
 * 상태 변경 시:
 * - expandButton 텍스트: "v" (펼침) ↔ ">" (접힘)
 * - sliders, labels, flatButton, presetCombo의 가시성 토글
 * - onExpandChange 콜백 호출 → PluginEditor::calculateNeededHeight() 실행 → 창 높이 재계산
 *
 * @param shouldBeExpanded  true=펼침, false=접힘
 *
 * @note [메인 스레드] expandButton 클릭 또는 PluginEditor에서 호출됨.
 */
void GraphicEQPanel::setExpanded (bool shouldBeExpanded)
{
    expanded = shouldBeExpanded;
    expandButton.setButtonText (expanded ? "v" : ">");

    for (int i = 0; i < numBands; ++i)
    {
        sliders[i].setVisible (expanded);
        labels[i].setVisible (expanded);
    }
    flatButton.setVisible (expanded);
    presetCombo.setVisible (expanded);

    if (onExpandChange)
        onExpandChange();
}

/**
 * @brief 선택한 프리셋의 10밴드 dB 곡선을 적용한다.
 *
 * presetId(2~7)에 해당하는 프리셋 배열의 dB 값들을 읽어
 * 각 밴드 파라미터에 setValueNotifyingHost()로 설정한다.
 * isApplyingPreset 플래그를 설정하여 해당 콜백에서 프리셋 상태 되돌리기 방지.
 *
 * @param presetId  프리셋 ComboBox ID (2=Flat, 3=BassBoost, 4=ScoopMid, ...)
 *
 * @note [메인 스레드] presetCombo onChange 콜백에서 호출됨.
 */
void GraphicEQPanel::applyPreset (int presetId)
{
    // 각 프리셋의 10밴드 dB 곡선 (31/63/125/250/500/1k/2k/4k/8k/16kHz)
    // 각 프리셋은 베이스 기타의 음악적 스타일이나 톤을 표현하도록 설정됨.
    static const float presets[][numBands] =
    {
        // Flat: 모든 밴드 0dB (기준선)
        {  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f },
        // Bass Boost: 저역 강조, 중고역 소폭 컷 (펀치감 강조)
        {  6.0f,  5.0f,  3.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f },
        // Scoop Mid: 저/고역 부스트, 중역(250~1k) 컷 (베이스 전형적 V자 사운드)
        {  4.0f,  3.0f,  1.0f, -3.0f, -5.0f, -4.0f, -2.0f,  1.0f,  3.0f,  2.0f },
        // Presence: 중고역(1k~8k) 부스트, 존재감 및 선명도 강화 (솔로 톤)
        {  0.0f,  0.0f,  0.0f,  0.0f,  1.0f,  2.0f,  4.0f,  5.0f,  4.0f,  2.0f },
        // Vintage Warmth: 저중역 부스트, 고역 완만하게 컷 (빈티지 튜브 톤)
        {  3.0f,  4.0f,  3.0f,  2.0f,  1.0f,  0.0f, -1.0f, -2.0f, -3.0f, -4.0f },
        // Hi-Fi: 저역 약간 부스트, 고역 확장 (모던, 투명한 톤)
        {  2.0f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f,  2.0f,  4.0f,  5.0f },
    };

    // presetId → 배열 인덱스 변환: ID 2="Flat"(idx 0), ID 3="BassBoost"(idx 1), ...
    const int idx = presetId - 2;
    if (idx < 0 || idx >= (int) std::size (presets))
        return;

    // 프리셋 적용 중 플래그를 설정하여 sliders[i].onValueChange에서
    // 프리셋을 "(Custom)"으로 되돌리지 않도록 방지
    isApplyingPreset = true;
    for (int i = 0; i < numBands; ++i)
    {
        if (auto* param = apvtsRef.getParameter (bandParamIds[i]))
            param->setValueNotifyingHost (param->convertTo0to1 (presets[idx][i]));
    }
    isApplyingPreset = false;
}

GraphicEQPanel::~GraphicEQPanel() = default;

/**
 * @brief GraphicEQPanel 배경과 0dB 센터라인을 그린다.
 *
 * 배경: 다크 네이비 라운드 사각형
 * 테두리: 어두운 회색 라인
 * 센터라인 (펼친 상태에서만): 반투명 주황색 수평선
 *   → 슬라이더 위치가 0dB(중앙)인지 시각적으로 쉽게 판단하게 함.
 */
void GraphicEQPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 배경: 다크 네이비
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 테두리: 어두운 회색
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    if (! expanded)
        return;

    // 0dB 센터라인 (펼친 상태에서만 표시)
    // 각 슬라이더의 가운데를 가로질러 반투명 주황색 선을 그어
    // 사용자가 0dB 위치를 시각적으로 인식하도록 도움
    auto sliderArea = getLocalBounds().reduced (4);
    sliderArea.removeFromTop (30);   // 헤더 영역(expandButton + enabledToggle + flatButton + presetCombo)
    sliderArea.removeFromTop (16);   // dB 텍스트박스
    sliderArea.removeFromBottom (16); // 주파수 라벨 영역
    const int centerY = sliderArea.getY() + sliderArea.getHeight() / 2;
    g.setColour (juce::Colour (0x44ff8800));  // 반투명 주황색 (알파 0x44)
    g.drawHorizontalLine (centerY, static_cast<float> (sliderArea.getX()),
                          static_cast<float> (sliderArea.getRight()));
}

/**
 * @brief GraphicEQPanel의 자식 컴포넌트들을 배치한다.
 *
 * 레이아웃:
 * - 헤더 행: [expandButton 28px] [enabledToggle 140px] [남은공간: presetCombo] [flatButton 60px]
 * - 펼친 상태: sliders와 labels를 10개 균등 분할하여 배치
 * - 접힌 상태: 헤더만 표시 (early return)
 */
void GraphicEQPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // 헤더 행: [▼ 버튼 28px] [ON/OFF 토글 140px] [FLAT 버튼 60px]
    // collapsedHeight(36px) - 8px = 28px 높이로 헤더 구성 (EffectBlock과 동일)
    auto headerRow = area.removeFromTop (collapsedHeight - 8);
    expandButton.setBounds (headerRow.removeFromLeft (28));
    enabledToggle.setBounds (headerRow.removeFromLeft (140));
    flatButton.setBounds (headerRow.removeFromRight (60).reduced (2));
    presetCombo.setBounds (headerRow.reduced (4, 2)); // 남은 공간에 프리셋 드롭다운

    if (! expanded)
        return;

    area.removeFromTop (2);  // 헤더와 슬라이더 영역 사이 갭

    // 주파수 라벨 행 (하단, 16px 높이)
    auto labelRow = area.removeFromBottom (16);

    // 남은 영역을 10개 슬라이더에 균등하게 분할
    // 각 슬라이더는 나머지 높이 전체를 사용하며, 상단에 dB 텍스트박스(16px)를 포함
    const int sliderWidth = area.getWidth() / numBands;

    for (int i = 0; i < numBands; ++i)
    {
        sliders[i].setBounds (area.getX() + i * sliderWidth, area.getY(),
                              sliderWidth, area.getHeight());
        labels[i].setBounds (labelRow.getX() + i * sliderWidth, labelRow.getY(),
                             sliderWidth, labelRow.getHeight());
    }
}
