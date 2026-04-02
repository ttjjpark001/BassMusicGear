#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 옥타버: YIN 피치 트래킹 기반 서브옥타브/옥타브업 사인파 합성
 *
 * **신호 체인 위치**: Overdrive -> **Octaver** -> EnvelopeFilter -> Preamp
 *
 * **동작 원리**:
 * 1. YIN 알고리즘으로 입력 신호의 기본 주파수(F0)를 추적
 * 2. F0/2 (서브옥타브)와 F0*2 (옥타브업) 주파수의 사인파를 합성
 * 3. 입력 신호의 엔벨로프를 추적하여 합성 사인파의 진폭을 조절
 * 4. Sub Level / Oct-Up Level / Dry Level로 혼합
 *
 * **파라미터**:
 * - oct_enabled: ON/OFF
 * - oct_sub_level: 서브옥타브(-1) 레벨 (0~1)
 * - oct_up_level: 옥타브업(+1) 레벨 (0~1) [P1: 음질 개선 예정]
 * - oct_dry_level: 원본 신호 레벨 (0~1)
 *
 * **YIN 파라미터**:
 * - 버퍼: 2048 samples
 * - 범위: 41Hz (E1) ~ 330Hz (E4)
 * - 임계값: 0.15
 */
class Octaver
{
public:
    Octaver();
    ~Octaver() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* subLevel,
                               std::atomic<float>* upLevel,
                               std::atomic<float>* dryLevel);

private:
    /**
     * @brief YIN 피치 감지: 입력 버퍼에서 기본 주파수를 추정한다
     * @param data   입력 신호
     * @param numSamples  샘플 수
     * @return 추정된 주파수 (Hz). 감지 실패 시 0.0f
     */
    float detectPitch (const float* data, int numSamples);

    double currentSampleRate = 44100.0;

    // YIN 분석용 내부 버퍼 (prepareToPlay에서 할당)
    std::vector<float> yinBuffer;
    static constexpr int yinBufferSize = 2048;

    // 입력 신호 링버퍼 (YIN에 충분한 샘플 축적)
    std::vector<float> inputRingBuffer;
    int ringWritePos = 0;
    int ringSamplesAccumulated = 0;

    // YIN 분석용 연속 버퍼 (prepareToPlay에서 할당, processBlock에서 재할당 금지)
    std::vector<float> contiguousBuffer;

    // 사인파 합성용 위상 누적기
    double subPhase = 0.0;  // 서브옥타브 위상
    double upPhase  = 0.0;  // 옥타브업 위상

    // 현재 감지된 주파수 (스무딩 적용)
    float currentFrequency = 0.0f;

    // 엔벨로프 팔로워 (합성 사인파 진폭 조절용)
    float envelopeLevel = 0.0f;
    float envelopeAttack  = 0.0f;  // prepare에서 계산
    float envelopeRelease = 0.0f;

    // APVTS 파라미터 포인터
    std::atomic<float>* enabledParam  = nullptr;
    std::atomic<float>* subLevelParam = nullptr;
    std::atomic<float>* upLevelParam  = nullptr;
    std::atomic<float>* dryLevelParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Octaver)
};
