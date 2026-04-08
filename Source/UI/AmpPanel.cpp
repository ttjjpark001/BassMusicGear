/**
 * @brief AmpPanel 생성자: 앰프 모델별 톤스택 및 드라이브 노브를 초기화한다.
 *
 * 각 Knob은 생성 시점에 APVTS 파라미터에 자동 바인딩되어
 * UI 변경 ↔ 오디오 스레드 간 값 동기화가 이루어진다.
 *
 * @param apvts  AudioProcessorValueTreeState 참조
 * @note [메인 스레드] PluginEditor 생성 시 호출됨
 */
#include "AmpPanel.h"

AmpPanel::AmpPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts),
      inputGainKnob ("Input Gain", "input_gain", apvts),
      volumeKnob    ("Volume",     "volume",     apvts),
      bassKnob      ("Bass",       "bass",       apvts),
      midKnob       ("Mid",        "mid",        apvts),
      trebleKnob    ("Treble",     "treble",     apvts),
      driveKnob     ("Drive",      "drive",      apvts),
      presenceKnob  ("Presence",   "presence",   apvts),
      sagKnob       ("Sag",        "sag",        apvts),
      vpfKnob       ("VPF",        "vpf",        apvts),
      vleKnob       ("VLE",        "vle",        apvts),
      gruntKnob     ("Grunt",      "grunt",      apvts),
      attackKnob    ("Attack",     "attack",     apvts)
{
    // --- 앰프 모델 선택 콤보박스 초기화 ---
    // AmpModelLibrary::getModelNames()에서 6종 앰프 이름 배열을 가져와 추가
    // ComboBox 항목 ID는 1부터 시작 (JUCE 관례: 0은 예약)
    auto names = AmpModelLibrary::getModelNames();
    for (int i = 0; i < names.size(); ++i)
        modelCombo.addItem (names[i], i + 1);
    modelCombo.setSelectedId (2);  // ID 2 = "Tweed Bass" (인덱스 1) 기본값
    addAndMakeVisible (modelCombo);
    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "amp_model", modelCombo);

    // --- 공통 노브 (모든 앰프 모델에서 보임) ---
    // Input Gain(입력 게인) + Volume(마스터 볼륨) + Bass/Mid/Treble(톤스택) + Drive + Presence(파워앰프)
    addAndMakeVisible (inputGainKnob);
    addAndMakeVisible (volumeKnob);
    addAndMakeVisible (bassKnob);
    addAndMakeVisible (midKnob);
    addAndMakeVisible (trebleKnob);
    addAndMakeVisible (driveKnob);
    addAndMakeVisible (presenceKnob);

    // --- Sag 노브 ---
    // 튜브 앰프(American Vintage, Tweed Bass, British Stack)에서만 표시
    // ClassD/SolidState 앰프에서는 sagEnabled=false이므로 숨김
    addAndMakeVisible (sagKnob);

    // --- American Vintage 전용: Mid Position 콤보박스 ---
    // Baxandall 톤스택의 미드 밴드 중심주파수를 250Hz~3kHz에서 선택
    // IIR 계수 재계산이 발생하므로 선택 변경 시 updateMidPosition() 호출
    midPositionCombo.addItem ("250 Hz",  1);
    midPositionCombo.addItem ("500 Hz",  2);
    midPositionCombo.addItem ("800 Hz",  3);
    midPositionCombo.addItem ("1.5 kHz", 4);
    midPositionCombo.addItem ("3 kHz",   5);
    addAndMakeVisible (midPositionCombo);
    midPositionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "mid_position", midPositionCombo);

    midPositionLabel.setText ("Mid Freq", juce::dontSendNotification);
    midPositionLabel.setFont (juce::FontOptions (11.0f));
    midPositionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    midPositionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (midPositionLabel);

    // --- Italian Clean 전용: VPF / VLE ---
    // VPF(Variable Pre-shape Filter): 미드 스쿱 (380Hz 노치 + 35Hz/10kHz 부스트)
    // VLE(Vintage Loudspeaker Emulator): 가변 로우패스 고역 롤오프
    // 독립적으로 작동, 세부 사항은 ToneStack::updateMarkbassExtras() 참조
    addAndMakeVisible (vpfKnob);
    addAndMakeVisible (vleKnob);

    // --- Modern Micro 전용: Grunt / Attack ---
    // Grunt: 비선형 왜곡 깊이, Attack: 반응 속도 (BaxandallGrunt 특화)
    addAndMakeVisible (gruntKnob);
    addAndMakeVisible (attackKnob);

    // --- amp_model 파라미터 리스너 등록 ---
    // 사용자가 모델 콤보박스를 선택하면 parameterChanged() 콜백 실행
    // → 모든 노브의 가시성을 동적으로 업데이트 (모델별 UI 재구성)
    apvts.addParameterListener ("amp_model", this);

    // 초기 가시성 설정 (Tweed Bass 모델 기본값에 맞춰 노브 표시/숨김)
    updateVisibility();
}

AmpPanel::~AmpPanel()
{
    // --- 파라미터 리스너 제거 (dangling 포인터 방지) ---
    // AmpPanel 소멸 후 APVTS의 콜백이 실행되지 않도록 사전 제거
    apvtsRef.removeParameterListener ("amp_model", this);
}

/**
 * @brief APVTS 파라미터 변경 콜백 (리스너 인터페이스)
 *
 * "amp_model" 파라미터가 바뀌면 호출되어 가시성을 동적으로 갱신한다.
 * 메인 스레드(메시지 큐)에서 비동기로 updateVisibility() 실행.
 *
 * @param parameterID  변경된 파라미터 ID ("amp_model" 확인)
 * @param newValue     미사용 (APVTS에서 직접 모델 ID를 읽음)
 * @note [메인 스레드] MessageManager::callAsync로 콜백 스케줄
 */
void AmpPanel::parameterChanged (const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID == "amp_model")
    {
        // 모델 전환 시 UI 색상 변경 (themeColour 적용)
        repaint();

        // SafePointer: AmpPanel이 소멸된 후 비동기 콜백이 실행되더라도
        // dangling pointer 역참조를 방지한다.
        auto safeThis = juce::Component::SafePointer<AmpPanel> (this);
        juce::MessageManager::callAsync ([safeThis]()
        {
            if (safeThis == nullptr)
                return;

            safeThis->updateVisibility();   // 노브 가시성 갱신
            safeThis->resized();            // 레이아웃 재계산
            safeThis->repaint();            // 배경 재그리기
        });
    }
}

/**
 * @brief 현재 앰프 모델에 맞춰 모든 노브 가시성을 갱신한다.
 *
 * 선택된 앰프 모델의 톤스택 타입과 Sag 활성화 여부를 확인하여
 * 모델별 특화 노브만 보이도록 한다:
 * - Sag: 모든 튜브 모델 (American Vintage, Tweed Bass, British Stack)
 * - Mid Position: American Vintage (Baxandall 톤스택)
 * - VPF/VLE: Italian Clean (MarkbassFourBand 톤스택)
 * - Grunt/Attack: Modern Micro (BaxandallGrunt 톤스택)
 */
void AmpPanel::updateVisibility()
{
    // APVTS에서 현재 선택된 앰프 모델 인덱스를 원자적으로 읽는다.
    // load(): atomic<float> 값을 int로 변환
    const int modelIndex = static_cast<int> (apvtsRef.getRawParameterValue ("amp_model")->load());
    const auto& model = AmpModelLibrary::getModel (modelIndex);

    // --- Sag 노브: sagEnabled=true인 모델만 표시 ---
    // 튜브 파워앰프 모델(Tube6550=American Vintage/Tweed, TubeEL34=British)에서만
    // 출력 트랜스 전압 새깅을 시뮬레이션한다. ClassD/SolidState 모델은 숨김.
    sagKnob.setVisible (model.sagEnabled);

    // --- Mid Position 콤보박스: American Vintage (Baxandall) 전용 ---
    // Baxandall 톤스택의 미드 밴드를 250Hz ~ 3kHz 사이에서 선택 가능
    bool isAmerican = (model.toneStack == ToneStackType::Baxandall);
    midPositionCombo.setVisible (isAmerican);
    midPositionLabel.setVisible (isAmerican);

    // --- VPF/VLE 노브: Italian Clean (MarkbassFourBand) 전용 ---
    // VPF(미드 스쿱 필터)와 VLE(고역 롤오프 로우패스)
    bool isItalian = (model.toneStack == ToneStackType::MarkbassFourBand);
    vpfKnob.setVisible (isItalian);
    vleKnob.setVisible (isItalian);

    // --- Grunt/Attack 노브: Modern Micro (BaxandallGrunt) 전용 ---
    // Baxandall에 Grunt(깊이) 레이어를 추가한 토폴로지
    bool isModern = (model.toneStack == ToneStackType::BaxandallGrunt);
    gruntKnob.setVisible (isModern);
    attackKnob.setVisible (isModern);
}

/**
 * @brief AmpPanel 배경 및 섹션 라벨을 그린다.
 *
 * 신호 체인의 3개 주요 섹션을 시각적으로 구분하는 라벨을 표시한다.
 *
 * @param g  Graphics 컨텍스트
 * @note [메인 스레드] UI 갱신 중 호출된다.
 */
void AmpPanel::paint (juce::Graphics& g)
{
    // 배경: 진한 파란-회색 + 둥근 모서리 (6px 반경)
    g.setColour (juce::Colour (0xff222244));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // 섹션 라벨: 현재 앰프 모델의 themeColour 적용
    const int modelIndex = static_cast<int> (apvtsRef.getRawParameterValue ("amp_model")->load());
    const auto& model = AmpModelLibrary::getModel (modelIndex);
    g.setColour (model.themeColour);
    g.setFont (juce::FontOptions (14.0f));

    // 신호 체인 3개 섹션 라벨
    g.drawFittedText ("PREAMP", 10, 35, 190, 20, juce::Justification::centred, 1);
    g.drawFittedText ("TONE STACK", 210, 35, 300, 20, juce::Justification::centred, 1);
    g.drawFittedText ("POWER AMP", 520, 35, 260, 20, juce::Justification::centred, 1);
}

/**
 * @brief 모든 노브 및 콘트롤의 위치와 크기를 계산하여 배치한다.
 *
 * **첫 줄 (rowY)**:
 * - PREAMP (0~160px): Input Gain, Volume
 * - TONE STACK (210~510px): Bass, Mid, Treble
 * - POWER AMP (520~800px): Drive, Presence, [Sag (튜브만)]
 *
 * **둘째 줄 (row2Y)** — 모델별 특화 컨트롤:
 * - American Vintage: Mid Position 콤보박스
 * - Italian Clean: VPF, VLE 노브
 * - Modern Micro: Grunt, Attack 노브
 */
void AmpPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    // --- 상단: 앰프 모델 선택 콤보박스 ---
    modelCombo.setBounds (area.removeFromTop (25).reduced (50, 0));
    area.removeFromTop (10);  // 여백

    const int knobW = 80;       // 각 노브 너비
    const int knobH = 95;       // 각 노브 높이 (텍스트 라벨 포함)
    const int gap = 5;          // 노브 사이 간격
    const int rowY = area.getY() + 25;  // 첫 줄 노브 Y 좌표

    // --- PREAMP 섹션: Input Gain, Volume ---
    int x = 15;
    inputGainKnob.setBounds (x, rowY, knobW, knobH);
    x += knobW + gap;
    volumeKnob.setBounds (x, rowY, knobW, knobH);

    // --- TONE STACK 섹션: Bass, Mid, Treble ---
    x = 215;
    bassKnob.setBounds (x, rowY, knobW, knobH);
    x += knobW + gap;
    midKnob.setBounds (x, rowY, knobW, knobH);
    x += knobW + gap;
    trebleKnob.setBounds (x, rowY, knobW, knobH);

    // --- POWER AMP 섹션: Drive, Presence, [Sag] ---
    x = 525;
    driveKnob.setBounds (x, rowY, knobW, knobH);
    x += knobW + gap;
    presenceKnob.setBounds (x, rowY, knobW, knobH);
    x += knobW + gap;
    // Sag는 튜브 모델에서만 표시
    if (sagKnob.isVisible())
        sagKnob.setBounds (x, rowY, knobW, knobH);

    // --- 둘째 줄: 모델별 특화 컨트롤 ---
    const int row2Y = rowY + knobH + 10;

    // American Vintage 전용: Mid Position 콤보박스
    if (midPositionCombo.isVisible())
    {
        midPositionLabel.setBounds (300, row2Y, 80, 15);
        midPositionCombo.setBounds (295, row2Y + 16, 90, 22);
    }

    // Italian Clean 전용: VPF / VLE 노브
    if (vpfKnob.isVisible())
    {
        vpfKnob.setBounds (215, row2Y, knobW, knobH);
        vleKnob.setBounds (215 + knobW + gap, row2Y, knobW, knobH);
    }

    // Modern Micro 전용: Grunt / Attack 노브
    if (gruntKnob.isVisible())
    {
        gruntKnob.setBounds (215, row2Y, knobW, knobH);
        attackKnob.setBounds (215 + knobW + gap, row2Y, knobW, knobH);
    }
}
