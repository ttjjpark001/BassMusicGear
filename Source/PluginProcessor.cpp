#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Models/AmpModelLibrary.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // --- APVTS 파라미터를 SignalChain 각 블록에 연결 ---
    // 이후 processBlock()에서 atomic으로 파라미터 값을 읽을 수 있음
    signalChain.connectParameters (apvts);

    // --- 메인 스레드 30Hz 타이머 시작 ---
    // DSP 계수 재계산(FIR/IIR 필터 계수 갱신)을 메인 스레드에서 수행
    // processBlock() 중에는 원자적 swap으로 오디오 스레드에 전달됨
    startTimerHz (30);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
double PluginProcessor::getTailLengthSeconds() const { return 0.5; }

const juce::String PluginProcessor::getProgramName (int)           { return {}; }
void PluginProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // --- DSP 처리 스펙 정의 ---
    // 신호 체인은 모노로 처리 (입력 모노, 출력 스테레오는 processBlock에서 처리)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;

    // --- SignalChain 초기화 ---
    // 모든 DSP 모듈(필터, 컨볼루션, 오버샘플링 등) 초기화
    // 버퍼 할당, 필터 계수 계산 등이 이루어짐
    signalChain.prepare (spec);

    // --- Plugin Delay Compensation(PDC) 지연 시간 보고 ---
    // Convolution(Cabinet) + Oversampling(Preamp, Overdrive) 누적 지연 시간을
    // setLatencySamples()로 DAW에 보고
    // DAW가 이 값을 사용해 다른 트랙과 타이밍을 자동 맞춤
    setLatencySamples (signalChain.getTotalLatencyInSamples());
}

void PluginProcessor::releaseResources()
{
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
    // --- Denormal 방지 (매우 작은 값의 부동소수점 연산 최적화) ---
    juce::ScopedNoDenormals noDenormals;

    // --- 출력 채널 초기화 ---
    // 입력보다 출력이 많은 경우 (예: 모노 입력 → 스테레오 출력)
    // 여분의 출력 채널을 0으로 클리어
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // --- 신호 체인 처리 (모노) ---
    // Gate → Tuner → Compressor → BiAmp Crossover → Pre-FX → Amp → Post-FX → PowerAmp → Cabinet → DIBlend
    signalChain.process (buffer);

    // --- 모노 → 스테레오 확장 ---
    // 처리된 모노 신호(버퍼 채널 0)을 채널 1(우측)에도 복사
    // 스테레오 출력 플러그인 형식 준수
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
void PluginProcessor::timerCallback()
{
    // --- 메인 스레드 30Hz 타이머 콜백 ---
    // 필터 계수, 톤스택 파라미터, 앰프 모델 전환 등을 처리
    // 오디오 스레드가 아닌 메인 스레드에서 실행되므로 계산 시간에 자유로움
    // 계산된 계수는 atomic 또는 FIFO로 오디오 스레드에 전달됨
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
    // Amp Model Selection
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "amp_model", 1 }, "Amp Model",
        AmpModelLibrary::getModelNames(), 1));  // default: Tweed Bass (index 1)

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
    // ToneStack (shared Bass/Mid/Treble)
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
    // American Vintage: Mid Position (5-position switch)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mid_position", 1 }, "Mid Position",
        juce::StringArray { "250 Hz", "500 Hz", "800 Hz", "1.5 kHz", "3 kHz" }, 1));

    //--------------------------------------------------------------------------
    // Italian Clean: VPF / VLE
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "vpf", 1 }, "VPF",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "vle", 1 }, "VLE",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    //--------------------------------------------------------------------------
    // Modern Micro: Grunt / Attack
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "grunt", 1 }, "Grunt",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack",
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

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sag", 1 }, "Sag",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    //--------------------------------------------------------------------------
    // Cabinet
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "cab_bypass", 1 }, "Cabinet Bypass", false));

    //--------------------------------------------------------------------------
    // Cabinet IR Selection
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "cab_ir", 1 }, "Cabinet IR",
        juce::StringArray { "8x10 SVT", "4x10 JBL", "1x15 Vintage",
                            "2x12 British", "2x10 Modern" }, 0));

    //--------------------------------------------------------------------------
    // Tuner
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tuner_reference_a", 1 }, "Reference A",
        juce::NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tuner_mute", 1 }, "Tuner Mute", false));

    //--------------------------------------------------------------------------
    // Compressor
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "comp_enabled", 1 }, "Compressor", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_threshold", 1 }, "Comp Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_ratio", 1 }, "Comp Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel (":1")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_attack", 1 }, "Comp Attack",
        juce::NormalisableRange<float> (0.1f, 200.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_release", 1 }, "Comp Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_makeup", 1 }, "Comp Makeup",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_dry_blend", 1 }, "Comp Dry Blend",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    //--------------------------------------------------------------------------
    // Overdrive (Pre-FX): Tube/JFET/Fuzz 웨이브쉐이핑
    //--------------------------------------------------------------------------
    // ON/OFF 토글
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "od_enabled", 1 }, "Overdrive", false));

    // 오버드라이브 타입 선택 (Tube=부드러운 포화, JFET=클린+그릿, Fuzz=극도의 클리핑)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "od_type", 1 }, "OD Type",
        juce::StringArray { "Tube", "JFET", "Fuzz" }, 0));

    // 드라이브 양 (0~1, 선형 → 1~20배 게인에 매핑)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "od_drive", 1 }, "OD Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // 톤 필터 로우패스 컷오프 (0=500Hz 어두움, 1=12kHz 밝음)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "od_tone", 1 }, "OD Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // 드라이 블렌드 (0=모두 웨트, 1=모두 드라이)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "od_dry_blend", 1 }, "OD Dry Blend",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    //--------------------------------------------------------------------------
    // Octaver (Pre-FX): YIN 피치 추적 + 서브/옥타브 사인파 합성
    //--------------------------------------------------------------------------
    // ON/OFF 토글
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "oct_enabled", 1 }, "Octaver", false));

    // 서브옥타브(F0/2) 레벨 (0~1)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "oct_sub_level", 1 }, "Sub Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // 옥타브업(F0*2) 레벨 (0~1) [P1: 음질 개선 예정]
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "oct_up_level", 1 }, "Oct-Up Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // 원본 신호(드라이) 레벨 (0~1)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "oct_dry_level", 1 }, "Oct Dry Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    //--------------------------------------------------------------------------
    // Envelope Filter (Pre-FX): SVF + 엔벨로프 팔로워 변조
    //--------------------------------------------------------------------------
    // ON/OFF 토글
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "ef_enabled", 1 }, "Envelope Filter", false));

    // 엔벨로프 감도 (0~1, 필터 스윕 범위 조절)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_sensitivity", 1 }, "EF Sensitivity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // 최소 컷오프 주파수 (Hz)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_freq_min", 1 }, "EF Freq Min",
        juce::NormalisableRange<float> (100.0f, 500.0f, 1.0f), 200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // 최대 컷오프 주파수 (Hz)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_freq_max", 1 }, "EF Freq Max",
        juce::NormalisableRange<float> (1000.0f, 8000.0f, 1.0f), 4000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // 레조넌스/Q (0.5~10, 밴드폭 조절)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ef_resonance", 1 }, "EF Resonance",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.1f), 3.0f));

    // 방향 선택: Up(엔벨로프 증가 시 컷오프 상승) vs Down(하강)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "ef_direction", 1 }, "EF Direction",
        juce::StringArray { "Up", "Down" }, 0));

    //--------------------------------------------------------------------------
    // Graphic EQ (10-band, Post-ToneStack)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "geq_enabled", 1 }, "Graphic EQ", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_31", 1 }, "31 Hz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_63", 1 }, "63 Hz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_125", 1 }, "125 Hz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_250", 1 }, "250 Hz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_500", 1 }, "500 Hz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_1k", 1 }, "1 kHz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_2k", 1 }, "2 kHz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_4k", 1 }, "4 kHz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_8k", 1 }, "8 kHz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "geq_16k", 1 }, "16 kHz",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    //--------------------------------------------------------------------------
    // Chorus (Post-FX)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "chorus_enabled", 1 }, "Chorus", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorus_rate", 1 }, "Chorus Rate",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorus_depth", 1 }, "Chorus Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorus_mix", 1 }, "Chorus Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    //--------------------------------------------------------------------------
    // Delay (Post-FX)
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "delay_enabled", 1 }, "Delay", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delay_time", 1 }, "Delay Time",
        juce::NormalisableRange<float> (10.0f, 2000.0f, 1.0f), 500.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delay_feedback", 1 }, "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delay_damping", 1 }, "Delay Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delay_mix", 1 }, "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    //--------------------------------------------------------------------------
    // BiAmp Crossover
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "biamp_on", 1 }, "Bi-Amp", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "crossover_freq", 1 }, "Crossover Freq",
        juce::NormalisableRange<float> (60.0f, 500.0f, 1.0f, 0.5f), 200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    //--------------------------------------------------------------------------
    // DI Blend
    //--------------------------------------------------------------------------
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "di_blend_on", 1 }, "DI Blend On", false));  // OFF: processed만 출력

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "di_blend", 1 }, "DI Blend",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "clean_level", 1 }, "Clean Level",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "processed_level", 1 }, "Processed Level",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "ir_position", 1 }, "IR Position", false));  // false=Post, true=Pre

    //--------------------------------------------------------------------------
    // Reverb (Post-FX): Hall/Plate 타입 추가 (Spring, Room, Hall, Plate 4종)
    //--------------------------------------------------------------------------
    // 각 타입의 음향 특성:
    // - Spring(0): 빈티지 스프링 탱크, 짧은 감쇠, 콤팩트한 느낌
    // - Room(1): 연습실 같은 자연스러운 공간감, 따뜻한 톤
    // - Hall(2): 콘서트홀의 웅장한 잔향, 밝고 긴 반향, 호화로움
    // - Plate(3): 금속판 리버브, 초기 반사 선명, 모던하고 밝은 음색
    // 각 타입별 roomSize/damping/width 매핑은 Reverb.cpp에서 구성된다.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "reverb_enabled", 1 }, "Reverb", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "reverb_type", 1 }, "Reverb Type",
        juce::StringArray { "Spring", "Room", "Hall", "Plate" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverb_size", 1 }, "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverb_decay", 1 }, "Reverb Decay",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverb_mix", 1 }, "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    return { params.begin(), params.end() };
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
