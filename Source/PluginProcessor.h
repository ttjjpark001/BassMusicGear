#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
// PluginProcessor — AudioProcessor 진입점
//
// Phase 0: 빈 프로세서. APVTS 빈 레이아웃 포함.
// processBlock 은 아무 처리도 하지 않는다.
//==============================================================================
class PluginProcessor final : public juce::AudioProcessor
{
public:
    //==========================================================================
    PluginProcessor();
    ~PluginProcessor() override;

    //==========================================================================
    // AudioProcessor 인터페이스
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    // Plugin 정보
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    // 프로그램 (프리셋) — Phase 0 에서는 단일 프로그램만 사용
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    // 상태 저장/복원
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // APVTS — 파라미터 트리
    juce::AudioProcessorValueTreeState apvts;

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
