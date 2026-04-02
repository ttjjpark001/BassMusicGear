#include "SignalChain.h"
#include <BinaryData.h>

/**
 * @brief 신호 체인 전체를 DSP 초기화한다.
 *
 * 모든 블록(Gate/Tuner/Compressor/Overdrive/Octaver/EnvelopeFilter/
 * Preamp/ToneStack/PowerAmp/Cabinet)을
 * 순서대로 prepare하여 버퍼 할당, 오버샘플링 초기화, 필터 계수 생성 등을 수행.
 *
 * **오버샘플링 지연**:
 * - Preamp: 4배 오버샘플링으로 웨이브쉐이핑 앨리어싱 방지
 * - Cabinet: 컨볼루션 IR 로드(~200ms)로 인한 지연
 * - 합산 → PluginProcessor::getTotalLatency()에서 DAW PDC 보고
 *
 * @param spec  오디오 스펙 (sampleRate, maximumBlockSize 포함)
 * @note [메인 스레드] PluginProcessor::prepareToPlay()에서 호출된다.
 */
void SignalChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    // 신호 체인은 모노 처리 (입력 1개 채널)
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // --- 신호 체인 순서: Gate → Tuner → Compressor → Overdrive → Octaver → EnvelopeFilter → Preamp → ToneStack → PowerAmp → Cabinet ---
    noiseGate.prepare (monoSpec);
    tuner.prepare (monoSpec);
    compressor.prepare (monoSpec);
    overdrive.prepare (monoSpec);
    octaver.prepare (monoSpec);
    envelopeFilter.prepare (monoSpec);
    preamp.prepare (monoSpec);
    toneStack.prepare (monoSpec);
    powerAmp.prepare (monoSpec);
    cabinet.prepare (monoSpec);
}

/**
 * @brief 신호 체인의 모든 필터/버퍼를 클리어한다.
 *
 * 재생 중지, 모델 전환, 또는 아티팩트 방지 시 호출.
 * 각 블록의 내부 상태(필터 메모리, 엔벨로프 추적 상태 등)를 초기화.
 *
 * @note [오디오 스레드] releaseResources() 또는 모델 전환 시 호출.
 */
void SignalChain::reset()
{
    noiseGate.reset();
    tuner.reset();
    compressor.reset();
    overdrive.reset();
    octaver.reset();
    envelopeFilter.reset();
    preamp.reset();
    toneStack.reset();
    powerAmp.reset();
    cabinet.reset();
}

/**
 * @brief 신호 체인 전체의 누적 지연(샘플 단위)을 반환한다.
 *
 * **지연 구성**:
 * - **Overdrive** (4x/8x 오버샘플링): 8x worst-case 지연으로 일관 보고
 * - **Preamp** (4배 오버샘플링): ~250 samples @ 44.1kHz
 *   - 오버샘플링 필터 및 다운샘플링 필터로 인한 지연
 * - **Cabinet** (컨볼루션 IR): 파티션 크기에 의한 처리 지연
 *   - juce::dsp::Convolution 내부 파티션 구조에 따라 결정
 *
 * 합산된 값은 PluginProcessor가 DAW에 보고하여,
 * DAW가 다른 트랙과의 시간 정렬(PDC - Plugin Delay Compensation)에 사용.
 *
 * @return  총 지연 샘플 수
 */
int SignalChain::getTotalLatencyInSamples() const
{
    return overdrive.getLatencyInSamples() + preamp.getLatencyInSamples() + cabinet.getLatencyInSamples();
}

/**
 * @brief 선택된 앰프 모델로 신호 체인의 모든 블록을 전환한다.
 *
 * **모델 전환 절차**:
 * 1. **ToneStack**: 톤스택 토폴로지(TMB/Baxandall/James/etc) 설정
 *    - 토폴로지별 고유한 IIR 계수 계산 함수 선택
 * 2. **Preamp**: 프리앰프 타입(Tube12AX7/JFET/ClassD) 변경
 *    - 웨이브쉐이핑 곡선/드라이브 특성 변경
 * 3. **PowerAmp**: 파워앰프 포화 특성 + Sag 활성화 여부 설정
 *    - 튜브 모델(American Vintage, Tweed Bass, British Stack)만 Sag 적용
 * 4. **Cabinet**: 모델 기본 IR 로드 (BinaryData에서)
 *    - 5종: SVT 8x10, JBL 4x10, Vintage 1x15, British 2x12, Modern 2x10
 * 5. **필터 메모리 클리어**: 이전 모델의 필터 상태 제거 (아티팩트 방지)
 * 6. **계수 갱신 플래그 초기화**: 다음 프레임에 Bass/Mid/Treble 재계산
 *
 * @param modelId  선택된 앰프 모델 ID (AmpModelLibrary의 enum)
 * @note [메인 스레드 전용] PluginEditor의 AmpPanel ComboBox 변경 시 호출.
 *       필터 reset() 호출 이후 processBlock() 재개하므로 오디오 스레드 안전.
 */
void SignalChain::setAmpModel (AmpModelId modelId)
{
    currentModelId = modelId;
    const auto& model = AmpModelLibrary::getModel (modelId);

    // --- DSP 모듈을 선택된 모델에 맞게 전환 ---
    toneStack.setType (model.toneStack);
    preamp.setPreampType (model.preamp);
    powerAmp.setPowerAmpType (model.powerAmp, model.sagEnabled);

    // --- 캐비닛 기본 IR 로드 (BinaryData로 컴파일된 내장 IR) ---
    // 앰프 모델별 기본 스피커 설정을 IR로 제공
    if (model.defaultIRName == "ir_8x10_svt_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_8x10_svt_wav,
                                       BinaryData::ir_8x10_svt_wavSize);
    else if (model.defaultIRName == "ir_4x10_jbl_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_4x10_jbl_wav,
                                       BinaryData::ir_4x10_jbl_wavSize);
    else if (model.defaultIRName == "ir_1x15_vintage_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_1x15_vintage_wav,
                                       BinaryData::ir_1x15_vintage_wavSize);
    else if (model.defaultIRName == "ir_2x12_british_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_2x12_british_wav,
                                       BinaryData::ir_2x12_british_wavSize);
    else if (model.defaultIRName == "ir_2x10_modern_wav")
        cabinet.loadIRFromBinaryData (BinaryData::ir_2x10_modern_wav,
                                       BinaryData::ir_2x10_modern_wavSize);
    else
        cabinet.loadDefaultIR();

    // --- 필터 메모리 클리어 ---
    // 이전 모델의 필터 상태가 남아 있으면 모델 전환 시 팝/클릭 아티팩트 발생 위험
    toneStack.reset();
    powerAmp.reset();

    // --- 다음 프레임에서 모든 계수를 재계산하도록 플래그 초기화 ---
    // updateCoefficientsFromMainThread()에서 이전값과 비교하므로,
    // 여기서 모두 -1.0f로 설정하면 다음 호출 시 필수 재계산 발생
    prevBass = prevMid = prevTreble = prevPresence = -1.0f;
    prevVpf = prevVle = prevGrunt = prevAttack = -1.0f;
    prevMidPos = -1;
}

/**
 * @brief APVTS 파라미터를 각 신호 체인 블록에 등록한다.
 *
 * 각 DSP 모듈이 atomic 포인터를 통해 파라미터 값을 폴링할 수 있도록 함.
 * 모든 노브, 슬라이더, 토글의 ID를 올바른 블록에 연결하는 것이 중요.
 *
 * **파라미터 ID 목록**:
 * - Gate: gate_threshold, gate_attack, gate_hold, gate_release, gate_enabled
 * - Preamp: input_gain, volume
 * - ToneStack: bass, mid, treble
 * - PowerAmp: drive, presence, sag
 * - Cabinet: cab_bypass
 * - Tuner: tuner_reference_a, tuner_mute
 * - Compressor: comp_enabled, comp_threshold, comp_ratio, comp_attack, comp_release, comp_makeup, comp_dry_blend
 * - 모델별 추가 파라미터: vpf, vle, grunt, attack, mid_position (updateCoefficientsFromMainThread에서 처리)
 *
 * @param apvts  APVTS 인스턴스
 * @note [메인 스레드] PluginProcessor 생성 시 호출된다.
 *       getRawParameterValue()는 std::atomic<float>* 반환 (락프리 접근).
 */
void SignalChain::connectParameters (juce::AudioProcessorValueTreeState& apvts)
{
    // --- NoiseGate 파라미터 ---
    noiseGate.setParameterPointers (
        apvts.getRawParameterValue ("gate_threshold"),
        apvts.getRawParameterValue ("gate_attack"),
        apvts.getRawParameterValue ("gate_hold"),
        apvts.getRawParameterValue ("gate_release"),
        apvts.getRawParameterValue ("gate_enabled"));

    // --- Preamp 파라미터 ---
    preamp.setParameterPointers (
        apvts.getRawParameterValue ("input_gain"),
        apvts.getRawParameterValue ("volume"));

    // --- ToneStack 파라미터 (기본) ---
    toneStack.setParameterPointers (
        apvts.getRawParameterValue ("bass"),
        apvts.getRawParameterValue ("mid"),
        apvts.getRawParameterValue ("treble"),
        nullptr);

    // --- PowerAmp 파라미터 ---
    powerAmp.setParameterPointers (
        apvts.getRawParameterValue ("drive"),
        apvts.getRawParameterValue ("presence"),
        apvts.getRawParameterValue ("sag"));

    // --- Cabinet 파라미터 ---
    cabinet.setParameterPointers (
        apvts.getRawParameterValue ("cab_bypass"));

    // --- Tuner 파라미터 ---
    tuner.setParameterPointers (
        apvts.getRawParameterValue ("tuner_reference_a"),
        apvts.getRawParameterValue ("tuner_mute"));

    // --- Compressor 파라미터 ---
    compressor.setParameterPointers (
        apvts.getRawParameterValue ("comp_enabled"),
        apvts.getRawParameterValue ("comp_threshold"),
        apvts.getRawParameterValue ("comp_ratio"),
        apvts.getRawParameterValue ("comp_attack"),
        apvts.getRawParameterValue ("comp_release"),
        apvts.getRawParameterValue ("comp_makeup"),
        apvts.getRawParameterValue ("comp_dry_blend"));

    // --- Overdrive 파라미터 ---
    overdrive.setParameterPointers (
        apvts.getRawParameterValue ("od_enabled"),
        apvts.getRawParameterValue ("od_type"),
        apvts.getRawParameterValue ("od_drive"),
        apvts.getRawParameterValue ("od_tone"),
        apvts.getRawParameterValue ("od_dry_blend"));

    // --- Octaver 파라미터 ---
    octaver.setParameterPointers (
        apvts.getRawParameterValue ("oct_enabled"),
        apvts.getRawParameterValue ("oct_sub_level"),
        apvts.getRawParameterValue ("oct_up_level"),
        apvts.getRawParameterValue ("oct_dry_level"));

    // --- EnvelopeFilter 파라미터 ---
    envelopeFilter.setParameterPointers (
        apvts.getRawParameterValue ("ef_enabled"),
        apvts.getRawParameterValue ("ef_sensitivity"),
        apvts.getRawParameterValue ("ef_freq_min"),
        apvts.getRawParameterValue ("ef_freq_max"),
        apvts.getRawParameterValue ("ef_resonance"),
        apvts.getRawParameterValue ("ef_direction"));
}

/**
 * @brief 메인 스레드에서 변경된 APVTS 파라미터를 감지하고 각 블록에 반영한다.
 *
 * **변경 감지 방식**: 이전값을 보관하여 현재값과 비교. 변경된 파라미터만 처리.
 * 이를 통해 매 프레임마다 불필요한 계수 재계산을 회피.
 *
 * **처리 순서**:
 * 1. **앰프 모델 전환** (amp_model)
 *    - setAmpModel() 호출로 모든 블록의 토폴로지 전환
 *    - cab_ir 파라미터도 동기화 (CabinetSelector UI 갱신)
 * 2. **톤스택 계수** (bass, mid, treble)
 *    - 어떤 모델이든 기본 톤 컨트롤이므로 항상 확인
 * 3. **파워앰프 Presence 필터** (presence)
 *    - 고음 부스트 필터
 * 4. **모델별 추가 파라미터**:
 *    - **Italian Clean** (MarkbassFourBand): VPF, VLE
 *    - **Modern Micro** (BaxandallGrunt): Grunt, Attack
 *    - **American Vintage** (Baxandall): Mid Position
 *
 * @param apvts  APVTS 인스턴스 (파라미터 값 읽기)
 * @note [메인 스레드 전용] PluginProcessor::timerCallback() 또는
 *       APVTS 리스너에서 호출. 오디오 스레드 세이프하지 않음.
 */
void SignalChain::updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts)
{
    // --- 앰프 모델 전환 ---
    // 모델이 바뀌면 전체 신호 체인의 토폴로지 재구성
    const int ampModelIndex = static_cast<int> (apvts.getRawParameterValue ("amp_model")->load());
    if (ampModelIndex != prevAmpModel)
    {
        setAmpModel (static_cast<AmpModelId> (ampModelIndex));
        prevAmpModel = ampModelIndex;

        // 모델 기본 IR에 맞춰 cab_ir 파라미터도 갱신
        // CabinetSelector ComboBox가 현재 모델의 기본 IR을 표시하도록 동기화
        const auto& newModel = AmpModelLibrary::getModel (static_cast<AmpModelId> (ampModelIndex));
        const int irIndex = irNameToIndex (newModel.defaultIRName);
        if (auto* p = apvts.getParameter ("cab_ir"))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (irIndex)));
    }

    // --- ToneStack 계수 (모든 모델 공통) ---
    // Bass/Mid/Treble 노브가 변경되면 해당 톤스택 토폴로지의 IIR 계수 재계산
    const float bass   = apvts.getRawParameterValue ("bass")->load();
    const float mid    = apvts.getRawParameterValue ("mid")->load();
    const float treble = apvts.getRawParameterValue ("treble")->load();

    if (bass != prevBass || mid != prevMid || treble != prevTreble)
    {
        toneStack.updateCoefficients (bass, mid, treble);
        prevBass   = bass;
        prevMid    = mid;
        prevTreble = treble;
    }

    // --- PowerAmp Presence 필터 ---
    // Presence 노브로 고음 영역(~2kHz)을 부스트/컷하는 피킹 필터 계수 계산
    const float presence = apvts.getRawParameterValue ("presence")->load();
    if (presence != prevPresence)
    {
        powerAmp.updatePresenceFilter (presence);
        prevPresence = presence;
    }

    // --- 모델별 추가 파라미터 ---
    const auto& model = AmpModelLibrary::getModel (currentModelId);

    // **Italian Clean (Markbass 4-Band)**: VPF (Vintage Presence Filter) / VLE (Vintage Loudspeaker Emulation)
    // VPF: 3개 필터 합산 (35Hz 부스트 + 380Hz 노치 + 10kHz 부스트)
    // VLE: 고주파 롤오프로 오래된 스피커 감성 추가
    if (model.toneStack == ToneStackType::MarkbassFourBand)
    {
        const float vpf = apvts.getRawParameterValue ("vpf")->load();
        const float vle = apvts.getRawParameterValue ("vle")->load();
        if (vpf != prevVpf || vle != prevVle)
        {
            toneStack.updateMarkbassExtras (vpf, vle);
            prevVpf = vpf;
            prevVle = vle;
        }
    }

    // **Modern Micro (Baxandall Grunt)**: Grunt (저음 공격성) / Attack (중음 힘)
    // Grunt/Attack이 변경되면 톤스택 계수도 재계산 필요
    if (model.toneStack == ToneStackType::BaxandallGrunt)
    {
        const float grunt  = apvts.getRawParameterValue ("grunt")->load();
        const float attack = apvts.getRawParameterValue ("attack")->load();
        if (grunt != prevGrunt || attack != prevAttack)
        {
            toneStack.updateModernExtras (grunt, attack);
            // Grunt/Attack 적용 후 Bass/Mid/Treble 계수 재계산
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevGrunt  = grunt;
            prevAttack = attack;
        }
    }

    // **American Vintage (Baxandall)**: Mid Position (5단 스위치: 250/500/800/1.5k/3kHz)
    // Mid Position이 변경되면 Baxandall 톤스택의 Mid 필터 중심 주파수 전환
    if (model.toneStack == ToneStackType::Baxandall)
    {
        const int midPos = static_cast<int> (apvts.getRawParameterValue ("mid_position")->load());
        if (midPos != prevMidPos)
        {
            toneStack.updateMidPosition (midPos);
            // Mid Position 적용 후 Bass/Mid/Treble 계수 재계산
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevMidPos = midPos;
        }
    }
}

/**
 * @brief 오디오 버퍼를 신호 체인 전체에 통과시킨다.
 *
 * **신호 체인 순서 (입력 → 출력)**:
 * ```
 * 입력 → [NoiseGate (노이즈 제거)]
 *     → [Tuner (YIN 피치 감지, Mute 토글)]
 *     → [Compressor (VCA 압축)]
 *     → [BiAmp placeholder] ← Phase 6에서 구현
 *     → [Overdrive (Tube/JFET/Fuzz + Dry Blend)]
 *     → [Octaver (YIN 서브옥타브/옥타브업)]
 *     → [EnvelopeFilter (SVF + 엔벨로프 팔로워)]
 *     → [Preamp (입력 게인 + 웨이브쉐이핑 + 4배 OS)]
 *     → [ToneStack (모델별 톤 컨트롤)]
 *     → [PowerAmp (포화 + Presence 필터 + Sag)]
 *     → [Cabinet (콘볼루션 IR)]
 *     → 출력
 * ```
 *
 * 각 블록은 In-place로 버퍼를 수정하므로,
 * 이전 블록의 출력이 다음 블록의 입력이 됨.
 *
 * @param buffer  모노 오디오 버퍼
 * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
 *       이 함수 내에서 메모리 할당, 파일 I/O, mutex, 시스템 콜 금지.
 */
void SignalChain::process (juce::AudioBuffer<float>& buffer)
{
    // --- 신호 체인 순서대로 처리 (입력 → 출력) ---
    // 각 블록은 In-place로 버퍼를 수정하므로, 이전 블록의 출력이 다음 블록의 입력
    noiseGate.process (buffer);          // 1. 히스테리시스 게이트: 허밍(60Hz) 및 배경 노이즈 제거
    tuner.process (buffer);              // 2. YIN 피치 감지: Tuner UI 표시용 (선택적 뮤트)
    compressor.process (buffer);         // 3. VCA 컴프레서 (패러렬 드라이 블렌드)
    // [BiAmp placeholder — Phase 6: Linkwitz-Riley 크로스오버 구현 예정]
    overdrive.process (buffer);          // 4. Pre-FX: Tube/JFET/Fuzz 웨이브쉐이핑 (4x/8x OS)
    octaver.process (buffer);            // 5. Pre-FX: YIN 피치 추적 + 서브/옥타브 사인파 합성
    envelopeFilter.process (buffer);     // 6. Pre-FX: SVF 필터 + 엔벨로프 팔로워 변조 (피킹 반응)
    preamp.process (buffer);             // 7. 입력 이득 스테이징 + 타입별 웨이브쉐이핑 (4배 OS)
    toneStack.process (buffer);          // 8. 모델별 톤 컨트롤 (Bass/Mid/Treble 이퀄라이저)
    powerAmp.process (buffer);           // 9. 파워 포화 + Presence 피킹 필터 + Sag 압축(튜브만)
    cabinet.process (buffer);            // 10. 컨볼루션 캐비닛 IR 적용 (스피커 시뮬레이션)
}
