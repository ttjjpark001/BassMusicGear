#include "Overdrive.h"

Overdrive::Overdrive() = default;

void Overdrive::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // 오버샘플링 필터 계수 생성 및 버퍼 할당
    oversampling4x.initProcessing (spec.maximumBlockSize);
    oversampling8x.initProcessing (spec.maximumBlockSize);

    // 톤 필터(로우패스): Overdrive 출력의 고역 밝기 조절
    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setCutoffFrequency (8000.0f);  // 기본값 8kHz

    // 드라이 신호 저장용 버퍼 (각 블록 처리 시 입력 신호 백업)
    dryBuffer.setSize (1, static_cast<int> (spec.maximumBlockSize), false, true);

    // DC 블로커 계수 계산: ~5Hz 하이패스 특성
    // 다음 식으로 필터링: y[n] = x[n] - x[n-1] + R*y[n-1]
    dcBlockerCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f
                             / static_cast<float> (spec.sampleRate));
    dcPrevInput  = 0.0f;
    dcPrevOutput = 0.0f;
}

void Overdrive::reset()
{
    oversampling4x.reset();
    oversampling8x.reset();
    toneFilter.reset();
    dcPrevInput  = 0.0f;
    dcPrevOutput = 0.0f;
}

int Overdrive::getLatencyInSamples() const
{
    // worst-case 지연을 항상 보고: 8x 오버샘플링이 4x보다 더 많이 지연
    // 실제 활성 타입(Tube/JFET는 4x, Fuzz는 8x)과 무관하게 일관된 PDC 값 제공
    return static_cast<int> (oversampling8x.getLatencyInSamples());
}

void Overdrive::setParameterPointers (std::atomic<float>* enabled,
                                       std::atomic<float>* type,
                                       std::atomic<float>* drive,
                                       std::atomic<float>* tone,
                                       std::atomic<float>* dryBlend)
{
    enabledParam  = enabled;
    typeParam     = type;
    driveParam    = drive;
    toneParam     = tone;
    dryBlendParam = dryBlend;
}

//==============================================================================
// Tube 웨이브쉐이핑: 비대칭 tanh 소프트 클리핑 (따뜻하고 자연스러운 포화)
//==============================================================================
void Overdrive::processTube (float* data, size_t numSamples, float driveGain)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        float x = driveGain * data[i];
        // 비대칭 tanh: tanh(x + 0.15*x²) — x² 항으로 짝수 고조파 강조 (튜브 특성)
        // tanh 내부에 x² 비선형 항을 합산하여, 양의 반파와 음의 반파의 포화가 비대칭이 되고
        // 짝수 고조파(2f, 4f, ...)를 생성하여 따뜻한 음색을 만든다
        data[i] = std::tanh (x + 0.15f * x * x);
    }
}

//==============================================================================
// JFET 웨이브쉐이핑: 평행 경로 혼합 (모던 베이스 톤, 클린함 유지)
//==============================================================================
void Overdrive::processJFET (float* data, size_t numSamples, float driveGain)
{
    // Drive 값(0~1)을 혼합 비율(0~1)로 매핑: driveGain=1~20일 때 blend 최대 1.0
    const float blend = std::min (driveGain / 5.0f, 1.0f);
    // Drive가 약할수록 클린 신호 비율 높음 (cleanMix = 1 ~ 0.4)
    // Drive가 강할수록 드라이브된 신호 비율 높음
    const float cleanMix = 1.0f - blend * 0.6f;
    const float driveMix = blend;

    for (size_t i = 0; i < numSamples; ++i)
    {
        float clean = data[i];
        float x = driveGain * data[i];

        // JFET 비대칭 클리핑: 양의 반파가 음의 반파보다 부드럽게 포화 (JFET 트랜지스터 포화 특성)
        float driven;
        if (x > 0.0f)
            driven = std::tanh (x * 1.5f);  // 양의 반파: 약한 클리핑
        else
            driven = std::tanh (x * 2.0f) * 0.8f;  // 음의 반파: 더 강한 포화 후 감쇠

        // 클린 신호와 드라이브된 신호를 혼합 → 클린 톤 유지 + 그릿
        data[i] = cleanMix * clean + driveMix * driven;
    }
}

//==============================================================================
// Fuzz 웨이브쉐이핑: 하드 클리핑 (극도의 포화, 8배 OS 필수)
//==============================================================================
void Overdrive::processFuzz (float* data, size_t numSamples, float driveGain)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        float x = driveGain * data[i];
        // 하드 클리핑 + 비대칭성 (soft knee로 부드럽게 포화 진행)
        // 양의 반파: +0.7 클리핑 → 천장 +0.8, 음의 반파: -0.6 클리핑 → 바닥 -0.7
        float clipped;
        if (x > 0.7f)
            // x > 0.7에서 부드럽게 천장값 0.8에 접근 (tanh로 부드러운 transition)
            clipped = 0.7f + 0.1f * std::tanh ((x - 0.7f) * 5.0f);
        else if (x < -0.6f)
            // x < -0.6에서 부드럽게 바닥값 -0.7에 접근
            clipped = -0.6f - 0.1f * std::tanh ((-x - 0.6f) * 5.0f);
        else
            // -0.6 ≤ x ≤ 0.7: 선형 구간 (unclipped)
            clipped = x;

        data[i] = clipped;
    }
}

//==============================================================================
// 프로세스 (오디오 스레드)
//==============================================================================
void Overdrive::process (juce::AudioBuffer<float>& buffer)
{
    // --- ON/OFF 체크 ---
    const bool enabled = enabledParam != nullptr ? enabledParam->load() > 0.5f : false;
    if (!enabled)
        return;

    // --- 파라미터 로드 (atomic, 락프리) ---
    const int odType      = typeParam     != nullptr ? static_cast<int> (typeParam->load())     : 0;
    const float drive     = driveParam    != nullptr ? driveParam->load()    : 0.5f;
    const float tone      = toneParam     != nullptr ? toneParam->load()     : 0.5f;
    const float dryBlend  = dryBlendParam != nullptr ? dryBlendParam->load() : 0.0f;

    const int numSamples = buffer.getNumSamples();

    // --- 드라이 신호 백업 (Dry Blend 계산용, prepareToPlay에서 할당했으므로 재할당 없음) ---
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

    // --- Drive 게인 매핑: 0~1 → 1~20배 ---
    // drive=0 → driveGain=1 (no saturation)
    // drive=1 → driveGain=20 (heavy saturation)
    const float driveGain = 1.0f + drive * 19.0f;

    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);

    // --- 타입별 오버샘플링 + 웨이브쉐이핑 ---
    if (odType == 2) // Fuzz: 8배 오버샘플링 (극도의 포화 → 고조파 많음)
    {
        // 입력을 8배로 업샘플링 (원본 샘플레이트 × 8)
        auto oversampledBlock = oversampling8x.processSamplesUp (block);
        for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
        {
            // Fuzz 웨이브쉐이핑 적용
            processFuzz (oversampledBlock.getChannelPointer (ch),
                         oversampledBlock.getNumSamples(), driveGain);
        }
        // 8배 오버샘플 신호를 다운샘플링 (원본 샘플레이트로 복원)
        oversampling8x.processSamplesDown (block);
    }
    else // Tube(0) 또는 JFET(1): 4배 오버샘플링 (소프트 클리핑)
    {
        // 입력을 4배로 업샘플링
        auto oversampledBlock = oversampling4x.processSamplesUp (block);
        for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
        {
            auto* data = oversampledBlock.getChannelPointer (ch);
            auto nSamples = oversampledBlock.getNumSamples();

            // 타입별 웨이브쉐이핑 적용
            if (odType == 1)
                processJFET (data, nSamples, driveGain);
            else
                processTube (data, nSamples, driveGain);  // 기본값 Tube
        }
        // 4배 오버샘플 신호를 다운샘플링
        oversampling4x.processSamplesDown (block);
    }

    // --- DC 블로커 ---
    // 웨이브쉐이핑 후 축적될 수 있는 DC 성분 제거 (차분 필터)
    {
        auto* blockData = block.getChannelPointer (0);
        auto numBlockSamples = block.getNumSamples();
        const float R = dcBlockerCoeff;  // 피드백 계수 (~0.9999)
        float xPrev = dcPrevInput;
        float yPrev = dcPrevOutput;

        for (size_t i = 0; i < numBlockSamples; ++i)
        {
            float x = blockData[i];
            // 차분 필터: y[n] = x[n] - x[n-1] + R*y[n-1]
            // 이전 입출력을 기억해 DC 성분(0Hz) 제거, AC 성분은 통과
            float y = x - xPrev + R * yPrev;
            xPrev = x;
            yPrev = y;
            blockData[i] = y;
        }

        dcPrevInput  = xPrev;
        dcPrevOutput = yPrev;
    }

    // --- 톤 필터: StateVariable 로우패스 (Overdrive 출력의 고역 조절) ---
    // tone=0 → cutoff=500Hz (어두운 톤)
    // tone=1 → cutoff=12kHz (밝은 톤)
    const float cutoff = 500.0f + tone * 11500.0f;
    toneFilter.setCutoffFrequency (cutoff);
    {
        auto singleBlock = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
        auto context = juce::dsp::ProcessContextReplacing<float> (singleBlock);
        toneFilter.process (context);
    }

    // --- Dry Blend: 원본 신호와 처리된 신호 혼합 ---
    // 공식: output = dryBlend * dry + (1 - dryBlend) * wet
    // dryBlend=0 → 100% 웨트(처리음), dryBlend=1 → 100% 드라이(원본)
    {
        const float* dry = dryBuffer.getReadPointer (0);
        float* wet = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = dryBlend * dry[i] + (1.0f - dryBlend) * wet[i];
    }
}
