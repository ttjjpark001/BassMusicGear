#include "SignalChain.h"
#include <BinaryData.h>

void SignalChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // --- 신호 체인 초기화 (순서는 processBlock()의 처리 순서와 일치) ---
    noiseGate.prepare (monoSpec);
    tuner.prepare (monoSpec);
    compressor.prepare (monoSpec);
    biAmpCrossover.prepare (monoSpec);  // LP → cleanDIBuffer, HP → ampChainBuffer 분할
    overdrive.prepare (monoSpec);
    octaver.prepare (monoSpec);
    envelopeFilter.prepare (monoSpec);
    preamp.prepare (monoSpec);
    ampVoicing.prepare (monoSpec);      // Preamp와 ToneStack 사이 위치
    toneStack.prepare (monoSpec);
    graphicEQ.prepare (monoSpec);
    chorus.prepare (monoSpec);
    delay.prepare (monoSpec);
    reverb.prepare (monoSpec);
    powerAmp.prepare (monoSpec);
    cabinet.prepare (monoSpec);         // 실제 처리 위치는 process()에서 IR Position에 따라 결정
    diBlend.prepare (monoSpec);

    // --- BiAmpCrossover 출력 버퍼 할당 (processBlock 내 메모리 할당 금지) ---
    // 오디오 스레드에서 일회용 버퍼 재할당을 피하기 위해 미리 할당
    const int maxSamples = static_cast<int> (spec.maximumBlockSize);
    cleanDIBuffer.setSize (1, maxSamples);  // LP(클린 DI) 출력
    ampChainBuffer.setSize (1, maxSamples); // HP(앰프 체인) 출력
    mixedBuffer.setSize (1, maxSamples);    // 예비 버퍼 (미사용, 향후 기능용)
}

void SignalChain::reset()
{
    noiseGate.reset();
    tuner.reset();
    compressor.reset();
    biAmpCrossover.reset();
    overdrive.reset();
    octaver.reset();
    envelopeFilter.reset();
    preamp.reset();
    ampVoicing.reset();
    toneStack.reset();
    graphicEQ.reset();
    chorus.reset();
    delay.reset();
    reverb.reset();
    powerAmp.reset();
    cabinet.reset();
}

int SignalChain::getTotalLatencyInSamples() const
{
    // --- PDC(Plugin Delay Compensation) 총 지연 시간 합산 ---
    // 신호 체인 내 모든 지연을 발생시키는 모듈의 지연 시간을 합산한다.
    // DAW가 다른 트랙과의 시간 정렬(음성동기화)을 정확히 수행하도록 이 값을 보고해야 한다.

    // 지연 발생 모듈들:
    // - Overdrive: 4x 오버샘플링 (선형 위상 FIR 필터)
    // - Preamp: 4x 오버샘플링 (선형 위상 FIR 필터)
    // - PowerAmp: 4x 오버샘플링 (선형 위상 FIR 필터)
    // - Cabinet: Convolution(컨볼루션, IR 길이에 비례)

    // 각 모듈의 지연 시간은 setupProcessing()/prepare()에서 결정되므로
    // 런타임에 변경되지 않아 여기서 안전하게 합산할 수 있다.
    return overdrive.getLatencyInSamples()
         + preamp.getLatencyInSamples()
         + powerAmp.getLatencyInSamples()
         + cabinet.getLatencyInSamples();
}

void SignalChain::setAmpModel (AmpModelId modelId)
{
    currentModelId = modelId;
    const auto& model = AmpModelLibrary::getModel (modelId);

    // --- DSP 모듈 재설정 (모델별 회로 특성 로드) ---
    // ToneStack: 모델에 맞는 톤스택 타입 (Baxandall/TMB/James/VPF-VLE 등)
    // Preamp: 튜브/JFET/ClassD 등 게인 스테이징 특성
    // PowerAmp: 튜브/솔리드스테이트/ClassD 포화 곡선 + Sag 유무
    toneStack.setType (model.toneStack);
    preamp.setPreampType (model.preamp);
    powerAmp.setPowerAmpType (model.powerAmp, model.sagEnabled);

    // --- AmpVoicing 필터 계수 재계산 ---
    // 앰프 모델의 고정 Voicing 필터(voicingBands[3])를 로드하여
    // 각 바이쿼드 계수를 재계산한다.
    // (메인 스레드에서 호출, 계수는 atomic으로 오디오 스레드에 전달됨)
    ampVoicing.setModel (modelId);

    // --- 모델 기본 캐비닛 IR 로드 ---
    // 각 앰프 모델마다 대표적인 스피커 캐비닛 IR을 기본으로 설정
    // 사용자는 CabinetSelector UI에서 다른 IR로 변경 가능
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

    // --- 필터 메모리 클리어 (이전 모델의 상태 초기화) ---
    // 소음 방지 및 깔끔한 전환
    toneStack.reset();
    powerAmp.reset();
    ampVoicing.reset();

    // --- 계수 재계산 강제 실행 (updateCoefficientsFromMainThread에서) ---
    // 이전 파라미터 값과 비교해 변경이 없어도 강제 업데이트하도록 표식
    prevBass = prevMid = prevTreble = prevPresence = -1.0f;
    prevVpf = prevVle = prevGrunt = prevAttack = -1.0f;
    prevMidPos = -1;
}

void SignalChain::connectParameters (juce::AudioProcessorValueTreeState& apvts)
{
    // --- NoiseGate ---
    noiseGate.setParameterPointers (
        apvts.getRawParameterValue ("gate_threshold"),
        apvts.getRawParameterValue ("gate_attack"),
        apvts.getRawParameterValue ("gate_hold"),
        apvts.getRawParameterValue ("gate_release"),
        apvts.getRawParameterValue ("gate_enabled"));

    // --- Preamp ---
    preamp.setParameterPointers (
        apvts.getRawParameterValue ("input_gain"),
        apvts.getRawParameterValue ("volume"));

    // --- ToneStack ---
    toneStack.setParameterPointers (
        apvts.getRawParameterValue ("bass"),
        apvts.getRawParameterValue ("mid"),
        apvts.getRawParameterValue ("treble"),
        nullptr);

    // --- PowerAmp ---
    powerAmp.setParameterPointers (
        apvts.getRawParameterValue ("drive"),
        apvts.getRawParameterValue ("presence"),
        apvts.getRawParameterValue ("sag"));

    // --- Cabinet ---
    cabinet.setParameterPointers (
        apvts.getRawParameterValue ("cab_bypass"));

    // --- Tuner ---
    tuner.setParameterPointers (
        apvts.getRawParameterValue ("tuner_reference_a"),
        apvts.getRawParameterValue ("tuner_mute"));

    // --- Compressor ---
    compressor.setParameterPointers (
        apvts.getRawParameterValue ("comp_enabled"),
        apvts.getRawParameterValue ("comp_threshold"),
        apvts.getRawParameterValue ("comp_ratio"),
        apvts.getRawParameterValue ("comp_attack"),
        apvts.getRawParameterValue ("comp_release"),
        apvts.getRawParameterValue ("comp_makeup"),
        apvts.getRawParameterValue ("comp_dry_blend"));

    // --- Overdrive ---
    overdrive.setParameterPointers (
        apvts.getRawParameterValue ("od_enabled"),
        apvts.getRawParameterValue ("od_type"),
        apvts.getRawParameterValue ("od_drive"),
        apvts.getRawParameterValue ("od_tone"),
        apvts.getRawParameterValue ("od_dry_blend"));

    // --- Octaver ---
    octaver.setParameterPointers (
        apvts.getRawParameterValue ("oct_enabled"),
        apvts.getRawParameterValue ("oct_sub_level"),
        apvts.getRawParameterValue ("oct_up_level"),
        apvts.getRawParameterValue ("oct_dry_level"));

    // --- EnvelopeFilter ---
    envelopeFilter.setParameterPointers (
        apvts.getRawParameterValue ("ef_enabled"),
        apvts.getRawParameterValue ("ef_sensitivity"),
        apvts.getRawParameterValue ("ef_freq_min"),
        apvts.getRawParameterValue ("ef_freq_max"),
        apvts.getRawParameterValue ("ef_resonance"),
        apvts.getRawParameterValue ("ef_direction"));

    // --- GraphicEQ ---
    {
        std::atomic<float>* geqGains[GraphicEQ::numBands] = {
            apvts.getRawParameterValue ("geq_31"),
            apvts.getRawParameterValue ("geq_63"),
            apvts.getRawParameterValue ("geq_125"),
            apvts.getRawParameterValue ("geq_250"),
            apvts.getRawParameterValue ("geq_500"),
            apvts.getRawParameterValue ("geq_1k"),
            apvts.getRawParameterValue ("geq_2k"),
            apvts.getRawParameterValue ("geq_4k"),
            apvts.getRawParameterValue ("geq_8k"),
            apvts.getRawParameterValue ("geq_16k")
        };
        graphicEQ.setParameterPointers (
            apvts.getRawParameterValue ("geq_enabled"),
            geqGains);
    }

    // --- Chorus ---
    chorus.setParameterPointers (
        apvts.getRawParameterValue ("chorus_enabled"),
        apvts.getRawParameterValue ("chorus_rate"),
        apvts.getRawParameterValue ("chorus_depth"),
        apvts.getRawParameterValue ("chorus_mix"));

    // --- Delay ---
    delay.setParameterPointers (
        apvts.getRawParameterValue ("delay_enabled"),
        apvts.getRawParameterValue ("delay_time"),
        apvts.getRawParameterValue ("delay_feedback"),
        apvts.getRawParameterValue ("delay_damping"),
        apvts.getRawParameterValue ("delay_mix"),
        apvts.getRawParameterValue ("delay_bpm_sync"),
        apvts.getRawParameterValue ("delay_note_value"));

    // --- Reverb ---
    reverb.setParameterPointers (
        apvts.getRawParameterValue ("reverb_enabled"),
        apvts.getRawParameterValue ("reverb_type"),
        apvts.getRawParameterValue ("reverb_size"),
        apvts.getRawParameterValue ("reverb_decay"),
        apvts.getRawParameterValue ("reverb_mix"));

    // --- BiAmpCrossover ---
    biAmpCrossover.setParameterPointers (
        apvts.getRawParameterValue ("biamp_on"),
        apvts.getRawParameterValue ("crossover_freq"));

    // --- DIBlend ---
    diBlend.setParameterPointers (
        apvts.getRawParameterValue ("di_blend"),
        apvts.getRawParameterValue ("clean_level"),
        apvts.getRawParameterValue ("processed_level"),
        apvts.getRawParameterValue ("ir_position"),
        apvts.getRawParameterValue ("di_blend_on"));

    // Cache IR position param for use in process()
    irPositionParam = apvts.getRawParameterValue ("ir_position");
}

void SignalChain::updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts)
{
    // --- Amp model switch ---
    const int ampModelIndex = static_cast<int> (apvts.getRawParameterValue ("amp_model")->load());
    if (ampModelIndex != prevAmpModel)
    {
        setAmpModel (static_cast<AmpModelId> (ampModelIndex));
        prevAmpModel = ampModelIndex;
        // cab_ir 파라미터를 강제로 덮어쓰지 않는다.
        // 대신 아래 cab_ir 블록이 현재 APVTS 값으로 IR을 로드하도록 prevCabIr을 리셋한다.
        // 이렇게 하면 프리셋이 설정한 cab_ir 값이 앰프 모델 기본값으로 덮어써지지 않는다.
        prevCabIr = -1;
    }

    // --- Cabinet IR ---
    // cab_ir APVTS 파라미터가 변경될 때마다(프리셋 로드 또는 사용자 변경) 올바른 IR을 로드한다.
    const int cabIr = static_cast<int> (apvts.getRawParameterValue ("cab_ir")->load());
    if (cabIr != prevCabIr)
    {
        if      (cabIr == 0) cabinet.loadIRFromBinaryData (BinaryData::ir_8x10_svt_wav,     BinaryData::ir_8x10_svt_wavSize);
        else if (cabIr == 1) cabinet.loadIRFromBinaryData (BinaryData::ir_4x10_jbl_wav,     BinaryData::ir_4x10_jbl_wavSize);
        else if (cabIr == 2) cabinet.loadIRFromBinaryData (BinaryData::ir_1x15_vintage_wav, BinaryData::ir_1x15_vintage_wavSize);
        else if (cabIr == 3) cabinet.loadIRFromBinaryData (BinaryData::ir_2x12_british_wav, BinaryData::ir_2x12_british_wavSize);
        else if (cabIr == 4) cabinet.loadIRFromBinaryData (BinaryData::ir_2x10_modern_wav,  BinaryData::ir_2x10_modern_wavSize);
        prevCabIr = cabIr;
    }

    // --- ToneStack coefficients ---
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

    // --- PowerAmp Presence filter ---
    const float presence = apvts.getRawParameterValue ("presence")->load();
    if (presence != prevPresence)
    {
        powerAmp.updatePresenceFilter (presence);
        prevPresence = presence;
    }

    // --- GraphicEQ coefficients ---
    {
        static const char* geqIds[GraphicEQ::numBands] = {
            "geq_31", "geq_63", "geq_125", "geq_250", "geq_500",
            "geq_1k", "geq_2k", "geq_4k", "geq_8k", "geq_16k"
        };
        float currentGains[GraphicEQ::numBands];
        bool changed = false;
        for (int i = 0; i < GraphicEQ::numBands; ++i)
        {
            currentGains[i] = apvts.getRawParameterValue (geqIds[i])->load();
            if (currentGains[i] != prevGEQGains[i])
                changed = true;
        }
        if (changed)
        {
            graphicEQ.updateCoefficients (currentGains);
            for (int i = 0; i < GraphicEQ::numBands; ++i)
                prevGEQGains[i] = currentGains[i];
        }
    }

    // --- BiAmpCrossover frequency ---
    const float crossoverFreq = apvts.getRawParameterValue ("crossover_freq")->load();
    if (crossoverFreq != prevCrossoverFreq)
    {
        biAmpCrossover.updateCrossoverFrequency (crossoverFreq);
        prevCrossoverFreq = crossoverFreq;
    }

    // --- 앰프 모델 전용 파라미터 업데이트 (메인 스레드 전용) ---
    // 각 앰프 모델은 고정된 톤스택 타입을 가지며, 추가 제어 파라미터를 제공할 수 있다.
    const auto& model = AmpModelLibrary::getModel (currentModelId);

    // Italian Clean (MarkbassFourBand): VPF(Variable Pre-shape Filter) / VLE(Vintage Loudspeaker Emulator)
    // VPF: 미드 스쿱 (380Hz 노치 + 35Hz/10kHz 부스트), VLE: 가변 로우패스 고역 롤오프
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

    // Modern Micro (BaxandallGrunt): Grunt(왜곡 깊이) / Attack(반응 속도)
    // Grunt 변경 시 ToneStack의 내부 필터 깊이가 조절되므로 전체 계수 재계산 필요
    if (model.toneStack == ToneStackType::BaxandallGrunt)
    {
        const float grunt  = apvts.getRawParameterValue ("grunt")->load();
        const float attack = apvts.getRawParameterValue ("attack")->load();
        if (grunt != prevGrunt || attack != prevAttack)
        {
            toneStack.updateModernExtras (grunt, attack);
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevGrunt  = grunt;
            prevAttack = attack;
        }
    }

    // American Vintage / Origin Pure (Baxandall): Mid Position(5단계 주파수 선택)
    // Baxandall 톤스택의 미드 밴드 중심주파수를 250Hz~3kHz 범위에서 선택
    // 선택 변경 시 IIR 계수 재계산 필요
    if (model.toneStack == ToneStackType::Baxandall)
    {
        const int midPos = static_cast<int> (apvts.getRawParameterValue ("mid_position")->load());
        if (midPos != prevMidPos)
        {
            toneStack.updateMidPosition (midPos);
            toneStack.updateCoefficients (
                apvts.getRawParameterValue ("bass")->load(),
                apvts.getRawParameterValue ("mid")->load(),
                apvts.getRawParameterValue ("treble")->load());
            prevMidPos = midPos;
        }
    }
}

/**
 * @brief Process audio through the entire signal chain.
 *
 * Signal routing depends on IR Position parameter:
 *
 * Post-IR mode (ir_position=0):
 *   Input -> Gate -> Tuner -> Compressor -> BiAmpCrossover
 *   HP -> Pre-FX -> Preamp -> AmpVoicing -> ToneStack -> GEQ -> Post-FX -> PowerAmp -> Cabinet
 *   DIBlend(cleanDI, processedWithCabinet) -> output
 *
 * Pre-IR mode (ir_position=1):
 *   Input -> Gate -> Tuner -> Compressor -> BiAmpCrossover
 *   HP -> Pre-FX -> Preamp -> AmpVoicing -> ToneStack -> GEQ -> Post-FX -> PowerAmp
 *   DIBlend(cleanDI, processedNoCabinet) -> Cabinet -> output
 */
void SignalChain::process (juce::AudioBuffer<float>& buffer)
{
    // --- Pre-crossover processing ---
    noiseGate.process (buffer);
    tuner.process (buffer);
    compressor.process (buffer);

    // --- BiAmpCrossover: split into LP (cleanDI) and HP (amp chain) ---
    biAmpCrossover.process (buffer, cleanDIBuffer, ampChainBuffer);

    // --- Amp chain processing (on HP output) ---
    overdrive.process (ampChainBuffer);
    octaver.process (ampChainBuffer);
    envelopeFilter.process (ampChainBuffer);
    preamp.process (ampChainBuffer);
    ampVoicing.process (ampChainBuffer);      // Preamp -> AmpVoicing -> ToneStack
    toneStack.process (ampChainBuffer);
    graphicEQ.process (ampChainBuffer);
    chorus.process (ampChainBuffer);
    delay.process (ampChainBuffer);
    reverb.process (ampChainBuffer);
    powerAmp.process (ampChainBuffer);

    // --- IR Position routing ---
    const int irPos = (irPositionParam != nullptr)
                    ? static_cast<int> (irPositionParam->load())
                    : 0;

    if (irPos == 0)
    {
        // Post-IR: Cabinet on processed path, then DIBlend
        cabinet.process (ampChainBuffer);
        diBlend.process (cleanDIBuffer, ampChainBuffer, buffer);
    }
    else
    {
        // Pre-IR: DIBlend first, then Cabinet on mixed result
        diBlend.process (cleanDIBuffer, ampChainBuffer, buffer);
        cabinet.process (buffer);
    }
}
