#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

/**
 * @brief VCA 컴프레서 — juce::dsp::Compressor 확장
 *
 * 신호 체인 위치: Tuner 직후, BiAmp Crossover 앞.
 * 드라이브/왜곡 전에 다이나믹을 먼저 정리하는 표준 순서.
 *
 * **파라미터**:
 * - Threshold: 압축 시작 레벨 (dB)
 * - Ratio: 압축 비율 (1:1 ~ 20:1)
 * - Attack: 압축 반응 시간 (ms)
 * - Release: 압축 해제 시간 (ms)
 * - MakeupGain: 메이크업 게인 (dB)
 * - DryBlend: 패러렐 컴프레션 (0.0 = wet only, 1.0 = dry only)
 *
 * **게인 리덕션**: atomic<float>으로 UI(VUMeter)에 전달
 */
class Compressor
{
public:
    Compressor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* threshold,
                               std::atomic<float>* ratio,
                               std::atomic<float>* attack,
                               std::atomic<float>* release,
                               std::atomic<float>* makeup,
                               std::atomic<float>* dryBlend);

    /** UI에서 읽을 게인 리덕션 값 (dB, 음수) */
    float getGainReductionDb() const { return gainReductionDb.load(); }

private:
    juce::dsp::Compressor<float> compressor;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    // Dry 버퍼 (prepareToPlay에서 할당, processBlock에서 재할당 금지)
    juce::AudioBuffer<float> dryBuffer;

    // UI 전달용 atomic
    std::atomic<float> gainReductionDb { 0.0f };

    // APVTS 파라미터 포인터
    std::atomic<float>* enabledParam   = nullptr;
    std::atomic<float>* thresholdParam = nullptr;  // dB
    std::atomic<float>* ratioParam     = nullptr;
    std::atomic<float>* attackParam    = nullptr;   // ms
    std::atomic<float>* releaseParam   = nullptr;   // ms
    std::atomic<float>* makeupParam    = nullptr;   // dB
    std::atomic<float>* dryBlendParam  = nullptr;   // 0.0 ~ 1.0

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compressor)
};
