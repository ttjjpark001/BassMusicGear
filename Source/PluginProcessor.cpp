#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const  { return false; }
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }

double PluginProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
int PluginProcessor::getNumPrograms()                              { return 1; }
int PluginProcessor::getCurrentProgram()                           { return 0; }
void PluginProcessor::setCurrentProgram (int)                      {}
const juce::String PluginProcessor::getProgramName (int)           { return {}; }
void PluginProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void PluginProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Phase 0: 아무 DSP 모듈도 없으므로 지연 샘플 0으로 설정
    setLatencySamples (0);
}

void PluginProcessor::releaseResources()
{
    // Phase 0: 해제할 리소스 없음
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 입력: 모노, 출력: 스테레오 또는 모노 허용
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Phase 0: 신호 체인 없음 — 버퍼를 무음으로 클리어
    buffer.clear();
}

//==============================================================================
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

bool PluginProcessor::hasEditor() const { return true; }

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // APVTS 전체 상태를 XML 로 직렬화
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // XML 에서 APVTS 상태 복원
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessor::createParameterLayout()
{
    // Phase 0: 파라미터 없는 빈 레이아웃
    // 이후 Phase 에서 파라미터가 추가된다
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    return { params.begin(), params.end() };
}

//==============================================================================
// 이 매크로는 JUCE 가 플러그인 팩토리를 생성하는 데 필요하다
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
