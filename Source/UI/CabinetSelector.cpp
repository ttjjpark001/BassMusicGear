// 캐비닛 IR 선택 UI: ComboBox(5종 IR) + Bypass 토글 버튼
// IR 선택 시 BinaryData에서 로드하여 Cabinet에 전달한다.
#include "CabinetSelector.h"
#include "../PluginProcessor.h"
#include <BinaryData.h>

/**
 * @brief CabinetSelector 생성자
 *
 * IR 선택 ComboBox, 커스텀 IR 로드 버튼, Bypass 토글을 초기화한다.
 * IR 파라미터("cab_ir")의 변경을 리스닝하여 자동 로드를 트리거한다.
 *
 * @param apvts      APVTS (파라미터 Attachment 생성)
 * @param processor  SignalChain의 Cabinet에 접근하기 위한 참조
 * @note [메인 스레드] PluginEditor 생성 시 호출됨
 */
CabinetSelector::CabinetSelector (juce::AudioProcessorValueTreeState& apvts,
                                   PluginProcessor& processor)
    : apvtsRef (apvts),
      processorRef (processor)
{
    // --- IR 선택 ComboBox: 5종 내장 캐비닛 IR ---
    // ComboBox ID는 1부터 시작 (JUCE 관례: 0 예약)
    // loadSelectedIR()에서 0~4 인덱스로 변환하여 BinaryData 로드
    irCombo.addItem ("8x10 SVT",      1);        // Ampeg SVT: 따뜻함, 풍성한 저역
    irCombo.addItem ("4x10 JBL",      2);        // Fender Bassman: 미드 밝음, 정의력
    irCombo.addItem ("1x15 Vintage",  3);        // 빈티지 1x15: 깊은 저음
    irCombo.addItem ("2x12 British",  4);        // Orange AD200: 타이트, 명확함
    irCombo.addItem ("2x10 Modern",   5);        // Markbass 스타일 현대식 콤팩트: 타이트/균형
    addAndMakeVisible (irCombo);

    // ComboBox와 APVTS "cab_ir" 파라미터 양방향 동기화
    irAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "cab_ir", irCombo);

    // --- 커스텀 IR 로드 버튼 ---
    // 클릭 시 FileChooser 열어 사용자 정의 WAV 파일 선택 가능
    // BinaryData 내장 IR 외에 추가 캐비닛 모델 로드 가능
    loadIRButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff3a3a5a));
    loadIRButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff8800));
    loadIRButton.onClick = [this] { openIRFileChooser(); };
    addAndMakeVisible (loadIRButton);

    // --- Bypass 토글 버튼: Cabinet Convolution 우회 ---
    // OFF (toggle=false): 선택된 IR 적용 (컨볼루션 활성)
    // ON (toggle=true): 캐비닛 제거, amp head 신호만 (건조한 톤, 앰프만 청취)
    bypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    bypassButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (bypassButton);

    // Bypass 토글과 APVTS "cab_bypass" 파라미터 양방향 동기화
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "cab_bypass", bypassButton);

    // --- "cab_ir" 파라미터 변경 리스너 등록 ---
    // 사용자가 IR 항목을 선택하면 parameterChanged() 콜백 실행
    // → MessageManager::callAsync로 loadSelectedIR() 메인 스레드에서 실행
    apvts.addParameterListener ("cab_ir", this);
}

CabinetSelector::~CabinetSelector()
{
    // 소멸 시 파라미터 리스너 제거 (dangling 포인터 방지)
    apvtsRef.removeParameterListener ("cab_ir", this);
}

/**
 * @brief APVTS "cab_ir" 파라미터 변경 콜백 (리스너 인터페이스)
 *
 * 사용자가 ComboBox에서 IR을 선택하면 호출된다.
 * 실제 IR 로드는 MessageManager::callAsync로 메인 스레드에서 수행된다.
 *
 * @param parameterID  변경된 파라미터 ID ("cab_ir" 확인)
 * @param newValue     미사용 (ComboBox에서 직접 getRawParameterValue로 읽음)
 */
void CabinetSelector::parameterChanged (const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID == "cab_ir")
    {
        // SafePointer: CabinetSelector가 소멸된 후 비동기 콜백이 실행되더라도
        // dangling pointer 역참조를 방지한다.
        auto safeThis = juce::Component::SafePointer<CabinetSelector> (this);
        juce::MessageManager::callAsync ([safeThis]()
        {
            if (safeThis != nullptr)
                safeThis->loadSelectedIR();
        });
    }
}

/**
 * @brief 현재 선택된 IR 인덱스를 읽고 BinaryData에서 로드하여 Cabinet에 전달한다.
 *
 * BinaryData의 WAV 포인터와 사이즈를 Cabinet::loadIRFromBinaryData()에 전달하면,
 * Cabinet이 내부적으로 백그라운드 스레드에서 FFT 컨볼루션을 준비한다.
 * 준비 완료 시 오디오 스레드에 원자적으로 교체된다.
 *
 * **IR 목록**:
 * - 0: 8x10 SVT (BinaryData::ir_8x10_svt_wav)
 * - 1: 4x10 JBL (BinaryData::ir_4x10_jbl_wav)
 * - 2: 1x15 Vintage (BinaryData::ir_1x15_vintage_wav)
 * - 3: 2x12 British (BinaryData::ir_2x12_british_wav)
 * - 4: 2x10 Modern (BinaryData::ir_2x10_modern_wav)
 *
 * @note [메인 스레드] MessageManager::callAsync에서 호출된다.
 *       파일 I/O 없이 BinaryData에서 로드하므로 완전히 안전하다.
 */
void CabinetSelector::loadSelectedIR()
{
    // APVTS에서 현재 선택된 IR 인덱스를 원자적으로 읽는다.
    // ComboBox ID는 1부터이지만, switch 케이스는 0부터이므로 자동 변환됨.
    const int irIndex = static_cast<int> (apvtsRef.getRawParameterValue ("cab_ir")->load());
    auto& cabinet = processorRef.getSignalChain().getCabinet();

    switch (irIndex)
    {
        case 0:  // 8x10 SVT (Ampeg 스타일)
            cabinet.loadIRFromBinaryData (BinaryData::ir_8x10_svt_wav,
                                           BinaryData::ir_8x10_svt_wavSize);
            break;
        case 1:  // 4x10 JBL (Fender Bassman 스타일)
            cabinet.loadIRFromBinaryData (BinaryData::ir_4x10_jbl_wav,
                                           BinaryData::ir_4x10_jbl_wavSize);
            break;
        case 2:  // 1x15 Vintage
            cabinet.loadIRFromBinaryData (BinaryData::ir_1x15_vintage_wav,
                                           BinaryData::ir_1x15_vintage_wavSize);
            break;
        case 3:  // 2x12 British (Orange AD200 스타일)
            cabinet.loadIRFromBinaryData (BinaryData::ir_2x12_british_wav,
                                           BinaryData::ir_2x12_british_wavSize);
            break;
        case 4:  // 2x10 Modern (Markbass 스타일)
            cabinet.loadIRFromBinaryData (BinaryData::ir_2x10_modern_wav,
                                           BinaryData::ir_2x10_modern_wavSize);
            break;
        default:
            // 예외 상황: 기본 IR 로드
            cabinet.loadDefaultIR();
            break;
    }
}

/**
 * @brief 커스텀 IR WAV 파일 브라우저를 열고 선택한 파일을 Cabinet에 로드한다.
 *
 * WAV 파일만 필터링. 선택 완료 시 Cabinet::loadIR(File)로 전달.
 * FileChooser는 비동기로 동작하며 선택 완료 후 콜백에서 처리한다.
 */
void CabinetSelector::openIRFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "커스텀 IR 파일 선택", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav");

    auto safeThis = juce::Component::SafePointer<CabinetSelector> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode |
                               juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            auto result = fc.getResult();
            if (result.existsAsFile())
                safeThis->processorRef.getSignalChain().getCabinet().loadIR (result);
        });
}

/**
 * @brief CabinetSelector 배경 및 "CABINET" 라벨을 그린다.
 *
 * @param g  Graphics 컨텍스트
 * @note [메인 스레드] UI 갱신 중 호출된다.
 */
void CabinetSelector::paint (juce::Graphics& g)
{
    // 배경: 진한 파란-회색 + 둥근 모서리 (6px 반경)
    g.setColour (juce::Colour (0xff222244));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // "CABINET" 라벨: 주황색 텍스트 (13pt, 가운데 정렬)
    g.setColour (juce::Colour (0xffff8800));
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText ("CABINET", 0, 5, getWidth(), 18, juce::Justification::centred, 1);
}

/**
 * @brief IR ComboBox와 Bypass 버튼의 위치와 크기를 계산하여 배치한다.
 *
 * - ComboBox (25px): IR 선택 드롭다운
 * - ToggleButton (22px): Bypass 토글
 */
void CabinetSelector::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (20);  // 라벨 공간

    // IR 선택 ComboBox + Load IR 버튼을 같은 행에 배치
    auto irRow = area.removeFromTop (25);
    loadIRButton.setBounds (irRow.removeFromRight (90));
    irRow.removeFromRight (4);  // 간격
    irCombo.setBounds (irRow);
    area.removeFromTop (5);  // 간격

    // Bypass 토글 버튼 배치
    bypassButton.setBounds (area.removeFromTop (22));
}
