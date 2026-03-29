#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>

/**
 * @brief YIN 알고리즘 기반 크로매틱 튜너
 *
 * 신호 체인 위치: NoiseGate 직후, Compressor 앞.
 * 피치 감지 범위: 41Hz (E1) ~ 330Hz (E4) — 베이스 기타 전체 음역.
 *
 * **YIN 알고리즘 단계**:
 * 1. Difference function
 * 2. Cumulative mean normalized difference function
 * 3. Absolute threshold (0.1)
 * 4. Parabolic interpolation
 *
 * **UI 전달**: std::atomic<float> detectedHz, centsDeviation, noteIndex
 * **Mute 모드**: tuner_mute 파라미터가 true이면 processBlock에서 buffer.clear()
 *
 * 버퍼: 2048 samples 내부 링버퍼 (44.1kHz 기준 ~46ms)
 */
class Tuner
{
public:
    Tuner() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* referenceA,
                               std::atomic<float>* muteEnabled);

    /** UI에서 읽을 값들 (atomic, 락프리) */
    float getDetectedHz()      const { return detectedHz.load(); }
    float getCentsDeviation()  const { return centsDeviation.load(); }
    int   getNoteIndex()       const { return noteIndex.load(); }         // 0=C, 1=C#, ..., 11=B
    bool  isNoteDetected()     const { return noteDetected.load(); }

    /** 음이름 문자열 반환 (0~11 -> C, C#, D, ..., B) */
    static const char* getNoteName (int index);

private:
    /** YIN 피치 감지 수행 (내부 버퍼가 가득 찰 때 호출) */
    void detectPitch();

    /** Hz를 가장 가까운 MIDI 노트 + 센트 편차로 변환 */
    void hzToNoteAndCents (float hz, float referenceA);

    // YIN 내부 파라미터
    static constexpr int kYinBufferSize = 2048;
    static constexpr float kYinThreshold = 0.1f;

    // 내부 링버퍼
    std::array<float, kYinBufferSize> inputBuffer {};
    int inputWritePos = 0;
    bool bufferFull = false;

    // YIN 작업 버퍼
    std::array<float, kYinBufferSize / 2> yinBuffer {};

    double sampleRate = 44100.0;

    // UI 전달용 atomic 값
    std::atomic<float> detectedHz      { 0.0f };
    std::atomic<float> centsDeviation  { 0.0f };
    std::atomic<int>   noteIndex       { -1 };
    std::atomic<bool>  noteDetected    { false };

    // APVTS 파라미터 포인터
    std::atomic<float>* referenceAParam = nullptr;   // 430~450 Hz, 기본 440
    std::atomic<float>* muteParam       = nullptr;   // bool: mute 모드

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tuner)
};
