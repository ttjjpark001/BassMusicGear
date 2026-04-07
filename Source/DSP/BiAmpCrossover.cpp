#include "BiAmpCrossover.h"

void BiAmpCrossover::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    // 모노 처리: 크로스오버는 L/R 독립적이 아닌 단일 경로로 동작
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // 4개의 바이쿼드 필터(LP 2단, HP 2단) 준비
    lp1.prepare (monoSpec);
    lp2.prepare (monoSpec);
    hp1.prepare (monoSpec);
    hp2.prepare (monoSpec);

    // 기본 크로스오버 주파수로 초기화 (200Hz)
    updateCrossoverFrequency (200.0f);
}

void BiAmpCrossover::reset()
{
    lp1.reset();
    lp2.reset();
    hp1.reset();
    hp2.reset();
}

void BiAmpCrossover::updateCrossoverFrequency (float freqHz)
{
    // 유효 범위로 클램핑 (60-500 Hz)
    freqHz = juce::jlimit (60.0f, 500.0f, freqHz);

    // 2차 Butterworth LP 계수 계산
    // Q = 0.7071(= 1/sqrt(2)): Butterworth 계수로 통과대역 평탄성 최대화
    // -3dB 포인트가 freqHz에서 정의된다
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
        currentSampleRate, static_cast<float> (freqHz), 0.7071f);

    // 2차 Butterworth HP 계수 (동일한 Q)
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
        currentSampleRate, static_cast<float> (freqHz), 0.7071f);

    // 메인 스레드에서 계산한 계수를 pending에 저장하고,
    // 오디오 스레드가 atomic 플래그를 감지하면 적용하도록 신호
    const juce::SpinLock::ScopedLockType lock (coeffLock);
    pendingLPCoeffs = lpCoeffs;
    pendingHPCoeffs = hpCoeffs;
    coeffsNeedUpdate.store (true);
}

void BiAmpCrossover::setParameterPointers (std::atomic<float>* enabled,
                                            std::atomic<float>* crossoverFreq)
{
    enabledParam = enabled;
    crossoverFreqParam = crossoverFreq;
}

void BiAmpCrossover::process (const juce::AudioBuffer<float>& inputBuffer,
                               juce::AudioBuffer<float>& lpOutput,
                               juce::AudioBuffer<float>& hpOutput)
{
    const int numSamples = inputBuffer.getNumSamples();

    // Bi-Amp OFF 상태: 크로스오버 필터링 없이 입력을 양쪽에 그대로 전달
    // (클린DI와 앰프체인 모두 전대역 신호를 받음)
    if (enabledParam == nullptr || enabledParam->load() < 0.5f)
    {
        lpOutput.copyFrom (0, 0, inputBuffer, 0, 0, numSamples);
        hpOutput.copyFrom (0, 0, inputBuffer, 0, 0, numSamples);
        return;
    }

    // 메인 스레드로부터의 pending 계수를 atomic exchange로 적용
    // (오디오 스레드가 지연 없이 최신 크로스오버 주파수 반영)
    if (coeffsNeedUpdate.exchange (false))
    {
        const juce::SpinLock::ScopedLockType lock (coeffLock);
        if (pendingLPCoeffs != nullptr)
        {
            // LP: 동일한 2차 Butterworth 필터 2개 직렬 = LR4 저역통과
            // 2차 필터 * 2 = 4차 Linkwitz-Riley 필터
            *lp1.coefficients = *pendingLPCoeffs;
            *lp2.coefficients = *pendingLPCoeffs;
        }
        if (pendingHPCoeffs != nullptr)
        {
            // HP: 동일한 2차 Butterworth 필터 2개 직렬 = LR4 고역통과
            *hp1.coefficients = *pendingHPCoeffs;
            *hp2.coefficients = *pendingHPCoeffs;
        }
    }

    // --- LP 경로: 저역통과 필터 처리 (클린DI 목적지) ---
    lpOutput.copyFrom (0, 0, inputBuffer, 0, 0, numSamples);
    {
        auto lpBlock = juce::dsp::AudioBlock<float> (lpOutput).getSingleChannelBlock (0);
        auto lpContext = juce::dsp::ProcessContextReplacing<float> (lpBlock);
        // 2단 캐스케이드: lp1(2차) → lp2(2차) = 4차 LR4 저역통과
        lp1.process (lpContext);
        lp2.process (lpContext);
    }

    // --- HP 경로: 고역통과 필터 처리 (앰프 체인 목적지) ---
    hpOutput.copyFrom (0, 0, inputBuffer, 0, 0, numSamples);
    {
        auto hpBlock = juce::dsp::AudioBlock<float> (hpOutput).getSingleChannelBlock (0);
        auto hpContext = juce::dsp::ProcessContextReplacing<float> (hpBlock);
        // 2단 캐스케이드: hp1(2차) → hp2(2차) = 4차 LR4 고역통과
        hp1.process (hpContext);
        hp2.process (hpContext);
    }
}
