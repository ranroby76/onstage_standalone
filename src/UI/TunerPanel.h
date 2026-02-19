#pragma once
// ==============================================================================
//  TunerPanel.h
//  OnStage — Vocal/Instrument Tuner panel
//
//  REWRITTEN:
//    - Full range keyboard: C1 (MIDI 24) to B8 (MIDI 107) = 7 octaves
//    - Piano height reduced to 50%
//    - Cents deviation needle/bar for fine tuning
//    - Timer at 30 Hz for faster response
//    - Detects any note across all octaves (singing, guitar, etc.)
// ==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/TunerProcessor.h"

// ==============================================================================
//  Piano Keyboard — 7-octave display with LED
// ==============================================================================
class TunerKeyboard : public juce::Component
{
public:
    TunerKeyboard() = default;

    void setActiveNote (int midiNote)
    {
        if (currentNote == midiNote) return;
        currentNote = midiNote;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xFF1A1A2E));
        g.fillRoundedRectangle (bounds, 4.0f);

        // 7 octaves: C1 (MIDI 24) to B7 (MIDI 107) = 84 notes
        const int startNote  = 24;
        const int totalNotes = 84;

        int whiteCount = 0;
        for (int i = 0; i < totalNotes; ++i)
            if (! isBlackKey (startNote + i)) ++whiteCount;

        if (whiteCount == 0) return;

        float kbX = bounds.getX() + 3.0f;
        float kbW = bounds.getWidth() - 6.0f;
        float kbH = bounds.getHeight() - 3.0f;
        float kbY = bounds.getY() + 1.5f;

        float whiteKeyW = kbW / (float) whiteCount;
        float blackKeyW = whiteKeyW * 0.6f;
        float blackKeyH = kbH * 0.58f;

        juce::Rectangle<float> activeKeyRect;
        bool activeIsBlack = false;

        // ── Pass 1: White keys ──────────────────────────────────────────
        int wIdx = 0;
        for (int i = 0; i < totalNotes; ++i)
        {
            int note = startNote + i;
            if (isBlackKey (note)) continue;

            float x = kbX + wIdx * whiteKeyW;
            juce::Rectangle<float> keyRect (x, kbY, whiteKeyW - 0.8f, kbH);

            bool isActive = (note == currentNote);

            g.setColour (isActive ? juce::Colour (0xFFDDDDFF) : juce::Colour (0xFFF0F0F0));
            g.fillRoundedRectangle (keyRect, 1.5f);

            g.setColour (juce::Colour (0xFF444444).withAlpha (0.25f));
            g.drawRoundedRectangle (keyRect, 1.5f, 0.4f);

            if (isActive) activeKeyRect = keyRect;

            // Label C keys
            if (note % 12 == 0)
            {
                g.setColour (juce::Colour (0xFF888888));
                g.setFont (juce::Font (7.5f));
                g.drawText ("C" + juce::String (note / 12 - 1),
                            keyRect.removeFromBottom (10.0f),
                            juce::Justification::centred);
            }

            ++wIdx;
        }

        // ── Pass 2: Black keys ──────────────────────────────────────────
        wIdx = 0;
        for (int i = 0; i < totalNotes; ++i)
        {
            int note = startNote + i;
            if (! isBlackKey (note)) { ++wIdx; continue; }

            float x = kbX + (wIdx - 1) * whiteKeyW + whiteKeyW - blackKeyW * 0.5f;
            juce::Rectangle<float> keyRect (x, kbY, blackKeyW, blackKeyH);

            bool isActive = (note == currentNote);

            g.setColour (isActive ? juce::Colour (0xFF444466) : juce::Colour (0xFF222233));
            g.fillRoundedRectangle (keyRect, 1.5f);

            g.setColour (juce::Colour (0xFF111111).withAlpha (0.3f));
            g.drawRoundedRectangle (keyRect, 1.5f, 0.4f);

            if (isActive)
            {
                activeKeyRect = keyRect;
                activeIsBlack = true;
            }
        }

        // ── Pass 3: LED dot ─────────────────────────────────────────────
        if (currentNote >= startNote
            && currentNote < startNote + totalNotes
            && ! activeKeyRect.isEmpty())
        {
            float dotSize = 6.0f;
            float dotY = activeIsBlack
                ? activeKeyRect.getBottom() - dotSize - 2.0f
                : activeKeyRect.getBottom() - dotSize - 10.0f;
            float dotX = activeKeyRect.getCentreX() - dotSize * 0.5f;

            g.setColour (juce::Colour (0xFF00CC66));
            g.fillEllipse (dotX, dotY, dotSize, dotSize);

            g.setColour (juce::Colour (0xFF00CC66).withAlpha (0.25f));
            g.drawEllipse (dotX - 1.5f, dotY - 1.5f,
                           dotSize + 3.0f, dotSize + 3.0f, 1.0f);
        }
    }

private:
    static bool isBlackKey (int midiNote)
    {
        int n = midiNote % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    int currentNote = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerKeyboard)
};

// ==============================================================================
//  CentsBar — horizontal deviation indicator (-50 to +50 cents)
// ==============================================================================
class CentsBar : public juce::Component
{
public:
    CentsBar() = default;

    void setCents (float c)
    {
        if (std::abs (cents - c) > 0.05f) { cents = c; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
        float centerX = b.getCentreX();
        float h = b.getHeight();

        // Background
        g.setColour (juce::Colour (0xFF1A1A2E));
        g.fillRoundedRectangle (b, 3.0f);

        // Center line
        g.setColour (juce::Colour (0xFF555555));
        g.drawVerticalLine ((int) centerX, b.getY() + 2, b.getBottom() - 2);

        // Tick marks at ±25 cents
        for (float tick : { -25.0f, 25.0f })
        {
            float x = centerX + (tick / 50.0f) * (b.getWidth() * 0.5f - 4.0f);
            g.setColour (juce::Colour (0xFF444444));
            g.drawVerticalLine ((int) x, b.getY() + 4, b.getBottom() - 4);
        }

        // Indicator needle
        float norm = juce::jlimit (-1.0f, 1.0f, cents / 50.0f);
        float needleX = centerX + norm * (b.getWidth() * 0.5f - 6.0f);

        // Color: green near center, yellow mid, red at edges
        float absNorm = std::abs (norm);
        juce::Colour needleCol;
        if (absNorm < 0.15f)
            needleCol = juce::Colour (0xFF00CC66);  // green — in tune
        else if (absNorm < 0.5f)
            needleCol = juce::Colour (0xFFCCCC00);  // yellow
        else
            needleCol = juce::Colour (0xFFCC3333);  // red — way off

        float needleW = 4.0f;
        g.setColour (needleCol);
        g.fillRoundedRectangle (needleX - needleW * 0.5f, b.getY() + 2,
                                 needleW, h - 4.0f, 2.0f);

        // Labels
        g.setColour (juce::Colour (0xFF888888));
        g.setFont (juce::Font (8.0f));
        g.drawText ("-50", b.removeFromLeft (22.0f), juce::Justification::centred);
        g.drawText ("+50", b.removeFromRight (22.0f), juce::Justification::centred);
    }

private:
    float cents = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CentsBar)
};

// ==============================================================================
//  TunerPanel
// ==============================================================================
class TunerPanel : public juce::Component,
                   private juce::Timer
{
public:
    TunerPanel (TunerProcessor& proc)
        : processor (proc)
    {
        titleLabel.setText ("Tuner", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFDDDDDD));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        noteLabel.setText ("-", juce::dontSendNotification);
        noteLabel.setFont (juce::Font (60.0f, juce::Font::bold));
        noteLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF555555));
        noteLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (noteLabel);

        freqLabel.setText ("", juce::dontSendNotification);
        freqLabel.setFont (juce::Font (12.0f));
        freqLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));
        freqLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (freqLabel);

        addAndMakeVisible (centsBar);
        addAndMakeVisible (keyboard);

        // 30 Hz for snappy response
        startTimerHz (30);
    }

    ~TunerPanel() override { stopTimer(); }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);

        titleLabel.setBounds (b.removeFromTop (20));
        b.removeFromTop (2);

        noteLabel.setBounds (b.removeFromTop (70));
        b.removeFromTop (2);

        freqLabel.setBounds (b.removeFromTop (16));
        b.removeFromTop (4);

        centsBar.setBounds (b.removeFromTop (18));
        b.removeFromTop (6);

        // FIX #5: Piano at 50% of remaining height
        int pianoH = b.getHeight() / 2;
        keyboard.setBounds (b.removeFromTop (pianoH));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF0E0E1A));
    }

private:
    void timerCallback() override
    {
        auto result = processor.getResult();

        if (result.active && result.midiNote >= 0)
        {
            if (result.midiNote != lastDisplayedNote)
            {
                lastDisplayedNote = result.midiNote;

                juce::String noteName = TunerProcessor::noteNameFromMidi (result.midiNote);
                noteLabel.setText (noteName, juce::dontSendNotification);
                noteLabel.setColour (juce::Label::textColourId, juce::Colours::white);

                keyboard.setActiveNote (result.midiNote);
            }

            // Always update cents and frequency
            centsBar.setCents (result.centsOff);

            juce::String freqText = juce::String (result.frequency, 1) + " Hz";
            freqLabel.setText (freqText, juce::dontSendNotification);
        }
    }

    TunerProcessor& processor;
    int lastDisplayedNote = -1;

    juce::Label      titleLabel;
    juce::Label      noteLabel;
    juce::Label      freqLabel;
    CentsBar         centsBar;
    TunerKeyboard    keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerPanel)
};
