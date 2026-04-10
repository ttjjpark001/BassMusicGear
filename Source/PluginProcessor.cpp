#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Models/AmpModelLibrary.h"

//==============================================================================
/**
 * @brief PluginProcessor 초기화
 *
 * **주요 작업**:
 * 1. 오디오 버스 설정: 입력 모노 / 출력 스테레오
 * 2. APVTS 파라미터 레이아웃 생성
 * 3. SignalChain에 파라미터 포인터 연결
 * 4. 메인 스레드 타이머 시작 (30Hz, 계수 재계산용)
 *
 * @note [메인 스레드] 플러그인 로드 시 한 번만 호출.
 */
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // --- APVTS 파라미터를 SignalChain 각 블록에 연결 ---
    // 각 DSP 모듈(NoiseGate, Preamp, ToneStack 등)이
    // processBlock() 내에서 atomic으로 파라미터 값을 읽을 수 있도록
    signalChain.connectParameters (apvts);

    // --- Active/Passive 입력 패드 파라미터 포인터 캐시 ---
    // processBlock() 최상단에서 락프리로 읽기 위해 atomic 포인터를 미리 캐시한다.
    // Passive(false, 기본값) vs Active(true, -10dB 감쇄)
    inputActiveParam = apvts.getRawParameterValue ("input_active");

    // --- 메인 스레드 타이머 시작 (30Hz = ~33ms 간격) ---
    // 필터 계수, 톤스택, 앰프 모델 전환 등을 메인 스레드에서 계산.
    // 계산된 계수는 atomic 또는 FIFO로 오디오 스레드에 전달되어
    // processBlock() 중에 원자적 swap으로 적용됨.
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

    // --- Active/Passive 입력 패드 (RT-safe: atomic load + gain 곱셈만) ---
    // Active 픽업 베이스는 Passive 대비 ~+10dB 출력이 크므로,
    // input_active=true일 때 입력 신호를 -10dB(×0.3162) 감쇄하여 프리앰프
    // 헤드룸을 맞춰준다. new/lock/파일I/O 없음 — RT 안전.
    if (inputActiveParam != nullptr && inputActiveParam->load() > 0.5f)
    {
        const float activePadGain = 0.3162277660f;  // -10 dB
        const int numInputChannels = getTotalNumInputChannels();
        for (int ch = 0; ch < numInputChannels; ++ch)
            buffer.applyGain (ch, 0, buffer.getNumSamples(), activePadGain);
    }

    // --- 출력 채널 초기화 ---
    // 입력보다 출력이 많은 경우 (예: 모노 입력 → 스테레오 출력)
    // 여분의 출력 채널을 0으로 클리어
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // --- BPM 동기화: AudioPlayHead에서 BPM을 추출하여 Delay에 전달 ---
    // DAW의 타이밍 정보(AudioPlayHead)로부터 현재 BPM을 읽어서
    // Delay의 BPM Sync 기능에 제공한다.
    // Standalone 모드에서는 PlayHead가 없으므로 120 BPM 기본값 사용.
    {
        double bpm = 120.0;  // Standalone 모드 기본값
        if (auto* ph = getPlayHead())
        {
            // getPosition()은 std::optional로 반환되므로 hasValue() 확인 필수
            if (auto posInfo = ph->getPosition())
            {
                // BPM 정보가 있으면 읽어서 적용
                // VST3/AU 호스트에서는 대부분 BPM을 제공한다.
                if (posInfo->getBpm().hasValue())
                    bpm = *posInfo->getBpm();
            }
        }
        // 추출한 BPM을 Delay DSP 모듈로 전달 (atomic으로 RT-safe)
        signalChain.getDelay().setBpm (bpm);
    }

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
/**
 * @brief APVTS 상태 전체를 바이너리 형식으로 직렬화하여 프리셋 저장
 *
 * @param destData  플러그인이 채울 메모리 블록 (DAW 저장소)
 *
 * **과정**:
 * 1. APVTS 상태 스냅샷 추출 (copyState)
 * 2. ValueTree → XML 변환 (createXml)
 * 3. XML → 바이너리 직렬화 (copyXmlToBinary)
 *
 * @note [메인 스레드] DAW의 "Save Preset" 또는 PresetManager에서 호출.
 *       반환된 바이너리는 DAW 파일 시스템에 저장된다.
 */
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // 현재 모든 파라미터 값 스냅샷 획득
    auto state = apvts.copyState();
    // ValueTree를 XML 엘리먼트로 변환
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    // XML을 바이너리로 변환하여 destData에 저장
    copyXmlToBinary (*xml, destData);
}

/**
 * @brief 저장된 프리셋 바이너리를 역직렬화하여 APVTS 상태 복원
 *
 * @param data        프리셋 바이너리 포인터
 * @param sizeInBytes 데이터 크기
 *
 * **과정**:
 * 1. 바이너리 → XML 변환 (getXmlFromBinary)
 * 2. 루트 엘리먼트 검증 (PARAMETERS 확인)
 * 3. 각 <PARAM> 엘리먼트를 개별 파라미터에 적용
 *
 * **하위 호환 전략**:
 * - 저장된 상태에 없는 파라미터는 현재 기본값 유지
 * - 저장된 상태에 존재하는 파라미터만 덮어씀
 * - 예: 이전 버전 프리셋이 새로 추가된 파라미터를 건드리지 않음
 *
 * @note [메인 스레드] DAW의 "Load Preset" 또는 프리셋 복원 시 호출.
 *       정규화 변환: raw 값(예: -20dB) → 0~1 범위 → setValueNotifyingHost()
 */
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // 바이너리를 XML로 변환
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    // 루트 엘리먼트가 PARAMETERS인지 검증
    if (! xmlState->hasTagName (apvts.state.getType()))
        return;

    // 하위 호환 로직:
    // 저장된 상태에 없는 파라미터는 현재 기본값을 그대로 유지하고,
    // 저장된 상태에 존재하는 파라미터만 개별적으로 덮어써
    // 이전 버전 프리셋이 새로 추가된 파라미터를 건드리지 않도록 한다.
    for (auto* child : xmlState->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = child->getStringAttribute ("id");
        if (id.isEmpty())
            continue;

        if (auto* param = apvts.getParameter (id))
        {
            // raw 값(파라미터 범위) → 정규화 0~1로 변환
            const float rawValue = (float) child->getDoubleAttribute ("value");
            const float normalized = param->getNormalisableRange().convertTo0to1 (rawValue);
            // 정규화 범위 체크 후 UI와 호스트에 전파
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
        }
    }
}

//==============================================================================
// A/B 슬롯 구현 — 즉시 비교용 임시 ValueTree 저장소
//==============================================================================

/**
 * @brief 현재 APVTS 상태를 A(0) 또는 B(1) 슬롯에 저장
 *
 * @param slot  슬롯 번호 (0=A, 1=B)
 *
 * **동작**:
 * - APVTS 상태 스냅샷 획득 (모든 파라미터 값)
 * - 지정한 슬롯의 ValueTree에 저장
 * - 나중에 loadFromSlot()으로 복원 가능
 *
 * **사용 시나리오**:
 * 1. 설정 A 작성 후 saveToSlot(0) 호출
 * 2. 설정 변경 후 saveToSlot(1) 호출
 * 3. loadFromSlot(0) / loadFromSlot(1)로 즉시 비교
 *
 * @note [메인 스레드] PresetPanel의 A/B 버튼 핸들러에서 호출.
 *       ValueTree는 메모리 기반 스냅샷 (파일 저장 아님).
 */
void PluginProcessor::saveToSlot (int slot)
{
    // 현재 모든 파라미터 값의 스냅샷 획득
    auto snapshot = apvts.copyState();
    if (slot == 0)
        slotA = snapshot;
    else if (slot == 1)
        slotB = snapshot;
}

/**
 * @brief A(0) 또는 B(1) 슬롯에 저장된 상태를 APVTS에 복원
 *
 * @param slot  슬롯 번호 (0=A, 1=B)
 *
 * **동작**:
 * 1. 슬롯의 ValueTree가 유효한지 확인
 * 2. XML 엘리먼트로 변환
 * 3. 각 <PARAM>을 개별 파라미터에 적용 (setValueNotifyingHost)
 *
 * **특징**:
 * - replaceState() 대신 파라미터 단위 적용
 * - UI Attachment와 리스너가 자동 갱신됨
 * - 편집 중 다른 파라미터는 영향 받지 않음
 *
 * @note [메인 스레드] PresetPanel의 A/B 버튼 핸들러에서 호출.
 */
void PluginProcessor::loadFromSlot (int slot)
{
    // 슬롯 선택 및 유효성 확인
    const juce::ValueTree& target = (slot == 0) ? slotA : slotB;
    if (! target.isValid())
        return;

    // replaceState를 쓰는 대신 파라미터 단위로 덮어써 setValueNotifyingHost()
    // 경로를 거쳐 UI Attachment와 리스너가 자동 갱신되도록 한다.
    // (이전 setStateInformation()과 동일 로직)
    std::unique_ptr<juce::XmlElement> xml (target.createXml());
    if (xml == nullptr)
        return;

    // 각 파라미터를 XML에서 읽어 개별 적용
    for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = child->getStringAttribute ("id");
        if (id.isEmpty())
            continue;

        if (auto* param = apvts.getParameter (id))
        {
            // raw 값 → 정규화 0~1 → setValueNotifyingHost()
            const float rawValue = (float) child->getDoubleAttribute ("value");
            const float normalized = param->getNormalisableRange().convertTo0to1 (rawValue);
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
        }
    }
}

/**
 * @brief 슬롯에 저장된 데이터가 있는지 확인
 *
 * @param slot  슬롯 번호 (0=A, 1=B)
 * @return      슬롯이 유효하면 true (데이터 있음), 아니면 false
 *
 * **용도**:
 * - PresetPanel::handleSlotButton()에서 첫 클릭 vs 이후 클릭 구분
 * - 첫 클릭: isSlotValid() == false → saveToSlot()
 * - 이후 클릭: isSlotValid() == true → loadFromSlot()
 */
bool PluginProcessor::isSlotValid (int slot) const
{
    if (slot == 0) return slotA.isValid();
    if (slot == 1) return slotB.isValid();
    return false;
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
    // Input Active/Passive Pad
    //--------------------------------------------------------------------------
    // Passive(false, 기본값) vs Active(true, -10 dB 감쇄)
    // Active 픽업 베이스는 Passive 대비 출력이 크므로 프리앰프 헤드룸 확보 목적
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "input_active", 1 }, "Input Active", false));

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

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "delay_bpm_sync", 1 }, "Delay BPM Sync", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "delay_note_value", 1 }, "Delay Note",
        juce::StringArray { "1/4", "1/8", "1/8 dot", "1/16", "1/4 trip" }, 0));

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
