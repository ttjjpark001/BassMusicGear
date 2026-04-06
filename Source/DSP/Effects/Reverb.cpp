#include "Reverb.h"

void Reverb::prepare (const juce::dsp::ProcessSpec& spec)
{
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    reverb.prepare (monoSpec);

    // 드라이 버퍼 미리 할당 (process()에서 RT-safe하게 드라이/웨트 믹싱하기 위함)
    // 최대 버퍼 크기만큼 미리 할당하여 오디오 스레드의 메모리 할당 회피
    dryBuffer.setSize (1, static_cast<int> (spec.maximumBlockSize));
}

void Reverb::reset()
{
    reverb.reset();
}

/**
 * @brief 오디오 스레드에서 리버브 파라미터 값을 읽을 atomic 포인터들을 등록한다.
 *
 * 메인 스레드에서 APVTS 파라미터 값이 변경되면, 오디오 스레드는 이 atomic 포인터들을 통해
 * 락프리(lock-free)로 최신 값을 읽을 수 있다.
 *
 * @param enabled  리버브 활성/비활성 (>0.5 → enabled)
 * @param type     리버브 타입 (0=Spring, 1=Room, 2=Hall, 3=Plate)
 * @param size     방의 크기 (0~1)
 * @param decay    감쇠 시간/잔향 길이 (0~1, 커질수록 길어짐)
 * @param mix      드라이/웨트 믹스 비율 (0=dry, 1=wet)
 *
 * @note [메인 스레드] AudioProcessorValueTreeState로부터 파라미터를 바인딩할 때 호출.
 *       processBlock() 내에서 이 포인터들을 통해 getValue() 호출 금지. 대신 load()로 원자적 읽기.
 */
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

    // --- 타입별 리버브 파라미터 구성 ---
    // 각 타입은 고유한 roomSize/damping/width 특성을 가지며, 사용자의 size/decay 노브로 세부 조절.
    // damping이 높을수록 고주파가 빨리 감쇠 → 따뜻한 음색.
    // damping이 낮을수록 고주파가 오래 남음 → 밝은 음색.
    juce::dsp::Reverb::Parameters params;

    const int typeIdx = juce::roundToInt (type);

    if (typeIdx == 0)
    {
        // Spring: 빈티지 스프링 탱크 앰프의 짧은 반향 (Fender Reverb 스타일)
        // 좁은 스테레오(width=0.5), 높은 damping으로 빠른 감쇠 → 콤팩트한 느낌
        params.roomSize   = 0.3f + size * 0.3f;      // 0.3~0.6 (작은 공간)
        params.damping    = 0.3f + (1.0f - decay) * 0.4f;  // decay↓ → damping↑ (고주파 빨리 컷)
        params.wetLevel   = 1.0f;
        params.dryLevel   = 0.0f;
        params.width      = 0.5f;  // 좁은 스테레오 폭
        params.freezeMode = 0.0f;
    }
    else if (typeIdx == 1)
    {
        // Room: 작은 연습실/홈스튜디오의 자연스러운 음향 반사 (인접한 벽의 조기 반사)
        // 중간 크기(roomSize 0.4~0.95), 따뜻한 톤 → 친근하고 정확한 느낌
        params.roomSize   = 0.4f + size * 0.55f;     // 0.4~0.95 (중간 크기)
        params.damping    = 0.4f + (1.0f - decay) * 0.5f;  // 따뜻한 톤 (중간 damping)
        params.wetLevel   = 1.0f;
        params.dryLevel   = 0.0f;
        params.width      = 0.8f;  // 자연스러운 스테레오 폭
        params.freezeMode = 0.0f;
    }
    else if (typeIdx == 2)
    {
        // Hall: 대형 콘서트홀의 웅장한 잔향 (클래식, 오케스트라 악기)
        // 큰 공간(roomSize 항상 0.7 이상), 낮은 damping으로 밝고 긴 잔향 → 호화로운 느낌
        params.roomSize   = 0.7f + size * 0.3f;          // 0.7~1.0 (항상 큰 공간)
        params.damping    = 0.1f + (1.0f - decay) * 0.4f;  // 밝고 개방적 (낮은 damping)
        params.wetLevel   = 1.0f;
        params.dryLevel   = 0.0f;
        params.width      = 1.0f;  // 최대 스테레오 폭 (넓은 공간감)
        params.freezeMode = 0.0f;
    }
    else
    {
        // Plate: 금속판 리버브 (EMT 140 스타일)
        // 매우 낮은 damping으로 초기 반사가 선명하고 밀도 높음 → 모던, 선명한 음색
        params.roomSize   = 0.5f + size * 0.35f;       // 0.5~0.85 (중간~큰 크기)
        params.damping    = 0.1f + (1.0f - decay) * 0.3f;  // 매우 낮은 damping → 밝고 청명
        params.wetLevel   = 1.0f;
        params.dryLevel   = 0.0f;
        params.width      = 0.9f;  // 매우 넓은 스테레오 폭 (금속판의 독특한 전개)
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
