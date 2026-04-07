#include "AmpVoicing.h"
#include "../Models/AmpModelLibrary.h"

void AmpVoicing::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // 모노 처리: Voicing은 L/R 채널 독립적이 아닌 단일 경로로 동작
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // 3개의 바이쿼드 필터(LowShelf, HighShelf, Peak, HighPass 등) 초기화
    for (auto& f : filters)
        f.prepare (monoSpec);
}

void AmpVoicing::reset()
{
    for (auto& f : filters)
        f.reset();
}

void AmpVoicing::setModel (AmpModelId modelId)
{
    computeCoefficients (modelId);
}

void AmpVoicing::computeCoefficients (AmpModelId modelId)
{
    const auto& model = AmpModelLibrary::getModel (modelId);

    PendingCoeffs newCoeffs;

    // 3개 밴드 각각에 대해 필터 계수 재계산
    for (int i = 0; i < maxBands; ++i)
    {
        const auto& band = model.voicingBands[(size_t) i];

        // 평탄 밴드 또는 gainDb=0(HighPass 제외): 처리 없음
        // FilterType::Flat이거나 수정이 없으면 해당 밴드 비활성화
        if (band.type == FilterType::Flat || (band.gainDb == 0.0f && band.type != FilterType::HighPass))
        {
            newCoeffs.active[(size_t) i] = false;
            newCoeffs.coeffs[(size_t) i] = nullptr;
            continue;
        }

        newCoeffs.active[(size_t) i] = true;

        switch (band.type)
        {
            case FilterType::LowShelf:
                // 저역 셸빙: 코너주파수 아래의 모든 주파수를 부스트 또는 컷
                // 앰프의 따뜻한 저음 특성 재현 (예: Ampeg SVT의 80Hz +3dB)
                newCoeffs.coeffs[(size_t) i] = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                    currentSampleRate, static_cast<float> (band.freq),
                    static_cast<float> (band.q),
                    static_cast<float> (juce::Decibels::decibelsToGain (band.gainDb)));
                break;

            case FilterType::HighShelf:
                // 고역 셸빙: 코너주파수 위의 모든 주파수를 부스트 또는 컷
                // 빈티지 앰프의 고역 롤오프 특성 재현 (예: Bassman의 5kHz -2dB)
                newCoeffs.coeffs[(size_t) i] = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    currentSampleRate, static_cast<float> (band.freq),
                    static_cast<float> (band.q),
                    static_cast<float> (juce::Decibels::decibelsToGain (band.gainDb)));
                break;

            case FilterType::Peak:
                // 피킹/벨 필터: 중심주파수 주변 좁은 대역만 부스트/컷
                // Q값이 높을수록 좁은 대역. 앰프의 프레즌스 피크 재현
                // (예: Ampeg 300Hz +2dB, Orange 500Hz +3dB)
                newCoeffs.coeffs[(size_t) i] = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, static_cast<float> (band.freq),
                    static_cast<float> (band.q),
                    static_cast<float> (juce::Decibels::decibelsToGain (band.gainDb)));
                break;

            case FilterType::HighPass:
                // 고역통과: 서브베이스 제거용 (gainDb 무시)
                // Q값으로 기울기 제어. 현대 앰프의 tight 저음 재현
                // (예: Orange/B3K의 60Hz/80Hz HighPass)
                newCoeffs.coeffs[(size_t) i] = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                    currentSampleRate, static_cast<float> (band.freq),
                    static_cast<float> (band.q));
                break;

            case FilterType::Flat:
            default:
                newCoeffs.active[(size_t) i] = false;
                newCoeffs.coeffs[(size_t) i] = nullptr;
                break;
        }
    }

    // 메인 스레드에서 계산한 계수를 pending에 저장
    // 다음 processBlock()에서 오디오 스레드가 atomic 플래그를 감지하면 적용
    {
        const juce::SpinLock::ScopedLockType lock (coeffLock);
        pending = newCoeffs;
    }
    coeffsNeedUpdate.store (true);  // 오디오 스레드에 업데이트 신호
}

void AmpVoicing::process (juce::AudioBuffer<float>& buffer)
{
    // 메인 스레드로부터의 pending 계수 적용 여부를 atomic exchange로 폴링
    // true를 받으면 새 계수를 로드하고, false로 리셋하여 다음 갱신을 대기
    if (coeffsNeedUpdate.exchange (false))
    {
        const juce::SpinLock::ScopedLockType lock (coeffLock);
        for (int i = 0; i < maxBands; ++i)
        {
            bandActive[(size_t) i] = pending.active[(size_t) i];
            if (pending.coeffs[(size_t) i] != nullptr)
                *filters[(size_t) i].coefficients = *pending.coeffs[(size_t) i];
        }
    }

    // --- Voicing 필터 체인 처리 ---
    // 활성화된 바이쿼드 필터만 순차적으로 적용 (최대 3개 직렬)
    // 예: Ampeg SVT = LowShelf(80Hz) → Peak(300Hz) → Peak(1500Hz)
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);

    for (int i = 0; i < maxBands; ++i)
    {
        if (bandActive[(size_t) i])
            filters[(size_t) i].process (context);
    }
}
