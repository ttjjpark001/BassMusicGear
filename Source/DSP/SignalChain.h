#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects/NoiseGate.h"
#include "Tuner.h"
#include "Effects/Compressor.h"
#include "BiAmpCrossover.h"
#include "Effects/Overdrive.h"
#include "Effects/Octaver.h"
#include "Effects/EnvelopeFilter.h"
#include "Preamp.h"
#include "AmpVoicing.h"
#include "ToneStack.h"
#include "GraphicEQ.h"
#include "Effects/Chorus.h"
#include "Effects/Delay.h"
#include "Effects/Reverb.h"
#include "PowerAmp.h"
#include "Cabinet.h"
#include "DIBlend.h"
#include "../Models/AmpModel.h"
#include "../Models/AmpModelLibrary.h"

/**
 * @brief 완전한 베이스 신호 처리 체인 조립 및 관리
 *
 * **신호 체인 전체 순서**:
 * 입력 -> [NoiseGate] -> [Tuner(YIN)] -> [Compressor]
 *     -> [BiAmpCrossover] -> (LP -> cleanDI 버퍼 / HP -> amp chain)
 *         amp chain: [Overdrive] -> [Octaver] -> [EnvelopeFilter]
 *                 -> [Preamp(4xOS)] -> [AmpVoicing] -> [ToneStack]
 *                 -> [GraphicEQ(10밴드)]
 *                 -> [Chorus] -> [Delay] -> [Reverb]
 *                 -> [PowerAmp(Drive/Presence/Sag)]
 *
 * IR Position routing:
 * - Post-IR: ... -> PowerAmp -> Cabinet -> DIBlend(cleanDI) -> output
 * - Pre-IR:  ... -> PowerAmp -> DIBlend(cleanDI) -> Cabinet -> output
 */
class SignalChain
{
public:
    SignalChain() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    int getTotalLatencyInSamples() const;

    void connectParameters (juce::AudioProcessorValueTreeState& apvts);
    void updateCoefficientsFromMainThread (juce::AudioProcessorValueTreeState& apvts);

    void setAmpModel (AmpModelId modelId);

    Cabinet& getCabinet() { return cabinet; }
    Tuner& getTuner() { return tuner; }
    Compressor& getCompressor() { return compressor; }
    Delay& getDelay() { return delay; }

private:
    /** IR 이름 문자열을 cab_ir 파라미터 인덱스(0~4)로 변환한다. */
    static int irNameToIndex (const juce::String& irName)
    {
        if (irName == "ir_8x10_svt_wav")     return 0;
        if (irName == "ir_4x10_jbl_wav")     return 1;
        if (irName == "ir_1x15_vintage_wav") return 2;
        if (irName == "ir_2x12_british_wav") return 3;
        if (irName == "ir_2x10_modern_wav")  return 4;
        return 0; // fallback
    }

    // --- 신호 체인 DSP 모듈 ---
    NoiseGate       noiseGate;
    Tuner           tuner;
    Compressor      compressor;
    BiAmpCrossover  biAmpCrossover;   // LR4 크로스오버: LP->cleanDI, HP->amp chain
    Overdrive       overdrive;
    Octaver         octaver;
    EnvelopeFilter  envelopeFilter;
    Preamp          preamp;
    AmpVoicing      ampVoicing;       // 앰프별 고정 Voicing 필터 (Preamp -> AmpVoicing -> ToneStack)
    ToneStack       toneStack;
    GraphicEQ       graphicEQ;
    Chorus          chorus;
    Delay           delay;
    Reverb          reverb;
    PowerAmp        powerAmp;
    Cabinet         cabinet;
    DIBlend         diBlend;          // Clean DI + Processed 혼합

    AmpModelId currentModelId = AmpModelId::TweedBass;

    // --- 신호 분기용 버퍼 (prepareToPlay에서 할당, processBlock에서 사용) ---
    juce::AudioBuffer<float> cleanDIBuffer;   // BiAmpCrossover LP 출력 (cleanDI)
    juce::AudioBuffer<float> ampChainBuffer;  // BiAmpCrossover HP 출력 (amp chain 입력)
    juce::AudioBuffer<float> mixedBuffer;     // DIBlend 출력 (mixed)

    // --- DIBlend 파라미터 (IR Position 읽기용) ---
    std::atomic<float>* irPositionParam = nullptr;

    // --- 메인 스레드: 파라미터 변경 감지 ---
    float prevBass      = -1.0f;
    float prevMid       = -1.0f;
    float prevTreble    = -1.0f;
    float prevPresence  = -1.0f;
    float prevVpf       = -1.0f;
    float prevVle       = -1.0f;
    float prevGrunt     = -1.0f;
    float prevAttack    = -1.0f;
    int   prevMidPos    = -1;
    int   prevAmpModel  = -1;
    float prevCrossoverFreq = -1.0f;

    // GraphicEQ 파라미터 변경 감지
    float prevGEQGains[GraphicEQ::numBands] = { -999.0f, -999.0f, -999.0f, -999.0f, -999.0f,
                                                 -999.0f, -999.0f, -999.0f, -999.0f, -999.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalChain)
};
