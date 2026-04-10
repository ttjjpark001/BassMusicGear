#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 딜레이: Time/Feedback/Damping/Mix
 *
 * **신호 체인 위치**: Chorus → **Delay** → Reverb → PowerAmp
 *
 * **특징**:
 * - 클래식 에코/반향 이펙트로 신호를 시간 지연시켜 재생
 * - Feedback: 딜레이된 신호를 입력에 섞어서 반복 에코 생성
 * - Damping: 피드백 경로에 로우패스 필터 적용하여 고음 감쇠 (자연스러운 에코)
 *
 * **파라미터**:
 * - delay_enabled: ON/OFF 토글
 * - delay_time: 딜레이 시간 (10~2000ms, 최대 2초)
 * - delay_feedback: 피드백 양 (0~0.95, 1.0 이상 시 발산)
 * - delay_damping: 피드백 경로 로우패스 컷오프 (0=밝음(20kHz), 1=어두움(1kHz))
 * - delay_mix: 드라이/웨트 블렌드 (0=원본, 1=딜레이 신호 100%)
 *
 * **BPM Sync**: delay_bpm_sync ON 시 AudioPlayHead에서 BPM을 읽어
 *   노트값(1/4, 1/8, 1/8dot, 1/16, 1/4trip)에 맞게 딜레이 시간을 자동 계산.
 *   Standalone에서 PlayHead 없을 시 120 BPM 기본값 사용.
 */
class Delay
{
public:
    Delay() = default;
    ~Delay() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* time,
                               std::atomic<float>* feedback,
                               std::atomic<float>* damping,
                               std::atomic<float>* mix,
                               std::atomic<float>* bpmSync,
                               std::atomic<float>* noteValue);

    /**
     * @brief DAW 재생 BPM을 업데이트한다.
     *
     * PluginProcessor::processBlock()에서 AudioPlayHead::PositionInfo를 통해
     * 매 버퍼마다 호출된다. BPM Sync ON 시 이 값으로 딜레이 시간을 자동 계산한다.
     *
     * **공식**: delay_time_ms = (60000 / bpm) × noteFraction
     *   - 60000 = 60초 × 1000ms
     *   - noteFraction: 노트값 선택(1/4, 1/8, 1/8점, 1/16, 1/4 삼연음)
     *   - 예) 120 BPM, 1/8박 → (60000 / 120) × 0.5 = 250ms
     *
     * @param bpm  현재 DAW BPM (Standalone/PlayHead 없으면 120.0 기본값)
     * @note [오디오 스레드] atomic store — RT-safe 락프리 연산
     */
    void setBpm (double bpm);

private:
    double currentSampleRate = 44100.0;
    // 현재 DAW BPM 값 (setBpm()으로 매 버퍼마다 갱신됨, atomic으로 스레드 안전)
    std::atomic<double> currentBpm { 120.0 };

    // 순환 딜레이 버퍼: 최대 2초 길이 (2초 × SR의 샘플 수)
    std::vector<float> delayBuffer;
    // 순환 버퍼 쓰기 위치 (매 샘플 후 증가)
    int writePos = 0;

    // 1차 로우패스 필터: 피드백 경로의 고음 감쇠(댐핑)에 사용
    // 이 필터 상태 변수로 이전 출력값을 기억하여 지수 평활화 계산
    float dampingFilterState = 0.0f;

    // --- APVTS 파라미터 포인터 (실시간 폴링용) ---
    std::atomic<float>* enabledParam   = nullptr;   // ON/OFF
    std::atomic<float>* timeParam      = nullptr;   // 딜레이 시간(ms)
    std::atomic<float>* feedbackParam  = nullptr;   // 피드백 양 (0~0.95)
    std::atomic<float>* dampingParam   = nullptr;   // 댐핑 정도 (0~1)
    std::atomic<float>* mixParam       = nullptr;   // 드라이/웨트 블렌드 (0~1)
    std::atomic<float>* bpmSyncParam   = nullptr;   // BPM Sync ON/OFF
    std::atomic<float>* noteValueParam = nullptr;   // 노트값 인덱스 (0~4)

    /**
     * @brief 노트값 인덱스를 1/4박 대비 상대 길이로 변환한다.
     *
     * BPM Sync 활성화 시 사용자가 선택한 노트값(1/4박, 1/8박 등)을
     * 딜레이 시간 계산에 사용할 분수 비율로 변환한다.
     *
     * 변환 테이블:
     * - 0: 1/4박(쿼터) = 1.0
     * - 1: 1/8박(에이스) = 0.5
     * - 2: 1/8점(점 에이스) = 0.75
     * - 3: 1/16박(식스턴스) = 0.25
     * - 4: 1/4 삼연음(트리플릿) = 2/3
     *
     * @param noteIndex  노트값 선택 인덱스
     * @return           상대 길이 비율 (1/4박=1.0 기준)
     * @note 이 값에 (60000 / bpm)을 곱하면 밀리초 단위 딜레이 시간이 된다.
     */
    static float noteIndexToFraction (int noteIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Delay)
};
