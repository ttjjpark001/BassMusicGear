#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <thread>

/**
 * @brief YIN 알고리즘 기반 크로매틱 튜너
 *
 * 신호 체인 위치: NoiseGate 직후, Compressor 앞.
 *
 * 피치 감지 범위:
 *   - 하한: 28Hz  (4현 E1=41Hz, 5현 B0=31Hz, 6현 B0=31Hz 모두 커버)
 *   - 상한: 600Hz (6현 고음 C5=523Hz 커버)
 *
 * **YIN 알고리즘 단계**:
 * 1. Difference function
 * 2. Cumulative mean normalized difference function (CMND)
 * 3. Absolute threshold (0.1) + 포물선 보간
 * 4. Hz → Note + Cents 변환
 *
 * **RT 안전성**:
 * - YIN 연산은 백그라운드 스레드에서 실행 (오디오 스레드 부하 제거)
 * - 오디오 스레드는 버퍼 복사 + atomic 플래그 설정만 수행
 * - UI 전달: std::atomic<float> detectedHz, centsDeviation, noteIndex
 *
 * **Mute 모드**: tuner_mute 파라미터가 true이면 buffer.clear()로 무음 출력
 *
 * 버퍼: 4096 samples (44.1kHz 기준 ~93ms)
 */
class Tuner
{
public:
    Tuner();
    ~Tuner();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* referenceA,
                               std::atomic<float>* muteEnabled);

    /** UI에서 읽을 값들 (atomic, 락프리) */
    float getDetectedHz()      const { return detectedHz.load(); }
    float getCentsDeviation()  const { return centsDeviation.load(); }
    int   getNoteIndex()       const { return noteIndex.load(); }   // 0=C, 1=C#, ..., 11=B
    bool  isNoteDetected()     const { return noteDetected.load(); }

    /** 음이름 문자열 반환 (0~11 -> C, C#, D, ..., B) */
    static const char* getNoteName (int index);

private:
    /** YIN 피치 감지 수행 — 백그라운드 스레드에서 pendingBuffer 분석 */
    void detectPitch();

    /** Hz를 가장 가까운 MIDI 노트 + 센트 편차로 변환 */
    void hzToNoteAndCents (float hz, float referenceA);

    /** 백그라운드 스레드 함수 — pendingReady 플래그를 감시하며 YIN 실행 */
    void analysisThreadFunc();

    // ── YIN 내부 파라미터 ───────────────────────────────────────────────
    // 4096 samples @ 44.1kHz → halfSize=2048
    //   tauMax(28Hz) = 44100/28 = 1575 (5·6현 B0 커버)
    //   tauMin(600Hz) = 44100/600 = 73  (6현 고음 C5 커버)
    static constexpr int   kYinBufferSize = 4096;
    static constexpr float kYinThreshold  = 0.1f;

    // 오디오 스레드 입력 링버퍼
    std::array<float, kYinBufferSize> inputBuffer {};
    int  inputWritePos = 0;
    bool bufferFull    = false;

    // ── 백그라운드 스레드 ────────────────────────────────────────────────
    // 오디오 스레드 → 분석 스레드 전달 버퍼 (lock-free single-producer/consumer)
    std::array<float, kYinBufferSize> pendingBuffer {};
    std::atomic<bool> pendingReady    { false };   // release: 오디오, acquire: 분석
    std::atomic<bool> threadShouldStop { false };
    std::thread       analysisThread;

    // YIN 작업 버퍼 (분석 스레드 전용, 락 불필요)
    std::array<float, kYinBufferSize / 2> yinBuffer {};

    double sampleRate = 44100.0;

    // ── UI 전달용 atomic ─────────────────────────────────────────────────
    std::atomic<float> detectedHz     { 0.0f };
    std::atomic<float> centsDeviation { 0.0f };
    std::atomic<int>   noteIndex      { -1 };
    std::atomic<bool>  noteDetected   { false };

    // APVTS 파라미터 포인터
    std::atomic<float>* referenceAParam = nullptr;  // 430~450 Hz, 기본 440
    std::atomic<float>* muteParam       = nullptr;  // bool: mute 모드

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tuner)
};
