#include "ToneStack.h"

void ToneStack::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // prepare() 전에 coefficients를 반드시 할당해야 한다.
    // JUCE IIR::Filter는 coefficients가 nullptr인 상태로 생성된다.
    // prepare()는 상태 변수만 초기화하고 coefficients에 영향을 주지 않으므로,
    // applyPendingCoefficients()가 non-null 포인터를 사용할 수 있도록 먼저 설정.
    bassFilter.coefficients   = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 80.0f, 0.707f, 1.0f);   // 0dB 평탄 초기값
    midFilter.coefficients    = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 500.0f, 1.5f,  1.0f);   // 0dB 평탄 초기값
    trebleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 1000.0f, 0.707f, 1.0f);  // 0dB 평탄 초기값

    bassFilter.prepare (spec);
    midFilter.prepare (spec);
    trebleFilter.prepare (spec);

    // 기본값(0.5 = 0dB 평탄)으로 계수 계산 및 즉시 적용
    updateCoefficients (0.5f, 0.5f, 0.5f);
    applyPendingCoefficients();
}

void ToneStack::reset()
{
    bassFilter.reset();
    midFilter.reset();
    trebleFilter.reset();
}

void ToneStack::setParameterPointers (std::atomic<float>* bass,
                                      std::atomic<float>* mid,
                                      std::atomic<float>* treble,
                                      std::atomic<float>* enabled)
{
    bassParam    = bass;
    midParam     = mid;
    trebleParam  = treble;
    enabledParam = enabled;
}

//==============================================================================
void ToneStack::updateCoefficients (float bass, float mid, float treble)
{
    bass   = juce::jlimit (0.0f, 1.0f, bass);
    mid    = juce::jlimit (0.0f, 1.0f, mid);
    treble = juce::jlimit (0.0f, 1.0f, treble);

    // 노브 0..1  →  dB 게인 매핑 (중앙 0.5 = 0dB)
    const float bassGainDB   = (bass   - 0.5f) * 30.0f;  // -15..+15 dB
    const float midGainDB    = (mid    - 0.5f) * 24.0f;  // -12..+12 dB
    const float trebleGainDB = (treble - 0.5f) * 30.0f;  // -15..+15 dB

    // 각 밴드 계수 생성 후 raw 배열에 복사 (RT-safe 전달용)
    auto copyCoeffs = [this] (float* dest,
                               juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            dest[i] = raw[i];
    };

    // Bass: 80Hz 저역 셸빙
    copyCoeffs (pendingBassCoeffs,
        juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 80.0f, 0.707f,
            juce::Decibels::decibelsToGain (bassGainDB)));

    // Mid: 500Hz 피킹 (Q=1.5)
    copyCoeffs (pendingMidCoeffs,
        juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 500.0f, 1.5f,
            juce::Decibels::decibelsToGain (midGainDB)));

    // Treble: 1kHz 고역 셸빙
    // 베이스 기타의 실질적인 배음 에너지가 있는 최상위 구간.
    // 1kHz부터 셸빙하면 손가락 연주에서도 확실히 들린다.
    copyCoeffs (pendingTrebleCoeffs,
        juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 1000.0f, 0.707f,
            juce::Decibels::decibelsToGain (trebleGainDB)));

    coeffsNeedUpdate.store (true);
}

void ToneStack::applyPendingCoefficients()
{
    if (! coeffsNeedUpdate.exchange (false))
        return;

    // 각 필터의 기존 Coefficients 객체에 값을 직접 덮어씀 (Ptr swap 없음 → RT-safe)
    auto apply = [] (juce::dsp::IIR::Filter<float>& filter, const float* src, int n)
    {
        if (filter.coefficients == nullptr)
            return;
        auto* dst = filter.coefficients->getRawCoefficients();
        for (int i = 0; i < n; ++i)
            dst[i] = src[i];
    };

    apply (bassFilter,   pendingBassCoeffs,   maxCoeffs);
    apply (midFilter,    pendingMidCoeffs,    maxCoeffs);
    apply (trebleFilter, pendingTrebleCoeffs, maxCoeffs);
}

void ToneStack::process (juce::AudioBuffer<float>& buffer)
{
    // 비활성화 시 통과
    if (enabledParam != nullptr && enabledParam->load() < 0.5f)
        return;

    // 대기 중인 계수 적용 (메인 스레드에서 계산된 값)
    applyPendingCoefficients();

    // 3개 필터 순서대로 적용: Bass → Mid → Treble
    auto block   = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);

    bassFilter.process (context);
    midFilter.process (context);
    trebleFilter.process (context);
}
