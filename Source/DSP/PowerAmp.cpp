#include "PowerAmp.h"

void PowerAmp::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // --- 4x 오버샘플링 초기화 ---
    oversampling.initProcessing (spec.maximumBlockSize);

    presenceFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 2000.0f, 0.707f, 1.0f);
    presenceFilter.prepare (spec);

    // Sag 엔벨로프 팔로워 계수 계산
    // Attack: 빠름 (~2ms) — 치는 순간의 트랜지언트를 빠르게 감지
    // Release: 느림 (~200ms) — 신호가 줄어들 때 자연스럽게 복귀하는 느낌
    // 1차 저역통과 필터 계수: coeff = 1 - exp(-1 / (SR × time_constant))
    // 값이 크면 빠른 응답, 작으면 느린 응답
    sagAttackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.002f));
    sagReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.200f));
    sagEnvelope = 0.0f;

    updatePresenceFilter (0.5f);
    presenceNeedsUpdate.store (false);
}

void PowerAmp::reset()
{
    presenceFilter.reset();
    oversampling.reset();
    sagEnvelope = 0.0f;
    prevPresence = -1.0f;
}

void PowerAmp::setPowerAmpType (PowerAmpType type, bool sagEnabled)
{
    currentType = type;
    sagActive = sagEnabled;
}

void PowerAmp::setParameterPointers (std::atomic<float>* drive,
                                      std::atomic<float>* presence,
                                      std::atomic<float>* sag)
{
    driveParam    = drive;
    presenceParam = presence;
    sagParam      = sag;
}

void PowerAmp::updatePresenceFilter (float presenceValue)
{
    const float gainDB = (presenceValue - 0.5f) * 12.0f;

    auto tempCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 2000.0f, 0.707f, juce::Decibels::decibelsToGain (gainDB));
    auto* raw = tempCoeffs->getRawCoefficients();
    for (int i = 0; i < maxCoeffs; ++i)
        pendingCoeffValues[i] = raw[i];
    presenceNeedsUpdate.store (true);
}

int PowerAmp::getLatencyInSamples() const
{
    return static_cast<int> (oversampling.getLatencyInSamples());
}

void PowerAmp::process (juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    // --- Presence 필터 계수 업데이트 (RT-safe) ---
    // 메인 스레드에서 계산한 presenceNeedsUpdate 플래그와 pendingCoeffValues를
    // atomic exchange를 사용하여 락 없이 안전하게 필터에 적용한다.
    if (presenceNeedsUpdate.exchange (false) && presenceFilter.coefficients != nullptr)
    {
        auto* c = presenceFilter.coefficients->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            c[i] = pendingCoeffValues[i];
    }

    const float driveAmount = driveParam != nullptr ? driveParam->load() : 0.5f;
    const float sagAmount   = (sagParam != nullptr && sagActive) ? sagParam->load() : 0.0f;

    // Drive 게인: 지수 곡선으로 1~40배 범위 매핑
    // driveAmount=0 → 1배(선형), driveAmount=1 → 40배(강한 포화)
    // 사용자 인지상 선형처럼 느껴지도록 지수 스케일 적용
    const float driveGain = std::pow (40.0f, driveAmount);
    // 포화 곡선(tanh 등)의 정규화 보정: 리미팅 손실을 보상하여 출력 레벨 유지
    const float compensation = 0.7f;

    // --- 4x 오버샘플링: 업샘플링 ---
    auto inputBlock = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto oversampledBlock = oversampling.processSamplesUp (inputBlock);

    const auto numOversampledSamples = static_cast<int> (oversampledBlock.getNumSamples());
    auto* data = oversampledBlock.getChannelPointer (0);

    for (int i = 0; i < numOversampledSamples; ++i)
    {
        float x = data[i];

        // --- Sag 시뮬레이션: 전압 새깅 모사 ---
        // 신호 레벨을 추적하여 강한 신호 시 게인을 동적으로 감소시킨다.
        // 튜브 파워앰프의 출력 트랜스포머 전압이 부하 증가 시 떨어지는 현상을 모사.
        // Sag는 Tube6550/TubeEL34에서만 활성화 (sagActive == true)
        float sagGainReduction = 1.0f;
        if (sagActive && sagAmount > 0.01f)
        {
            float absVal = std::abs (x * driveGain);

            // 비대칭 엔벨로프 팔로워: 공격(빠름) + 해제(느림)
            // 신호 상승 시 빠르게 추적하고, 신호 감소 시 천천히 회복된다.
            // 이 특성이 치는 순간 강하고 뒤로 갈수록 눌리는 느낌을 만든다.
            if (absVal > sagEnvelope)
                sagEnvelope += sagAttackCoeff * (absVal - sagEnvelope);
            else
                sagEnvelope += sagReleaseCoeff * (absVal - sagEnvelope);

            // Sag 게인 감소: 엔벨로프가 높을수록(신호가 강할수록) 게인을 더 많이 감소
            // sagAmount는 이펙트의 깊이를 조절 (0: 꺼짐, 1: 최대 50% 감소)
            float sagDepth = sagAmount * 0.5f;  // 최대 50% 게인 감소
            sagGainReduction = 1.0f - sagDepth * juce::jlimit (0.0f, 1.0f, sagEnvelope);
        }

        // Drive + Sag applied before type-specific saturation
        float driven = x * driveGain * sagGainReduction;

        // --- 앰프 타입별 포화 곡선 ---
        switch (currentType)
        {
            case PowerAmpType::Tube6550:
            {
                // Ampeg SVT: 부드러운 비대칭 포화
                // 비대칭 오프셋이 짝수 고조파를 강조하여 따뜻하고 둥근 톤을 생성
                // 높은 헤드룸으로 천천히, 부드럽게 포화됨
                // output = tanh(x + 0.08×x²)
                // 여기서 0.08×x² 항이 양의 반파에 더 영향을 주어 짝수 고조파 강화
                constexpr float asymOffset = 0.08f;
                data[i] = std::tanh (driven + asymOffset * driven * driven) * compensation;
                break;
            }

            case PowerAmpType::TubeEL34:
            {
                // Orange AD200: 빠른 포화, 낮은 헤드룸, 중대역 중심의 따뜻함
                // 곡률이 높을수록 포화 시작점이 낮음 (더 적은 드라이브로도 왜곡 시작)
                // output = tanh(1.8×x) / 1.8
                // 1.8배 프리게인 후 tanh로 포화, 정규화로 출력 레벨 조절
                constexpr float curvature = 1.8f;
                data[i] = std::tanh (curvature * driven) / curvature * compensation;
                break;
            }

            case PowerAmpType::SolidState:
            {
                // Darkglass B3K (Modern Micro): 풍부한 고조파의 하드 클리핑
                // x / (1 + |x|) 곡선: 칼날처럼 날카로우면서도 매끄러운 클리핑
                // 포화 후에도 부드러운 곡선을 유지하여 매우 공격적이면서 음악적
                // output = x / (1 + |x|)
                // 양쪽 방향으로 대칭적 클리핑, 짝수 고조파보다 홀수 고조파 강조
                data[i] = (driven / (1.0f + std::abs (driven))) * compensation;
                break;
            }

            case PowerAmpType::ClassD:
            {
                // Markbass Little Mark III (Italian Clean) / Origin Pure:
                // 거의 선형이면서 극단적 수준에서만 미세한 포화
                // 고조파 왜곡 최소, 초저음과 초고음까지 깔끔한 헤드룸 유지
                // output = x - 0.02×x³
                // 3차 항이 매우 작아서 극도로 드라이브되어야 포화 시작
                // 가장 "깨끗한" 톤으로 고정밀 모니터링과 믹싱 용도에 이상적
                constexpr float subtleSat = 0.02f;
                data[i] = (driven - subtleSat * driven * driven * driven) * compensation;
                break;
            }

            default:
                data[i] = std::tanh (driven) * compensation;
                break;
        }
    }

    // --- 4x 오버샘플링: 다운샘플링 ---
    oversampling.processSamplesDown (inputBlock);

    // --- Presence filter ---
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    presenceFilter.process (context);
}
