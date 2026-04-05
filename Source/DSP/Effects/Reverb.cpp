#include "Reverb.h"

void Reverb::prepare (const juce::dsp::ProcessSpec& spec)
{
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    reverb.prepare (monoSpec);

    // 드라이 버퍼 미리 할당 (processBlock에서 RT-safe하게 건너뛰기 위함)
    // 최대 버퍼 크기만큼 미리 할당하여 오디오 스레드의 메모리 할당 회피
    dryBuffer.setSize (1, static_cast<int> (spec.maximumBlockSize));
}

void Reverb::reset()
{
    reverb.reset();
}

void Reverb::setParameterPointers (std::atomic<float>* enabled,
                                    std::atomic<float>* type,
                                    std::atomic<float>* size,
                                    std::atomic<float>* decay,
                                    std::atomic<float>* mix)
{
    enabledParam = enabled;
    typeParam    = type;
    sizeParam    = size;
    decayParam   = decay;
    mixParam     = mix;
}

void Reverb::process (juce::AudioBuffer<float>& buffer)
{
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    const float type  = typeParam  != nullptr ? typeParam->load()  : 0.0f;
    const float size  = sizeParam  != nullptr ? sizeParam->load()  : 0.5f;
    const float decay = decayParam != nullptr ? decayParam->load() : 0.5f;
    const float mix   = mixParam   != nullptr ? mixParam->load()   : 0.3f;

    const int numSamples = buffer.getNumSamples();

    // 리버브 파라미터 설정: 타입에 따라 다른 음향 특성 제공
    juce::dsp::Reverb::Parameters params;

    const bool isSpring = type < 0.5f;

    if (isSpring)
    {
        // Spring 리버브: 짧은 감쇠, 밝음, 많은 반향
        // roomSize: 0.3~0.6 범위 (작을수록 촉박함)
        // damping: (1-decay)로 제어 → decay=0일 때 damping↑(어두움), decay=1일 때 damping↓(밝음)
        params.roomSize   = 0.3f + size * 0.3f;
        params.damping    = 0.3f + (1.0f - decay) * 0.4f;
        params.wetLevel   = 1.0f;  // 출력: wet만 사용
        params.dryLevel   = 0.0f;  // dry는 믹스 단계에서 별도 처리
        params.width      = 0.5f;  // 스테레오 폭 (Spring = 좁음)
        params.freezeMode = 0.0f;
    }
    else
    {
        // Room 리버브: 긴 감쇠, 따뜻함, 자연스러운 공간감
        // roomSize: 0.4~0.95 범위 (클수록 넓은 홀)
        // damping: decay=0일 때 damping↑(고음 제거 → 따뜻한 음색), decay=1일 때 damping↓(밝음)
        params.roomSize   = 0.4f + size * 0.55f;
        params.damping    = 0.4f + (1.0f - decay) * 0.5f;
        params.wetLevel   = 1.0f;
        params.dryLevel   = 0.0f;
        params.width      = 0.8f;  // 스테레오 폭 (Room = 넓음)
        params.freezeMode = 0.0f;
    }

    reverb.setParameters (params);

    // 드라이 신호 백업: 믹싱을 위해 처리 전 신호 보존
    // 버퍼는 prepare()에서 이미 할당됨 (RT-safe)
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

    // 웨트 신호 처리: 리버브 알고리즘 적용 (in-place)
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    reverb.process (context);

    // 드라이/웨트 믹싱: 원본 신호와 리버브 신호를 비율에 따라 합성
    // mix=0 → dry(원본) 100%, mix=1 → wet(리버브) 100%
    const float* dry = dryBuffer.getReadPointer (0);
    float* wet = buffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
}
