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

    // --- 펼침/접힘 콜백 생성 ---
    // 블록 토글 시 상위 Viewport에 높이 변경 통지
    auto expandCb = [this] { if (onHeightChanged) onHeightChanged(); };

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

    delayBlock = std::make_unique<EffectBlock> (
        "Delay", p.apvts, "delay_enabled",
        juce::StringArray { "delay_time", "delay_feedback", "delay_damping", "delay_mix" },
        juce::StringArray { "Time", "Feedback", "Damp", "Mix" });
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

    // --- 신호 체인 전체 높이 합산 (순서: 튜너 → BiAmp → Pre-FX → 앰프 → Post-FX → DIBlend → 캐비닛) ---
    return 42 + 4                           // TunerDisplay(크로매틱 튜너) + 간격
         + biAmpH + 4                       // BiAmpPanel(크로스오버) + 간격
         + fxH (*overdriveBlock)      + 2   // Overdrive(Pre-FX) 이펙트
         + fxH (*octaverBlock)        + 2   // Octaver(Pre-FX) 이펙트
         + fxH (*envelopeFilterBlock) + 4   // EnvelopeFilter(Pre-FX) + 간격
         + 290 + 4                          // AmpPanel(톤스택 + 드라이브) + 간격
         + geqH + 4                         // GraphicEQPanel(10밴드 EQ) + 간격
         + fxH (*chorusBlock)   + 2         // Chorus(Post-FX) 이펙트
         + fxH (*delayBlock)    + 2         // Delay(Post-FX) 이펙트
         + fxH (*reverbBlock)   + 4         // Reverb(Post-FX) + 간격
         + diBlendH + 4                     // DIBlendPanel(클린 DI 혼합) + 간격
         + 95                               // CabinetSelector(스피커 IR 선택, 고정 높이)
         + 10;                              // 하단 여백
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
      content (p)
{
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
    g.drawFittedText ("Phase 6 -- BiAmp + DIBlend + AmpVoicing",
                      0, getHeight() - footerHeight, getWidth(), footerHeight,
                      juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (titleHeight);
    area.removeFromBottom (footerHeight);
    viewport.setBounds (area);
    updateContentSize();
}

void PluginEditor::updateContentSize()
{
    const int w = viewport.getMaximumVisibleWidth();
    const int h = content.calculateNeededHeight();
    content.setSize (w > 0 ? w : getWidth(), h);
}
