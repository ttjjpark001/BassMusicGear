#include "PresetPanel.h"
#include "../PluginProcessor.h"

//==============================================================================
/**
 * @brief PresetPanel 초기화: UI 컴포넌트 색상 설정 및 이벤트 연결
 *
 * @param p  PluginProcessor 참조
 *
 * 역할:
 * - 프리셋 드롭다운 색상 설정 및 onChange 콜백 연결
 * - Save/Delete/Export/Import/A/B 버튼 색상 설정 및 onClick 콜백 연결
 * - A/B 버튼에 mouseListener 등록 (우클릭 슬롯 해제용)
 * - 모든 컴포넌트를 화면에 추가
 * - 초기 프리셋 목록 로드
 *
 * @note [메인 스레드] 생성자에서 UI 셋업. FileChooser는 나중에 필요시 동적 생성.
 */
PresetPanel::PresetPanel (PluginProcessor& p)
    : processorRef (p)
{
    // --- 프리셋 드롭다운 색상 설정 ---
    // 배경: 어두운 보라(#3a3a5a), 텍스트: 흰색, 화살표: 주황색
    presetCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff3a3a5a));
    presetCombo.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    presetCombo.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff444466));
    presetCombo.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xffff8800));
    presetCombo.setTextWhenNothingSelected ("-- Select Preset --");
    presetCombo.onChange = [this]
    {
        const int id = presetCombo.getSelectedId();
        if (id > 0)
            presetSelected (id);
    };
    addAndMakeVisible (presetCombo);

    // --- 모든 버튼에 공통 색상 스타일 적용 ---
    // 배경: 드롭다운과 동일, 텍스트: 흰색(Off) / 주황색(On)
    auto styleButton = [] (juce::TextButton& btn)
    {
        btn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff3a3a5a));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
        btn.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xffff8800));
    };

    styleButton (saveButton);
    styleButton (deleteButton);
    styleButton (exportButton);
    styleButton (importButton);
    styleButton (slotAButton);
    styleButton (slotBButton);

    // --- 버튼 이벤트 핸들러 연결 ---
    saveButton.onClick   = [this] { handleSave();   };
    deleteButton.onClick = [this] { handleDelete(); };
    exportButton.onClick = [this] { handleExport(); };
    importButton.onClick = [this] { handleImport(); };
    slotAButton.onClick  = [this] { handleSlotButton (0); };
    slotBButton.onClick  = [this] { handleSlotButton (1); };

    // A/B 버튼 우클릭 감지: 슬롯 해제용
    slotAButton.addMouseListener (this, false);
    slotBButton.addMouseListener (this, false);

    addAndMakeVisible (saveButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (importButton);
    addAndMakeVisible (slotAButton);
    addAndMakeVisible (slotBButton);

    // --- 초기 프리셋 목록 로드 및 표시 ---
    refreshPresetList();
}

PresetPanel::~PresetPanel() = default;

/**
 * @brief 프리셋 드롭다운 목록 갱신
 *
 * 팩토리 프리셋(섹션 헤딩) + 사용자 프리셋(구분선으로 분리)을 표시.
 * 각 항목에 고유 ID를 할당하므로 presetSelected()에서 구분 가능.
 *
 * @note [메인 스레드] UI 갱신 시 호출. 사용자가 프리셋을 저장하거나 삭제한 후에도 호출.
 */
void PresetPanel::refreshPresetList()
{
    // 기존 목록 초기화 (onChange 콜백 송신 안 함)
    presetCombo.clear (juce::dontSendNotification);

    auto& pm = processorRef.getPresetManager();

    // 1. 팩토리 프리셋 (ID: factoryIdOffset + index, 즉 1000+idx)
    presetCombo.addSectionHeading ("Factory");
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
        presetCombo.addItem (pm.getFactoryPresetName (i), factoryIdOffset + i);

    // 2. 사용자 프리셋 (ID: 2000+idx, 있을 경우만 표시)
    const auto userNames = pm.getUserPresetNames();
    if (userNames.size() > 0)
    {
        // 시각적 구분을 위해 구분선 + 섹션 헤딩 추가
        presetCombo.addSeparator();
        presetCombo.addSectionHeading ("User");
        for (int i = 0; i < userNames.size(); ++i)
            presetCombo.addItem (userNames[i], userIdOffset + i);
    }
}

/**
 * @brief 프리셋 선택 콜백 — 팩토리 또는 사용자 프리셋 로드
 *
 * @param menuId  ComboBox 메뉴 ID (factoryIdOffset 또는 userIdOffset 포함)
 *
 * 메뉴 ID 범위로 프리셋 종류를 판정:
 * - 1000~1999: 팩토리 프리셋 (ID - 1000 = 인덱스)
 * - 2000+: 사용자 프리셋 (ID - 2000 = 인덱스)
 *
 * isUserPresetSelected 플래그를 갱신하여 Delete 버튼 활성화 여부 결정.
 */
void PresetPanel::presetSelected (int menuId)
{
    auto& pm = processorRef.getPresetManager();

    if (menuId >= factoryIdOffset && menuId < userIdOffset)
    {
        // 팩토리 프리셋 (ID 1000~1999)
        const int idx = menuId - factoryIdOffset;
        // 프리셋 로드 전에 suppressNextCabIrOverride() 호출:
        // 로드될 cab_ir 값이 앰프 모델 자동 전환에 의해 덮어써지지 않도록 함
        processorRef.suppressNextCabIrOverride();
        pm.loadFactoryPreset (idx);
        isUserPresetSelected = false;
    }
    else if (menuId >= userIdOffset)
    {
        // 사용자 프리셋 (ID 2000+)
        const int idx = menuId - userIdOffset;
        const auto names = pm.getUserPresetNames();
        if (idx >= 0 && idx < names.size())
        {
            processorRef.suppressNextCabIrOverride();
            pm.loadUserPreset (names[idx]);
            isUserPresetSelected = true;
        }
    }
}

/**
 * @brief Save 버튼 핸들러 — 새 사용자 프리셋 저장 대화
 *
 * AlertWindow로 프리셋 이름 입력받아, PresetManager::saveUserPreset() 호출.
 * 저장 후 프리셋 목록을 갱신하여 새로 저장된 프리셋이 드롭다운에 나타나도록 함.
 *
 * @note [메인 스레드] AlertWindow::enterModalState() 비동기 콜백.
 *       파일 I/O 블로킹 가능 (별도 스레드로 분리하지 않음).
 */
void PresetPanel::handleSave()
{
    // AlertWindow 생성 (사용자 입력용)
    auto* aw = new juce::AlertWindow ("Save Preset",
                                      "Enter a preset name:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "", "Name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // 모달 진입 후 사용자 입력 대기
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            if (result == 1)
            {
                // Save 버튼 눌림 → 프리셋 이름 획득 및 저장
                const auto name = aw->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                {
                    // PresetManager::saveUserPreset() 호출
                    processorRef.getPresetManager().saveUserPreset (name);
                    refreshPresetList();
                }
            }
            delete aw;
        }));
}

/**
 * @brief Delete 버튼 핸들러 — 선택한 사용자 프리셋 삭제
 *
 * 조건:
 * - 사용자 프리셋만 삭제 가능 (팩토리 프리셋은 읽기 전용)
 * - isUserPresetSelected 플래그로 팩토리/사용자 구분
 *
 * 동작:
 * - 확인 대화(AlertWindow) 표시
 * - 사용자 확인 후 PresetManager::deleteUserPreset() 호출
 * - 프리셋 목록 갱신
 *
 * @note [메인 스레드] 파일 삭제 (블로킹 가능).
 */
void PresetPanel::handleDelete()
{
    // 사용자 프리셋이 선택되지 않았으면 조용히 반환
    if (! isUserPresetSelected)
        return;

    const int id = presetCombo.getSelectedId();
    if (id < userIdOffset)
        return;

    const int idx = id - userIdOffset;
    const auto names = processorRef.getPresetManager().getUserPresetNames();
    if (idx < 0 || idx >= names.size())
        return;

    const auto name = names[idx];

    // 삭제 확인 대화 표시
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Delete Preset")
            .withMessage ("Delete user preset \"" + name + "\"?")
            .withButton ("Delete")
            .withButton ("Cancel"),
        [this, name] (int result)
        {
            if (result == 1)  // Delete 버튼 선택
            {
                processorRef.getPresetManager().deleteUserPreset (name);
                refreshPresetList();
                isUserPresetSelected = false;
            }
        });
}

/**
 * @brief Export 버튼 핸들러 — 현재 상태를 임의 경로로 내보내기
 *
 * FileChooser로 저장 경로 선택. 확장자가 없으면 .bmg 추가.
 * 기존 파일이 있으면 덮어쓰기 경고 표시.
 *
 * @note [메인 스레드] FileChooser::launchAsync() 비동기. 파일 쓰기는 동기.
 *       userDocumentsDirectory부터 시작 (사용자 편의).
 */
void PresetPanel::handleExport()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Export Preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.bmg");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                // 확장자 없으면 .bmg 추가
                if (file.getFileExtension().isEmpty())
                    file = file.withFileExtension (".bmg");
                processorRef.getPresetManager().exportPresetToFile (file);
            }
        });
}

/**
 * @brief Import 버튼 핸들러 — 파일에서 프리셋 로드
 *
 * FileChooser로 .bmg 또는 .xml 파일 선택. 호환 형식이면 자동 파싱.
 * 로드 전에 suppressNextCabIrOverride()를 호출해 프리셋의 cab_ir 값을 유지.
 * 로드된 상태는 현재 APVTS에 병합되며, 없는 파라미터는 현재 값 유지.
 *
 * @note [메인 스레드] FileChooser::launchAsync() 비동기. 파일 읽기는 동기.
 */
void PresetPanel::handleImport()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import Preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.bmg;*.xml");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                // 임포트할 프리셋의 cab_ir 값이 앰프 모델 자동 전환에 의해
                // 덮어써지지 않도록 미리 플래그를 설정
                processorRef.suppressNextCabIrOverride();
                processorRef.getPresetManager().importPresetFromFile (file);
            }
        });
}

/**
 * @brief A/B 슬롯 버튼 핸들러 — 상태 저장/복원 토글
 *
 * @param slot  슬롯 번호 (0=A, 1=B)
 *
 * **동작**:
 * - 슬롯이 비어있으면(처음 클릭): 현재 상태 저장 + 버튼 토글
 * - 슬롯이 차있으면(이후 클릭): suppressNextCabIrOverride() 호출 후
 *   저장된 상태 복원. 슬롯의 cab_ir 값이 유지되고, ComboBox 선택 해제.
 *
 * **사용 예**:
 * 1. 설정 A(특정 IR 포함) 조성 후 A 버튼 클릭 → 저장
 * 2. 설정 B로 변경 후 B 버튼 클릭 → 저장
 * 3. A 버튼 클릭 → 설정 A 복원 (IR 포함, 즉시 비교)
 * 4. B 버튼 클릭 → 설정 B 복원
 *
 * @note [메인 스레드] 슬롯 복원 시 cab_ir 자동 전환을 방지하기 위해
 *       suppressNextCabIrOverride()를 호출하므로, 슬롯 저장 당시의
 *       IR 선택이 앰프 모델 변경에도 불구하고 유지된다.
 */
void PresetPanel::handleSlotButton (int slot)
{
    // 우클릭(mouseDown)으로 인한 호출이면 무시 (슬롯 해제는 이미 처리됨)
    if (suppressSlotClick)
    {
        suppressSlotClick = false;
        return;
    }

    // 슬롯 유효성 확인 (첫 사용 vs 재사용)
    if (! processorRef.isSlotValid (slot))
    {
        // 첫 클릭: 현재 상태를 슬롯에 저장
        processorRef.saveToSlot (slot);
        (slot == 0 ? slotAButton : slotBButton).setToggleState (true, juce::dontSendNotification);
    }
    else
    {
        // 이후 클릭: 저장된 상태를 복원 (즉시 비교 모드)
        // suppressNextCabIrOverride()로 슬롯 복원 시 cab_ir 자동 전환 방지
        processorRef.suppressNextCabIrOverride();
        processorRef.loadFromSlot (slot);
        // 프리셋 드롭다운 선택 해제 (슬롯은 프리셋이 아니므로)
        presetCombo.setSelectedId (0, juce::dontSendNotification);
        isUserPresetSelected = false;
    }
}

void PresetPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown())
        return;

    int slot = -1;
    if (e.eventComponent == &slotAButton) slot = 0;
    else if (e.eventComponent == &slotBButton) slot = 1;

    if (slot < 0)
        return;

    // mouseDown 직후 자동으로 onClick이 호출되므로,
    // suppressSlotClick 플래그로 handleSlotButton 진입을 방지한다.
    suppressSlotClick = true;

    // 슬롯에 저장된 상태가 있으면 우클릭으로 해제
    if (processorRef.isSlotValid (slot))
    {
        processorRef.clearSlot (slot);
        (slot == 0 ? slotAButton : slotBButton).setToggleState (false, juce::dontSendNotification);
    }
}

//==============================================================================
/**
 * @brief 패널 배경 및 테두리 그리기
 *
 * @note 배경색: 어두운 회색-보라(#2a2a3e)
 *       테두리색: 밝은 회색(#444466)
 *       모서리: 4px 라운드
 */
void PresetPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    // 배경 채우기
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);
    // 테두리 그리기
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

/**
 * @brief 모든 UI 컴포넌트 크기 및 위치 설정
 *
 * **레이아웃** (우측부터 역순 배치):
 * [드롭다운 (남은 공간)] [Save 42px] [Del 42px] [Exp 42px] [Imp 42px] [A 28px] [B 28px]
 *
 * @note removeFromRight() 순서: B → A → [gap] → Imp → Exp → Del → Save → [gap] → Combo
 *       각 버튼 사이 3px 간격 (큰 간격은 6px)
 */
void PresetPanel::resized()
{
    // 패널 내부 여백 제거 (좌우상하 4px)
    auto area = getLocalBounds().reduced (4);

    // 레이아웃: [Combo 넓은 공간] [Save][Del][Exp][Imp] [A][B]
    const int buttonW = 42;  // 일반 버튼 폭
    const int slotW   = 28;  // A/B 슬롯 버튼 폭
    const int gap     = 3;   // 버튼 간 작은 간격

    // 우측부터 역순 배치
    slotBButton.setBounds (area.removeFromRight (slotW));
    area.removeFromRight (gap);
    slotAButton.setBounds (area.removeFromRight (slotW));
    area.removeFromRight (gap * 2);  // 큰 간격 (A/B 그룹과 분리)

    importButton.setBounds (area.removeFromRight (buttonW));
    area.removeFromRight (gap);
    exportButton.setBounds (area.removeFromRight (buttonW));
    area.removeFromRight (gap);
    deleteButton.setBounds (area.removeFromRight (buttonW));
    area.removeFromRight (gap);
    saveButton.setBounds (area.removeFromRight (buttonW));
    area.removeFromRight (gap * 2);  // 큰 간격 (버튼 그룹과 드롭다운 분리)

    // 남은 공간 전부 드롭다운에 할당
    presetCombo.setBounds (area);
}
