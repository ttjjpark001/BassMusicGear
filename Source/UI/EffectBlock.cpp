#include "EffectBlock.h"

EffectBlock::EffectBlock (const juce::String& name,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& enabledParamId,
                           const juce::StringArray& paramIds,
                           const juce::StringArray& paramLabels)
    : effectName (name),
      knobLabels (paramLabels)
{
    // --- ON/OFF 토글 버튼 ---
    enabledToggle.setButtonText (name);
    // 토글 텍스트: 흰색
    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    // 토글 체크 표시: 주황색 (#ff8800)
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8800));
    addAndMakeVisible (enabledToggle);

    // 토글을 APVTS 파라미터에 바인딩 (자동 상태 동기화)
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, enabledParamId, enabledToggle);

    // --- 파라미터 노브 배열 생성 ---
    for (int i = 0; i < paramIds.size(); ++i)
    {
        const auto& paramId = paramIds[i];
        // 라벨이 없으면 파라미터 ID 그 자체를 라벨로 사용
        const auto& label = i < paramLabels.size() ? paramLabels[i] : paramId;

        // Knob 생성 및 APVTS와 자동 바인딩
        auto* knob = knobs.add (new Knob (label, paramId, apvts));
        addAndMakeVisible (knob);
    }
}

EffectBlock::~EffectBlock() = default;

void EffectBlock::paint (juce::Graphics& g)
{
    // --- 배경 그리기 ---
    // 어두운 배경색(#2a2a3e)으로 이펙터 블록의 영역 표시
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (bounds, 4.0f);

    // --- 테두리 그리기 ---
    // 서브틀한 테두리(#444466)로 시각적 분리 (1px, 모서리 4px 라운드)
    g.setColour (juce::Colour (0xff444466));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void EffectBlock::resized()
{
    auto area = getLocalBounds().reduced (4);  // 내부 패딩 4px

    // --- 좌측: ON/OFF 토글 (고정 80px 너비) ---
    auto toggleArea = area.removeFromLeft (80);
    enabledToggle.setBounds (toggleArea.reduced (2));

    // --- 우측: 파라미터 노브 (남은 공간을 노브 개수로 균등 분할) ---
    if (!knobs.isEmpty())
    {
        const int knobWidth = area.getWidth() / knobs.size();
        for (auto* knob : knobs)
        {
            knob->setBounds (area.removeFromLeft (knobWidth));
        }
    }
}
