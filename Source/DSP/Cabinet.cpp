#include "Cabinet.h"
#include <BinaryData.h>

void Cabinet::prepare (const juce::dsp::ProcessSpec& spec)
{
    // 처리 스펙 저장 (getLatencyInSamples() 및 일반 정보용)
    currentSpec = spec;

    // JUCE Convolution 엔진에 처리 스펙 전달
    convolution.prepare (spec);

    // 아직 IR을 로드하지 않았으면 기본 placeholder IR 로드
    if (! irLoaded)
        loadDefaultIR();
}

void Cabinet::reset()
{
    // 컨볼루션 필터의 내부 상태(지연 라인) 초기화
    convolution.reset();
}

int Cabinet::getLatencyInSamples() const
{
    // --- 균일 분할 컨볼루션(Uniform Partitioned Convolution)의 지연 ---
    // JUCE는 효율적인 FFT 기반 컨볼루션을 위해 균일 분할 구조를 사용.
    // 각 파티션은 버퍼 크기와 같으므로, 전체 지연은 버퍼 크기 정도.
    //
    // 더 정확한 지연 값: convolution.getLatencyInSamples() 사용 가능
    // (JUCE 버전에 따라 지원 여부 확인 필요)
    //
    // 현재 구현: 보수적으로 버퍼 크기를 반환
    // (PDC 보고에서 다소 과대평가되지만, 안전성이 더 중요함)
    return static_cast<int> (currentSpec.maximumBlockSize);
}

void Cabinet::setParameterPointers (std::atomic<float>* bypass)
{
    // APVTS 파라미터 포인터를 캐시해두면, process()에서 락프리로 읽을 수 있다.
    bypassParam = bypass;
}

void Cabinet::loadIRFromBinaryData (const void* data, size_t sizeBytes)
{
    // BinaryData(컴파일된 리소스)에서 IR 로드
    // Stereo::no: 모노 입력 → 모노 IR 처리
    // Trim::yes: 묵음 구간 자동 제거 (지연 최소화)
    // 마지막 0: 정규화 없음 (IR이 이미 적절하게 스케일됨)
    convolution.loadImpulseResponse (
        data, sizeBytes,
        juce::dsp::Convolution::Stereo::no,   // 모노 IR
        juce::dsp::Convolution::Trim::yes,    // 자동 트림
        0);                                    // 정규화 없음
    irLoaded = true;
}

void Cabinet::loadIR (const juce::File& irFile)
{
    // 파일 시스템의 IR 파일 로드 (유저 커스텀 또는 팩토리 IR)
    // Stereo::yes: 스테레오 IR 지원 (IR이 스테레오면 양쪽 모두 로드)
    // Trim::yes: 자동 트림으로 불필요한 도입부 제거
    //
    // 주의: 이 호출은 백그라운드 스레드에서 비동기로 처리되므로
    // 메인 스레드를 블로킹하지 않는다. 단, loadIR() 호출 시점부터
    // 새로운 IR을 사용할 때까지 약간의 지연이 있을 수 있다.
    convolution.loadImpulseResponse (
        irFile,
        juce::dsp::Convolution::Stereo::yes,  // 스테레오 IR 지원
        juce::dsp::Convolution::Trim::yes,    // 자동 트림
        0);                                    // 정규화 없음
    irLoaded = true;
}

void Cabinet::loadDefaultIR()
{
    // BinaryData에 포함된 placeholder IR(기본값) 로드
    // 실제 스튜디오 IR은 Phase 2 이후 추가 예정
    loadIRFromBinaryData (BinaryData::placeholder_ir_wav,
                          BinaryData::placeholder_ir_wavSize);
}

void Cabinet::process (juce::AudioBuffer<float>& buffer)
{
    // --- Bypass 체크 ---
    // bypassParam > 0.5 → Bypass ON → IR 미적용, 원본 신호 통과
    // bypassParam ≤ 0.5 → Bypass OFF → 컨볼루션 적용
    if (bypassParam != nullptr && bypassParam->load() > 0.5f)
        return;

    // --- 컨볼루션 처리 ---
    // 현재 버퍼(모노)에 IR을 컨볼루션
    auto block = juce::dsp::AudioBlock<float> (buffer).getSingleChannelBlock (0);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    convolution.process (context);
}
