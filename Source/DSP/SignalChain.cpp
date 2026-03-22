#include "SignalChain.h"

void SignalChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    // --- 모든 DSP 모듈을 모노로 설정 ---
    // 베이스 입력은 항상 모노(채널 0)이므로 모든 모듈도 모노 처리
    // 출력은 UI 패널에서 스테레오로 복제됨
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // 신호 체인 순서대로 prepare() 호출
    noiseGate.prepare (monoSpec);
    preamp.prepare (monoSpec);
    toneStack.prepare (monoSpec);
    powerAmp.prepare (monoSpec);
    cabinet.prepare (monoSpec);
}

void SignalChain::reset()
{
    // 재생 중지 또는 노브 조절 시 모든 모듈의 상태 초기화
    noiseGate.reset();
    preamp.reset();
    toneStack.reset();
    powerAmp.reset();
    cabinet.reset();
}

int SignalChain::getTotalLatencyInSamples() const
{
    // --- 전체 신호 체인 지연 시간 ---
    // Preamp: 4배 오버샘플링 필터 지연 (~200샘플)
    // Cabinet: 균일 분할 컨볼루션 지연 (~버퍼 크기)
    // NoiseGate, ToneStack, PowerAmp: 지연 없음 (IIR 필터는 즉시 응답)
    return preamp.getLatencyInSamples() + cabinet.getLatencyInSamples();
}

void SignalChain::connectParameters (juce::AudioProcessorValueTreeState& apvts)
{
    // --- NoiseGate 파라미터 연결 ---
    noiseGate.setParameterPointers (
        apvts.getRawParameterValue ("gate_threshold"),
        apvts.getRawParameterValue ("gate_attack"),
        apvts.getRawParameterValue ("gate_hold"),
        apvts.getRawParameterValue ("gate_release"),
        apvts.getRawParameterValue ("gate_enabled"));

    // --- Preamp 파라미터 연결 ---
    preamp.setParameterPointers (
        apvts.getRawParameterValue ("input_gain"),
        apvts.getRawParameterValue ("volume"));

    // --- ToneStack 파라미터 연결 ---
    toneStack.setParameterPointers (
        apvts.getRawParameterValue ("bass"),
        apvts.getRawParameterValue ("mid"),
        apvts.getRawParameterValue ("treble"),
        nullptr);  // Phase 1: 항상 활성화 (나중에 enable 파라미터 추가 가능)

    // --- PowerAmp 파라미터 연결 ---
    powerAmp.setParameterPointers (
        apvts.getRawParameterValue ("drive"),
        apvts.getRawParameterValue ("presence"));

    // --- Cabinet 파라미터 연결 ---
    cabinet.setParameterPointers (
        apvts.getRawParameterValue ("cab_bypass"));
}

void SignalChain::updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts)
{
    // --- ToneStack 계수 업데이트 (메인 스레드 전용) ---
    // 복잡한 RC 회로 해석 + 이중 선형 변환 계산 (CPU 집약적)
    // 파라미터 값 읽기 (락프리 atomic load)
    // 변화 감지: 파라미터가 변하지 않았으면 재계산을 건너뛴다.
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

    // --- PowerAmp Presence 필터 계수 업데이트 (메인 스레드 전용) ---
    // 고주파 셸빙 필터 계수 계산
    const float presence = apvts.getRawParameterValue ("presence")->load();

    if (presence != prevPresence)
    {
        powerAmp.updatePresenceFilter (presence);
        prevPresence = presence;
    }
}

void SignalChain::process (juce::AudioBuffer<float>& buffer)
{
    // --- 신호 체인 실행 순서 ---
    // Gate → Preamp → PowerAmp → ToneStack → Cabinet
    //
    // ToneStack을 PowerAmp(Drive 포화) 뒤에 배치:
    // Drive의 tanh 포화 전에 EQ를 적용하면 고역(Treble) 부스트가
    // tanh에 의해 다시 압축되어 효과가 들리지 않음.
    // 포화 이후 EQ를 적용하면 모든 밴드가 명확하게 동작함.
    noiseGate.process (buffer);
    preamp.process (buffer);
    powerAmp.process (buffer);
    toneStack.process (buffer);
    cabinet.process (buffer);
}
