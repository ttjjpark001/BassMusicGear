#include "GraphicEQ.h"

// Constant-Q 계산: Q = center_freq / bandwidth
// 그래픽 EQ에서 대역폭은 인접 밴드 간격으로 정의된다.
// 옥타브 기반 간격 사용: Q = sqrt(ratio) / (ratio - 1), ratio = f_upper / f_lower
// 이를 통해 모든 밴드가 동일한 "옥타브" 폭을 가지며 음악적으로 균형잡힌 응답을 제공한다.
float GraphicEQ::computeQ (int bandIndex)
{
    // 1옥타브 간격 그래픽 EQ에서 Q ≈ 1.41 (대략값)
    // 정확한 계산: 인접 밴드의 기하 평균(geometric mean)으로 대역폭 정의
    float fLow, fHigh;

    if (bandIndex == 0)
    {
        // 첫 밴드: 이전 밴드를 31Hz의 비율 관계로 외삽
        fLow  = bandFrequencies[0] / (bandFrequencies[1] / bandFrequencies[0]);
        fHigh = std::sqrt (bandFrequencies[0] * bandFrequencies[1]);
    }
    else if (bandIndex == numBands - 1)
    {
        // 마지막 밴드: 이후 밴드를 16kHz의 비율 관계로 외삽
        fLow  = std::sqrt (bandFrequencies[numBands - 2] * bandFrequencies[numBands - 1]);
        fHigh = bandFrequencies[numBands - 1] * (bandFrequencies[numBands - 1] / bandFrequencies[numBands - 2]);
    }
    else
    {
        // 중간 밴드: 양 옆 밴드의 기하 평균으로 경계 정의
        fLow  = std::sqrt (bandFrequencies[bandIndex - 1] * bandFrequencies[bandIndex]);
        fHigh = std::sqrt (bandFrequencies[bandIndex] * bandFrequencies[bandIndex + 1]);
    }

    float ratio = fHigh / fLow;
    if (ratio <= 1.0f) return 1.41f;  // 폴백값 (정상 불가능)
    return std::sqrt (ratio) / (ratio - 1.0f);
}

void GraphicEQ::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // 10개 밴드 필터 초기화
    for (int i = 0; i < numBands; ++i)
        filters[i].prepare (monoSpec);

    // 초기상태: 모든 밴드를 0dB(플랫)으로 설정
    float flatGains[numBands] = {};
    updateCoefficients (flatGains);
}

void GraphicEQ::reset()
{
    for (int i = 0; i < numBands; ++i)
        filters[i].reset();
}

void GraphicEQ::setParameterPointers (std::atomic<float>* enabled,
                                       std::atomic<float>* bandGains[numBands])
{
    enabledParam = enabled;
    for (int i = 0; i < numBands; ++i)
        bandGainParams[i] = bandGains[i];
}

void GraphicEQ::updateCoefficients (const float gains[numBands])
{
    const float sr = static_cast<float> (currentSampleRate);

    for (int i = 0; i < numBands; ++i)
    {
        const float freq   = bandFrequencies[i];
        const float gainDB = gains[i];
        const float Q      = computeQ (i);

        // Constant-Q 피킹 바이쿼드 계수 계산
        // 표준 biquad 설계식 (RBJ cookbook)에 따른 계수 도출
        const float omega = 2.0f * juce::MathConstants<float>::pi * freq / sr;
        const float sinW  = std::sin (omega);
        const float cosW  = std::cos (omega);
        const float alpha = sinW / (2.0f * Q);           // 대역폭 제어
        const float A     = std::pow (10.0f, gainDB / 40.0f);  // dB → 선형 (40 = 20*2, 양방향)

        // 분자 계수 (b)
        const float b0 =  1.0f + alpha * A;
        const float b1 = -2.0f * cosW;
        const float b2 =  1.0f - alpha * A;
        // 분모 계수 (a)
        const float a0 =  1.0f + alpha / A;
        const float a1 = -2.0f * cosW;
        const float a2 =  1.0f - alpha / A;

        // JUCE IIR 계수로 정규화 (a0으로 나눔)
        // Coefficients 포인터 할당 시 JUCE의 ReferenceCountedObjectPtr atomic swap 작동
        // → 메인 스레드에서 할당 중에도 오디오 스레드가 이전 계수를 안전하게 읽음
        filters[i].coefficients = new juce::dsp::IIR::Coefficients<float> (
            b0 / a0, b1 / a0, b2 / a0,
            1.0f,    a1 / a0, a2 / a0);
    }
}

void GraphicEQ::process (juce::AudioBuffer<float>& buffer)
{
    // Bypass 체크: geq_enabled 파라미터가 0.5 이상이면 활성화
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : true;
    if (!enabled)
        return;

    // 신호를 10개 밴드 필터를 통해 순차 처리 (in-place)
    // 각 필터는 이전 필터의 출력을 입력으로 받아 음색을 점진적으로 변형
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);

    for (int i = 0; i < numBands; ++i)
        filters[i].process (context);
}
