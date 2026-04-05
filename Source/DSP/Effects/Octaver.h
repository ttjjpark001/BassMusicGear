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

    /**
     * @brief DSP 초기화: YIN 버퍼, 링 버퍼, 엔벨로프 팔로워 계수를 준비한다
     *
     * @param spec  오디오 스펙
     * @note [메인 스레드] SignalChain::prepare() 경유로 호출된다
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 오디오 버퍼를 처리한다
     *
     * 입력 신호의 기본 주파수(F0)를 YIN으로 추적하고,
     * Sub(F0/2)와 Oct-Up(F0*2) 사인파를 합성한 후 혼합한다.
     *
     * @param buffer  모노 오디오 버퍼
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief YIN 상태 및 위상 누적기를 초기화한다
     *
     * @note [오디오 스레드] 재생 중지 시 호출
     */
    void reset();

    /**
     * @brief APVTS 파라미터 포인터를 등록한다
     *
     * @param enabled   ON/OFF
     * @param subLevel   서브옥타브(-1) 레벨 (0~1)
     * @param upLevel    옥타브업(+1) 레벨 (0~1)
     * @param dryLevel   원본 신호 레벨 (0~1)
     * @note [메인 스레드] 생성 시 호출된다
     */
    void setParameterPointers (std::atomic<float>* enabled,
                               std::atomic<float>* subLevel,
                               std::atomic<float>* upLevel,
                               std::atomic<float>* dryLevel);

private:
    /**
     * @brief YIN 알고리즘으로 피치 감지
     *
     * 입력 신호의 기본 주파수(F0)를 추정한다. YIN은 자기상관 기반이므로
     * 부음역 추적에 강건하며, 베이스 기타의 41Hz~330Hz 범위에 최적화됨.
     *
     * **YIN 알고리즘 단계**:
     * 1. 차분 함수: 모든 tau에 대해 Σ(x[n]-x[n+tau])² 계산
     * 2. CMND (누적 정규화 차분): 임계값 기준으로 신뢰도 계산
     * 3. 절대 임계값: 0.15 이하인 첫 tau 검색
     * 4. 포물선 보간: 서브샘플 정확도로 정제
     *
     * @param data       입력 신호 (컨티그 버퍼, 최소 yinBufferSize 샘플)
     * @param numSamples 샘플 수
     * @return           추정된 주파수 (Hz). 감지 실패 시 0.0f
     * @note             [오디오 스레드] 매 hop(yinBufferSize/4) 샘플마다 호출
     */
    float detectPitch (const float* data, int numSamples);

    double currentSampleRate = 44100.0;

    // YIN 차분 함수 계산용 버퍼 (yinBufferSize의 절반 크기, prepare()에서 할당)
    // d[tau] = Σ(x[n]-x[n+tau])² 저장
    std::vector<float> yinBuffer;
    static constexpr int yinBufferSize = 2048;  // YIN 분석 윈도우: 46ms @ 44.1kHz

    // 입력 신호 링 버퍼: 연속 샘플을 순환 저장하여 YIN 분석에 충분한 데이터 축적
    std::vector<float> inputRingBuffer;
    int ringWritePos = 0;  // 링 버퍼의 현재 쓰기 위치
    int ringSamplesAccumulated = 0;  // 축적 샘플 카운터 (hop=512에서 한 번 YIN 실행)

    // YIN 분석용 컨티그 버퍼 (링 버퍼를 일렬로 펼침, prepare()에서 할당)
    // 링 버퍼는 순환이므로, YIN 알고리즘에 필요한 연속 메모리 영역을 따로 유지
    std::vector<float> contiguousBuffer;

    // 사인파 합성용 위상 누적기 (0~1 정규화, wrap-around 처리)
    double subPhase = 0.0;  // 서브옥타브(F0/2) 위상
    double upPhase  = 0.0;  // 옥타브업(F0*2) 위상

    // 현재 감지된 주파수 (Hz), 스무딩 적용 (이전값 70% + 새값 30%)
    // 스무딩으로 주파수 지터 감소
    float currentFrequency = 0.0f;

    // 엔벨로프 팔로워: 입력 신호의 진폭 추적, 합성 사인파 진폭 조절용
    float envelopeLevel = 0.0f;
    float envelopeAttack  = 0.0f;  // prepare에서 계산 (~5ms)
    float envelopeRelease = 0.0f;  // prepare에서 계산 (~50ms)

    // APVTS 파라미터 포인터
    std::atomic<float>* enabledParam  = nullptr;
    std::atomic<float>* subLevelParam = nullptr;
    std::atomic<float>* upLevelParam  = nullptr;
    std::atomic<float>* dryLevelParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Octaver)
};
