#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    // 버스 레이아웃: 베이스 기타(모노 1ch) 입력, 스테레오 2ch 출력
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      // APVTS 초기화: 파라미터 레이아웃은 createParameterLayout()이 제공한다.
      // "PARAMETERS"는 ValueTree 루트 식별자로, getStateInformation/setStateInformation에서 사용된다.
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

// 베이스 앰프 시뮬레이터는 리버브·딜레이 테일이 있지만 Phase 0에서는 0으로 설정
double PluginProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
// Phase 0: 단일 프로그램 스텁 — Phase 7에서 PresetManager로 교체된다
int PluginProcessor::getNumPrograms()                              { return 1; }
int PluginProcessor::getCurrentProgram()                           { return 0; }
void PluginProcessor::setCurrentProgram (int)                      {}
const juce::String PluginProcessor::getProgramName (int)           { return {}; }
void PluginProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void PluginProcessor::prepareToPlay ([[maybe_unused]] double sampleRate,
                                     [[maybe_unused]] int    samplesPerBlock)
{
    // Phase 0: DSP 모듈이 없으므로 지연 샘플을 0으로 설정한다.
    // Phase 1 이후에는 오버샘플링·컨볼루션 지연을 합산해 DAW PDC에 반영해야 한다.
    setLatencySamples (0);
}

void PluginProcessor::releaseResources()
{
    // Phase 0: 해제할 리소스 없음.
    // Phase 1 이후에는 SignalChain::reset() 등을 여기서 호출한다.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 출력은 스테레오 또는 모노만 허용한다.
    // 5.1 서라운드 등 멀티채널 출력은 지원하지 않는다.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    // 입력은 모노 또는 스테레오만 허용한다.
    // 베이스 기타는 단일 채널이므로 실제 사용에서는 모노가 권장된다.
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    // 비정규 부동소수점(denormal) 값을 0으로 처리해 CPU 스파이크를 방지한다.
    // 극히 작은 신호가 있을 때 x86 프로세서가 소프트웨어 처리 모드로 전환되는
    // 현상을 막기 위한 JUCE 관례적 처리다.
    juce::ScopedNoDenormals noDenormals;

    // Phase 0: 신호 체인이 없으므로 버퍼를 무음으로 클리어한다.
    // Phase 1 이후에는 signalChain.process(buffer)로 교체된다.
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
    // --- APVTS 상태 직렬화 ---
    // APVTS 전체 파라미터 트리를 ValueTree로 복사한 뒤 XML로 변환하고,
    // JUCE 표준 바이너리 형식으로 인코딩해 destData에 기록한다.
    // DAW 프로젝트 저장, 플러그인 상태 복사(Copy/Paste) 시 호출된다.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // --- APVTS 상태 복원 ---
    // 바이너리 데이터를 XML로 역직렬화한 뒤 APVTS에 적용한다.
    // 태그 이름 검사는 다른 플러그인의 상태 데이터가 잘못 적용되는 것을 막는다.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessor::createParameterLayout()
{
    // Phase 0: 파라미터가 없는 빈 레이아웃을 반환한다.
    // Phase 1 이후에 게인, 톤스택(Bass/Mid/Treble), 이펙터 파라미터가 추가된다.
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    return { params.begin(), params.end() };
}

//==============================================================================
// JUCE 플러그인 팩토리 함수 — 호스트가 플러그인 인스턴스를 생성할 때 호출한다.
// 이 매크로 없이는 VST3/AU 형식으로 로드되지 않는다.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
