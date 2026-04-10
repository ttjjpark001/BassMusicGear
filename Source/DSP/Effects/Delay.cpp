#include "Delay.h"

void Delay::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // 최대 딜레이 길이 설정: 2초
    // 순환 버퍼 크기 = 샘플레이트 × 2초 + 1 (오버플로우 방지)
    // 예) 44.1kHz × 2.0 = 88200 샘플 = 약 2초
    const int maxSamples = static_cast<int> (spec.sampleRate * 2.0) + 1;
    delayBuffer.resize (static_cast<size_t> (maxSamples), 0.0f);
    writePos = 0;
    dampingFilterState = 0.0f;
}

void Delay::reset()
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writePos = 0;
    dampingFilterState = 0.0f;
}

void Delay::setParameterPointers (std::atomic<float>* enabled,
                                   std::atomic<float>* time,
                                   std::atomic<float>* feedback,
                                   std::atomic<float>* damping,
                                   std::atomic<float>* mix,
                                   std::atomic<float>* bpmSync,
                                   std::atomic<float>* noteValue)
{
    enabledParam   = enabled;
    timeParam      = time;
    feedbackParam  = feedback;
    dampingParam   = damping;
    mixParam       = mix;
    bpmSyncParam   = bpmSync;
    noteValueParam = noteValue;
}

void Delay::setBpm (double bpm)
{
    currentBpm.store (bpm);
}

float Delay::noteIndexToFraction (int noteIndex)
{
    // 노트값 인덱스를 1/4박 대비 상대 길이로 변환
    // 각 값은 BPM Sync 공식에 곱해진다:
    //   delay_ms = (60000 / bpm) × fraction
    // 예) 120 BPM일 때:
    //   - 1/4박(1.0): (60000 / 120) × 1.0 = 500ms
    //   - 1/8박(0.5): (60000 / 120) × 0.5 = 250ms
    //   - 1/4 삼연음(2/3): (60000 / 120) × 2/3 = 333ms
    // 이를 통해 DAW 템포에 자동으로 맞춰지는 에코 리듬을 생성한다.
    switch (noteIndex)
    {
        case 0: return 1.0f;        // 1/4박(쿼터 노트)
        case 1: return 0.5f;        // 1/8박(에이스 노트)
        case 2: return 0.75f;       // 1/8점(점 에이스, 3/2 × 1/8)
        case 3: return 0.25f;       // 1/16박(식스턴스 노트)
        case 4: return 2.0f / 3.0f; // 1/4 삼연음(트리플릿)
        default: return 1.0f;
    }
}

void Delay::process (juce::AudioBuffer<float>& buffer)
{
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    // --- BPM Sync: DAW 템포 기반 자동 딜레이 시간 계산 ---
    const bool bpmSyncOn = bpmSyncParam != nullptr ? bpmSyncParam->load() > 0.5f : false;

    float timeMs;
    if (bpmSyncOn)
    {
        // BPM Sync ON: AudioPlayHead에서 읽은 currentBpm과 사용자 선택 노트값으로 계산
        const double bpm = currentBpm.load();
        const int noteIdx = noteValueParam != nullptr ? static_cast<int> (noteValueParam->load()) : 0;
        const float fraction = noteIndexToFraction (noteIdx);

        // **BPM 동기화 공식**: delay_time_ms = (60000 / bpm) × noteFraction
        // - 60000 = 1분(60초) × 1000ms/초
        // - bpm: 분당 박자 수 (예: 120 BPM)
        // - noteFraction: 노트값 선택 (1/4=1.0, 1/8=0.5, 1/4삼연음=2/3)
        // **예시 계산**:
        //   120 BPM, 1/8박 선택
        //   → (60000 / 120) × 0.5 = 500 × 0.5 = 250ms
        //   → DAW가 120 BPM일 때 정확히 1/8박(8분음) 길이의 에코 반복
        timeMs = static_cast<float> ((60000.0 / bpm) * fraction);

        // 버퍼 크기 제한 준수: prepare()에서 할당한 2초 버퍼의 범위(10~2000ms)로 제한
        timeMs = juce::jlimit (10.0f, 2000.0f, timeMs);
    }
    else
    {
        // Sync OFF: 사용자가 직접 지정한 수동 딜레이 시간(timeParam) 사용
        // Time 노브에서 10~2000ms 범위로 선택 가능
        timeMs = timeParam != nullptr ? timeParam->load() : 500.0f;
    }
    const float feedback = feedbackParam != nullptr ? feedbackParam->load() : 0.3f;
    const float damping  = dampingParam  != nullptr ? dampingParam->load()  : 0.3f;
    const float mix      = mixParam      != nullptr ? mixParam->load()      : 0.5f;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);
    const int bufferSize = static_cast<int> (delayBuffer.size());

    // 딜레이 시간을 샘플 수로 변환
    // delaySamples = SR(Hz) × timeMs / 1000(ms)
    const float delaySamples = static_cast<float> (currentSampleRate * timeMs / 1000.0);

    // --- 피드백 경로 댐핑 필터 계수 계산 (1차 로우패스) ---
    // damping 파라미터는 0~1 범위, 고음 감쇠 정도를 제어한다.
    // damping=0 → 컷오프 20kHz (밝음, 고음 보존, 에코가 명확함)
    // damping=1 → 컷오프 1kHz (어두움, 고음 제거, 에코가 둥글게 감소)
    // 지수 보간으로 매끄러운 스위핑 구현
    const float dampCutoff = 20000.0f * std::pow (1000.0f / 20000.0f, damping);

    // 1차 로우패스 필터 계수 유도:
    // dampCoeff = 1 - exp(-2π × fc / SR)
    // 이 계수를 엔벨로프 팔로워처럼 사용하면:
    // state(n) = state(n-1) + dampCoeff × (input(n) - state(n-1))
    // 값이 크면 빠른 반응(높은 컷오프), 작으면 느린 반응(낮은 컷오프)
    const float dampCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampCutoff
                                              / static_cast<float> (currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // --- 순환 버퍼에서 분수 딜레이 샘플 읽기 (선형 보간) ---
        // writePos에서 delaySamples만큼 이전 위치 계산
        // 정수 부분(readIdx0)과 소수 부분(frac)을 분리하여 보간
        const float readPos = static_cast<float> (writePos) - delaySamples;
        int readIdx0 = static_cast<int> (std::floor (readPos));
        // 보간 가중치: 0~1 범위 (0=완전히 readIdx0, 1=완전히 readIdx1)
        float frac = readPos - static_cast<float> (readIdx0);

        // 모듈로 연산으로 순환 버퍼 경계 처리 (음수 인덱스도 안전하게 처리)
        readIdx0 = ((readIdx0 % bufferSize) + bufferSize) % bufferSize;
        int readIdx1 = (readIdx0 + 1) % bufferSize;

        // 선형 보간: delayed(t) = sample[idx0] × (1-frac) + sample[idx1] × frac
        // 분수 딜레이 구현으로 부드러운 피치 시프트 없는 딜레이 제공
        const float delayed = delayBuffer[static_cast<size_t> (readIdx0)] * (1.0f - frac)
                            + delayBuffer[static_cast<size_t> (readIdx1)] * frac;

        // --- 피드백 경로 댐핑 필터 (1차 로우패스) ---
        // 상태 변수 dampingFilterState는 필터의 이전 출력값을 저장
        // 이 필터가 고음을 감쇠시켜 반복 에코가 자연스럽게 감소하는 효과 제공
        // 계산식: state(n) = state(n-1) + dampCoeff × (input(n) - state(n-1))
        dampingFilterState += dampCoeff * (delayed - dampingFilterState);
        const float dampedDelayed = dampingFilterState;

        // --- 버퍼 기록: 입력신호 + 피드백(댐핑된 딜레이) ---
        // 다음 반복에서 이 값이 읽혀져 피드백 루프를 형성한다.
        // feedback < 1.0이어야 발산하지 않음 (보통 0~0.95)
        delayBuffer[static_cast<size_t> (writePos)] = inputSample + feedback * dampedDelayed;

        // --- 출력: 드라이/웨트 블렌딩 ---
        // 피드백에는 dampedDelayed를 사용하여 고음 감쇠를 반영하지만,
        // 출력 신호로는 원래의 delayed를 사용하여 명확한 에코음을 전달한다.
        // mix=0 → 원본만, mix=1 → 딜레이만
        data[i] = inputSample * (1.0f - mix) + delayed * mix;

        // 다음 샘플을 위해 쓰기 위치 증가 (순환 버퍼)
        writePos = (writePos + 1) % bufferSize;
    }
}
