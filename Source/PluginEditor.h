#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/AmpPanel.h"
#include "UI/CabinetSelector.h"
#include "UI/TunerDisplay.h"
#include "UI/EffectBlock.h"
#include "UI/GraphicEQPanel.h"

/**
 * @brief BassMusicGear 플러그인 에디터 (UI, Phase 4)
 *
 * **레이아웃** (위에서 아래 순서):
 * - **TunerDisplay**: 크로매틱 튜너 (상단 고정, 42px)
 *
 * - **Pre-FX 이펙터 블록** (앰프 앞단, 접기/펼치기 가능):
 *   - Overdrive: Drive, Tone, Blend
 *   - Octaver: Sub, Oct-Up, Dry
 *   - Envelope Filter: Sens, Reso
 *
 * - **AmpPanel** (290px):
 *   - 앰프 모델 선택 ComboBox (5종)
 *   - 모델별 노브 레이아웃
 *     - PREAMP: Input Gain, Volume
 *     - TONE STACK: Bass, Mid, Treble
 *     - POWER AMP: Drive, Presence, [Sag]
 *   - 모델별 특화 컨트롤
 *     - American Vintage: Mid Position (250Hz ~ 3kHz 선택)
 *     - Italian Clean: VPF, VLE 노브
 *     - Modern Micro: Grunt, Attack 노브
 *
 * - **CabinetSelector** (95px):
 *   - 내장 IR 선택 ComboBox (5종: 8x10 SVT / 4x10 JBL / 1x15 Vintage / 2x12 British / 2x10 Modern)
 *   - Bypass 토글 (Cabinet Convolution 우회)
 *
 * **UI 소재 및 색상**:
 * - 배경: 다크 네이비 (#1a1a2e)
 * - 강조색: 주황색 (#ff8800) — 앰프 모델별 테마 색상
 * - 텍스트: 타이틀 흰색, 푸터 회색 (#666688)
 * - 노브: RotarySlider 커스텀 (드래그, 우클릭 리셋)
 *
 * **스레드 안전성**:
 * - 모든 UI 업데이트는 메인 스레드(메시지 스레드)에서만 호출
 * - PluginProcessor::apvts와 Attachment로 파라미터 동기화
 * - 오디오 스레드는 atomic 포인터로 파라미터 값을 폴링만 함 (락 없음)
 */
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    /**
     * @brief PluginEditor 생성 및 UI 초기화
     *
     * @param processor  PluginProcessor 참조 (APVTS, SignalChain 접근)
     * @note [메인 스레드] DAW가 플러그인 로드 시 호출.
     *       TunerDisplay, Pre-FX 블록, AmpPanel, CabinetSelector를 생성 및 배치.
     */
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    /**
     * @brief 배경 및 섹션 라벨을 그린다.
     *
     * @param g  Graphics 컨텍스트
     * @note [메인 스레드] 리페인트 필요 시 호출.
     */
    void paint (juce::Graphics& g) override;

    /**
     * @brief 전체 UI 컴포넌트의 위치/크기를 계산하여 배치한다.
     *
     * 배치 순서 (위→아래):
     * - TunerDisplay: 42px 고정
     * - Pre-FX 블록: 접힘/펼침 상태에 따라 동적 높이
     * - AmpPanel: 290px 고정
     * - CabinetSelector: 95px 고정
     *
     * @note [메인 스레드] 윈도우 리사이즈 또는 초기화 시 호출.
     */
    void resized() override;

private:
    /**
     * @brief Pre-FX 블록의 접힘/펼침 상태에 따라 필요한 전체 창 높이를 계산한다.
     *
     * 각 Pre-FX 블록(Overdrive, Octaver, EnvelopeFilter)의 현재 상태를 읽어
     * 총 높이를 예측하여 반환한다. 블록이 펼쳐지면 expandedHeight(130px),
     * 접히면 collapsedHeight(36px)를 더한다.
     *
     * @return 필요한 창 높이 (픽셀)
     * @note calculateNeededHeight()는 setSize() 호출 시점에 전체 레이아웃이
     *       결정되어야 하므로, onExpandChange 콜백에서 호출되어
     *       창 크기를 동적으로 조정한다.
     */
    int calculateNeededHeight() const;

    PluginProcessor& processorRef;  // PluginProcessor 참조 (APVTS, SignalChain 접근)

    TunerDisplay    tunerDisplay;     // 크로매틱 튜너 (에디터 상단 상시 표시)
    AmpPanel        ampPanel;         // 5종 앰프 모델 선택 및 톤 컨트롤 패널
    CabinetSelector cabinetSelector;  // 캐비닛 IR 선택 및 Bypass 패널

    // Pre-FX effect blocks
    std::unique_ptr<EffectBlock> overdriveBlock;
    std::unique_ptr<EffectBlock> octaverBlock;
    std::unique_ptr<EffectBlock> envelopeFilterBlock;

    // Graphic EQ panel (10-band)
    GraphicEQPanel graphicEQPanel;

    // Post-FX effect blocks
    std::unique_ptr<EffectBlock> chorusBlock;
    std::unique_ptr<EffectBlock> delayBlock;
    std::unique_ptr<EffectBlock> reverbBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
