#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PluginProcessor;

/**
 * @brief 프리셋 브라우저 + A/B 슬롯 비교 UI 컴포넌트
 *
 * **UI 구성**:
 * - 드롭다운 리스트: 팩토리 프리셋 15종 (섹션 헤딩)
 *                   + 사용자 프리셋 (구분선으로 분리)
 * - 버튼 그룹 1: Save, Delete, Export, Import
 * - 버튼 그룹 2: A, B (A/B 슬롯 전환)
 *
 * **A/B 슬롯 동작**:
 * - 첫 클릭: 현재 상태를 선택한 슬롯(A 또는 B)에 저장
 * - 이후 클릭: 슬롯에 저장된 상태를 복원하여 즉시 비교
 * - Standalone에서 두 개 설정을 빠르게 오갈 때 유용
 *
 * 모든 파일 I/O(Save/Load/Import/Export)는 메인 스레드에서만 호출되며,
 * FileChooser 비동기 콜백으로 UI 블로킹을 피한다.
 *
 * @note [메인 스레드] PresetPanel::paint() / resized()는 메시지 스레드에서만 호출.
 *       PresetManager 호출도 메인 스레드 안전.
 */
class PresetPanel : public juce::Component
{
public:
    static constexpr int panelHeight = 46;

    /**
     * @brief UI 패널 초기화
     *
     * @param processor  PluginProcessor 참조 (프리셋 관리자 및 A/B 슬롯 접근용)
     *
     * 역할:
     * - 모든 UI 컴포넌트(ComboBox, TextButton) 생성 및 색상 설정
     * - 이벤트 핸들러(onChange, onClick) 바인딩
     * - 초기 프리셋 목록 로드
     */
    explicit PresetPanel (PluginProcessor& processor);
    ~PresetPanel() override;

    /**
     * @brief 패널 배경 및 테두리 그리기
     *
     * @note [메인 스레드] 배경색은 어두운 회색(#2a2a3e), 테두리는 밝은 회색
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief 우측 마우스 이벤트 처리 — A/B 버튼 우클릭 시 슬롯 해제
     *
     * @note mouseDown과 onClick의 순서 제어: suppressSlotClick 플래그로
     *       우클릭 감지 후 onClick 호출을 방지한다.
     */
    void mouseDown (const juce::MouseEvent& e) override;

    /**
     * @brief UI 컴포넌트 크기 및 위치 설정
     *
     * @note 레이아웃: [드롭다운 넓음] [Save][Del][Exp][Imp] [A][B]
     *       우측부터 역순 배치(removeFromRight 사용)
     */
    void resized() override;

private:
    /**
     * @brief 프리셋 드롭다운 목록 갱신
     *
     * 팩토리 프리셋(섹션 헤딩) + 사용자 프리셋(구분선) 순서로 표시.
     * 각 항목에 고유 ID(1000번대 또는 2000번대)를 할당하여 presetSelected()에서 구분.
     */
    void refreshPresetList();

    /**
     * @brief 프리셋 선택 핸들러
     *
     * @param menuId  ComboBox에서 선택한 항목 ID (1000+idx 또는 2000+idx)
     *
     * 메뉴 ID 범위로 팩토리 또는 사용자 프리셋을 판정. 프리셋 로드 직전에
     * suppressNextCabIrOverride()를 호출해 프리셋의 cab_ir 값이 앰프 모델
     * 기본값으로 덮어써지지 않도록 한다.
     */
    void presetSelected (int menuId);

    /**
     * @brief Save 버튼 핸들러 — 새 사용자 프리셋 저장
     *
     * AlertWindow로 사용자 입력을 받아 프리셋 이름 획득.
     * 저장 후 프리셋 목록 갱신.
     */
    void handleSave();

    /**
     * @brief Delete 버튼 핸들러 — 선택한 사용자 프리셋 삭제
     *
     * 팩토리 프리셋은 삭제 불가 (isUserPresetSelected 체크).
     * 확인 대화 후 파일 제거.
     */
    void handleDelete();

    /**
     * @brief Export 버튼 핸들러 — 현재 상태를 임의 경로로 저장
     *
     * FileChooser로 저장 경로 선택. 확장자가 없으면 .bmg 추가.
     */
    void handleExport();

    /**
     * @brief Import 버튼 핸들러 — .bmg 또는 .xml 파일에서 프리셋 로드
     *
     * FileChooser로 파일 선택. 호환 형식이면 자동 파싱.
     */
    void handleImport();

    /**
     * @brief A/B 슬롯 버튼 핸들러 — 상태 저장/복원 및 cab_ir 자동 전환 억제
     *
     * @param slot  슬롯 번호 (0=A, 1=B)
     *
     * 좌클릭 동작:
     * - 첫 클릭: processorRef.saveToSlot() 호출 → 현재 상태 저장, 버튼 토글
     * - 이후 클릭: suppressNextCabIrOverride() 호출 후 processorRef.loadFromSlot()
     *   → 저장된 상태 복원 (슬롯의 cab_ir 값 유지), ComboBox 선택 해제
     *
     * @note [메인 스레드] 슬롯 복원 시 cab_ir이 프리셋처럼 변경되는 것을
     *       방지하기 위해 suppressNextCabIrOverride()를 호출한다.
     */
    void handleSlotButton (int slot);

    PluginProcessor& processorRef;

    juce::ComboBox   presetCombo;
    juce::TextButton saveButton   { "Save" };
    juce::TextButton deleteButton { "Del"  };
    juce::TextButton exportButton { "Exp"  };
    juce::TextButton importButton { "Imp"  };

    juce::TextButton slotAButton  { "A" };
    juce::TextButton slotBButton  { "B" };

    bool isUserPresetSelected = false;  // 선택된 프리셋이 사용자 프리셋인지 여부
    bool suppressSlotClick    = false;  // 우클릭(mouseDown) 후 onClick 호출 억제

    // 메뉴 ID 오프셋: 팩토리/사용자 프리셋 구분용
    // ComboBox 메뉴 ID = factoryIdOffset + index (또는 userIdOffset + index)
    static constexpr int factoryIdOffset = 1000;
    static constexpr int userIdOffset    = 2000;

    // Export/Import 파일 선택 대화 (비동기, unique_ptr로 수명 관리)
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPanel)
};
