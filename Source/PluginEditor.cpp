#include "PluginEditor.h"

//==============================================================================
/**
 * @brief BassMusicGear 플러그인 에디터 생성
 *
 * UI 컴포넌트들을 생성하고 배치하며, 접기/펼치기 콜백을 등록하여
 * 동적 창 높이 조정 시스템을 연결한다.
 *
 * 신호 흐름: 튜너(상단) → 프리앰프/톤 → 캐비닛 → 그래픽EQ → 코러스/딜레이/리버브
 */
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      tunerDisplay (p.getSignalChain().getTuner(), p.apvts),
      ampPanel (p.apvts),
      cabinetSelector (p.apvts, p),
      graphicEQPanel (p.apvts)
{
    // 메인 UI 컴포넌트 추가 (위에서 아래 순서로 배치)
    addAndMakeVisible (tunerDisplay);      // 크로매틱 튜너 (상단 고정, 항상 펼침)
    addAndMakeVisible (ampPanel);          // 앰프 모델 및 톤 컨트롤 (항상 펼침)
    addAndMakeVisible (cabinetSelector);   // 캐비닛 IR 선택 (항상 펼침)

    // GraphicEQ 패널: 접기/펼치기 시 창 높이 동적 조정
    // onExpandChange 콜백을 통해 calculateNeededHeight()를 호출하고 창 크기 업데이트
    graphicEQPanel.onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (graphicEQPanel);    // 10밴드 그래픽 EQ (접힘/펼침 가능)

    // --- Pre-FX 이펙터 블록 (앰프 앞단, 접기/펼치기 가능) ---
    // 신호 흐름: 게이트 → Overdrive → Octaver → EnvelopeFilter → Preamp → [앰프 체인]
    // 각 블록은 수평 배치된 파라미터 노브 + ComboBox 선택지(타입 등)를 포함.
    // 접기/펼치기 시 onExpandChange 콜백이 호출되어 calculateNeededHeight()를 실행.
    // calculateNeededHeight()는 모든 블록의 현재 상태(접힘/펼침)를 읽어 필요한 전체 높이를 계산.
    // 계산된 높이로 setSize()를 호출하여 창을 자동 리사이즈.

    // Overdrive: 웨이브쉐이퍼 타입(Tube/JFET/Fuzz) + Drive/Tone/Blend 노브
    overdriveBlock = std::make_unique<EffectBlock> (
        "Overdrive", p.apvts, "od_enabled",
        juce::StringArray { "od_drive", "od_tone", "od_dry_blend" },
        juce::StringArray { "Drive", "Tone", "Blend" },
        juce::StringArray { "od_type" },
        juce::StringArray { "Type" });
    overdriveBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*overdriveBlock);

    // Octaver: 서브옥타브/옥타브업/드라이 레벨 노브 (피치 추적 기반, ComboBox 없음)
    octaverBlock = std::make_unique<EffectBlock> (
        "Octaver", p.apvts, "oct_enabled",
        juce::StringArray { "oct_sub_level", "oct_up_level", "oct_dry_level" },
        juce::StringArray { "Sub", "Oct-Up", "Dry" });
    octaverBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*octaverBlock);

    // EnvelopeFilter: 엔벨로프 감도/주파수 범위/레조넌스 노브 + Direction ComboBox (Up/Down)
    envelopeFilterBlock = std::make_unique<EffectBlock> (
        "Env Filter", p.apvts, "ef_enabled",
        juce::StringArray { "ef_sensitivity", "ef_freq_min", "ef_freq_max", "ef_resonance" },
        juce::StringArray { "Sens", "Freq Min", "Freq Max", "Reso" },
        juce::StringArray { "ef_direction" },
        juce::StringArray { "Direction" });
    envelopeFilterBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*envelopeFilterBlock);

    // --- Post-FX 이펙터 블록 (앰프 뒷단, 접기/펼치기 가능) ---
    // 신호 흐름: [앰프 체인] → GraphicEQ → Chorus → Delay → Reverb → PowerAmp → DIBlend → 출력
    // GraphicEQ 자체는 위에서 초기화됨 (고정 패널, 동일한 접기/펼치기 콜백 사용).

    // Chorus: LFO 딜레이 모듈레이션 (Rate/Depth/Mix 노브, ComboBox 없음)
    chorusBlock = std::make_unique<EffectBlock> (
        "Chorus", p.apvts, "chorus_enabled",
        juce::StringArray { "chorus_rate", "chorus_depth", "chorus_mix" },
        juce::StringArray { "Rate", "Depth", "Mix" });
    chorusBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*chorusBlock);

    // Delay: 피드백 딜레이/에코 (Time/Feedback/Damping/Mix 노브, ComboBox 없음)
    delayBlock = std::make_unique<EffectBlock> (
        "Delay", p.apvts, "delay_enabled",
        juce::StringArray { "delay_time", "delay_feedback", "delay_damping", "delay_mix" },
        juce::StringArray { "Time", "Feedback", "Damp", "Mix" });
    delayBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*delayBlock);

    // Reverb: 리버브 알고리즘 (타입 ComboBox: Spring/Room/Hall/Plate + Size/Decay/Mix 노브)
    reverbBlock = std::make_unique<EffectBlock> (
        "Reverb", p.apvts, "reverb_enabled",
        juce::StringArray { "reverb_size", "reverb_decay", "reverb_mix" },
        juce::StringArray { "Size", "Decay", "Mix" },
        juce::StringArray { "reverb_type" },
        juce::StringArray { "Type" });
    reverbBlock->onExpandChange = [this] { setSize (getWidth(), calculateNeededHeight()); };
    addAndMakeVisible (*reverbBlock);

    // 초기 창 크기: 800px 너비 × calculateNeededHeight()로 계산된 높이
    // 모든 EffectBlock/GraphicEQPanel의 현재 상태(초기값: 모두 펼침)에 기반하여 필요한 높이를 계산.
    setSize (800, calculateNeededHeight());
}

PluginEditor::~PluginEditor() = default;

/**
 * @brief 에디터의 배경과 정적 텍스트(타이틀, Phase 표시)를 그린다.
 *
 * - 배경: 다크 네이비 색상 (모든 자식 컴포넌트의 뒷배경)
 * - 타이틀: "BassMusicGear" (상단, 하얀색, 굵은체, 24pt)
 * - Phase: "Phase 5 -- GEQ + Post-FX" (하단, 회색, 11pt)
 */
void PluginEditor::paint (juce::Graphics& g)
{
    // 배경 색상: 다크 네이비
    g.fillAll (juce::Colour (0xff1a1a2e));

    // 타이틀: "BassMusicGear" (상단, 하얀색, 굵은체)
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 5, getWidth(), 30,
                      juce::Justification::centred, 1);

    // 하단 텍스트: Phase 표시 (회색, 현재 버전/기능 상태)
    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 5 -- GEQ + Post-FX",
                      0, getHeight() - 20, getWidth(), 20,
                      juce::Justification::centred, 1);
}

/**
 * @brief 모든 EffectBlock과 GraphicEQPanel의 현재 상태(접힘/펼침)를 기반으로
 *        플러그인 에디터가 필요한 전체 높이를 계산한다.
 *
 * 높이 구성:
 * - 고정: 타이틀, 하단 Phase 텍스트, TunerDisplay, AmpPanel, CabinetSelector
 * - 동적: Pre-FX 블록(Overdrive/Octaver/EnvelopeFilter), GraphicEQ, Post-FX 블록(Chorus/Delay/Reverb)
 *
 * EffectBlock.expandedHeight = 130px (펼침), EffectBlock.collapsedHeight = 36px (접힘)
 * GraphicEQPanel.expandedHeight = 220px (펼침), GraphicEQPanel.collapsedHeight = 36px (접힘)
 *
 * @return  에디터가 표시해야 할 전체 높이 (픽셀)
 *
 * @note [메인 스레드] EffectBlock 또는 GraphicEQPanel의 접기/펼치기 버튼 클릭 시
 *       onExpandChange 콜백을 통해 호출되고, setSize()로 창을 리사이즈한다.
 */
int PluginEditor::calculateNeededHeight() const
{
    // 람다: EffectBlock의 현재 상태(접힘/펼침)에 따른 높이를 반환
    auto fxH = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    // 전체 창 높이 = 고정 요소 + 동적 Pre-FX 블록 + 고정 중앙부 + 동적 Post-FX 블록 + 하단 패딩
    return 35                              // 타이틀 "BassMusicGear" (35px)
         + 22                              // 하단 Phase 표시 "Phase 5 -- GEQ + Post-FX" (22px)
         + 42 + 4                          // TunerDisplay(42px) + 갭(4px)
         + fxH (*overdriveBlock)     + 2   // Overdrive (접힘36/펼침130px) + 갭(2px)
         + fxH (*octaverBlock)       + 2   // Octaver (접힘36/펼침130px) + 갭(2px)
         + fxH (*envelopeFilterBlock) + 6  // EnvelopeFilter (접힘36/펼침130px) + 갭(6px)
         + 290 + 4                         // AmpPanel(290px 고정, 톤스택 노브 배치) + 갭(4px)
         + (graphicEQPanel.getExpanded() ? GraphicEQPanel::expandedHeight : GraphicEQPanel::collapsedHeight) + 4
                                          // GraphicEQ (접힘36/펼침220px) + 갭(4px)
         + fxH (*chorusBlock)   + 2        // Chorus (접힘36/펼침130px) + 갭(2px)
         + fxH (*delayBlock)    + 2        // Delay (접힘36/펼침130px) + 갭(2px)
         + fxH (*reverbBlock)   + 6        // Reverb (접힘36/펼침130px) + 갭(6px)
         + 95                              // CabinetSelector(IR 선택, 95px 고정)
         + 10;                             // 상하 마진(10px)
}

/**
 * @brief 에디터의 모든 자식 컴포넌트들의 위치와 크기를 결정한다.
 *
 * calculateNeededHeight()로 계산된 창 높이에 맞춰
 * 각 블록을 위에서 아래 순서로 배치한다.
 * 각 EffectBlock과 GraphicEQPanel의 getExpanded() 상태를 읽어
 * 그에 해당하는 높이(expandedHeight 또는 collapsedHeight)를 할당한다.
 *
 * 배치 순서:
 * 타이틀 → TunerDisplay → Pre-FX(Overdrive/Octaver/EnvelopeFilter) →
 * AmpPanel → GraphicEQ → Post-FX(Chorus/Delay/Reverb) → CabinetSelector → Phase 표시
 */
void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (5);
    area.removeFromTop (35);   // 타이틀 영역 제거
    area.removeFromBottom (22); // 하단 Phase 표시 영역 제거

    // 튜너: 상단 고정 (항상 펼침)
    tunerDisplay.setBounds (area.removeFromTop (42));
    area.removeFromTop (4);  // 갭

    // Pre-FX 블록 (앰프 앞단) — 접힘/펼침 상태에 따라 높이 동적 계산
    // 각 블록은 접힘 상태(36px)면 헤더만 표시, 펼침 상태(130px)면 파라미터 노브 표시
    auto preFxHeight = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    overdriveBlock->setBounds (area.removeFromTop (preFxHeight (*overdriveBlock)));
    area.removeFromTop (2);  // 블록 간 갭
    octaverBlock->setBounds (area.removeFromTop (preFxHeight (*octaverBlock)));
    area.removeFromTop (2);
    envelopeFilterBlock->setBounds (area.removeFromTop (preFxHeight (*envelopeFilterBlock)));
    area.removeFromTop (6);  // Pre-FX와 AmpPanel 사이 더 큰 갭

    // 앰프 패널: 톤스택 컨트롤 (항상 펼침)
    ampPanel.setBounds (area.removeFromTop (290));
    area.removeFromTop (4);  // 갭

    // GraphicEQ 패널: 접힘/펼침 상태에 따라 동적 높이 (36px/220px)
    const int geqHeight = graphicEQPanel.getExpanded() ? GraphicEQPanel::expandedHeight : GraphicEQPanel::collapsedHeight;
    graphicEQPanel.setBounds (area.removeFromTop (geqHeight));
    area.removeFromTop (4);

    // Post-FX 블록 (앰프 뒷단) — 접힘/펼침 상태에 따라 높이 동적 계산
    auto postFxHeight = [](const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    chorusBlock->setBounds (area.removeFromTop (postFxHeight (*chorusBlock)));
    area.removeFromTop (2);
    delayBlock->setBounds (area.removeFromTop (postFxHeight (*delayBlock)));
    area.removeFromTop (2);
    reverbBlock->setBounds (area.removeFromTop (postFxHeight (*reverbBlock)));
    area.removeFromTop (6);

    // 캐비닛 선택 (IR 로드, 항상 펼침)
    // 좌우 마진 200px으로 중앙 정렬 (너비 800px일 때 400px 가운데 배치)
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}
