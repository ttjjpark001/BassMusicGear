#include "EffectBlock.h"

EffectBlock::EffectBlock (const juce::String& name,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& enabledParamId,
                           const juce::StringArray& paramIds,
                           const juce::StringArray& paramLabels,
                           const juce::StringArray& comboParamIds,
                           const juce::StringArray& comboLabelsArray,
                           const juce::StringArray& toggleParamIds,
                           const juce::StringArray& toggleLabelsArray)
    : effectName (name),
      knobLabels (paramLabels)
{
    // --- 접기/펼치기 버튼 (> / v) ---
    expandButton.setButtonText (">");
    expandButton.setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff3a3a5a));
    expandButton.setColour (juce::TextButton::buttonOnColourId,  juce::Colour (0xff5a5a7a));
    expandButton.setColour (juce::TextButton::textColourOffId,   juce::Colour (0xffffffff));
    expandButton.setColour (juce::TextButton::textColourOnId,    juce::Colour (0xffff8800));
    expandButton.onClick = [this]
    {
        setExpanded (!expanded);
    };
    addAndMakeVisible (expandButton);

    // --- ON/OFF 토글 버튼 ---
    enabledToggle.setButtonText (name);
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, enabledParamId, enabledToggle);

    // --- 파라미터 노브 생성 ---
    for (int i = 0; i < paramIds.size(); ++i)
    {
        const auto& paramId = paramIds[i];
        const auto& label   = i < paramLabels.size() ? paramLabels[i] : paramId;
        auto* knob = knobs.add (new Knob (label, paramId, apvts));
        addChildComponent (knob);  // 처음에는 숨김 (접힌 상태)
    }

    // --- ComboBox 생성 (선택 파라미터 UI) ---
    // AudioParameterChoice 파라미터를 UI 드롭다운으로 제어한다.
    // 예: Overdrive Type (Tube/JFET/Fuzz), Reverb Type (Spring/Room/Hall/Plate), Delay Note Value (1/4/1/8 등)
    for (int i = 0; i < comboParamIds.size(); ++i)
    {
        const auto& comboId = comboParamIds[i];
        const auto& comboLabel = i < comboLabelsArray.size() ? comboLabelsArray[i] : comboId;

        // ComboBox 스타일링 (다크 테마 유지)
        auto* combo = combos.add (new juce::ComboBox());
        combo->setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xff3a3a5a));
        combo->setColour (juce::ComboBox::textColourId,        juce::Colours::white);
        combo->setColour (juce::ComboBox::outlineColourId,     juce::Colour (0xff444466));
        combo->setColour (juce::ComboBox::arrowColourId,       juce::Colour (0xffff8800));
        addChildComponent (combo);  // 처음에는 숨김 (접힌 상태)

        // ComboBox 라벨 추가
        auto* lbl = comboLabelComponents.add (new juce::Label());
        lbl->setText (comboLabel, juce::dontSendNotification);
        lbl->setJustificationType (juce::Justification::centred);
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setFont (juce::FontOptions (10.0f));
        addChildComponent (lbl);  // 처음에는 숨김

        // --- ComboBox 항목 자동 채우기 ---
        // JUCE의 ComboBoxAttachment는 APVTS 파라미터 동기화만 담당하며,
        // 드롭다운 항목을 자동으로 추가하지 않는다.
        // AudioParameterChoice의 choices 배열을 수동으로 순회하며 등록해야 한다.
        // ComboBox 항목 ID는 JUCE 관례상 1부터 시작 (0은 사용 불가)
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (comboId)))
        {
            // j번째 선택지를 ComboBox에 추가 (항목 ID = j+1)
            // 예: Overdrive Type → Tube(1), JFET(2), Fuzz(3)
            for (int j = 0; j < choiceParam->choices.size(); ++j)
                combo->addItem (choiceParam->choices[j], j + 1);
        }

        // ComboBoxAttachment: APVTS 파라미터 값 ↔ ComboBox 선택 항목 동기화
        // addItem()과는 별개로 동작하며, 파라미터 변경 시 ComboBox를 업데이트하고
        // ComboBox 선택 시 파라미터 값을 변경한다.
        auto* att = comboAttachments.add (
            new juce::AudioProcessorValueTreeState::ComboBoxAttachment (apvts, comboId, *combo));
        juce::ignoreUnused (att);
    }

    // --- 추가 토글 버튼 생성 (이펙터 블록 특화 선택) ---
    // ON/OFF 토글과는 별개로, 이펙터별 특수 모드 선택을 처리한다.
    // 예시:
    //   - Delay: BPM Sync ON/OFF (수동 시간 vs DAW BPM 동기화)
    //   - (향후) Gate: Envelope Follower ON/OFF (외부 신호 vs 자체 신호)
    // Delay BPM Sync가 해당하는 경우, 펼침 상태에서만 표시된다.
    for (int i = 0; i < toggleParamIds.size(); ++i)
    {
        const auto& toggleId = toggleParamIds[i];
        const auto& toggleLabel = i < toggleLabelsArray.size() ? toggleLabelsArray[i] : toggleId;

        auto* toggle = extraToggles.add (new juce::ToggleButton (toggleLabel));
        toggle->setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        toggle->setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
        addChildComponent (toggle);  // 처음에는 숨김 (접힌 상태)

        auto* toggleAtt = extraToggleAttachments.add (
            new juce::AudioProcessorValueTreeState::ButtonAttachment (apvts, toggleId, *toggle));
        juce::ignoreUnused (toggleAtt);
    }
}

EffectBlock::~EffectBlock() = default;

void EffectBlock::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;

    // 확장 버튼 텍스트 변경: ">" (접힘) ↔ "v" (펼침)
    expandButton.setButtonText (expanded ? "v" : ">");

    // 모든 컨트롤의 가시성 일괄 토글
    // 접힌 상태: 헤더(버튼 + ON/OFF 토글) 만 표시 (36px 높이)
    // 펼친 상태: 헤더 + 노브/ComboBox/토글 전체 표시 (130px 높이)
    for (auto* knob : knobs)
        knob->setVisible (expanded);

    for (auto* combo : combos)
        combo->setVisible (expanded);
    for (auto* lbl : comboLabelComponents)
        lbl->setVisible (expanded);

    for (auto* toggle : extraToggles)
        toggle->setVisible (expanded);

    // --- PluginEditor 콜백: 전체 창 레이아웃 재계산 트리거 ---
    // ContentComponent의 calculateNeededHeight()가 호출되어
    // 모든 EffectBlock의 getExpanded() 상태를 재조사하고,
    // Viewport 스크롤 영역의 새 높이를 계산한다.
    // 이로써 사용자가 블록을 펼칠 때마다 창 높이가 동적으로 조정된다.
    if (onExpandChange)
        onExpandChange();
}

void EffectBlock::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 배경
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 테두리
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void EffectBlock::resized()
{
    auto area = getLocalBounds().reduced (4);

    // --- 헤더 행 배치 (항상 표시) ---
    // [▼/▶ 버튼(28px)] [ON/OFF 토글("Overdrive", "BPM Sync" 등)]
    auto headerRow = area.removeFromTop (collapsedHeight - 8);
    expandButton.setBounds (headerRow.removeFromLeft (28));
    enabledToggle.setBounds (headerRow);

    // 접혀 있거나 컨트롤이 없으면 헤더만 표시하고 반환
    if (!expanded || knobs.isEmpty())
        return;

    // --- 펼친 상태: 파라미터 노브, ComboBox, 토글을 수평으로 균등 배치 ---
    area.removeFromTop (4);  // 헤더와 컨트롤 사이 갭(스페이싱)

    // 배치할 총 컨트롤 수: 노브 + ComboBox(선택 파라미터) + 추가 토글
    // 예: Delay의 경우 노브 4개(Time/Feedback/Damping/Mix) + 토글 1개(BPM Sync) = 5개
    const int totalControls = knobs.size() + combos.size() + extraToggles.size();
    if (totalControls == 0) return;

    // 각 컨트롤에 할당할 폭: 사용 가능한 전체 폭을 컨트롤 개수로 균등 분할
    // 예) 400px / 5 = 80px per control
    const int controlWidth = area.getWidth() / totalControls;

    // 파라미터 노브 배치: 수평 나열
    // 노브는 내부적으로 고정 높이를 가지고 있으며, 폭만 여기서 설정된다.
    for (auto* knob : knobs)
        knob->setBounds (area.removeFromLeft (controlWidth));

    // ComboBox 배치 (선택 파라미터, 예: Overdrive Type, Reverb Type)
    for (int i = 0; i < combos.size(); ++i)
    {
        auto comboArea = area.removeFromLeft (controlWidth);
        // 라벨 영역 분리: 하단 16px
        auto labelArea = comboArea.removeFromBottom (16);
        // 드롭다운을 세로 중앙에 배치하기 위해 상단 1/3 여백 제거
        comboArea.removeFromTop (comboArea.getHeight() / 3);
        // 드롭다운 높이 24px, 좌우 마진 4px
        combos[i]->setBounds (comboArea.reduced (4, 0).removeFromTop (24));
        // ComboBox 라벨: 선택 폭 전체 사용
        comboLabelComponents[i]->setBounds (labelArea);
    }

    // 추가 토글 버튼 배치 (예: Delay의 BPM Sync)
    // ComboBox와 유사한 세로 중앙 배치
    for (auto* toggle : extraToggles)
    {
        auto toggleArea = area.removeFromLeft (controlWidth);
        // 토글을 세로 중앙에 배치: 상단 1/3 여백 제거
        toggleArea.removeFromTop (toggleArea.getHeight() / 3);
        // 토글 버튼 높이 24px, 좌우 마진 4px
        toggle->setBounds (toggleArea.reduced (4, 0).removeFromTop (24));
    }
}
