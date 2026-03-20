#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/**
 * @brief BassMusicGear 플러그인 UI의 루트 컴포넌트
 *
 * JUCE AudioProcessorEditor를 상속하며, 플러그인 창 전체를 담당한다.
 * DAW가 플러그인 UI 창을 열면 PluginProcessor::createEditor()가 이 클래스를
 * 인스턴스화한다. 모든 자식 컴포넌트(노브, 패널, 미터 등)는 이 클래스 아래에
 * 배치된다.
 *
 * 신호 체인 위치: UI만 담당하며 오디오 신호를 직접 처리하지 않는다.
 * 파라미터 변경은 APVTS SliderAttachment를 통해 PluginProcessor로 전달된다.
 *
 * - Phase 0: 800x500 창, 타이틀 텍스트만 표시하는 플레이스홀더 UI.
 * - Phase 8: 전체 레이아웃, 다크 테마, 리사이즈 지원이 완성된다.
 *
 * @note [메인 스레드 전용] 모든 UI 작업은 메시지 스레드에서만 수행해야 한다.
 *       오디오 스레드에서 컴포넌트 함수를 직접 호출하면 안 된다.
 */
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    /**
     * @brief 에디터를 생성하고 초기 창 크기를 설정한다.
     *
     * @param processor 이 에디터가 제어할 PluginProcessor 참조
     * @note [메인 스레드] PluginProcessor::createEditor()에서 호출된다.
     */
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    //==========================================================================
    // Component 인터페이스
    //==========================================================================

    /**
     * @brief 컴포넌트 영역을 그린다.
     *
     * 배경색, 타이틀, 버전 텍스트를 렌더링한다. 자식 컴포넌트는 각자의
     * paint()에서 직접 그리므로 여기서는 배경과 공통 요소만 처리한다.
     *
     * @param g JUCE 그래픽 컨텍스트 (GPU 또는 소프트웨어 렌더러)
     * @note [메인 스레드] repaint() 요청 후 메시지 루프에서 호출된다.
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief 창 크기가 변경될 때 자식 컴포넌트의 위치와 크기를 재배치한다.
     *
     * Phase 0에서는 배치할 자식 컴포넌트가 없어 비어 있다.
     * Phase 8에서 FlexBox/Grid 기반 반응형 레이아웃으로 완성된다.
     *
     * @note [메인 스레드] setSize() 호출 또는 창 리사이즈 시 자동 호출된다.
     */
    void resized() override;

private:
    // PluginProcessor 참조 — 파라미터 읽기 및 APVTS Attachment에 사용한다.
    // 에디터의 전체 생명주기 동안 프로세서가 먼저 해제되지 않음이 보장된다.
    PluginProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
