#pragma once
// ==============================================================================
//  TunerPanel.h
//  OnStage — Vocal/Instrument Tuner panel
//
//  6-octave piano keyboard (C2–B7) with LED dot indicator.
//  Display never clears — last detected note persists forever.
//  No animation, no color changes, no blinking.
// ==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/TunerProcessor.h"

// ==============================================================================
//  Piano Keyboard — 6-octave display with LED
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
        g.fillRoundedRectangle (bounds, 6.0f);

        // 6 octaves: C2 (MIDI 36) to B7 (MIDI 95) = 72 notes
        const int startNote  = 36;
        const int totalNotes = 72;

        // Count white keys
        int whiteCount = 0;
        for (int i = 0; i < totalNotes; ++i)
            if (! isBlackKey (startNote + i)) ++whiteCount;

        if (whiteCount == 0) return;

        float kbX = bounds.getX() + 4.0f;
        float kbW = bounds.getWidth() - 8.0f;
        float kbH = bounds.getHeight() - 4.0f;
        float kbY = bounds.getY() + 2.0f;

        float whiteKeyW = kbW / (float) whiteCount;
        float blackKeyW = whiteKeyW * 0.6f;
        float blackKeyH = kbH * 0.6f;

        juce::Rectangle<float> activeKeyRect;
        bool activeIsBlack = false;

        // ── Pass 1: White keys ──────────────────────────────────────────
        int wIdx = 0;
        for (int i = 0; i < totalNotes; ++i)
        {
            int note = startNote + i;
            if (isBlackKey (note)) continue;

            float x = kbX + wIdx * whiteKeyW;
            juce::Rectangle<float> keyRect (x, kbY, whiteKeyW - 1.0f, kbH);

            bool isActive = (note == currentNote);

            g.setColour (isActive ? juce::Colour (0xFFDDDDFF)
                                  : juce::Colour (0xFFF0F0F0));
            g.fillRoundedRectangle (keyRect, 2.0f);

            g.setColour (juce::Colour (0xFF444444).withAlpha (0.3f));
            g.drawRoundedRectangle (keyRect, 2.0f, 0.5f);

            if (isActive)
                activeKeyRect = keyRect;

            // Label C keys
            if (note % 12 == 0)
            {
                g.setColour (juce::Colour (0xFF888888));
                g.setFont (juce::Font (9.0f));
                g.drawText ("C" + juce::String (note / 12 - 1),
                            keyRect.removeFromBottom (12.0f),
                            juce::Justification::centred);
            }

            ++wIdx;
        }

        // ── Pass 2: Black keys ──────────────────────────────────────────
        wIdx = 0;
        for (int i = 0; i < totalNotes; ++i)
        {
            int note = startNote + i;

            if (! isBlackKey (note))
            {
                ++wIdx;
                continue;
            }

            float x = kbX + (wIdx - 1) * whiteKeyW + whiteKeyW - blackKeyW * 0.5f;
            juce::Rectangle<float> keyRect (x, kbY, blackKeyW, blackKeyH);

            bool isActive = (note == currentNote);

            g.setColour (isActive ? juce::Colour (0xFF444466)
                                  : juce::Colour (0xFF222233));
            g.fillRoundedRectangle (keyRect, 2.0f);

            g.setColour (juce::Colour (0xFF111111).withAlpha (0.4f));
            g.drawRoundedRectangle (keyRect, 2.0f, 0.5f);

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
            float dotSize = 8.0f;
            float dotY = activeIsBlack
                ? activeKeyRect.getBottom() - dotSize - 3.0f
                : activeKeyRect.getBottom() - dotSize - 14.0f;
            float dotX = activeKeyRect.getCentreX() - dotSize * 0.5f;

            g.setColour (juce::Colour (0xFF00CC66));
            g.fillEllipse (dotX, dotY, dotSize, dotSize);

            g.setColour (juce::Colour (0xFF00CC66).withAlpha (0.3f));
            g.drawEllipse (dotX - 2.0f, dotY - 2.0f,
                           dotSize + 4.0f, dotSize + 4.0f, 1.5f);
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
        noteLabel.setFont (juce::Font (72.0f, juce::Font::bold));
        noteLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF555555));
        noteLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (noteLabel);

        addAndMakeVisible (keyboard);

        startTimerHz (20);
    }

    ~TunerPanel() override { stopTimer(); }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10);

        titleLabel.setBounds (b.removeFromTop (24));
        b.removeFromTop (4);

        noteLabel.setBounds (b.removeFromTop (90));
        b.removeFromTop (8);

        keyboard.setBounds (b);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF0E0E1A));
    }

private:
    void timerCallback() override
    {
        auto result = processor.getResult();

        if (result.active && result.midiNote >= 0
            && result.midiNote != lastDisplayedNote)
        {
            lastDisplayedNote = result.midiNote;

            juce::String noteName = TunerProcessor::noteNameFromMidi (result.midiNote);
            noteLabel.setText (noteName, juce::dontSendNotification);
            noteLabel.setColour (juce::Label::textColourId, juce::Colours::white);

            keyboard.setActiveNote (result.midiNote);
        }
    }

    TunerProcessor& processor;
    int lastDisplayedNote = -1;

    juce::Label      titleLabel;
    juce::Label      noteLabel;
    TunerKeyboard    keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerPanel)
};
