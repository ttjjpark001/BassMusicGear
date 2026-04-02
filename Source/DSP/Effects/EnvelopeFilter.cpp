#include "EnvelopeFilter.h"

EnvelopeFilter::EnvelopeFilter() = default;

void EnvelopeFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // --- SVF 필터 초기화 ---
    // Bandpass 타입: 지정된 주파수 주변의 대역만 통과, Q값으로 밴드폭 제어
    svfFilter.prepare (spec);
    svfFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

    envelopeLevel = 0.0f;

    // --- 엔벨로프 팔로워 계수 계산 ---
    // Attack: ~1ms (피킹 검출 빠른 응답)
    // Release: ~30ms (필터가 부드럽게 닫힘)
    // 공식: coeff = 1 - exp(-1 / (SR * time_ms / 1000))
    const float attackMs  = 1.0f;
    const float releaseMs = 30.0f;
    envAttackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * attackMs  / 1000.0f));
    envReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (spec.sampleRate) * releaseMs / 1000.0f));
}

void EnvelopeFilter::reset()
{
    svfFilter.reset();
    envelopeLevel = 0.0f;
}

void EnvelopeFilter::setParameterPointers (std::atomic<float>* enabled,
                                             std::atomic<float>* sensitivity,
                                             std::atomic<float>* freqMin,
                                             std::atomic<float>* freqMax,
                                             std::atomic<float>* resonance,
                                             std::atomic<float>* direction)
{
    enabledParam     = enabled;
    sensitivityParam = sensitivity;
    freqMinParam     = freqMin;
    freqMaxParam     = freqMax;
    resonanceParam   = resonance;
    directionParam   = direction;
}

//==============================================================================
// 프로세스 (오디오 스레드)
//==============================================================================
void EnvelopeFilter::process (juce::AudioBuffer<float>& buffer)
{
    // --- ON/OFF 체크 ---
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    // --- 파라미터 로드 (atomic, 락프리) ---
    const float sensitivity = sensitivityParam != nullptr ? sensitivityParam->load() : 0.5f;
    const float freqMin     = freqMinParam     != nullptr ? freqMinParam->load()     : 200.0f;
    const float freqMax     = freqMaxParam     != nullptr ? freqMaxParam->load()     : 4000.0f;
    const float resonance   = resonanceParam   != nullptr ? resonanceParam->load()   : 3.0f;
    const bool  directionUp = directionParam   != nullptr ? directionParam->load() < 0.5f : true;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);

    // --- 레조넌스 범위 제한 (SVF 안정성) ---
    const float clampedQ = juce::jlimit (0.5f, 10.0f, resonance);
    svfFilter.setResonance (clampedQ);

    // --- 매 샘플마다 필터 계수 갱신 (엔벨로프-동적 변조) ---
    // 이 방식은 계산량이 높지만 아나로그 엔벨로프 필터의 자연스러운 스윕을 구현
    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // --- 엔벨로프 팔로워: 입력 신호의 진폭 추적 ---
        const float absInput = std::abs (inputSample);
        if (absInput > envelopeLevel)
            // Attack(1ms): 빠르게 상승 (피킹 감지)
            envelopeLevel += envAttackCoeff * (absInput - envelopeLevel);
        else
            // Release(30ms): 천천히 감소 (필터 닫힘)
            envelopeLevel += envReleaseCoeff * (absInput - envelopeLevel);

        // --- 엔벨로프 → 컷오프 매핑 ---
        // Sensitivity가 엔벨로프의 영향력 조절 (0~1 → 0~10배 스케일)
        float envValue = juce::jlimit (0.0f, 1.0f, envelopeLevel * sensitivity * 10.0f);

        // --- Direction: Up(엔벨로프 증가 시 컷오프 상승) vs Down(하강) ---
        float cutoff;
        if (directionUp)
            // Up: envValue=0 → freqMin, envValue=1 → freqMax (필터 열림)
            cutoff = freqMin + envValue * (freqMax - freqMin);
        else
            // Down: envValue=0 → freqMax, envValue=1 → freqMin (필터 닫힘)
            cutoff = freqMax - envValue * (freqMax - freqMin);

        // --- 컷오프 범위 제한 (SVF 안정성 및 Nyquist) ---
        // 최소 20Hz, 최대 SR*0.45 (Nyquist 안전 마진)
        cutoff = juce::jlimit (20.0f, static_cast<float> (currentSampleRate) * 0.45f, cutoff);

        // --- SVF 필터 적용 (bandpass 타입) ---
        svfFilter.setCutoffFrequency (cutoff);
        data[i] = svfFilter.processSample (0, inputSample);
    }
}
