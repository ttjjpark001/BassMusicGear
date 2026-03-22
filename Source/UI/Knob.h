#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * @brief 우클릭 기본값 리셋 기능이 있는 커스텀 로터리 슬라이더 래퍼
 *
 * UI 전체에서 일관된 노브 모양과 느낌을 제공.
 * - 회전 드래그: 값 변경
 * - 우클릭: 기본값으로 리셋
 * - 라벨: 노브 아래 파라미터 이름 표시
 *
 * 사용 예:
 *   Knob gainKnob("Gain", "input_gain", processor.apvts);
 */
class Knob : public juce::Component
{
public:
    /**
     * @brief 노브 컴포넌트 생성
     *
     * @param labelText 노브 아래 표시될 라벨 텍스트
     * @param paramId   APVTS 파라미터 ID (값 읽기/쓰기 및 기본값 조회용)
     * @param apvts     AudioProcessorValueTreeState 인스턴스
     */
    Knob (const juce::String& labelText, const juce::String& paramId,
          juce::AudioProcessorValueTreeState& apvts);
    ~Knob() override;

    /**
     * @brief 자식 컴포넌트 레이아웃 설정
     *
     * 노브(회전 슬라이더)와 라벨의 크기/위치 지정.
     * 라벨은 아래쪽, 노브는 위쪽에 배치.
     */
    void resized() override;

    /**
     * @brief 내부 슬라이더 참조 반환
     * @return Slider 인스턴스 (고급 커스터마이징용)
     */
    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;  // 회전 드래그 슬라이더
    juce::Label  label;   // 파라미터 이름 라벨

    // APVTS 파라미터에 슬라이더를 바인딩
    // SliderAttachment가 존재하는 동안 양방향 자동 동기화
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    // --- 우클릭 리셋 핸들러 ---
    /**
     * @brief 마우스 이벤트를 감지해 우클릭 시 기본값으로 리셋
     *
     * 역할:
     * - mouseDown()에서 우클릭 감지
     * - param->getDefaultValue() 조회
     * - 슬라이더 값을 기본값으로 설정 (notification 전송)
     *
     * 주의: 파라미터의 기본값은 생성자에서 APVTS의 범위 정보로부터 결정됨
     */
    class ResetMouseListener : public juce::MouseListener
    {
    public:
        /**
         * @param s   리셋할 Slider 인스턴스
         * @param p   파라미터 정보 (기본값 조회용)
         */
        ResetMouseListener (juce::Slider& s, juce::RangedAudioParameter* p)
            : slider (s), param (p) {}

        /**
         * @brief 마우스 다운 이벤트 처리
         *
         * 우클릭이 감지되면 슬라이더 값을 기본값으로 설정.
         * param이 null이면 무시 (기본값 정보 없음).
         */
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isRightButtonDown() && param != nullptr)
                // param->getDefaultValue(): 0..1 정규화 범위의 기본값
                // 이를 슬라이더 범위(min..max)로 변환:
                // slider_value = default_norm * (max - min) + min
                slider.setValue (param->getDefaultValue() * (slider.getMaximum() - slider.getMinimum()) + slider.getMinimum(),
                               juce::sendNotificationSync);
        }

    private:
        juce::Slider& slider;
        juce::RangedAudioParameter* param;
    };

    std::unique_ptr<ResetMouseListener> resetListener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
