#include "PluginEditor.h"

// =============================================================================
// ContentComponent
// =============================================================================

ContentComponent::ContentComponent (PluginProcessor& p)
    : tunerDisplay (p.getSignalChain().getTuner(), p.apvts),
      ampPanel (p.apvts),
      cabinetSelector (p.apvts, p),
      biAmpPanel (p.apvts),
      graphicEQPanel (p.apvts),
      diBlendPanel (p.apvts)
{
    // --- 신호 체인 시작: 튜너 → 앰프 패널 → 캐비닛 선택 ---
    addAndMakeVisible (tunerDisplay);
    addAndMakeVisible (ampPanel);
    addAndMakeVisible (cabinetSelector);

    // --- 펼침/접힘 콜백 정의 ---
    // 이펙터 블록의 상태가 변경될 때마다 호출되는 콜백.
    // 블록의 높이가 변경되면 상위 Viewport에 알림을 보내
    // 전체 콘텐츠 높이를 재계산하고 스크롤 영역을 업데이트한다.
    auto expandCb = [this] { if (onHeightChanged) onHeightChanged(); };

    // --- NoiseGate EffectBlock (신호 체인 최선두) ---
    // 4개 파라미터: Threshold, Attack, Hold, Release
    // 신호 체인의 첫 번째 블록으로 매우 조용한 신호를 차단한다.
    // Threshold 이하의 신호는 완전히 뮤트되어 배경 노이즈(허밍, 누수음)를 제거한다.
    noiseGateBlock = std::make_unique<EffectBlock> (
        "Noise Gate", p.apvts, "gate_enabled",
        juce::StringArray { "gate_threshold", "gate_attack", "gate_hold", "gate_release" },
        juce::StringArray { "Threshold", "Attack", "Hold", "Release" });
    noiseGateBlock->onExpandChange = expandCb;
    addAndMakeVisible (*noiseGateBlock);

    // --- Compressor EffectBlock (NoiseGate 다음) ---
    // 6개 파라미터: Threshold, Ratio, Attack, Release, Makeup Gain, Dry Blend
    // 신호 레벨을 자동으로 제어하여 큰 다이나믹 레인지를 일정 범위로 압축한다.
    // Dry Blend로 컴프레서 원본 신호와 처리 신호를 섞어 음감의 자연스러움을 조절한다.
    compressorBlock = std::make_unique<EffectBlock> (
        "Compressor", p.apvts, "comp_enabled",
        juce::StringArray { "comp_threshold", "comp_ratio", "comp_attack",
                            "comp_release", "comp_makeup", "comp_dry_blend" },
        juce::StringArray { "Thresh", "Ratio", "Attack", "Release", "Makeup", "Blend" });
    compressorBlock->onExpandChange = expandCb;
    addAndMakeVisible (*compressorBlock);

    // --- 그래픽 EQ 패널 (10밴드 Constant-Q EQ) ---
    graphicEQPanel.onExpandChange = expandCb;
    addAndMakeVisible (graphicEQPanel);

    // --- Bi-Amp 패널 (크로스오버 설정) ---
    biAmpPanel.onExpandChange = expandCb;
    addAndMakeVisible (biAmpPanel);

    // --- 클린 DI + 처리 신호 혼합 패널 ---
    diBlendPanel.onExpandChange = expandCb;
    addAndMakeVisible (diBlendPanel);

    overdriveBlock = std::make_unique<EffectBlock> (
        "Overdrive", p.apvts, "od_enabled",
        juce::StringArray { "od_drive", "od_tone", "od_dry_blend" },
        juce::StringArray { "Drive", "Tone", "Blend" },
        juce::StringArray { "od_type" },
        juce::StringArray { "Type" });
    overdriveBlock->onExpandChange = expandCb;
    addAndMakeVisible (*overdriveBlock);

    octaverBlock = std::make_unique<EffectBlock> (
        "Octaver", p.apvts, "oct_enabled",
        juce::StringArray { "oct_sub_level", "oct_up_level", "oct_dry_level" },
        juce::StringArray { "Sub", "Oct-Up", "Dry" });
    octaverBlock->onExpandChange = expandCb;
    addAndMakeVisible (*octaverBlock);

    envelopeFilterBlock = std::make_unique<EffectBlock> (
        "Env Filter", p.apvts, "ef_enabled",
        juce::StringArray { "ef_sensitivity", "ef_freq_min", "ef_freq_max", "ef_resonance" },
        juce::StringArray { "Sens", "Freq Min", "Freq Max", "Reso" },
        juce::StringArray { "ef_direction" },
        juce::StringArray { "Direction" });
    envelopeFilterBlock->onExpandChange = expandCb;
    addAndMakeVisible (*envelopeFilterBlock);

    chorusBlock = std::make_unique<EffectBlock> (
        "Chorus", p.apvts, "chorus_enabled",
        juce::StringArray { "chorus_rate", "chorus_depth", "chorus_mix" },
        juce::StringArray { "Rate", "Depth", "Mix" });
    chorusBlock->onExpandChange = expandCb;
    addAndMakeVisible (*chorusBlock);

    // --- Delay EffectBlock (Post-FX) ---
    // 4개 노브 파라미터: Time, Feedback, Damping, Mix
    // 1개 ComboBox 파라미터: Note Value (1/4, 1/8, 1/8 점, 1/16, 1/4 삼연음)
    // 1개 추가 토글 파라미터: BPM Sync (수동 시간 vs BPM 자동 계산 선택)
    //
    // Time: 딜레이 시간 (10~2000ms)
    // Feedback: 반복 에코 강도 (0~0.95, 1.0 이상 발산)
    // Damping: 피드백 경로 고음 감쇠 (0=밝음, 1=어두움)
    // Mix: 원본/딜레이 블렌드 (0=원본만, 1=딜레이만)
    // BPM Sync: ON 시 DAW BPM에 따라 딜레이 시간 자동 계산
    delayBlock = std::make_unique<EffectBlock> (
        "Delay", p.apvts, "delay_enabled",
        juce::StringArray { "delay_time", "delay_feedback", "delay_damping", "delay_mix" },
        juce::StringArray { "Time", "Feedback", "Damp", "Mix" },
        juce::StringArray { "delay_note_value" },
        juce::StringArray { "Note" },
        juce::StringArray { "delay_bpm_sync" },
        juce::StringArray { "BPM Sync" });
    delayBlock->onExpandChange = expandCb;
    addAndMakeVisible (*delayBlock);

    reverbBlock = std::make_unique<EffectBlock> (
        "Reverb", p.apvts, "reverb_enabled",
        juce::StringArray { "reverb_size", "reverb_decay", "reverb_mix" },
        juce::StringArray { "Size", "Decay", "Mix" },
        juce::StringArray { "reverb_type" },
        juce::StringArray { "Type" });
    reverbBlock->onExpandChange = expandCb;
    addAndMakeVisible (*reverbBlock);
}

int ContentComponent::calculateNeededHeight() const
{
    // --- 가변 높이 이펙트 블록 헬퍼 함수 ---
    // 접힘 상태 vs 펼침 상태에 따라 높이를 다르게 반환
    auto fxH = [] (const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    // --- 각 패널의 현재 높이 조회 ---
    const int biAmpH   = biAmpPanel.getExpanded()   ? BiAmpPanel::expandedHeight   : BiAmpPanel::collapsedHeight;
    const int diBlendH = diBlendPanel.getExpanded()  ? DIBlendPanel::expandedHeight : DIBlendPanel::collapsedHeight;
    const int geqH     = graphicEQPanel.getExpanded() ? GraphicEQPanel::expandedHeight : GraphicEQPanel::collapsedHeight;

    // --- 신호 체인 전체 높이 합산 ---
    // UI 배치 순서: 튜너 → 신호 체인(NoiseGate→Compressor→BiAmp→Pre-FX→앰프→GEQ→Post-FX) → DIBlend → 캐비닛
    // 각 블록의 높이는 접힘/펼침 상태에 따라 동적으로 결정된다.
    // 이 합계가 ContentComponent의 필요 높이가 되며,
    // Viewport는 이 높이에 따라 스크롤 바를 자동으로 조정한다.
    return 42 + 4                           // TunerDisplay(크로매틱 튜너) + 간격(4px)
         + fxH (*noiseGateBlock)  + 2       // NoiseGate(신호 체인 최선두) + 간격
         + fxH (*compressorBlock) + 4       // Compressor(NoiseGate 다음) + 간격
         + biAmpH + 4                       // BiAmpPanel(크로스오버: LP/HP 분할) + 간격
         + fxH (*overdriveBlock)      + 2   // Overdrive(Pre-FX: 최초 왜곡 스테이지)
         + fxH (*octaverBlock)        + 2   // Octaver(Pre-FX: 서브/옥타브 생성)
         + fxH (*envelopeFilterBlock) + 4   // EnvelopeFilter(Pre-FX: 동적 필터) + 간격
         + 290 + 4                          // AmpPanel(톤스택 + 드라이브/프레전스/새그) + 간격
         + geqH + 4                         // GraphicEQPanel(10밴드 Constant-Q EQ) + 간격
         + fxH (*chorusBlock)   + 2         // Chorus(Post-FX: 모듈레이션)
         + fxH (*delayBlock)    + 2         // Delay(Post-FX: 딜레이/에코 + BPM Sync)
         + fxH (*reverbBlock)   + 4         // Reverb(Post-FX: 알고리즘 리버브) + 간격
         + diBlendH + 4                     // DIBlendPanel(클린DI + 처리신호 블렌드) + 간격
         + 95                               // CabinetSelector(스피커 IR 선택, 고정 높이)
         + 10;                              // 하단 여백(스크롤 편의성)
}

void ContentComponent::resized()
{
    auto area = getLocalBounds().reduced (5);

    auto fxH = [] (const EffectBlock& b) {
        return b.getExpanded() ? EffectBlock::expandedHeight : EffectBlock::collapsedHeight;
    };

    // TunerDisplay
    tunerDisplay.setBounds (area.removeFromTop (42));
    area.removeFromTop (4);

    // NoiseGate (신호 체인 선두)
    noiseGateBlock->setBounds (area.removeFromTop (fxH (*noiseGateBlock)));
    area.removeFromTop (2);

    // Compressor (NoiseGate 다음)
    compressorBlock->setBounds (area.removeFromTop (fxH (*compressorBlock)));
    area.removeFromTop (4);

    // BiAmp (Pre-FX 위)
    const int biAmpH = biAmpPanel.getExpanded() ? BiAmpPanel::expandedHeight : BiAmpPanel::collapsedHeight;
    biAmpPanel.setBounds (area.removeFromTop (biAmpH));
    area.removeFromTop (4);

    // Pre-FX
    overdriveBlock->setBounds (area.removeFromTop (fxH (*overdriveBlock)));
    area.removeFromTop (2);
    octaverBlock->setBounds (area.removeFromTop (fxH (*octaverBlock)));
    area.removeFromTop (2);
    envelopeFilterBlock->setBounds (area.removeFromTop (fxH (*envelopeFilterBlock)));
    area.removeFromTop (4);

    // Amp panel
    ampPanel.setBounds (area.removeFromTop (290));
    area.removeFromTop (4);

    // GraphicEQ
    const int geqH = graphicEQPanel.getExpanded() ? GraphicEQPanel::expandedHeight : GraphicEQPanel::collapsedHeight;
    graphicEQPanel.setBounds (area.removeFromTop (geqH));
    area.removeFromTop (4);

    // Post-FX
    chorusBlock->setBounds (area.removeFromTop (fxH (*chorusBlock)));
    area.removeFromTop (2);
    delayBlock->setBounds (area.removeFromTop (fxH (*delayBlock)));
    area.removeFromTop (2);
    reverbBlock->setBounds (area.removeFromTop (fxH (*reverbBlock)));
    area.removeFromTop (4);

    // DIBlend
    const int diBlendH = diBlendPanel.getExpanded() ? DIBlendPanel::expandedHeight : DIBlendPanel::collapsedHeight;
    diBlendPanel.setBounds (area.removeFromTop (diBlendH));
    area.removeFromTop (4);

    // Cabinet selector
    cabinetSelector.setBounds (area.removeFromTop (95).reduced (200, 0));
}

// =============================================================================
// PluginEditor
// =============================================================================

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      presetPanel (p),
      content (p)
{
    // --- Phase 8: 프리셋 패널 (헤더 영역) ---
    addAndMakeVisible (presetPanel);

    // --- Phase 8: Active/Passive 입력 패드 토글 ---
    // Passive (기본, 토글 꺼짐) vs Active (-10dB, 토글 켜짐)
    // Active 선택 시 강조색(붉은 계열) 틱 표시
    inputActiveToggle.setButtonText ("Active");
    inputActiveToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    inputActiveToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff3344));
    inputActiveToggle.setTooltip ("Active pickup (-10 dB input pad)");
    addAndMakeVisible (inputActiveToggle);

    inputActiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "input_active", inputActiveToggle);

    // ContentComponent 높이 변경 시 Viewport 콘텐츠 크기 갱신
    content.onHeightChanged = [this] { updateContentSize(); };

    // Viewport 설정: 세로 스크롤만 활성화
    viewport.setScrollBarsShown (true, false);
    viewport.setViewedComponent (&content, false); // false = Viewport가 content를 소유하지 않음
    addAndMakeVisible (viewport);

    setSize (800, fixedHeight);
    updateContentSize();
}

PluginEditor::~PluginEditor()
{
    // Viewport가 content를 소유하지 않으므로 먼저 분리
    viewport.setViewedComponent (nullptr, false);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawFittedText ("BassMusicGear", 0, 5, getWidth(), 30,
                      juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff666688));
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("Phase 8 -- Presets + Active/Passive + GEQ User Presets",
                      0, getHeight() - footerHeight, getWidth(), footerHeight,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (titleHeight);
    area.removeFromBottom (footerHeight);

    // Phase 8 헤더: PresetPanel + Active/Passive 토글 (좌: preset, 우: 토글)
    auto headerRow = area.removeFromTop (presetBarHeight).reduced (4, 4);
    auto toggleArea = headerRow.removeFromRight (80);
    inputActiveToggle.setBounds (toggleArea);
    headerRow.removeFromRight (6);
    presetPanel.setBounds (headerRow);

    viewport.setBounds (area);
    updateContentSize();
}

void PluginEditor::updateContentSize()
{
    const int w = viewport.getMaximumVisibleWidth();
    const int h = content.calculateNeededHeight();
    content.setSize (w > 0 ? w : getWidth(), h);
}
