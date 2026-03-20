#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// PluginEditor — AudioProcessorEditor UI 루트
//
// Phase 0: 800x500 창, "BassMusicGear - Work in Progress" 텍스트 표시.
// 모든 UI 작업은 메시지 스레드에서만 수행한다.
//==============================================================================
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    //==========================================================================
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    //==========================================================================
    // Component 인터페이스
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==========================================================================
    // PluginProcessor 참조 (에디터 생명주기 동안 유효)
    PluginProcessor& processorRef;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
