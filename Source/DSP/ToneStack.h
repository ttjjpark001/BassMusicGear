#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "../Models/AmpModel.h"

/**
 * @brief 5종 앰프 모델별 톤스택(EQ) 구현
 *
 * **신호 체인 위치**: Preamp → [ToneStack] → PowerAmp → Cabinet
 *
 * 각 모델의 톤스택 토폴로지는 물리적으로 전혀 다른 구조를 가지고 있어,
 * 같은 Bass/Mid/Treble 노브 값이어도 음색이 크게 달라진다.
 *
 * **지원 토폴로지**:
 * 1. **TMB** (Fender Tweed Bass): 수동 RC 네트워크, 3-band 상호작용
 *    - 각 컨트롤이 다른 대역에 영향을 미침 (진정한 음 상호작용)
 *
 * 2. **Baxandall** (American Vintage, Ampeg SVT 스타일): 능동형 4-band
 *    - Bass/Mid/Treble 각각 독립적인 피킹/셸빙 필터
 *    - Mid Position: 5개 주파수 선택(250/500/800/1.5k/3kHz)
 *
 * 3. **James** (British Stack, Orange AD200 스타일): 2-band 셸빙 + 1-band 미드 피킹
 *    - Bass/Treble은 독립적 셸빙, Mid는 피킹
 *    - 영국식 스택 특성: 중간대역 제어 용이
 *
 * 4. **BaxandallGrunt** (Modern Micro, Darkglass B3K): Baxandall + 깊이 조절
 *    - Baxandall 기반 + Grunt(HPF+LPF 하이패스/로우패스 깊이)
 *    - Attack: 반응 속도 조절
 *
 * 5. **MarkbassFourBand** (Italian Clean, Markbass 4-band): 4개 고정주파수 + VPF/VLE
 *    - 40/360/800/10kHz 각각 독립 바이쿼드 (Constant-Q 피킹)
 *    - VPF(Variable Pre-shape Filter): 35Hz 저셸프 + 380Hz 노치 + 10kHz 고셸프 (3필터 직렬)
 *    - VLE(Vintage Loudspeaker Emulator): 상태변수 로우패스 (20kHz ~ 4kHz)
 *
 * **스레드 안전성**:
 * - 계수 계산: 메인 스레드 only (updateCoefficients, updateMarkbassExtras, updateModernExtras)
 * - 계수 적용: 오디오 스레드 (applyPendingCoefficients via process)
 * - 동기화: atomic<bool> 플래그 + 오디오 스레드의 polled 적용 (RT-safe, 락 없음)
 */
class ToneStack
{
public:
    ToneStack() = default;

    /**
     * @brief DSP 초기화: 모든 필터를 준비하고 계수를 기본값(평탄)으로 설정한다.
     *
     * @param spec  오디오 스펙 (sampleRate, samplesPerBlock 포함)
     * @note [메인 스레드] prepareToPlay()에서 호출된다.
     *       모든 필터의 기본 계수를 피크 필터 1.0 게인으로 초기화 (평탄 응답).
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 오디오 버퍼에 현재 설정된 톤스택 필터를 적용한다.
     *
     * @param buffer  입출력 오디오 버퍼 (In-place 처리)
     * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
     *       메인 스레드에서 설정한 계수 변경 사항(coeffsNeedUpdate 플래그)을 폴링하여
     *       필요하면 applyPendingCoefficients()로 적용한다 (RT-safe, 락 없음).
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 모든 필터 버퍼를 클리어한다.
     *
     * @note [오디오 스레드] 재생 중지, 모델 전환 등에서 호출된다.
     *       필터 메모리가 버퍼되지 않으면 팝 노이즈 방지.
     */
    void reset();

    /**
     * @brief 현재 톤스택 토폴로지를 설정하고 필터 개수를 갱신한다.
     *
     * @param type  ToneStackType (TMB, Baxandall, James, BaxandallGrunt, MarkbassFourBand)
     * @note [메인 스레드 전용] 앰프 모델 전환 시 호출된다.
     *       activeFilterCount와 vleActive를 토폴로지에 맞춰 설정한다.
     */
    void setType (ToneStackType type);

    /**
     * @brief Bass/Mid/Treble 파라미터로부터 톤스택 IIR 계수를 재계산한다.
     *
     * 각 토폴로지별 compute*Coefficients() 함수를 호출하여
     * pendingCoeffs에 결과를 저장하고 coeffsNeedUpdate 플래그를 set한다.
     * 실제 필터 적용은 오디오 스레드의 applyPendingCoefficients()에서 이루어진다.
     *
     * @param bass    Bass 파라미터 (0.0 ~ 1.0, 자동으로 clamp)
     * @param mid     Mid 파라미터 (0.0 ~ 1.0)
     * @param treble  Treble 파라미터 (0.0 ~ 1.0)
     * @note [메인 스레드 전용] UI 노브 변경 또는 프리셋 로드 시 호출된다.
     */
    void updateCoefficients (float bass, float mid, float treble);

    /**
     * @brief Italian Clean(MarkbassFourBand) 톤스택 전용: VPF/VLE 계수 업데이트
     *
     * **VPF (Variable Pre-shape Filter)**:
     * - 3개 필터의 합산으로 구현되는 멀티밴드 부스트
     * - ① 35Hz 로우셸프 부스트
     * - ② 380Hz 피킹 컷 (노치)
     * - ③ 10kHz 하이셸프 부스트
     * - 모든 필터의 깊이가 VPF 노브 값(0~1)에 선형 비례 (0~12dB)
     *
     * **VLE (Vintage Loudspeaker Emulator)**:
     * - StateVariableTPTFilter 로우패스, 6dB/oct
     * - 컷오프 주파수: 0%(노브) = 20kHz (전대역 통과), 100% = 4kHz (저음만 강조)
     *
     * @param vpf  VPF 깊이 (0.0 ~ 1.0 → 0 ~ 12dB)
     * @param vle  VLE 로우패스 깊이 (0.0 ~ 1.0 → 20kHz ~ 4kHz)
     * @note [메인 스레드 전용] PluginProcessor가 주기적으로 호출한다.
     */
    void updateMarkbassExtras (float vpf, float vle);

    /**
     * @brief Modern Micro(BaxandallGrunt) 톤스택 전용: Grunt/Attack 계수 업데이트
     *
     * **Grunt**: Baxandall 기본 필터에 추가적인 하이패스/로우패스 깊이 조절
     * **Attack**: 반응 속도 (필터 Q 값 또는 차단 주파수에 영향)
     *
     * @param grunt  왜곡/깊이 (0.0 ~ 1.0)
     * @param attack 반응 속도 (0.0 ~ 1.0)
     * @note [메인 스레드 전용]
     */
    void updateModernExtras (float grunt, float attack);

    /**
     * @brief American Vintage(Baxandall) 톤스택 전용: Mid Position 선택
     *
     * Baxandall 미드 밴드의 중심 주파수를 5개 선택지 중 선택한다:
     * - 0: 250Hz (저중음)
     * - 1: 500Hz (중음, 기본값)
     * - 2: 800Hz (상향 중음)
     * - 3: 1.5kHz (중상음)
     * - 4: 3kHz (상중음)
     *
     * @param position  0 ~ 4 (범위 초과 시 자동 clamp)
     * @note [메인 스레드 전용] American Vintage 모델에서만 의미 있음.
     *       updateCoefficients()를 다시 호출할 때 이 값이 반영된다.
     */
    void updateMidPosition (int position);

    /**
     * @brief APVTS 파라미터(Bass/Mid/Treble/Enabled) 원자 포인터를 설정한다.
     *
     * 오디오 스레드가 이 포인터들을 폴링하여 real-time 파라미터 변경을 감지한다.
     * (선택적: 사용하지 않으면 NULL 유지 가능)
     *
     * @param bass     Bass 파라미터 atomic 포인터
     * @param mid      Mid 파라미터 atomic 포인터
     * @param treble   Treble 파라미터 atomic 포인터
     * @param enabled  ToneStack 활성화 플래그 atomic 포인터
     * @note [메인 스레드] PluginProcessor 생성 시 호출.
     */
    void setParameterPointers (std::atomic<float>* bass,
                               std::atomic<float>* mid,
                               std::atomic<float>* treble,
                               std::atomic<float>* enabled);

private:
    /**
     * @brief pendingCoeffs 배열의 계수를 필터에 적용한다 (RT-safe).
     *
     * 오디오 스레드에서만 호출되며, coeffsNeedUpdate 플래그를 확인하여
     * 메인 스레드가 설정한 새 계수를 filters 배열에 반영한다.
     * 락 없음, 원자 플래그 폴링만 사용 (실시간 안전).
     *
     * @note [오디오 스레드] process() 시작 시 호출된다.
     */
    void applyPendingCoefficients();

    // --- 토폴로지별 계수 계산 (메인 스레드 전용) ---

    /**
     * @brief TMB(Fender Tweed Bass) 톤스택 계수 계산
     *
     * Fender의 클래식 3-band 이퀄라이저.
     * 세 컨트롤의 RC 네트워크가 상호작용하여, 각 노브가 다른 대역에도 영향을 미친다.
     * 독립 필터 3개로는 정확히 모사 불가능하며, bilinear transform으로
     * 원본 전달함수(transfer function)를 이산화한다.
     *
     * @note [메인 스레드 only]
     */
    void computeTMBCoefficients (float bass, float mid, float treble);

    /**
     * @brief Baxandall(American Vintage, Ampeg SVT 스타일) 계수 계산
     *
     * 능동형 4-band 이퀄라이저.
     * - Bass: 200Hz 이하 셸빙
     * - Mid: 현재 midPosition(250~3kHz)의 피킹 필터
     * - Treble: 5kHz 이상 셸빙
     * - Extra: 내부 필터 (reserve)
     *
     * 각 대역이 완벽히 독립적이므로 상호작용이 거의 없다.
     *
     * @note [메인 스레드 only]
     */
    void computeBaxandallCoefficients (float bass, float mid, float treble);

    /**
     * @brief James(British Stack, Orange AD200 스타일) 계수 계산
     *
     * 영국식 스택 특성: Bass/Treble은 독립 셸빙, Mid는 피킹
     * - Bass: 100Hz 이하 셸빙
     * - Mid: 1kHz 피킹 (Q = 2.0)
     * - Treble: 5kHz 이상 셸빙
     *
     * @note [메인 스레드 only]
     */
    void computeJamesCoefficients (float bass, float mid, float treble);

    /**
     * @brief BaxandallGrunt(Modern Micro, Darkglass B3K 스타일) 계수 계산
     *
     * Baxandall 기반 + Grunt/Attack 추가 레이어:
     * - Bass/Mid/Treble: Baxandall 3-band
     * - Grunt: HPF + LPF 깊이 조절 (사이드체인 필터)
     * - Attack: 반응 속도 (필터 Q 또는 차단 주파수)
     *
     * @note [메인 스레드 only]
     */
    void computeBaxandallGruntCoefficients (float bass, float mid, float treble);

    /**
     * @brief MarkbassFourBand(Italian Clean, Markbass 4-band 스타일) 계수 계산
     *
     * 4개 고정주파수 Constant-Q 피킹 바이쿼드:
     * - 40Hz, 360Hz, 800Hz, 10kHz (각각 ±12dB 범위)
     *
     * VPF/VLE는 updateMarkbassExtras()로 별도 처리.
     *
     * @note [메인 스레드 only]
     */
    void computeMarkbassCoefficients (float bass, float mid, float treble);

    // --- 필터 뱅크 (최대 6개 바이쿼드) ---
    // 토폴로지별로 사용하는 필터 개수가 다름:
    // - TMB: 3개 (상호작용 RC 네트워크, 실제로는 연속 시간 모델을 이산화)
    // - Baxandall: 4개 (Bass/Mid/Treble + reserve)
    // - James: 3개 (Bass shelving / Mid peaking / Treble shelving)
    // - BaxandallGrunt: 5개 (Bass/Mid/Treble + Grunt HPF + Grunt LPF)
    // - MarkbassFourBand: 7개 (40/360/800/10kHz 밴드 + VPF 저셸프 + VPF 노치 + VPF 고셸프)
    static constexpr int maxFilters = 7;
    juce::dsp::IIR::Filter<float> filters[maxFilters];  // 각 필터는 바이쿼드 구현
    int activeFilterCount = 3;                          // 현재 활성 필터 개수

    // --- VLE 필터 (Italian Clean 전용: 상태변수 로우패스) ---
    // Markbass VLE(Vintage Loudspeaker Emulator)는 StateVariableTPTFilter로 구현
    // (6dB/oct 기울기, 공명 없음)
    juce::dsp::StateVariableTPTFilter<float> vleFilter;
    bool vleActive = false;                     // MarkbassFourBand일 때만 활성
    float vleTargetCutoff = 20000.0f;           // 목표 차단 주파수

    // --- RT-safe 계수 전달 메커니즘 ---
    // 메인 스레드: pendingCoeffs에 계산된 계수 저장 후 coeffsNeedUpdate = true
    // 오디오 스레드: applyPendingCoefficients()가 폴링하여 필터에 반영
    static constexpr int maxCoeffs = 5;         // 바이쿼드 5개 계수 (b0, b1, b2, a1, a2)
    float pendingCoeffs[maxFilters][maxCoeffs] = {};  // 메인 스레드가 채움
    std::atomic<bool> coeffsNeedUpdate { false };     // 계수 갱신 필요 플래그
    std::atomic<bool> vleNeedsUpdate { false };       // VLE 필터 업데이트 필요
    float pendingVleCutoff = 20000.0f;                // 목표 VLE 차단 주파수

    ToneStackType currentType = ToneStackType::TMB;   // 현재 토폴로지
    double sampleRate = 44100.0;                      // 오디오 샘플레이트

    // --- American Vintage 전용: Mid Position ---
    // Baxandall 톤스택의 미드 밴드를 5개 선택 주파수 중 선택
    static constexpr float midFrequencies[5] = { 250.0f, 500.0f, 800.0f, 1500.0f, 3000.0f };
    int currentMidPosition = 1;                 // 기본값: 500Hz

    // --- Modern Micro 전용: Grunt/Attack ---
    float currentGrunt = 0.5f;
    float currentAttack = 0.5f;

    // --- APVTS 파라미터 포인터 (선택적, real-time 폴링용) ---
    std::atomic<float>* bassParam    = nullptr;
    std::atomic<float>* midParam     = nullptr;
    std::atomic<float>* trebleParam  = nullptr;
    std::atomic<float>* enabledParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStack)
};
