#include "Compressor.h"
#include <cmath>

/**
 * @brief DSP 초기화: 샘플레이트와 버퍼를 설정한다.
 *
 * juce::dsp::Compressor와 dry 블렌드용 버퍼를 prepare한다.
 * Dry 버퍼는 여기서 한 번만 할당하고, processBlock에서 재할당하지 않는다.
 *
 * @param spec  오디오 스펙 (sampleRate, maximumBlockSize 포함)
 * @note [메인 스레드] PluginProcessor::prepareToPlay()에서 호출된다.
 */
void Compressor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = static_cast<int> (spec.maximumBlockSize);

    compressor.prepare (spec);

    // Dry 버퍼를 prepareToPlay에서 미리 할당 (processBlock에서 재할당 금지)
    dryBuffer.setSize (1, maxBlockSize);
    dryBuffer.clear();

    reset();
}

/**
 * @brief 내부 상태를 초기화한다.
 *
 * juce::dsp::Compressor의 에벨로프 추적 상태와 게인 리덕션 값을 클리어.
 * @note [오디오 스레드] 재생 중지 시 호출.
 */
void Compressor::reset()
{
    compressor.reset();
    gainReductionDb.store (0.0f);
}

/**
 * @brief APVTS 파라미터 포인터를 등록한다.
 *
 * @param enabled      ON/OFF 토글 (0.5 이상 = enabled)
 * @param threshold    압축 시작 레벨 (dB, 음수)
 * @param ratio        압축 비율 (1:1 ~ 20:1, 예: 4 = 4:1)
 * @param attack       어택 시간 (ms)
 * @param release      릴리스 시간 (ms)
 * @param makeup       메이크업 게인 (dB, 양수)
 * @param dryBlend     드라이 혼합 비율 (0.0 = 100% wet, 1.0 = 100% dry)
 * @note [메인 스레드] SignalChain::connectParameters()에서 호출된다.
 */
void Compressor::setParameterPointers (std::atomic<float>* enabled,
                                        std::atomic<float>* threshold,
                                        std::atomic<float>* ratio,
                                        std::atomic<float>* attack,
                                        std::atomic<float>* release,
                                        std::atomic<float>* makeup,
                                        std::atomic<float>* dryBlend)
{
    enabledParam   = enabled;
    thresholdParam = threshold;
    ratioParam     = ratio;
    attackParam    = attack;
    releaseParam   = release;
    makeupParam    = makeup;
    dryBlendParam  = dryBlend;
}

/**
 * @brief 컴프레서를 오디오 버퍼에 적용한다.
 *
 * **처리 순서**:
 * 1. 활성화 여부 확인 (비활성화 시 바이패스)
 * 2. APVTS 파라미터를 atomic으로 읽기
 * 3. Dry 버퍼 복사 (패러렐 드라이 블렌드용)
 * 4. juce::dsp::Compressor 적용 (threshold, ratio, attack, release)
 * 5. 메이크업 게인 적용
 * 6. 게인 리덕션 측정 (입력 RMS vs 출력 RMS)
 * 7. Dry/Wet 혼합 (패러렐 컴프레션)
 *    - dryBlend=0 → 100% 압축음(wet)
 *    - dryBlend=1 → 100% 원본(dry)
 *    - 중간값 → 혼합
 *
 * @param buffer  모노 오디오 버퍼 (In-place 처리)
 * @note [오디오 스레드] processBlock()에서 매 버퍼마다 호출된다.
 *       게인 리덕션 값(dB)은 atomic으로 저장되어 UI VUMeter에서 실시간 표시됨.
 */
void Compressor::process (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 1)
        return;

    // --- 바이패스 체크 ---
    if (enabledParam != nullptr && enabledParam->load() < 0.5f)
    {
        gainReductionDb.store (0.0f);
        return;
    }

    const int numSamples = buffer.getNumSamples();

    // dryBuffer 오버런 방지: prepareToPlay에서 maxBlockSize로 할당된 버퍼 한계 초과 시 바이패스
    if (numSamples > dryBuffer.getNumSamples())
    {
        gainReductionDb.store (0.0f);
        return;
    }

    // --- APVTS 파라미터 읽기 (atomic, 락프리) ---
    const float threshold = (thresholdParam != nullptr) ? thresholdParam->load() : -20.0f;
    const float ratio     = (ratioParam     != nullptr) ? ratioParam->load()     : 4.0f;
    const float attackMs  = (attackParam    != nullptr) ? attackParam->load()    : 10.0f;
    const float releaseMs = (releaseParam   != nullptr) ? releaseParam->load()   : 100.0f;
    const float makeupDb  = (makeupParam    != nullptr) ? makeupParam->load()    : 0.0f;
    const float dryBlend  = (dryBlendParam  != nullptr) ? dryBlendParam->load()  : 0.0f;

    // --- juce::dsp::Compressor 파라미터 설정 ---
    compressor.setThreshold (threshold);
    compressor.setRatio (ratio);
    compressor.setAttack (attackMs);
    compressor.setRelease (releaseMs);

    // --- 입력 RMS 측정 (게인 리덕션 UI 표시용) ---
    const float inputRms = buffer.getRMSLevel (0, 0, numSamples);

    // --- Dry 버퍼 복사 (패러렐 컴프레션용) ---
    // 컴프레서 처리 전 원본 신호를 저장해 나중에 블렌드
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

    // --- 컴프레서 처리 (In-place) ---
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    compressor.process (context);

    // --- 메이크업 게인 적용 ---
    // 압축으로 인한 평균 레벨 손실을 보상
    if (std::abs (makeupDb) > 0.01f)
    {
        const float makeupLinear = juce::Decibels::decibelsToGain (makeupDb);
        buffer.applyGain (0, 0, numSamples, makeupLinear);
    }

    // --- 게인 리덕션 계산 (UI VUMeter용) ---
    // GR[dB] = 20 * log10(output_RMS / input_RMS)
    // 메이크업을 제외한 순수 감소량(음수 값)을 저장
    const float outputRms = buffer.getRMSLevel (0, 0, numSamples);
    if (inputRms > 1e-6f)
    {
        float grDb = juce::Decibels::gainToDecibels (outputRms / inputRms);
        // 메이크업 게인의 영향을 제거하여 순수 압축 감소량만 표시
        grDb -= makeupDb;
        gainReductionDb.store (std::min (0.0f, grDb));
    }
    else
    {
        gainReductionDb.store (0.0f);
    }

    // --- Dry Blend 혼합 (패러렐 컴프레션) ---
    // dryBlend 파라미터로 원본과 압축음 사이의 비율 조절
    // output = dryBlend * dry + (1 - dryBlend) * wet
    if (dryBlend > 0.001f)
    {
        const float* dry = dryBuffer.getReadPointer (0);
        float* wet = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = dryBlend * dry[i] + (1.0f - dryBlend) * wet[i];
    }
}
