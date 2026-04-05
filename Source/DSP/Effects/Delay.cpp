#include "Delay.h"

void Delay::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // Max delay: 2 seconds
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
                                   std::atomic<float>* mix)
{
    enabledParam  = enabled;
    timeParam     = time;
    feedbackParam = feedback;
    dampingParam  = damping;
    mixParam      = mix;
}

void Delay::process (juce::AudioBuffer<float>& buffer)
{
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    const float timeMs   = timeParam     != nullptr ? timeParam->load()     : 500.0f;
    const float feedback = feedbackParam != nullptr ? feedbackParam->load() : 0.3f;
    const float damping  = dampingParam  != nullptr ? dampingParam->load()  : 0.3f;
    const float mix      = mixParam      != nullptr ? mixParam->load()      : 0.5f;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);
    const int bufferSize = static_cast<int> (delayBuffer.size());

    // 딜레이 시간을 샘플 단위로 변환
    const float delaySamples = static_cast<float> (currentSampleRate * timeMs / 1000.0);

    // 피드백 경로 댐핑: 1차 로우패스 필터 계수
    // damping=0 → 컷오프=20kHz (밝음, 고음 보존)
    // damping=1 → 컷오프=1kHz (어두움, 고음 제거)
    // 지수 보간: 20kHz^(1-damping) × 1kHz^damping
    const float dampCutoff = 20000.0f * std::pow (1000.0f / 20000.0f, damping);
    // 1차 LP 필터 계수: dampCoeff = 1 - exp(-2π × fc / SR)
    // 값이 클수록 빠른 필터 응답(높은 컷오프), 작을수록 느린 응답(낮은 컷오프)
    const float dampCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * dampCutoff
                                              / static_cast<float> (currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // 순환 버퍼에서 딜레이 샘플 읽기 (선형 보간)
        const float readPos = static_cast<float> (writePos) - delaySamples;
        int readIdx0 = static_cast<int> (std::floor (readPos));
        float frac = readPos - static_cast<float> (readIdx0);  // 보간 계수 (0~1)

        readIdx0 = ((readIdx0 % bufferSize) + bufferSize) % bufferSize;
        int readIdx1 = (readIdx0 + 1) % bufferSize;

        // 선형 보간으로 분수 딜레이 구현
        const float delayed = delayBuffer[static_cast<size_t> (readIdx0)] * (1.0f - frac)
                            + delayBuffer[static_cast<size_t> (readIdx1)] * frac;

        // 피드백 경로에 1차 로우패스 필터 적용 (댐핑)
        // 상태 변수: dampingFilterState = 필터의 이전 출력값
        // 이 필터는 고음을 감쇠시켜 반복 에코가 자연스럽게 감소하도록 함
        dampingFilterState += dampCoeff * (delayed - dampingFilterState);
        const float dampedDelayed = dampingFilterState;

        // 버퍼 기록: 입력신호 + 피드백(댐핑된 딜레이) → 다음 반복에 사용
        delayBuffer[static_cast<size_t> (writePos)] = inputSample + feedback * dampedDelayed;

        // 출력: 드라이/웨트 믹싱 (읽기 전용, 원본 delayed 사용)
        // delayed를 mix하고, 기록할 때는 dampedDelayed를 피드백에 사용
        data[i] = inputSample * (1.0f - mix) + delayed * mix;

        writePos = (writePos + 1) % bufferSize;
    }
}
