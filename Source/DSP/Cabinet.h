#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 캐비닛 IR(임펄스 응답) 컨볼루션 시뮬레이션
 *
 * 신호 체인 위치: 파워앰프 → [캐비닛] → 최종 출력
 *
 * 역할:
 * - JUCE Convolution 엔진 래퍼 (균일 분할 컨볼루션)
 * - 실제 스피커/캐비닛의 음향 특성 임펄스 응답(IR) 적용
 * - IR 로드는 백그라운드 스레드에서 비동기 처리
 * - Bypass 토글로 원본 신호 또는 컨볼루션 신호 선택
 *
 * Phase 1: 기본 IR (placeholder)
 * 향후 Phase: 여러 캐비닛 모델(Fender, Ampeg, Eden 등) 추가 예정
 *
 * 주의: loadIR() / loadIRFromBinaryData()는 메인 스레드에서만 호출해야 함.
 *      IR 로드 완료는 JUCE가 백그라운드에서 처리하므로 processBlock 중단 없음.
 */
class Cabinet
{
public:
    Cabinet() = default;

    /**
     * @brief DSP 처리 스펙 설정
     * @param spec 샘플레이트, 버퍼 크기 등 처리 정보
     */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /**
     * @brief 현재 버퍼에 캐비닛 IR 컨볼루션 적용
     * @note [오디오 스레드] prepareToPlay() 이후 매 버퍼마다 호출된다.
     */
    void process (juce::AudioBuffer<float>& buffer);

    /**
     * @brief 컨볼루션 상태 초기화 (지연 라인, 내부 상태)
     */
    void reset();

    /**
     * @brief 균일 분할 컨볼루션의 지연 시간(샘플 단위) 반환
     * @return 지연 샘플 수 (PDC 보고용)
     *
     * JUCE Convolution은 균일 분할 구조(Uniform Partitioned Convolution)를
     * 사용하므로 지연은 파티션 크기(= 버퍼 크기)에 비례한다.
     */
    int getLatencyInSamples() const;

    /**
     * @brief BinaryData에 포함된 IR 파일을 로드한다.
     *
     * 메모리에 컴파일된 리소스에서 로드하므로 파일 I/O 없음.
     * IR 로드는 내부적으로 백그라운드 스레드에서 처리된다.
     *
     * @param data       IR WAV 파일 메모리 포인터
     * @param sizeBytes  IR WAV 파일 크기 (바이트)
     * @note [메인 스레드 전용]
     */
    void loadIRFromBinaryData (const void* data, size_t sizeBytes);

    /**
     * @brief 파일 시스템의 IR 파일을 로드한다.
     *
     * 유저 커스텀 IR 파일 또는 팩토리 IR 파일을 런타임에 로드 가능.
     *
     * @param irFile IR 파일 (일반적으로 mono 또는 stereo WAV)
     * @note [메인 스레드 전용]
     */
    void loadIR (const juce::File& irFile);

    /**
     * @brief APVTS 파라미터 포인터를 캐시한다.
     *
     * @param bypass 캐비닛 Bypass 플래그 (0 = OFF, 1 = ON)
     * @note [메인 스레드 전용]
     */
    void setParameterPointers (std::atomic<float>* bypass);

    /**
     * @brief BinaryData에서 기본 Placeholder IR을 로드한다.
     */
    void loadDefaultIR();

private:
    // JUCE 균일 분할 컨볼루션 엔진
    juce::dsp::Convolution convolution;
    bool irLoaded = false;

    juce::dsp::ProcessSpec currentSpec {};

    std::atomic<float>* bypassParam = nullptr;  // bool (0 = OFF, 1 = ON)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Cabinet)
};
