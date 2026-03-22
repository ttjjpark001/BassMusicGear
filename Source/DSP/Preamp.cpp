#include "Preamp.h"

Preamp::Preamp() = default;

void Preamp::prepare (const juce::dsp::ProcessSpec& spec)
{
    // 오버샘플링 처리 초기화
    // 오버샘플링은 모노 입력을 기대하며, 버퍼 크기를 미리 알아야 한다.
    oversampling.initProcessing (spec.maximumBlockSize);
}

void Preamp::reset()
{
    // 오버샘플링 상태 초기화 (업샘플링 필터의 지연 라인)
    oversampling.reset();
}

int Preamp::getLatencyInSamples() const
{
    // 오버샘플링 필터의 지연 시간 반환
    // 이 값은 PluginProcessor에서 PDC(Plugin Delay Compensation) 보고에 사용된다.
    // DAW가 이 값을 알면 다른 트랙과 시간을 정확히 맞출 수 있다.
    return static_cast<int> (oversampling.getLatencyInSamples());
}

void Preamp::setParameterPointers (std::atomic<float>* inputGain,
                                    std::atomic<float>* volume)
{
    // APVTS 파라미터 포인터를 캐시해두면, process()에서 락프리로 읽을 수 있다.
    inputGainParam = inputGain;
    volumeParam    = volume;
}

void Preamp::process (juce::AudioBuffer<float>& buffer)
{
    // --- 파라미터 읽기 (락프리 atomic load) ---
    const float gainDB  = inputGainParam != nullptr ? inputGainParam->load() : 0.0f;
    const float volDB   = volumeParam    != nullptr ? volumeParam->load()    : 0.0f;
    // dB → 선형 진폭 변환 (20 * log10(gain) 역변환)
    const float inGain  = juce::Decibels::decibelsToGain (gainDB);
    const float outGain = juce::Decibels::decibelsToGain (volDB);

    // --- AudioBlock 생성 (모노, 채널 0) ---
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);

    // --- 4배 업샘플링 ---
    // 원본 샘플레이트(e.g., 44.1kHz)를 4배(176.4kHz)로 상향
    auto oversampledBlock = oversampling.processSamplesUp (block);

    // --- 오버샘플된 속도에서 웨이브쉐이핑 처리 ---
    // 비선형 함수(tanh)는 많은 고조파를 생성하므로,
    // 높은 샘플레이트에서 처리해 앨리어싱을 최소화한다.
    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer (ch);
        for (size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
        {
            float x = inGain * data[i];

            // --- 비대칭 tanh 소프트 클리핑 (튜브 캐릭터) ---
            // tanh(x + 0.1*x*x): x*x 항이 비대칭성 도입
            //
            // 물리적 의미:
            //   - tanh(x): 선형 → 비선형 부드러운 전이 (포화 곡선)
            //   - +0.1*x*x 항: 양의 피크 쪽을 더 강하게 누움
            //     → 양의 반파(positive half) 쪽 포화 전압 낮음
            //     → 음의 반파(negative half) 상대적으로 약하게 누움
            //     → 비대칭 왜곡 → 짝수 고조파 강조 (2f, 4f, 6f, ...)
            //
            // 튜브 앰프 효과:
            //   실제 진공관은 비선형 I-V 특성으로 짝수 고조파가 풍부하다.
            //   이 비대칭 웨이브쉐이핑은 그 음질을 에뮬레이트한다.
            //
            // 계수 0.1f: 경험적으로 결정한 비대칭 정도
            //   - 0 → 완전 대칭 tanh
            //   - 0.1f → 부드러운 튜브 톤
            //   - 더 크면 → 더 공격적인 클리핑
            data[i] = std::tanh (x + 0.1f * x * x) * outGain;
        }
    }

    // --- 4배 다운샘플링 (원본 샘플레이트로 복원) ---
    // 다운샘플링 필터가 나이퀴스트(오버샘플 전 SR/2) 위의
    // 고조파와 앨리어싱 성분을 자동으로 제거한다.
    oversampling.processSamplesDown (block);
}
