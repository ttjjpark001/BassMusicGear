#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)      // 모노 입력
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),    // 스테레오 출력
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())  // APVTS 초기화
{
    // --- APVTS 파라미터 포인터를 DSP 모듈에 연결 ---
    // 각 DSP 모듈이 getRawParameterValue()로 오디오 스레드에서 락프리 읽기 가능
    signalChain.connectParameters (apvts);

    // --- 메인 스레드 타이머 시작 (약 30Hz = 33.3ms 주기) ---
    // 역할: ToneStack 및 PowerAmp 필터 계수의 주기적 갱신
    // 이를 통해 노브 변화에 빠르게 반응하면서도 오디오 스레드를 블로킹하지 않음
    startTimerHz (30);
}

PluginProcessor::~PluginProcessor()
{
    // 타이머 중지 (소멸자에서 필수)
    stopTimer();
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
bool PluginProcessor::acceptsMidi() const  { return false; }
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }
double PluginProcessor::getTailLengthSeconds() const { return 0.5; }

int PluginProcessor::getNumPrograms()                              { return 1; }
int PluginProcessor::getCurrentProgram()                           { return 0; }
void PluginProcessor::setCurrentProgram (int)                      {}
const juce::String PluginProcessor::getProgramName (int)           { return {}; }
void PluginProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // --- 처리 스펙 구성 ---
    // 샘플레이트, 버퍼 크기, 채널 수 정보를 DSP 모듈에 전달
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;  // 모노 처리 체인

    // --- 신호 체인 초기화 ---
    // 모든 DSP 모듈의 버퍼 할당, 필터 계수 초기화
    signalChain.prepare (spec);

    // --- PDC(Plugin Delay Compensation) 보고 ---
    // 오버샘플링(Preamp) + 컨볼루션(Cabinet) 지연 합산
    // DAW가 이 값을 알면 플러그인 지연을 다른 트랙과 정확히 맞출 수 있다.
    setLatencySamples (signalChain.getTotalLatencyInSamples());
}

void PluginProcessor::releaseResources()
{
    // 재생 중지 시 모든 DSP 모듈의 상태 초기화
    // (필터 지연 라인, 오버샘플링 버퍼 등)
    signalChain.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
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
    // --- 유정규화(Denormalization) 억제 ---
    // 매우 작은 부동소수점 값(denormal numbers)이 연산을 느리게 하지 않도록 처리
    // x87 FPU에서는 denormal 감지 시 예외 발생 후 처리로 성능 저하
    // 이를 방지하기 위해 MXCSR 플래그 설정 (프로세서 의존적)
    juce::ScopedNoDenormals noDenormals;

    // --- 여분의 출력 채널 클리어 ---
    // 호스트가 더 많은 출력 채널을 기대할 수 있으므로 불필요한 채널 초기화
    // (모노 입력 → 스테레오 출력이므로 실제로는 불필요하지만 안전성)
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // --- 신호 체인 처리 (모노 채널 0) ---
    // 스테레오 입력 시에도 채널 0(L)만 처리하고 R 채널은 무시한다.
    // Gate → Preamp → ToneStack → PowerAmp → Cabinet 순서로 처리
    signalChain.process (buffer);

    // --- 모노 결과를 스테레오 출력으로 복제 ---
    // 신호 체인은 채널 0에서만 작동하므로,
    // 처리된 채널 0을 채널 1(R)에도 복사해 스테레오 아웃 구성
    // (실제 앰프처럼 L=R 신호, 즉 모노 신호를 스테레오 스피커로 출력)
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
void PluginProcessor::timerCallback()
{
    // --- 메인 스레드 타이머 콜백 (약 30Hz) ---
    // ToneStack(TMB 계수) 및 PowerAmp(Presence 필터) 계수의 주기적 갱신
    //
    // 왜 여기서? processBlock()은 오디오 스레드에서 실시간 실행되므로
    // 복잡한 수식 계산(특히 TMB 이중 선형 변환)을 할 수 없다.
    // 따라서 메인 스레드의 타이머에서 주기적으로 계산하고,
    // 원자적 변수(atomic)를 통해 오디오 스레드로 전달한다.
    //
    // 30Hz 선택 이유:
    // - 사용자 노브 변화에 빠르게 반응 (> 30fps 느낌으로 충분)
    // - 메인 스레드 부하 최소화 (프레임 드롭 방지)
    signalChain.updateCoefficientsFromMainThread (apvts);
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
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    //--------------------------------------------------------------------------
    // Noise Gate
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_threshold", 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_attack", 1 }, "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.1f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_hold", 1 }, "Gate Hold",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate_release", 1 }, "Gate Release",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gate_enabled", 1 }, "Gate Enabled", true));

    //--------------------------------------------------------------------------
    // Preamp
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input_gain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-20.0f, 40.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    //--------------------------------------------------------------------------
    // ToneStack (Tweed Bass TMB)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bass", 1 }, "Bass",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mid", 1 }, "Mid",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "treble", 1 }, "Treble",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    //--------------------------------------------------------------------------
    // PowerAmp
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "presence", 1 }, "Presence",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    //--------------------------------------------------------------------------
    // Cabinet
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "cab_bypass", 1 }, "Cabinet Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
