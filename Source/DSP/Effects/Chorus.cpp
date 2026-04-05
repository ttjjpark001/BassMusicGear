#include "Chorus.h"

void Chorus::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // Allocate circular delay buffer
    const int maxDelaySamples = static_cast<int> (spec.sampleRate * maxDelayMs / 1000.0) + 1;
    delayBuffer.resize (static_cast<size_t> (maxDelaySamples), 0.0f);
    delayWritePos = 0;

    // Center delay: 7ms
    centerDelaySamples = static_cast<float> (spec.sampleRate * 7.0 / 1000.0);

    lfoPhase = 0.0;
}

void Chorus::reset()
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    delayWritePos = 0;
    lfoPhase = 0.0;
}

void Chorus::setParameterPointers (std::atomic<float>* enabled,
                                    std::atomic<float>* rate,
                                    std::atomic<float>* depth,
                                    std::atomic<float>* mix)
{
    enabledParam = enabled;
    rateParam    = rate;
    depthParam   = depth;
    mixParam     = mix;
}

void Chorus::process (juce::AudioBuffer<float>& buffer)
{
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    const float rate  = rateParam  != nullptr ? rateParam->load()  : 1.0f;
    const float depth = depthParam != nullptr ? depthParam->load() : 0.5f;
    const float mix   = mixParam   != nullptr ? mixParam->load()   : 0.5f;

    const int numSamples = buffer.getNumSamples();
    float* data = buffer.getWritePointer (0);
    const int bufferSize = static_cast<int> (delayBuffer.size());
    const double twoPi = juce::MathConstants<double>::twoPi;

    // 변조 깊이: depth=1일 때 5ms 범위로 딜레이 변화
    const float depthSamples = depth * static_cast<float> (currentSampleRate * 5.0 / 1000.0);
    // LFO 위상 증분: rate(Hz) / SR = 한 샘플당 위상 증가량
    const double lfoIncrement = rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = data[i];

        // 순환 버퍼에 입력 샘플 기록
        delayBuffer[static_cast<size_t> (delayWritePos)] = inputSample;

        // 정현파 LFO: 0~1 위상을 sin으로 변환하여 -1~+1 범위의 변조 신호 생성
        const float lfoValue = static_cast<float> (std::sin (twoPi * lfoPhase));
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;  // 위상 래핑

        // 변조된 딜레이 시간: 중심값 ± LFO×깊이
        // LFO=+1일 때 최대 확장, LFO=-1일 때 최소 축소
        const float delaySamples = centerDelaySamples + lfoValue * depthSamples;

        // 순환 버퍼에서 분수 딜레이 샘플 읽기 (선형 보간)
        const float readPos = static_cast<float> (delayWritePos) - delaySamples;
        int readIdx0 = static_cast<int> (std::floor (readPos));
        float frac = readPos - static_cast<float> (readIdx0);  // 보간 계수 (0~1)

        // 버퍼 범위 내로 래핑 (모듈로 연산, 음수 처리 포함)
        readIdx0 = ((readIdx0 % bufferSize) + bufferSize) % bufferSize;
        int readIdx1 = (readIdx0 + 1) % bufferSize;

        // 선형 보간: delayed = s[idx0]×(1-frac) + s[idx1]×frac
        const float delayed = delayBuffer[static_cast<size_t> (readIdx0)] * (1.0f - frac)
                            + delayBuffer[static_cast<size_t> (readIdx1)] * frac;

        // 드라이/웨트 믹싱
        data[i] = inputSample * (1.0f - mix) + delayed * mix;

        delayWritePos = (delayWritePos + 1) % bufferSize;
    }
}
