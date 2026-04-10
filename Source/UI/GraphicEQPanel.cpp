#include "GraphicEQPanel.h"

/**
 * @brief 10밴드 그래픽 EQ 패널 UI 생성
 *
 * 수직 슬라이더(31-16kHz), 프리셋 드롭다운, ON/OFF 토글, FLAT 버튼을 초기화한다.
 * 초기 상태는 펼침(expanded=true)이므로 모든 슬라이더와 컨트롤이 표시된다.
 *
 * @param apvts  AudioProcessorValueTreeState 참조
 *               (geq_enabled, geq_31~16k 파라미터 포함 필수)
 * @note [메인 스레드] PluginEditor 생성 시 호출됨
 */
GraphicEQPanel::GraphicEQPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // --- 접기/펼치기 버튼 ---
    // 초기: "v" (펼침 상태). setExpanded()로 토글 시 ">" (접힘) / "v" (펼침)
    // EffectBlock과 동일한 색상 스타일 적용
    expandButton.setButtonText ("v");
    expandButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a3a5a));
    expandButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5a5a7a));
    expandButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffffffff));
    expandButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffff8800));
    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    // --- Graphic EQ ON/OFF 토글 ---
    // OFF (toggle=false): EQ 필터 바이패스, 신호 통과만
    // ON (toggle=true): 10밴드 슬라이더 값 적용
    enabledToggle.setButtonText ("Graphic EQ");
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "geq_enabled", enabledToggle);

    // --- EQ 프리셋 드롭다운 ---
    // 선택지:
    //   ID=1: (Custom) — 사용자 수동 조정 시 자동 표시됨 (읽기 전용)
    //   ID=2~7: 프리셋 (Flat, Bass Boost, Scoop Mid, Presence, Vintage Warmth, Hi-Fi)
    // onChange: ID >= 2이면 applyPreset(id) 호출 → 10밴드 값 일괄 로드
    presetCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff3a3a5a));
    presetCombo.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    presetCombo.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff444466));
    presetCombo.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xffff8800));

    // 빌트인 + 유저 프리셋 드롭다운 구성
    refreshPresetCombo();

    presetCombo.onChange = [this]
    {
        const int id = presetCombo.getSelectedId();
        if (id >= 2 && id <= 7)
        {
            applyPreset (id);
        }
        else if (id == savePresetMenuId)
        {
            // 선택 후 UI는 "(Custom)"으로 되돌리고 저장 다이얼로그 표시
            presetCombo.setSelectedId (1, juce::dontSendNotification);
            saveCurrentAsUserPreset();
        }
        else if (id >= userPresetIdBase)
        {
            const int userIdx = id - userPresetIdBase;
            if (userIdx >= 0 && userIdx < userPresetNames.size())
                applyUserPreset (userPresetNames[userIdx]);
        }
    };
    addAndMakeVisible (presetCombo);

    // --- FLAT 리셋 버튼 ---
    // 클릭 시 모든 10밴드를 0dB로 설정하고 presetCombo를 "Flat"으로 표시
    // setValueNotifyingHost() 호출 → 각 슬라이더 onValueChange 발동
    // → presetCombo가 일시적으로 "(Custom)"으로 바뀌지만,
    //    마지막에 강제로 "Flat"(ID=2)로 설정하여 UI 일관성 유지
    flatButton.setButtonText ("FLAT");
    flatButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a3a5a));
    flatButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffff8800));
    flatButton.onClick = [this]
    {
        // 모든 10밴드를 0dB로 설정
        for (int i = 0; i < numBands; ++i)
        {
            if (auto* param = apvtsRef.getParameter (bandParamIds[i]))
                param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
        }
        // 프리셋 표시를 "Flat"으로 강제 설정 (onValueChange 콜백 무시)
        presetCombo.setSelectedId (2, juce::dontSendNotification);
    };
    addAndMakeVisible (flatButton);

    // --- 10밴드 수직 슬라이더 초기화 ---
    // 각 슬라이더는 이동 방향(위 = 증가), 텍스트 표시, 색상, APVTS 바인딩을 수행
    for (int i = 0; i < numBands; ++i)
    {
        // 슬라이더 스타일: LinearVertical (위로 드래그 = 게인 증가)
        sliders[i].setSliderStyle (juce::Slider::LinearVertical);

        // 텍스트 박스: 슬라이더 위에 표시 (40px 너비, 16px 높이), 1소수점 정밀도
        sliders[i].setTextBoxStyle (juce::Slider::TextBoxAbove, true, 40, 16);
        sliders[i].setNumDecimalPlacesToDisplay (1);
        sliders[i].setTextValueSuffix (" dB");

        // 마우스 휠 비활성화 (Viewport 스크롤 우선)
        sliders[i].setScrollWheelEnabled (false);

        // 색상 설정: thumb(주황), track(중간 회색), background(진한 네이비), text(밝은 회색)
        sliders[i].setColour (juce::Slider::thumbColourId,        juce::Colour (0xffff8800));
        sliders[i].setColour (juce::Slider::trackColourId,        juce::Colour (0xff5a5a7a));
        sliders[i].setColour (juce::Slider::backgroundColourId,   juce::Colour (0xff2a2a3e));
        sliders[i].setColour (juce::Slider::textBoxTextColourId,  juce::Colour (0xffdddddd));
        sliders[i].setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a1a2e));
        sliders[i].setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        addAndMakeVisible (sliders[i]);

        // APVTS 파라미터에 슬라이더 바인딩 (양방향 동기화)
        sliderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, bandParamIds[i], sliders[i]);

        // 슬라이더 수동 조정 시 프리셋 표시 변경
        // isApplyingPreset=true이면 무시 (프리셋 적용 중에 "(Custom)"으로 되돌리지 않음)
        sliders[i].onValueChange = [this]
        {
            if (! isApplyingPreset)
                presetCombo.setSelectedId (1, juce::dontSendNotification);
        };

        // 각 슬라이더 아래 주파수 라벨 (31Hz, 63Hz, 125Hz, ... 16kHz)
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

//==============================================================================
// Phase 8: 사용자 EQ 프리셋 (저장/불러오기/삭제)
//==============================================================================

juce::File GraphicEQPanel::getUserEqPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("BassMusicGear")
               .getChildFile ("EQPresets");
}

void GraphicEQPanel::refreshPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);

    // 1) "(Custom)" 및 빌트인 6종
    presetCombo.addItem ("(Custom)",       1);
    presetCombo.addItem ("Flat",           2);
    presetCombo.addItem ("Bass Boost",     3);
    presetCombo.addItem ("Scoop Mid",      4);
    presetCombo.addItem ("Presence",       5);
    presetCombo.addItem ("Vintage Warmth", 6);
    presetCombo.addItem ("Hi-Fi",          7);

    // 2) 사용자 프리셋 디렉터리 스캔
    userPresetNames.clear();
    auto dir = getUserEqPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
        for (const auto& f : files)
            userPresetNames.add (f.getFileNameWithoutExtension());
        userPresetNames.sort (true);
    }

    // 3) "Save Preset..." 항목 + 사용자 프리셋 목록을 구분선 아래로 배치
    presetCombo.addSeparator();
    presetCombo.addItem ("Save Preset...", savePresetMenuId);

    if (userPresetNames.size() > 0)
    {
        presetCombo.addSeparator();
        presetCombo.addSectionHeading ("User Presets");
        for (int i = 0; i < userPresetNames.size(); ++i)
            presetCombo.addItem (userPresetNames[i], userPresetIdBase + i);
    }

    presetCombo.setSelectedId (1, juce::dontSendNotification);
}

void GraphicEQPanel::saveCurrentAsUserPreset()
{
    auto* aw = new juce::AlertWindow ("Save EQ Preset",
                                      "Enter a name for this EQ curve:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "", "Name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            if (result == 1)
            {
                auto name = aw->getTextEditorContents ("name").trim();
                // 파일 시스템에 부적합한 문자 제거
                name = juce::File::createLegalFileName (name);
                if (name.isNotEmpty())
                {
                    // 10밴드 현재 값을 XML로 직렬화
                    auto xml = std::make_unique<juce::XmlElement> ("EQPreset");
                    xml->setAttribute ("name", name);
                    for (int i = 0; i < numBands; ++i)
                    {
                        if (auto* p = apvtsRef.getParameter (bandParamIds[i]))
                        {
                            auto* band = xml->createNewChildElement ("Band");
                            band->setAttribute ("id", bandParamIds[i]);
                            band->setAttribute ("gainDb",
                                (double) p->getNormalisableRange().convertFrom0to1 (p->getValue()));
                        }
                    }

                    auto dir = getUserEqPresetDirectory();
                    if (! dir.exists())
                        dir.createDirectory();

                    auto file = dir.getChildFile (name + ".xml");
                    xml->writeTo (file);

                    // 드롭다운 재구성 후 새로 저장된 프리셋 선택
                    refreshPresetCombo();
                    for (int i = 0; i < userPresetNames.size(); ++i)
                    {
                        if (userPresetNames[i] == name)
                        {
                            presetCombo.setSelectedId (userPresetIdBase + i,
                                                       juce::dontSendNotification);
                            break;
                        }
                    }
                }
            }
            delete aw;
        }));
}

void GraphicEQPanel::applyUserPreset (const juce::String& presetName)
{
    auto file = getUserEqPresetDirectory().getChildFile (presetName + ".xml");
    if (! file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("EQPreset"))
        return;

    isApplyingPreset = true;
    for (auto* band : xml->getChildWithTagNameIterator ("Band"))
    {
        const auto id = band->getStringAttribute ("id");
        const float gainDb = (float) band->getDoubleAttribute ("gainDb", 0.0);
        if (auto* p = apvtsRef.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (gainDb));
    }
    isApplyingPreset = false;
}

void GraphicEQPanel::mouseDown (const juce::MouseEvent& e)
{
    // 사용자 EQ 프리셋을 우클릭으로 삭제 (presetCombo 영역에서만 동작)
    if (! e.mods.isRightButtonDown())
        return;

    // presetCombo의 경계 내부에서 발생한 우클릭만 처리
    if (! presetCombo.getBounds().contains (e.getPosition()))
        return;

    const int id = presetCombo.getSelectedId();
    if (id < userPresetIdBase)
        return;

    const int userIdx = id - userPresetIdBase;
    if (userIdx < 0 || userIdx >= userPresetNames.size())
        return;

    deleteUserPreset (userPresetNames[userIdx]);
}

void GraphicEQPanel::deleteUserPreset (const juce::String& presetName)
{
    auto file = getUserEqPresetDirectory().getChildFile (presetName + ".xml");
    if (! file.existsAsFile())
        return;

    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Delete EQ Preset")
            .withMessage ("Delete user EQ preset \"" + presetName + "\"?")
            .withButton ("Delete")
            .withButton ("Cancel"),
        [this, file] (int result)
        {
            if (result == 1)
            {
                file.deleteFile();
                refreshPresetCombo();
            }
        });
}

//==============================================================================
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

    // 헤더 행: [expandButton 28px] [enabledToggle 140px] [presetCombo: 남은 공간] [flatButton 60px]
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
