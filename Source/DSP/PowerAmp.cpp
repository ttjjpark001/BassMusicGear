#include "PowerAmp.h"

void PowerAmp::prepare (const juce::dsp::ProcessSpec& spec)
{
    // 샘플레이트를 저장해 필터 계수 계산에 사용
    sampleRate = spec.sampleRate;

    // Presence 필터 초기 계수 생성 (0.5 = 0dB 평탄)
    // prepare 시점에서는 Coefficients Ptr를 직접 할당해도 안전하다 (오디오 스레드 아님).
    presenceFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 2000.0f, 0.707f, 1.0f);  // gain=1.0 → 0dB (평탄)
    presenceFilter.prepare (spec);

    // 메인 스레드 타이머용 초기값도 설정
    updatePresenceFilter (0.5f);
    presenceNeedsUpdate.store (false);
}

void PowerAmp::reset()
{
    // Presence 필터 상태 초기화 (지연 라인)
    presenceFilter.reset();
    prevPresence = -1.0f;
}

void PowerAmp::setParameterPointers (std::atomic<float>* drive,
                                      std::atomic<float>* presence)
{
    // APVTS 파라미터 포인터를 캐시해두면, process()에서 락프리로 읽을 수 있다.
    driveParam    = drive;
    presenceParam = presence;
}

void PowerAmp::updatePresenceFilter (float presenceValue)
{
    // --- Presence 필터: 고주파 셸빙 필터 ---
    // 중심 주파수 2.5kHz에서 부스트/컷해 앰프 톤 보이싱
    // presenceValue 0..1 범위를 -6..+6 dB 게인으로 매핑
    //
    // 피킹 필터 (Q=2): 셸빙보다 집중된 대역 부스트/컷
    //   - 0 (= -15dB): 1.8kHz 컷 → 어두운 톤
    //   - 0.5 (= 0dB): 평탄 응답
    //   - 1 (= +15dB): 1.8kHz 부스트 → 베이스 어택감, 음력 있는 느낌
    //
    // Presence: 2.5kHz 피킹 필터 (Treble 1kHz 위에서 동작)
    // 베이스 기타 픽 어택, 슬랩 클릭, 상위 배음 영역.
    // Treble(1kHz)이 전체 상위 배음을 올리는 반면,
    // Presence는 2.5kHz 좁은 대역을 집중적으로 제어해 "앙상블 존재감"을 조절.
    const float gainDB = (presenceValue - 0.5f) * 12.0f;  // -6..+6 dB
    const float freq = 2000.0f;  // 셸빙 시작 주파수 (Hz)
    const float Q = 0.707f;      // Butterworth — 공진 없음, 완만한 전환

    // JUCE 하이 셸빙 필터 계수 생성
    // 피킹 필터 대비 특정 대역을 집중 부스트하지 않으므로
    // Drive 포화 후 고조파 노이즈를 덜 증폭한다.
    auto tempCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, freq, Q, juce::Decibels::decibelsToGain (gainDB));
    auto* raw = tempCoeffs->getRawCoefficients();
    for (int i = 0; i < maxCoeffs; ++i)
        pendingCoeffValues[i] = raw[i];
    presenceNeedsUpdate.store (true);
}

void PowerAmp::process (juce::AudioBuffer<float>& buffer)
{
    // --- Presence 계수 교체 (RT-safe) ---
    // pendingCoeffValues 배열의 값을 기존 Coefficients 객체에 직접 복사.
    // Ptr를 swap하지 않으므로 참조 카운트 변경이 없고 delete도 발생하지 않는다.
    if (presenceNeedsUpdate.exchange (false) && presenceFilter.coefficients != nullptr)
    {
        auto* c = presenceFilter.coefficients->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            c[i] = pendingCoeffValues[i];
    }

    // 락프리 atomic load로 Drive 파라미터 값 읽기
    const float driveAmount = driveParam != nullptr ? driveParam->load() : 0.5f;

    auto* data = buffer.getWritePointer (0);
    const int numSamples = buffer.getNumSamples();

    // --- Drive 계산 및 포화 처리 ---
    // driveAmount 0..1 범위를 지수 커브로 1..40배 게인에 매핑.
    // 지수 매핑: 낮은 drive 값에서 섬세한 워밍, 높은 값에서 강한 포화.
    //   driveAmount=0:   게인 1배  → 클린 (포화 없음)
    //   driveAmount=0.3: 게인 ~4배 → 가벼운 드라이브
    //   driveAmount=0.7: 게인 ~14배 → 중간 포화
    //   driveAmount=1:   게인 40배  → 강한 포화
    const float driveGain = std::pow (40.0f, driveAmount);

    // --- 출력 레벨 보정 ---
    // tanh 포화 후 출력은 항상 [-1, 1] 범위로 클램핑된다.
    // 단순히 driveGain으로 나누면 클린 신호와 레벨이 맞지 않으므로,
    // 고정 보정값(0.7)을 사용해 중간 drive에서 자연스러운 레벨을 유지한다.
    // 높은 drive에서는 의도적으로 출력이 약간 커지며 포화감이 살아난다.
    const float compensation = 0.7f;

    // --- 드라이브 + tanh 소프트 클리핑 ---
    for (int i = 0; i < numSamples; ++i)
    {
        // tanh(driveGain * x): drive가 클수록 포화 구간에서 동작
        // compensation: 출력 레벨을 클린 신호와 유사하게 유지
        data[i] = std::tanh (data[i] * driveGain) * compensation;
    }

    // --- Presence 필터 적용 ---
    // 파워앰프 포화 후 고주파 음질 조절 필터 적용
    // (실제 튜브 앰프 출력 트랜스 → 스피커의 주파수 응답 에뮬레이트)
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    presenceFilter.process (context);
}
