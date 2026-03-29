#include "TunerDisplay.h"

TunerDisplay::TunerDisplay (Tuner& tuner,
                             juce::AudioProcessorValueTreeState& apvts)
    : tunerRef (tuner)
{
    muteButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    muteButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff4444));
    addAndMakeVisible (muteButton);

    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "tuner_mute", muteButton);

    startTimerHz (30);
}

TunerDisplay::~TunerDisplay()
{
    stopTimer();
}

void TunerDisplay::timerCallback()
{
    detected = tunerRef.isNoteDetected();
    if (detected)
    {
        int idx = tunerRef.getNoteIndex();
        noteName = Tuner::getNoteName (idx);
        cents = tunerRef.getCentsDeviation();
    }
    else
    {
        noteName = "--";
        cents = 0.0f;
    }
    repaint();
}

void TunerDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 배경
    g.setColour (juce::Colour (0xff111122));
    g.fillRoundedRectangle (bounds, 4.0f);

    // 라벨
    g.setColour (juce::Colour (0xff888899));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("TUNER", 8, 2, 50, 14, juce::Justification::centredLeft);

    // 음이름 표시 (중앙 왼쪽)
    auto noteArea = bounds.withWidth (80.0f).withX (60.0f).withTrimmedTop (2.0f);
    if (detected)
        g.setColour (juce::Colour (0xff00cc66));
    else
        g.setColour (juce::Colour (0xff666688));

    g.setFont (juce::FontOptions (28.0f).withStyle ("Bold"));
    g.drawText (noteName, noteArea.toNearestInt(), juce::Justification::centred);

    // 센트 편차 바 — 중앙 영역
    float barX = 150.0f;
    float barW = bounds.getWidth() - 300.0f;
    float barY = bounds.getCentreY() - 6.0f;
    float barH = 12.0f;

    if (barW > 20.0f)
    {
        // 배경 바
        g.setColour (juce::Colour (0xff333344));
        g.fillRoundedRectangle (barX, barY, barW, barH, 3.0f);

        // 중앙 마커
        float centreX = barX + barW * 0.5f;
        g.setColour (juce::Colour (0xff666688));
        g.drawLine (centreX, barY, centreX, barY + barH, 2.0f);

        if (detected)
        {
            // 센트 인디케이터 (cents: -50 ~ +50)
            float normCents = juce::jlimit (-50.0f, 50.0f, cents) / 50.0f;  // -1..+1
            float indicatorX = centreX + normCents * (barW * 0.5f - 4.0f);
            float indicatorW = 6.0f;

            // 색상: 중앙 근처 초록, 바깥쪽 빨강
            float absCents = std::abs (cents);
            juce::Colour indicatorColour;
            if (absCents < 5.0f)
                indicatorColour = juce::Colour (0xff00ff66);  // 튜닝 정확
            else if (absCents < 15.0f)
                indicatorColour = juce::Colour (0xffffff00);  // 근접
            else
                indicatorColour = juce::Colour (0xffff4444);  // 멀리

            g.setColour (indicatorColour);
            g.fillRoundedRectangle (indicatorX - indicatorW * 0.5f, barY + 1.0f,
                                     indicatorW, barH - 2.0f, 2.0f);
        }

        // Hz 표시
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

void TunerDisplay::resized()
{
    auto bounds = getLocalBounds();
    // Mute 버튼 — 오른쪽 끝
    muteButton.setBounds (bounds.removeFromRight (80).reduced (4, 6));
}
