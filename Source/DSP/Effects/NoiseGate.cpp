#include "NoiseGate.h"

void NoiseGate::prepare (const juce::dsp::ProcessSpec& spec)
{
    // 샘플레이트를 저장해 시간상수(ms) → 샘플 수 변환에 사용
    sampleRate = spec.sampleRate;
    reset();
}

void NoiseGate::reset()
{
    // 게이트를 닫힌 상태로 초기화하고 엔벨로프를 0으로 설정
    state = State::Closed;
    envelope = 0.0f;
    holdCounter = 0;
}

void NoiseGate::setParameterPointers (std::atomic<float>* threshold,
                                       std::atomic<float>* attack,
                                       std::atomic<float>* hold,
                                       std::atomic<float>* release,
                                       std::atomic<float>* enabled)
{
    // APVTS 파라미터 포인터를 캐시해두면, process()에서 락프리로 읽을 수 있다.
    thresholdParam = threshold;
    attackParam    = attack;
    holdParam      = hold;
    releaseParam   = release;
    enabledParam   = enabled;
}

void NoiseGate::process (juce::AudioBuffer<float>& buffer)
{
    // 게이트가 비활성화되면 신호를 통과시킨다
    if (enabledParam == nullptr || enabledParam->load() < 0.5f)
        return;

    // 락프리 atomic load로 파라미터 값 읽기
    const float thresholdDB  = thresholdParam != nullptr ? thresholdParam->load() : -40.0f;
    const float attackMs     = attackParam    != nullptr ? attackParam->load()    : 1.0f;
    const float holdMs       = holdParam     != nullptr ? holdParam->load()     : 50.0f;
    const float releaseMs    = releaseParam  != nullptr ? releaseParam->load()  : 50.0f;

    // dB → 선형 진폭 변환
    // openThreshold: 게이트가 열리는 레벨
    // closeThreshold: 게이트가 닫히는 레벨 (히스테리시스 적용)
    const float openThreshold  = juce::Decibels::decibelsToGain (thresholdDB);
    const float closeThreshold = juce::Decibels::decibelsToGain (thresholdDB - hysteresisDB);

    // --- 일차 저역통과 필터 계수 (부드러운 엔벨로프 곡선) ---
    // 시간상수 τ = T * τ_ms (T = 1/SR, τ_ms = attackMs / 1000)
    // exp(-1/τ) 형태로 계산해 매 샘플마다 지수 감소/증가 구현
    const float attackCoeff  = (attackMs  > 0.0f) ? std::exp (-1.0f / (float (sampleRate) * attackMs  * 0.001f)) : 0.0f;
    const float releaseCoeff = (releaseMs > 0.0f) ? std::exp (-1.0f / (float (sampleRate) * releaseMs * 0.001f)) : 0.0f;
    const int   holdSamples  = static_cast<int> (sampleRate * holdMs * 0.001);

    auto* data = buffer.getWritePointer (0);
    const int numSamples = buffer.getNumSamples();

    // 샘플 단위 상태 머신 처리
    for (int i = 0; i < numSamples; ++i)
    {
        const float inputLevel = std::abs (data[i]);

        switch (state)
        {
            case State::Closed:
                // 신호가 임계값을 넘으면 attack 시작
                if (inputLevel >= openThreshold)
                    state = State::Attack;
                break;

            case State::Attack:
                // 지수 곡선으로 부드럽게 엔벨로프 상승
                // envelope = envelope * coeff + (1 - coeff): RC 회로의 충전 특성
                envelope = envelope * attackCoeff + (1.0f - attackCoeff);
                if (envelope >= 0.999f)
                {
                    envelope = 1.0f;
                    state = State::Open;
                }
                break;

            case State::Open:
                // 신호가 임계값 아래로 내려가면 hold 시작
                if (inputLevel < closeThreshold)
                {
                    state = State::Hold;
                    holdCounter = holdSamples;
                }
                break;

            case State::Hold:
                // Hold 시간 대기. 다시 신호가 올라오면 open 유지, 시간 경과 후 release
                --holdCounter;
                if (inputLevel >= openThreshold)
                    state = State::Open;
                else if (holdCounter <= 0)
                    state = State::Release;
                break;

            case State::Release:
                // 지수 곡선으로 부드럽게 엔벨로프 하강
                envelope = envelope * releaseCoeff;
                if (envelope <= 0.001f)
                {
                    envelope = 0.0f;
                    state = State::Closed;
                }
                else if (inputLevel >= openThreshold)
                    // 닫히는 도중 신호가 다시 올라오면 attack으로 전환
                    state = State::Attack;
                break;
        }

        // 현재 엔벨로프로 입력 신호 변조
        data[i] *= envelope;
    }
}
