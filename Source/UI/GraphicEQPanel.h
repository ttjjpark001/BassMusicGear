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
    static constexpr int collapsedHeight = 36;
    static constexpr int expandedHeight  = 220;

    GraphicEQPanel (juce::AudioProcessorValueTreeState& apvts);
    ~GraphicEQPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // 우클릭으로 사용자 EQ 프리셋 삭제
    void mouseDown (const juce::MouseEvent& e) override;

    bool getExpanded() const { return expanded; }
    void setExpanded (bool shouldBeExpanded);

    std::function<void()> onExpandChange;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;

    // 접기(">") / 펼치기("v") 토글 버튼
    // 클릭 시 sliders/labels/flatButton/presetCombo의 가시성을 토글하고
    // onExpandChange 콜백을 호출하여 전체 에디터 높이를 재계산하도록 신호.
    juce::TextButton expandButton;

    // GraphicEQ ON/OFF 토글 버튼
    // geq_enabled 파라미터에 바인딩되어 GraphicEQ 처리 활성/비활성 제어
    juce::ToggleButton enabledToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    // EQ 프리셋 드롭다운
    // "(Custom)", "Flat", "Bass Boost", "Scoop Mid", "Presence", "Vintage Warmth", "Hi-Fi"
    // 사용자가 선택하면 applyPreset()을 호출하여 해당 곡선의 10밴드 값을 적용.
    // 사용자가 슬라이더를 수동 조정하면 "(Custom)"으로 자동 리셋 (isApplyingPreset 플래그로 프리셋 적용 중은 무시).
    juce::ComboBox presetCombo;
    void applyPreset (int presetIndex);

    // --- 사용자 EQ 프리셋 (Phase 8) ---
    // 저장 경로: userApplicationDataDirectory/BassMusicGear/EQPresets/*.xml
    // 빌트인 프리셋 아래 구분선으로 구분되어 나열되며, "Save Preset..." 항목과
    // 사용자 프리셋이 함께 표시된다.
    static juce::File getUserEqPresetDirectory();
    void refreshPresetCombo();                              // 빌트인 + 유저 목록 재구성
    void applyUserPreset (const juce::String& presetName);  // XML에서 10밴드 값 로드
    void saveCurrentAsUserPreset();                         // AlertWindow로 이름 입력 후 저장
    void deleteUserPreset (const juce::String& presetName); // 확인 후 파일 삭제

    // 메뉴 ID 매핑:
    //   1       = (Custom)
    //   2..7    = 빌트인 프리셋 (Flat, Bass Boost, ..., Hi-Fi)
    //   100     = "Save Preset..." 항목
    //   1000..  = 사용자 프리셋 (인덱스 순)
    static constexpr int savePresetMenuId = 100;
    static constexpr int userPresetIdBase = 1000;

    // 현재 로드된 사용자 프리셋 이름 리스트 (Delete 및 인덱스 매핑용)
    juce::StringArray userPresetNames;

    // FLAT(전체 0dB) 리셋 버튼
    // 모든 밴드를 0dB로 설정하고 presetCombo를 "Flat"으로 선택
    juce::TextButton flatButton;

    // 10밴드 수직 슬라이더 (31/63/125/250/500/1k/2k/4k/8k/16kHz)
    // 각 슬라이더는 -12~+12dB 범위, 슬라이더 위에 dB 값 표시
    // sliderAttachment를 통해 APVTS 파라미터에 동기화됨
    static constexpr int numBands = 10;
    juce::Slider sliders[numBands];
    juce::Label  labels[numBands];  // 각 슬라이더 아래 주파수 라벨 (31, 63, 125, ... 16k)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments[numBands];

    // expanded: GraphicEQPanel의 접힘/펼침 상태
    // 펼쳐진 상태(true)에서는 sliders/labels 표시, 접힌 상태(false)에서는 헤더만 표시
    bool expanded = true;

    // isApplyingPreset: 프리셋 적용 중 플래그
    // applyPreset()가 각 슬라이더 값을 setValueNotifyingHost()로 변경할 때,
    // 해당 변경이 각 슬라이더의 onValueChange 콜백을 트리거한다.
    // 이 콜백에서 presetCombo를 "(Custom)"으로 되돌리는 것을 방지하기 위해
    // onValueChange 내부에서 isApplyingPreset을 확인하여 프리셋 적용 중은 무시한다.
    bool isApplyingPreset = false;

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
