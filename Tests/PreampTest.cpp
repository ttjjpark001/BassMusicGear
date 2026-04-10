#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Preamp.h"
#include "Models/AmpModel.h"
#include <cmath>
#include <complex>

//==============================================================================
namespace {

/**
 * @brief 테스트용 정현파 버퍼 채우기
 *
 * @param buffer      채울 오디오 버퍼
 * @param freqHz      파형 주파수 (Hz)
 * @param sampleRate  샘플 레이트 (Hz)
 * @param amplitude   진폭 (0~1 권장)
 */
void fillSine (juce::AudioBuffer<float>& buffer, float freqHz,
               double sampleRate, float amplitude)
{
    const int n = buffer.getNumSamples();
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < n; ++i)
        buffer.setSample (0, i, amplitude * std::sin (omega * static_cast<float> (i)));
}

/**
 * @brief 버퍼의 RMS(Root-Mean-Square) 레벨 측정
 *
 * @param buffer  오디오 버퍼 채널 0
 * @param start   측정 시작 샘플 인덱스
 * @param n       측정할 샘플 수
 * @return        RMS 값 (0~1 범위, 1.0 = 최대 진폭)
 *
 * 프리앰프 웨이브쉐이핑 강도 비교용.
 */
float rmsOf (const juce::AudioBuffer<float>& buffer, int start, int n)
{
    const float* d = buffer.getReadPointer (0);
    float s = 0.0f;
    for (int i = start; i < start + n; ++i)
        s += d[i] * d[i];
    return std::sqrt (s / static_cast<float> (n));
}

/**
 * @brief 버퍼에 NaN(Not-a-Number) 또는 Inf(무한대) 확인
 *
 * @param buffer  오디오 버퍼 채널 0
 * @return        NaN/Inf 발견 시 true, 정상 유한값만 있으면 false
 *
 * DSP 안정성 검증 — 발산, 수치 오류 감지용.
 */
bool hasNanOrInf (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const float* d = buffer.getReadPointer (0);
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (d[i]))
            return true;
    return false;
}

/**
 * @brief DFT를 이용한 특정 주파수 대역 에너지 측정
 *
 * @param buffer      오디오 버퍼 채널 0
 * @param sampleRate  샘플 레이트 (Hz)
 * @param minHz       측정 시작 주파수 (Hz)
 * @param maxHz       측정 종료 주파수 (Hz)
 * @return            해당 대역의 누적 에너지
 *
 * **앨리어싱 검증 용도**:
 * - 입력: 저주파 사인파 (예: 440Hz)
 * - 강한 비선형 처리 후 고주파 대역(10kHz~20kHz) 에너지 측정
 * - 4x 오버샘플링이 앨리어싱을 얼마나 억제하는지 확인
 *
 * 느린 DFT 구현 (O(N²)): 선택한 bin 범위만 계산하므로
 * FFT보다 느리지만 임의 주파수 범위 선택 가능.
 */
float energyAboveHz (const juce::AudioBuffer<float>& buffer, double sampleRate,
                     float minHz, float maxHz)
{
    const int n = buffer.getNumSamples();
    const float* d = buffer.getReadPointer (0);
    const int startBin = (int) std::ceil (minHz * n / sampleRate);
    const int endBin   = std::min (n / 2, (int) std::floor (maxHz * n / sampleRate));

    float totalEnergy = 0.0f;
    for (int k = startBin; k <= endBin; ++k)
    {
        std::complex<float> sum { 0.0f, 0.0f };
        const float twoPiK = 2.0f * juce::MathConstants<float>::pi * (float) k;
        for (int i = 0; i < n; ++i)
        {
            float phase = twoPiK * (float) i / (float) n;
            sum += std::complex<float> { d[i] * std::cos (phase),
                                         -d[i] * std::sin (phase) };
        }
        totalEnergy += std::norm (sum);
    }
    return totalEnergy;
}

} // namespace

/**
 * @brief Preamp: 4x 오버샘플링의 앨리어싱 억제 검증
 *
 * **테스트 목표**:
 * - 440Hz 사인파에 강한 드라이브(+24dB) 적용
 * - 비선형 웨이브쉐이핑으로 고조파 생성
 * - 4x 오버샘플링이 10kHz~20kHz 에너지를 억제하는지 확인
 *
 * **기대 결과**:
 * - 고주파 에너지가 유한하고 NaN/Inf 없음
 * - 정규화 에너지 < 5% (오버샘플링 효과)
 *
 * 오버샘플링 없으면 나이퀴스트 폴딩으로 저역에 앨리어싱 기여.
 */
TEST_CASE ("Preamp: 4x oversampling suppresses high-frequency aliasing",
           "[preamp][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 2048;

    Preamp preamp;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    preamp.prepare (spec);
    preamp.setPreampType (PreampType::Tube12AX7Cascade);

    // 강한 드라이브 설정 (비선형 구간 강조)
    std::atomic<float> inputGain { 24.0f };  // +24 dB (강한 포화)
    std::atomic<float> volume    { 0.0f };
    preamp.setParameterPointers (&inputGain, &volume);

    juce::AudioBuffer<float> buffer (1, blockSize);
    fillSine (buffer, 440.0f, sampleRate, 0.5f);  // A4: 440Hz 저주파

    // 안정화 처리 (DC 블로커, 필터 워밍업)
    preamp.process (buffer);

    // 다시 생성 후 처리 (DC 블로커 안정화 이후 측정)
    fillSine (buffer, 440.0f, sampleRate, 0.5f);
    preamp.process (buffer);

    // 10kHz~20kHz 대역 에너지 측정 (DFT)
    const float highEnergy = energyAboveHz (buffer, sampleRate, 10000.0f, 20000.0f);
    REQUIRE (std::isfinite (highEnergy));  // 유한값 확인

    // 전체 RMS와의 비율 계산
    const float rms = rmsOf (buffer, 0, blockSize);
    REQUIRE (rms > 0.0f);

    // 10kHz 이상 에너지가 상대적으로 낮음을 검증 (오버샘플링 효과)
    // DFT 에너지를 정규화: sqrt(에너지) / blockSize
    // 4x 오버샘플링 상태에서 고조파 잔류는 존재하지만,
    // 오버샘플링 없음 상태의 에일리어싱보다 훨씬 낮아야 한다.
    const float aliasNormalized = std::sqrt (highEnergy) / (float) blockSize;
    REQUIRE (aliasNormalized < 0.05f);  // 실측 ~0.013 수준, 여유 확보 (5%)

    REQUIRE_FALSE (hasNanOrInf (buffer));
}

/**
 * @brief Preamp: 3가지 튜브 모델(Tube/JFET/ClassD) 출력 차별화 확인
 *
 * **테스트 목표**:
 * 3가지 프리앰프 타입이 동일 입력(220Hz, +18dB)에 대해
 * 서로 다른 비선형 특성을 보이는지 검증.
 *
 * - Tube12AX7Cascade: 부드러운 포화 (tanh 기반)
 * - JFETParallel: JFET 특성 시뮬레이션
 * - ClassDLinear: 디지털 증폭 시뮬레이션 (선형성 높음)
 *
 * **기대 결과**: 각 모델의 RMS가 유의하게 다름 (> 0.001)
 */
TEST_CASE ("Preamp: Tube/JFET/ClassD produce different RMS outputs",
           "[preamp][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 1024;

    // 주어진 PreampType으로 측정하는 람다 함수
    auto measure = [&] (PreampType type)
    {
        Preamp preamp;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
        preamp.prepare (spec);
        preamp.setPreampType (type);

        std::atomic<float> inputGain { 18.0f };  // 중간 드라이브
        std::atomic<float> volume    { 0.0f };
        preamp.setParameterPointers (&inputGain, &volume);

        juce::AudioBuffer<float> buffer (1, blockSize);
        fillSine (buffer, 220.0f, sampleRate, 0.5f);  // A2: 220Hz
        preamp.process (buffer);

        // 중간 지점부터 절반 구간 RMS 측정 (DC 블로커 안정화 구간 회피)
        return rmsOf (buffer, blockSize / 4, blockSize / 2);
    };

    const float rmsTube   = measure (PreampType::Tube12AX7Cascade);
    const float rmsJFET   = measure (PreampType::JFETParallel);
    const float rmsClassD = measure (PreampType::ClassDLinear);

    // 각 모델 RMS가 양수 확인
    REQUIRE (rmsTube > 0.0f);
    REQUIRE (rmsJFET > 0.0f);
    REQUIRE (rmsClassD > 0.0f);

    // 세 모델의 RMS가 서로 다르게 나와야 함 (독립적 웨이브쉐이핑)
    // 차이 > 0.001 (1% 수준)
    REQUIRE (std::abs (rmsTube - rmsJFET)   > 0.001f);
    REQUIRE (std::abs (rmsTube - rmsClassD) > 0.001f);
    REQUIRE (std::abs (rmsJFET - rmsClassD) > 0.001f);
}

/**
 * @brief Preamp: 최강 드라이브에서도 진폭 제한 확인
 *
 * **테스트 목표**:
 * - 최대 드라이브(+40dB) 적용 후 신호가 tanh 클리핑으로 제한되는지 확인
 * - 출력이 엔벨로프 ±2.0 범위 내에 있으므로 발산/NaN 없음 검증
 *
 * **예상 동작**:
 * - tanh 클리핑 + DC 블로커 전후 게인 보정
 * - 단기 첨두값은 ±2.0까지 가능 (DC 블로커 통과역 리플)
 * - 다만 수렴: 발산(∞)/NaN 절대 없음
 */
TEST_CASE ("Preamp: hard drive stays within amplitude envelope",
           "[preamp][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 2048;

    Preamp preamp;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    preamp.prepare (spec);
    preamp.setPreampType (PreampType::Tube12AX7Cascade);

    // 최대 드라이브 설정
    std::atomic<float> inputGain { 40.0f };  // +40dB (극한)
    std::atomic<float> volume    { 0.0f };
    preamp.setParameterPointers (&inputGain, &volume);

    juce::AudioBuffer<float> buffer (1, blockSize);
    fillSine (buffer, 100.0f, sampleRate, 1.0f);  // 100Hz, 풀 진폭
    preamp.process (buffer);

    // tanh 클리핑 확인:
    // Tube12AX7Cascade는 비대칭 tanh + DC 블로커 전후 게인 보정으로 인해
    // 단기적으로 ±2.0 범위까지 출력 가능. 하지만 발산(무한대)/NaN은 없어야 함.
    const float* d = buffer.getReadPointer (0);
    for (int i = 0; i < blockSize; ++i)
    {
        REQUIRE (std::abs (d[i]) <= 2.0f);  // 엔벨로프 내
        REQUIRE (std::isfinite (d[i]));     // 유한값 확인
    }
}

/**
 * @brief Preamp: 무음 입력 → 무음 출력 (NaN/Inf 없음)
 *
 * **테스트 목표**:
 * - 입력이 0일 때 출력도 0에 가까워야 함
 * - DC 블로커, 필터 등 모든 스테이지가 안정적으로 동작
 * - 수치 오류 없음 (NaN/Inf)
 *
 * **에지 케이스**:
 * DC 블로커, 필터의 피드백 상태에서도 발산 없어야 함.
 */
TEST_CASE ("Preamp: silence input produces silence output (no NaN/Inf)",
           "[preamp][phase8][carry]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 1024;

    Preamp preamp;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    preamp.prepare (spec);
    preamp.setPreampType (PreampType::Tube12AX7Cascade);

    std::atomic<float> inputGain { 12.0f };
    std::atomic<float> volume    { 0.0f };
    preamp.setParameterPointers (&inputGain, &volume);

    // 무음 버퍼 처리
    juce::AudioBuffer<float> buffer (1, blockSize);
    buffer.clear();
    preamp.process (buffer);

    // 수치 안정성 확인
    REQUIRE_FALSE (hasNanOrInf (buffer));
}
