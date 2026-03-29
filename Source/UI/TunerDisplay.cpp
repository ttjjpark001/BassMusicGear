#include "TunerDisplay.h"

/**
 * @brief TunerDisplay UI 초기화
 *
 * @param tuner  Tuner DSP 인스턴스 (감지된 음정 데이터 읽음)
 * @param apvts  APVTS (tuner_mute 파라미터 바인딩)
 *
 * **동작**:
 * - Mute 토글 버튼 스타일 설정 (흰 텍스트, 빨간 틱)
 * - tuner_mute 파라미터에 ButtonAttachment로 바인딩
 * - 30Hz 타이머 시작 (감지 음정 갱신 주기)
 */
TunerDisplay::TunerDisplay (Tuner& tuner,
                             juce::AudioProcessorValueTreeState& apvts)
    : tunerRef (tuner)
{
    muteButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    muteButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff4444));
    addAndMakeVisible (muteButton);

    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "tuner_mute", muteButton);

    // 30Hz (~33ms) 주기로 Tuner의 감지 결과를 읽어 UI 갱신
    startTimerHz (30);
}

/**
 * @brief 소멸자 — 타이머를 중지한다.
 */
TunerDisplay::~TunerDisplay()
{
    stopTimer();
}

/**
 * @brief 30Hz 타이머 콜백 — Tuner DSP에서 감지된 음정을 읽고 UI 갱신 요청
 *
 * Tuner가 atomic으로 제공하는 데이터를 읽는 것이므로 락프리이고,
 * 메인 스레드(GUI 스레드)에서 실행되어 UI 업데이트에 안전.
 *
 * @note [메인 스레드] juce::Timer 콜백이므로 메인 스레드에서만 실행됨.
 *       repaint() 호출로 paint() 재실행 트리거.
 */
void TunerDisplay::timerCallback()
{
    // Tuner에서 감지된 음정 읽기 (atomic, 락프리)
    detected = tunerRef.isNoteDetected();
    if (detected)
    {
        int idx = tunerRef.getNoteIndex();
        noteName = Tuner::getNoteName (idx);

        // EMA 스무딩: smoothedCents = alpha * raw + (1 - alpha) * prev
        // 약 200ms 정착 시간으로 바늘이 부드럽게 이동
        float rawCents = tunerRef.getCentsDeviation();
        smoothedCents = kCentsSmoothing * rawCents + (1.0f - kCentsSmoothing) * smoothedCents;
        cents = smoothedCents;
    }
    else
    {
        noteName = "--";
        smoothedCents = 0.0f;
        cents = 0.0f;
    }
    // UI 갱신 요청
    repaint();
}

/**
 * @brief 튜너 UI를 렌더링한다.
 *
 * **레이아웃**:
 * ```
 * [TUNER] [음이름: C]  [센트 편차 바 (L----|----R)]  [주파수: 65.4 Hz]  [MUTE]
 * ```
 *
 * - **음이름**: 감지되면 초록(#00cc66), 미감지 시 회색(#666688)
 * - **센트 바**: 중앙(0 cents)를 기준으로 좌우로 확장 (-50 ~ +50 cents)
 *   - 초록 점(|absCents| < 5 cents): 매우 정확
 *   - 노랑 점(5 ≤ |absCents| < 15 cents): 근접
 *   - 빨강 점(|absCents| ≥ 15 cents): 벗어남
 * - **주파수**: 감지된 실제 주파수(Hz) 소수 첫째 자리까지 표시
 *
 * @param g  그래픽 컨텍스트
 * @note [메인 스레드] paint() 메서드는 항상 메인 스레드에서 호출됨.
 */
void TunerDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // --- 배경 ---
    g.setColour (juce::Colour (0xff111122));
    g.fillRoundedRectangle (bounds, 4.0f);

    // --- "TUNER" 라벨 ---
    g.setColour (juce::Colour (0xff888899));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("TUNER", 8, 2, 50, 14, juce::Justification::centredLeft);

    // --- 음이름 표시 (큰 글씨, 중앙-왼쪽) ---
    auto noteArea = bounds.withWidth (80.0f).withX (60.0f).withTrimmedTop (2.0f);
    if (detected)
        g.setColour (juce::Colour (0xff00cc66));  // 음정 감지됨: 초록
    else
        g.setColour (juce::Colour (0xff666688));  // 음정 미감지: 회색

    g.setFont (juce::FontOptions (28.0f).withStyle ("Bold"));
    g.drawText (noteName, noteArea.toNearestInt(), juce::Justification::centred);

    // --- 센트 편차 바 (중앙 영역) ---
    // -50 cents (저음) ←[|||||중심|||||]→ +50 cents (고음)
    float barX = 150.0f;
    float barW = bounds.getWidth() - 300.0f;
    float barY = bounds.getCentreY() - 6.0f;
    float barH = 12.0f;

    if (barW > 20.0f)
    {
        // 배경 바
        g.setColour (juce::Colour (0xff333344));
        g.fillRoundedRectangle (barX, barY, barW, barH, 3.0f);

        // 중앙 마커 (0 cents 위치)
        float centreX = barX + barW * 0.5f;
        g.setColour (juce::Colour (0xff666688));
        g.drawLine (centreX, barY, centreX, barY + barH, 2.0f);

        if (detected)
        {
            // --- 센트 인디케이터 (이동 가능한 점) ---
            // cents를 정규화하여 -1.0 ~ +1.0 범위로 변환
            // 이후 바의 절반 폭(barW * 0.5)에 매핑
            float normCents = juce::jlimit (-50.0f, 50.0f, cents) / 50.0f;  // -1..+1
            float indicatorX = centreX + normCents * (barW * 0.5f - 4.0f);
            float indicatorW = 6.0f;

            // 색상 선택: 절대값 센트 기반
            float absCents = std::abs (cents);
            juce::Colour indicatorColour;
            if (absCents < 5.0f)
                indicatorColour = juce::Colour (0xff00ff66);  // 초록: 정확 튜닝 (±5 cents 이내)
            else if (absCents < 15.0f)
                indicatorColour = juce::Colour (0xffffff00);  // 노랑: 근접 (±5~15 cents)
            else
                indicatorColour = juce::Colour (0xffff4444);  // 빨강: 벗어남 (≥15 cents)

            g.setColour (indicatorColour);
            g.fillRoundedRectangle (indicatorX - indicatorW * 0.5f, barY + 1.0f,
                                     indicatorW, barH - 2.0f, 2.0f);
        }

        // --- Hz 표시 (바 오른쪽) ---
        g.setColour (juce::Colour (0xff888899));
        g.setFont (juce::FontOptions (11.0f));
        if (detected)
        {
            float hz = tunerRef.getDetectedHz();
            g.drawText (juce::String (hz, 1) + " Hz",
                        static_cast<int> (barX + barW + 8.0f), static_cast<int> (barY - 2.0f),
                        70, static_cast<int> (barH + 4.0f),
                        juce::Justification::centredLeft);
        }
    }
}

/**
 * @brief 레이아웃을 갱신한다.
 *
 * Mute 버튼을 오른쪽 끝에 배치 (폭 80px, 좌우/상하 패딩 4/6px).
 * 나머지 영역은 센트 편차 바와 음이름에 사용.
 */
void TunerDisplay::resized()
{
    auto bounds = getLocalBounds();
    // Mute 버튼 — 오른쪽 끝 (폭 80px, 마진 4/6)
    muteButton.setBounds (bounds.removeFromRight (80).reduced (4, 6));
}
