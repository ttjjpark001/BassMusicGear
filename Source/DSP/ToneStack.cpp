// 5종 앰프 톤스택 구현: TMB(상호작용형) / Baxandall / James / BaxandallGrunt / MarkbassFourBand
// 각 토폴로지별로 다른 IIR 계수 계산 알고리즘을 사용하며, RT-safe 원자 플래그로 동기화한다.
#include "ToneStack.h"

//==============================================================================
// Prepare / Reset — DSP 초기화 및 클리어
//==============================================================================

/**
 * @brief ToneStack DSP 초기화: 모든 필터를 준비하고 기본 계수를 설정한다.
 *
 * @param spec  오디오 스펙 (sampleRate, samplesPerBlock 등)
 * @note [메인 스레드] PluginProcessor::prepareToPlay()에서 호출된다.
 */
void ToneStack::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // --- 모든 필터를 평탄 응답(1.0 게인 피크 필터)으로 초기화 ---
    // 실제 계수는 이후 updateCoefficients()에서 설정된다.
    for (int i = 0; i < maxFilters; ++i)
    {
        filters[i].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 1000.0f, 0.707f, 1.0f);  // 1kHz, Q=0.707, gain=1.0 (평탄)
        filters[i].prepare (spec);
    }

    // --- VLE 필터 초기화 (Italian Clean 전용) ---
    // Markbass VLE: StateVariableTPTFilter 로우패스, 20kHz부터 시작 (전대역 통과)
    vleFilter.prepare (spec);
    vleFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    vleFilter.setCutoffFrequency (20000.0f);

    // --- 기본 계수 설정: Bass/Mid/Treble = 0.5 (중앙) → 평탄 응답 ---
    updateCoefficients (0.5f, 0.5f, 0.5f);
    applyPendingCoefficients();  // 즉시 적용
}

/**
 * @brief 모든 필터 버퍼를 클리어하여 초기 상태로 리셋한다.
 *
 * @note [오디오 스레드] processBlock() 시작 또는 모델 전환 시 호출.
 *       이전 상태의 잔향 제거 (팝 노이즈 방지).
 */
void ToneStack::reset()
{
    for (int i = 0; i < maxFilters; ++i)
        filters[i].reset();

    vleFilter.reset();
}

/**
 * @brief 톤스택 토폴로지를 설정하고 필터 뱅크 구성을 갱신한다.
 *
 * @param type  ToneStackType (TMB, Baxandall, James, BaxandallGrunt, MarkbassFourBand)
 * @note [메인 스레드 전용] 앰프 모델 전환 시 호출된다.
 */
void ToneStack::setType (ToneStackType type)
{
    currentType = type;

    // 토폴로지별로 활성 필터 개수 및 VLE 활성 여부를 설정한다.
    // 사용하지 않는 필터는 계수를 갱신하지 않으므로 오버헤드가 없다.
    switch (type)
    {
        case ToneStackType::TMB:
            // Fender TMB: 3개 연속 시간 모델을 이산화 (상호작용 있음)
            activeFilterCount = 3;
            vleActive = false;
            break;
        case ToneStackType::Baxandall:
            // Ampeg SVT 스타일: 4개 독립 필터 (Bass/Mid/Treble + reserve)
            activeFilterCount = 4;
            vleActive = false;
            break;
        case ToneStackType::James:
            // British Stack: 3개 필터 (Bass shelving / Mid peaking / Treble shelving)
            activeFilterCount = 3;
            vleActive = false;
            break;
        case ToneStackType::BaxandallGrunt:
            // Modern Micro: 5개 필터 (Bass/Mid/Treble + Grunt HPF + Grunt LPF)
            activeFilterCount = 5;
            vleActive = false;
            break;
        case ToneStackType::MarkbassFourBand:
            // Italian Clean: 7개 필터 (40/360/800/10kHz + VPF 저셸프 + VPF 노치 + VPF 고셸프)
            // + VLE 로우패스(StateVariableTPTFilter) 별도
            activeFilterCount = 7;
            vleActive = true;  // VLE(Vintage Loudspeaker Emulator) 로우패스 활성화
            break;
    }
}

/**
 * @brief APVTS 파라미터 원자 포인터를 저장한다 (선택적).
 *
 * 오디오 스레드가 이 포인터들을 폴링하여 real-time 파라미터 변경을 감지할 수 있다.
 *
 * @param bass     Bass 파라미터 atomic<float>* 포인터
 * @param mid      Mid 파라미터 atomic<float>* 포인터
 * @param treble   Treble 파라미터 atomic<float>* 포인터
 * @param enabled  ToneStack 활성화 플래그 atomic<float>* 포인터
 * @note [메인 스레드] 생성 시 호출되거나 NULL 유지 가능.
 */
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
// 계수 계산 — 메인 스레드 전용
//==============================================================================

/**
 * @brief Bass/Mid/Treble 파라미터로부터 톤스택 IIR 계수를 계산한다.
 *
 * 각 토폴로지의 compute*Coefficients() 함수를 호출하여
 * pendingCoeffs 배열에 결과를 저장한다.
 * 실제 필터 적용은 오디오 스레드의 applyPendingCoefficients()에서 이루어진다.
 *
 * @param bass    Bass 파라미터 (0.0 ~ 1.0, 자동 clamp)
 * @param mid     Mid 파라미터 (0.0 ~ 1.0)
 * @param treble  Treble 파라미터 (0.0 ~ 1.0)
 * @note [메인 스레드 전용] UI 변경 또는 프리셋 로드 시 호출된다.
 *       RT-safe: 원자 플래그(coeffsNeedUpdate)만 사용하여 오디오 스레드와 동기화.
 */
void ToneStack::updateCoefficients (float bass, float mid, float treble)
{
    // 파라미터 범위 안전성 확인 (UI에서 이미 done되지만 redundant check)
    bass   = juce::jlimit (0.0f, 1.0f, bass);
    mid    = juce::jlimit (0.0f, 1.0f, mid);
    treble = juce::jlimit (0.0f, 1.0f, treble);

    // 현재 톤스택 토폴로지에 맞는 계수 계산 함수 호출
    switch (currentType)
    {
        case ToneStackType::TMB:
            computeTMBCoefficients (bass, mid, treble);
            break;
        case ToneStackType::Baxandall:
            computeBaxandallCoefficients (bass, mid, treble);
            break;
        case ToneStackType::James:
            computeJamesCoefficients (bass, mid, treble);
            break;
        case ToneStackType::BaxandallGrunt:
            computeBaxandallGruntCoefficients (bass, mid, treble);
            break;
        case ToneStackType::MarkbassFourBand:
            computeMarkbassCoefficients (bass, mid, treble);
            break;
    }

    // RT-safe: 원자 플래그 설정 → 오디오 스레드가 폴링하여 applyPendingCoefficients() 호출
    coeffsNeedUpdate.store (true);
}

/**
 * @brief American Vintage(Baxandall) 톤스택 전용: Mid Position 설정
 *
 * Baxandall 미드 밴드의 중심 주파수를 5개 선택지 중 선택한다.
 *
 * @param position  0~4 (250Hz, 500Hz, 800Hz, 1.5kHz, 3kHz)
 * @note [메인 스레드 전용] ComboBox 변경 시 호출된다.
 */
void ToneStack::updateMidPosition (int position)
{
    // 범위 검증: 0~4 범위 내로 clamp
    currentMidPosition = juce::jlimit (0, 4, position);
}

/**
 * @brief Italian Clean(MarkbassFourBand) 톤스택 전용: VPF/VLE 계수 업데이트
 *
 * **VPF (Variable Pre-shape Filter)** — 3필터 합산으로 구현:
 * ① 35Hz 로우셸프 부스트: 저음 강조
 * ② 380Hz 피킹 컷(노치): 중음 영역 제거
 * ③ 10kHz 하이셸프 부스트: 고음 강조
 * 모든 필터의 깊이가 VPF 노브(0~1)에 선형 비례: 0~12dB
 *
 * **VLE (Vintage Loudspeaker Emulator)** — StateVariableTPTFilter 로우패스:
 * 차단 주파수: 0%(노브) = 20kHz (전대역), 100% = 4kHz (저음 강조)
 *
 * @param vpf  VPF 깊이 (0.0 ~ 1.0 → 0 ~ 12dB)
 * @param vle  VLE 로우패스 깊이 (0.0 ~ 1.0 → 20kHz ~ 4kHz)
 * @note [메인 스레드 전용] PluginProcessor가 주기적으로 호출.
 */
void ToneStack::updateMarkbassExtras (float vpf, float vle)
{
    vpf = juce::jlimit (0.0f, 1.0f, vpf);
    vle = juce::jlimit (0.0f, 1.0f, vle);

    // --- VPF 계수 계산: 3개 필터 (35Hz shelf / 380Hz notch / 10kHz shelf) ---
    // VPF 깊이: 0 = 중립 (1.0 게인), 1 = 최대 (12dB 부스트/컷)
    const float vpfDepthDB = vpf * 12.0f;  // 0dB (vpf=0) ~ 12dB (vpf=1)

    auto copyCoeffs = [this] (int idx, juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            pendingCoeffs[idx][i] = raw[i];
    };

    // VPF: 3개 필터 직렬 배치
    //   filter[4] = 35Hz 저셸프 부스트 (서브베이스 강조)
    //   filter[5] = 380Hz 노치 (미드 스쿱)
    //   filter[6] = 10kHz 고셸프 부스트 (고역 에어 강조)
    if (vpfDepthDB > 0.1f)
    {
        // 35Hz 저셸프: 서브베이스 부스트
        auto lowShelf = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 35.0f, 0.707f,
            juce::Decibels::decibelsToGain (vpfDepthDB));
        copyCoeffs (4, lowShelf);

        // 380Hz 노치: 미드 스쿱 (피킹 컷)
        auto notch = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 380.0f, 2.0f,
            juce::Decibels::decibelsToGain (-vpfDepthDB));
        copyCoeffs (5, notch);

        // 10kHz 고셸프: 고역 에어 부스트
        auto highShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 10000.0f, 0.707f,
            juce::Decibels::decibelsToGain (vpfDepthDB));
        copyCoeffs (6, highShelf);
    }
    else
    {
        // VPF off: 전 슬롯 평탄 (패스스루)
        auto flat = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 1000.0f, 0.707f, 1.0f);
        copyCoeffs (4, flat);
        copyCoeffs (5, flat);
        copyCoeffs (6, flat);
    }

    // VLE: StateVariableTPTFilter lowpass cutoff mapping
    // 0 = 20kHz (flat), max = 4kHz
    pendingVleCutoff = 20000.0f * std::pow (4000.0f / 20000.0f, vle);
    vleNeedsUpdate.store (true);

    coeffsNeedUpdate.store (true);
}

void ToneStack::updateModernExtras (float grunt, float attack)
{
    currentGrunt = juce::jlimit (0.0f, 1.0f, grunt);
    currentAttack = juce::jlimit (0.0f, 1.0f, attack);
}

//==============================================================================
// TMB: Fender TMB passive RC network (simplified Yeh 2006)
//==============================================================================

void ToneStack::computeTMBCoefficients (float bass, float mid, float treble)
{
    // Fender Bassman 5F6-A 수동 RC 네트워크 근사 구현
    //
    // 핵심 물리 특성: Bass(R1=250kΩ)와 Treble(C1=250pF 경유) 컨트롤이
    // Mid 섹션(R3=25kΩ, C2=20nF)의 임피던스 경로를 공유한다.
    // Bass와 Treble을 동시에 부스트하면 공통 임피던스 로딩으로 인해
    // 미드 대역(~500Hz)에 자연 감쇠(스쿱)가 발생한다 — TMB의 특징적 "스마일 커브".
    //
    // 구현: 독립 필터 3개 + bass/treble 부스트 교차 항(cross-term)으로 상호작용 근사
    // (완전한 이산화 전달 함수 대신 경험적 근사로 핵심 음색 특성을 재현)

    auto copyCoeffs = [this] (int idx, juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            pendingCoeffs[idx][i] = raw[i];
    };

    const float fs = (float)sampleRate;

    // 각 컨트롤의 게인(dB) — 중앙(0.5) = 0dB
    const float bassGainDB   = (bass   - 0.5f) * 28.0f;  // ±14dB
    const float trebleGainDB = (treble - 0.5f) * 28.0f;  // ±14dB

    // --- RC 네트워크 상호작용: bass/treble 동시 부스트 → mid 자연 스쿱 ---
    // 물리 근거: 두 컨트롤이 부스트 위치일 때만 공통 경로 로딩이 발생함
    // 스쿱 깊이 ≈ min(bassBoost, trebleBoost) × 0.6 (경험적 상수)
    const float bassBoostDB   = std::max (0.0f, bassGainDB);
    const float trebleBoostDB = std::max (0.0f, trebleGainDB);
    const float interactionScoopDB = std::min (bassBoostDB, trebleBoostDB) * 0.6f;

    // Mid 컨트롤: 직접 게인 ± 상호작용 스쿱 보상
    // - Mid=0.5(중앙): 직접 게인 0dB, 스쿱은 bass/treble 상태에 따라 결정
    // - Mid=1.0(최대): +10dB 직접 부스트로 스쿱 상쇄 가능
    // - Mid=0.0(최소): -10dB로 스쿱 심화
    const float midDirectDB = (mid - 0.5f) * 20.0f;
    const float midGainDB   = midDirectDB - interactionScoopDB;

    // Mid 컨트롤이 올라갈수록 스쿱 중심 주파수가 약간 상승 (임피던스 비율 변화)
    const float midFreq = 480.0f + mid * 200.0f;   // 480~680Hz
    const float midQ    = 0.9f  + mid * 0.5f;      // Q 0.9~1.4

    // --- Filter 0: Bass 저셸프 (80Hz) ---
    copyCoeffs (0, juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        fs, 80.0f, 0.707f, juce::Decibels::decibelsToGain (bassGainDB)));

    // --- Filter 1: Mid 피킹/노치 (480~680Hz) — 핵심 TMB 상호작용 ---
    copyCoeffs (1, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        fs, midFreq, midQ, juce::Decibels::decibelsToGain (midGainDB)));

    // --- Filter 2: Treble 고셸프 (2kHz) ---
    copyCoeffs (2, juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        fs, 2000.0f, 0.707f, juce::Decibels::decibelsToGain (trebleGainDB)));
}

//==============================================================================
// Baxandall (American Vintage): Active 4-band + Mid 5-position switch
//==============================================================================

void ToneStack::computeBaxandallCoefficients (float bass, float mid, float treble)
{
    auto copyCoeffs = [this] (int idx, juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            pendingCoeffs[idx][i] = raw[i];
    };

    const float bassGainDB   = (bass - 0.5f) * 30.0f;    // -15..+15dB
    const float midGainDB    = (mid - 0.5f) * 24.0f;     // -12..+12dB
    const float trebleGainDB = (treble - 0.5f) * 30.0f;  // -15..+15dB

    // Bass: 80Hz low shelf
    copyCoeffs (0, juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 80.0f, 0.707f,
        juce::Decibels::decibelsToGain (bassGainDB)));

    // Mid: peaking at selected frequency (5-position switch)
    float midFreq = midFrequencies[currentMidPosition];
    copyCoeffs (1, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, midFreq, 1.5f,
        juce::Decibels::decibelsToGain (midGainDB)));

    // Treble: 3kHz high shelf
    copyCoeffs (2, juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3000.0f, 0.707f,
        juce::Decibels::decibelsToGain (trebleGainDB)));

    // Extra presence peak at 2kHz for SVT character
    copyCoeffs (3, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 2000.0f, 2.0f,
        juce::Decibels::decibelsToGain (trebleGainDB * 0.3f)));
}

//==============================================================================
// James (British Stack): Bass/Treble independent shelving + Mid peaking
//==============================================================================

void ToneStack::computeJamesCoefficients (float bass, float mid, float treble)
{
    auto copyCoeffs = [this] (int idx, juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            pendingCoeffs[idx][i] = raw[i];
    };

    // James topology: Bass and Treble are INDEPENDENT shelving filters
    const float bassGainDB   = (bass - 0.5f) * 30.0f;
    const float midGainDB    = (mid - 0.5f) * 24.0f;
    const float trebleGainDB = (treble - 0.5f) * 30.0f;

    // Bass: 100Hz low shelf (independent)
    copyCoeffs (0, juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 100.0f, 0.707f,
        juce::Decibels::decibelsToGain (bassGainDB)));

    // Mid: 600Hz peaking (Q=1.2)
    copyCoeffs (1, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 600.0f, 1.2f,
        juce::Decibels::decibelsToGain (midGainDB)));

    // Treble: 3kHz high shelf (independent from bass)
    copyCoeffs (2, juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3000.0f, 0.707f,
        juce::Decibels::decibelsToGain (trebleGainDB)));
}

//==============================================================================
// BaxandallGrunt (Modern Micro): Baxandall + Grunt(HPF+LPF) + Attack(HPF)
//==============================================================================

void ToneStack::computeBaxandallGruntCoefficients (float bass, float mid, float treble)
{
    auto copyCoeffs = [this] (int idx, juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            pendingCoeffs[idx][i] = raw[i];
    };

    const float bassGainDB   = (bass - 0.5f) * 30.0f;
    const float midGainDB    = (mid - 0.5f) * 24.0f;
    const float trebleGainDB = (treble - 0.5f) * 30.0f;

    // Bass: 80Hz low shelf
    copyCoeffs (0, juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 80.0f, 0.707f,
        juce::Decibels::decibelsToGain (bassGainDB)));

    // Mid: 500Hz peaking
    copyCoeffs (1, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 500.0f, 1.5f,
        juce::Decibels::decibelsToGain (midGainDB)));

    // Treble: 3kHz high shelf
    copyCoeffs (2, juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3000.0f, 0.707f,
        juce::Decibels::decibelsToGain (trebleGainDB)));

    // Grunt: Low-frequency drive emphasis (HPF around 100Hz + LPF around 1kHz)
    // Boosts the low-mid "growl" region (100Hz-1kHz)
    const float gruntGainDB = (currentGrunt - 0.5f) * 20.0f;  // -10..+10dB
    copyCoeffs (3, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 250.0f, 0.8f,
        juce::Decibels::decibelsToGain (gruntGainDB)));

    // Attack: High-frequency drive emphasis (HPF around 2kHz)
    const float attackGainDB = (currentAttack - 0.5f) * 20.0f;  // -10..+10dB
    copyCoeffs (4, juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 2000.0f, 0.707f,
        juce::Decibels::decibelsToGain (attackGainDB)));
}

//==============================================================================
// MarkbassFourBand (Italian Clean): 4-band + VPF + VLE
//==============================================================================

void ToneStack::computeMarkbassCoefficients (float bass, float mid, float treble)
{
    // Italian Clean: bass=40Hz, mid is split into low-mid(360Hz) and high-mid(800Hz),
    // treble=10kHz. For the 3-knob interface: bass controls 40Hz, mid controls 360+800Hz,
    // treble controls 10kHz.

    auto copyCoeffs = [this] (int idx, juce::dsp::IIR::Coefficients<float>::Ptr src)
    {
        auto* raw = src->getRawCoefficients();
        for (int i = 0; i < maxCoeffs; ++i)
            pendingCoeffs[idx][i] = raw[i];
    };

    const float bassGainDB   = (bass - 0.5f) * 24.0f;    // -12..+12dB
    const float midGainDB    = (mid - 0.5f) * 24.0f;     // -12..+12dB
    const float trebleGainDB = (treble - 0.5f) * 24.0f;  // -12..+12dB

    // Band 1: 40Hz peaking
    copyCoeffs (0, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 40.0f, 1.0f,
        juce::Decibels::decibelsToGain (bassGainDB)));

    // Band 2: 360Hz peaking (Low-Mid)
    copyCoeffs (1, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 360.0f, 1.0f,
        juce::Decibels::decibelsToGain (midGainDB * 0.7f)));

    // Band 3: 800Hz peaking (High-Mid)
    copyCoeffs (2, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 800.0f, 1.0f,
        juce::Decibels::decibelsToGain (midGainDB)));

    // Band 4: 10kHz peaking (Treble)
    copyCoeffs (3, juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 10000.0f, 1.0f,
        juce::Decibels::decibelsToGain (trebleGainDB)));

    // VPF/VLE filters default to flat (pass-through)
    // updateMarkbassExtras() will overwrite these when VPF/VLE are adjusted
    auto flat = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 1000.0f, 0.707f, 1.0f);
    copyCoeffs (4, flat);  // VPF 저셸프 (35Hz)
    copyCoeffs (5, flat);  // VPF 노치 (380Hz)
    copyCoeffs (6, flat);  // VPF 고셸프 (10kHz)
}

//==============================================================================
// Apply Coefficients (audio thread)
//==============================================================================

void ToneStack::applyPendingCoefficients()
{
    if (! coeffsNeedUpdate.exchange (false))
        return;

    auto apply = [] (juce::dsp::IIR::Filter<float>& filter, const float* src, int n)
    {
        if (filter.coefficients == nullptr)
            return;
        auto* dst = filter.coefficients->getRawCoefficients();
        for (int i = 0; i < n; ++i)
            dst[i] = src[i];
    };

    for (int i = 0; i < activeFilterCount; ++i)
        apply (filters[i], pendingCoeffs[i], maxCoeffs);
}

//==============================================================================
// Process (audio thread)
//==============================================================================

void ToneStack::process (juce::AudioBuffer<float>& buffer)
{
    if (enabledParam != nullptr && enabledParam->load() < 0.5f)
        return;

    applyPendingCoefficients();

    // Apply VLE cutoff update
    if (vleNeedsUpdate.exchange (false))
        vleFilter.setCutoffFrequency (pendingVleCutoff);

    auto block   = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);

    // Apply active biquad filters
    for (int i = 0; i < activeFilterCount; ++i)
        filters[i].process (context);

    // Apply VLE lowpass (Italian Clean only)
    if (vleActive)
        vleFilter.process (context);
}
