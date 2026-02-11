// ==============================================================================
//  PitchPanel.h
//  OnStage - Tuner UI with perfect synchronization
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/PitchProcessor.h"

// ==============================================================================
// Semitone Bar
// ==============================================================================
class SemitoneBar : public juce::Component
{
public:
    void update(int lockedNoteIndex, float cents, bool active)
    {
        this->noteIndex = lockedNoteIndex;
        this->cents = cents;
        this->isActive = active;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();
        float noteW = w / 12.0f;
        
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        static const bool isBlack[] = {false,true,false,true,false,false,true,false,true,false,true,false};
        
        for (int i = 0; i < 12; ++i)
        {
            float x = i * noteW;
            
            if (i == noteIndex && isActive)
            {
                float absCents = std::abs(cents);
                juce::Colour bgCol;
                if (absCents < 10.0f)
                    bgCol = juce::Colour(0xFF2A4A2A);
                else if (absCents < 25.0f)
                    bgCol = juce::Colour(0xFF4A4A2A);
                else
                    bgCol = juce::Colour(0xFF4A2A2A);
                
                g.setColour(bgCol);
                g.fillRect(x + 1, 1.0f, noteW - 2, h - 2);
            }
            
            g.setColour(isBlack[i] ? juce::Colour(0xFF555555) : juce::Colour(0xFF777777));
            g.setFont(9.0f);
            g.drawText(noteNames[i], (int)(x + 2), (int)(h - 13), (int)(noteW - 4), 12, 
                       juce::Justification::centred);
            
            if (i > 0)
            {
                g.setColour(juce::Colour(0xFF333333));
                g.drawVerticalLine((int)x, 0.0f, h - 14.0f);
            }
        }
        
        if (isActive && noteIndex >= 0)
        {
            float cellCenter = (noteIndex + 0.5f) * noteW;
            float centsOffset = (cents / 100.0f) * noteW;
            float indicatorX = cellCenter + centsOffset;
            indicatorX = juce::jlimit(4.0f, w - 4.0f, indicatorX);
            
            float barH = h - 16.0f;
            
            float absCents = std::abs(cents);
            juce::Colour col;
            if (absCents < 10.0f)
                col = juce::Colour(0xFF50C878);
            else if (absCents < 25.0f)
                col = juce::Colour(0xFFD4AF37);
            else
                col = juce::Colour(0xFFFF6B6B);
            
            for (int i = 2; i >= 0; --i)
            {
                g.setColour(col.withAlpha(0.15f - i * 0.04f));
                g.fillRoundedRectangle(indicatorX - 4 - i*2, 2.0f - i, 
                                       8 + i*4, barH + i*2, 3.0f);
            }
            
            g.setColour(col);
            g.fillRoundedRectangle(indicatorX - 3.0f, 2.0f, 6.0f, barH, 2.0f);
            
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.fillRect(indicatorX - 1.0f, 4.0f, 2.0f, barH - 4.0f);
        }
    }
    
private:
    int noteIndex = 0;
    float cents = 0.0f;
    bool isActive = false;
};

// ==============================================================================
// Piano Keyboard
// ==============================================================================
class PianoKeyboard : public juce::Component
{
public:
    void update(int noteIndex, bool active)
    {
        this->activeNote = active ? noteIndex : -1;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float whiteW = bounds.getWidth() / 7.0f;
        float blackW = whiteW * 0.6f;
        float blackH = bounds.getHeight() * 0.6f;
        float h = bounds.getHeight();
        
        static const int whiteNotes[] = {0, 2, 4, 5, 7, 9, 11};
        
        for (int i = 0; i < 7; ++i)
        {
            float x = i * whiteW;
            int noteIdx = whiteNotes[i];
            bool isActive = (activeNote == noteIdx);
            
            if (isActive)
                g.setColour(juce::Colour(0xFFD4AF37));
            else
                g.setColour(juce::Colour(0xFFE8E8E8));
            
            g.fillRect(x + 1, 0.0f, whiteW - 2, h);
            
            g.setColour(juce::Colour(0xFF333333));
            g.drawRect(x, 0.0f, whiteW, h, 1.0f);
        }
        
        static const int blackNotes[] = {1, 3, -1, 6, 8, 10};
        static const float blackPos[] = {0.7f, 1.7f, -1.0f, 3.7f, 4.7f, 5.7f};
        
        for (int i = 0; i < 6; ++i)
        {
            if (blackNotes[i] < 0) continue;
            
            float x = blackPos[i] * whiteW - blackW / 2;
            int noteIdx = blackNotes[i];
            bool isActive = (activeNote == noteIdx);
            
            if (isActive)
                g.setColour(juce::Colour(0xFFD4AF37));
            else
                g.setColour(juce::Colour(0xFF222222));
            
            g.fillRoundedRectangle(x, 0.0f, blackW, blackH, 2.0f);
            
            g.setColour(juce::Colour(0xFF111111));
            g.drawRoundedRectangle(x, 0.0f, blackW, blackH, 2.0f, 1.0f);
        }
    }
    
private:
    int activeNote = -1;
};

// ==============================================================================
// Cents Meter
// ==============================================================================
class CentsMeter : public juce::Component
{
public:
    void update(float cents, bool active)
    {
        this->cents = juce::jlimit(-50.0f, 50.0f, cents);
        this->isActive = active;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();
        
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        g.setColour(juce::Colour(0xFF444444));
        g.drawVerticalLine((int)(w / 2), 2.0f, h - 2.0f);
        
        g.setColour(juce::Colour(0xFF333333));
        for (int i = -4; i <= 4; ++i)
        {
            if (i == 0) continue;
            float x = w/2 + (i * w / 10);
            g.drawVerticalLine((int)x, h - 6.0f, h - 2.0f);
        }
        
        if (!isActive) return;
        
        float absCents = std::abs(cents);
        juce::Colour col;
        if (absCents < 10.0f)
            col = juce::Colour(0xFF50C878);
        else if (absCents < 25.0f)
            col = juce::Colour(0xFFD4AF37);
        else
            col = juce::Colour(0xFFFF6B6B);
        
        float centerX = w / 2;
        float indicatorX = centerX + (cents / 50.0f) * (w / 2 - 10);
        
        for (int i = 2; i >= 0; --i)
        {
            g.setColour(col.withAlpha(0.12f + i * 0.03f));
            g.fillRoundedRectangle(indicatorX - 5 - i*2, 3.0f - i, 
                                   10 + i*4, h - 6 + i*2, 3.0f);
        }
        
        g.setColour(col);
        g.fillRoundedRectangle(indicatorX - 4, 4.0f, 8.0f, h - 8.0f, 3.0f);
    }
    
private:
    float cents = 0.0f;
    bool isActive = false;
};

// ==============================================================================
// Guitar String Display
// ==============================================================================
class GuitarStringDisplay : public juce::Component
{
public:
    void update(int stringIndex, float cents, bool active)
    {
        this->activeString = active ? stringIndex : -1;
        this->stringCents = cents;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float stringW = bounds.getWidth() / 6.0f;
        float h = bounds.getHeight();
        
        static const char* stringNames[] = {"E", "A", "D", "G", "B", "E"};
        
        for (int i = 0; i < 6; ++i)
        {
            float x = i * stringW;
            bool isActive = (i == activeString);
            
            g.setColour(isActive ? juce::Colour(0xFF2A2A2A) : juce::Colour(0xFF1A1A1A));
            g.fillRoundedRectangle(x + 2, 0.0f, stringW - 4, h, 4.0f);
            
            juce::Colour textCol = juce::Colour(0xFF555555);
            if (isActive)
            {
                float absCents = std::abs(stringCents);
                if (absCents < 10.0f)
                    textCol = juce::Colour(0xFF50C878);
                else if (absCents < 25.0f)
                    textCol = juce::Colour(0xFFD4AF37);
                else
                    textCol = juce::Colour(0xFFFF6B6B);
            }
            
            g.setColour(textCol);
            g.setFont(juce::Font(16.0f, juce::Font::bold));
            g.drawText(stringNames[i], (int)x, 0, (int)stringW, (int)h, juce::Justification::centred);
            
            if (isActive)
            {
                g.setColour(textCol.withAlpha(0.4f));
                g.fillRoundedRectangle(x + 6, h - 6, stringW - 12, 4.0f, 2.0f);
            }
        }
    }
    
private:
    int activeString = -1;
    float stringCents = 0.0f;
};

// ==============================================================================
// Main Pitch Panel
// ==============================================================================
class PitchPanel : public juce::Component, private juce::Timer
{
public:
    PitchPanel(PitchProcessor& proc, [[maybe_unused]] class PresetManager& presets)
        : processor(proc)
    {
        addAndMakeVisible(enableBtn);
        enableBtn.setButtonText("ON");
        enableBtn.setClickingTogglesState(true);
        enableBtn.setToggleState(true, juce::dontSendNotification);
        enableBtn.onClick = [this] { 
            processor.setBypassed(!enableBtn.getToggleState());
            enableBtn.setButtonText(enableBtn.getToggleState() ? "ON" : "OFF");
        };
        
        addAndMakeVisible(sensitivitySlider);
        sensitivitySlider.setRange(0.08, 0.25, 0.01);
        sensitivitySlider.setValue(0.15);
        sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        sensitivitySlider.onValueChange = [this] { 
            auto p = processor.getParams();
            p.sensitivity = (float)sensitivitySlider.getValue();
            processor.setParams(p);
        };
        
        addAndMakeVisible(sensitivityLabel);
        sensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
        sensitivityLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        
        addAndMakeVisible(centsMeter);
        addAndMakeVisible(semitoneBar);
        addAndMakeVisible(keyboard);
        addAndMakeVisible(guitarStrings);
        
        startTimerHz(30);
    }
    
    ~PitchPanel() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        
        g.setColour(juce::Colour(0xFFD4AF37));
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText("Vocal Tuner", 15, 8, 150, 25, juce::Justification::centredLeft);
        
        auto noteArea = juce::Rectangle<int>(15, 45, 200, 95);
        g.setColour(juce::Colour(0xFF252525));
        g.fillRoundedRectangle(noteArea.toFloat(), 8.0f);
        
        auto pitch = processor.getCurrentPitch();
        bool hasNote = (pitch.midiNote >= 0);
        
        g.setFont(juce::Font(56.0f, juce::Font::bold));
        
        if (hasNote)
        {
            float absCents = std::abs(pitch.cents);
            juce::Colour noteCol;
            if (absCents < 10.0f)
                noteCol = juce::Colour(0xFF50C878);
            else if (absCents < 25.0f)
                noteCol = juce::Colour(0xFFD4AF37);
            else
                noteCol = juce::Colour(0xFFFF6B6B);
            
            if (!pitch.isActive)
                noteCol = noteCol.withAlpha(0.5f);
            
            g.setColour(noteCol);
            juce::String noteName = PitchProcessor::getNoteName(pitch.noteIndex) + juce::String(pitch.octave);
            g.drawText(noteName, noteArea.withTrimmedBottom(35), juce::Justification::centred);
            
            g.setColour(juce::Colour(0xFF888888).withAlpha(pitch.isActive ? 1.0f : 0.5f));
            g.setFont(14.0f);
            g.drawText(juce::String(pitch.frequency, 1) + " Hz", 
                       noteArea.getX(), noteArea.getBottom() - 32, noteArea.getWidth(), 15, 
                       juce::Justification::centred);
            
            juce::Colour centsCol = noteCol;
            if (!pitch.isActive)
                centsCol = centsCol.withAlpha(0.5f);
            
            g.setColour(centsCol);
            juce::String centsText = (pitch.cents >= 0 ? "+" : "") + 
                                     juce::String((int)std::round(pitch.cents)) + " ct";
            g.drawText(centsText, 
                       noteArea.getX(), noteArea.getBottom() - 17, noteArea.getWidth(), 15, 
                       juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colour(0xFF444444));
            g.drawText("--", noteArea.withTrimmedBottom(35), juce::Justification::centred);
            
            g.setFont(14.0f);
            g.setColour(juce::Colour(0xFF555555));
            g.drawText("--- Hz", noteArea.getX(), noteArea.getBottom() - 32, noteArea.getWidth(), 15, 
                       juce::Justification::centred);
            g.drawText("-- ct", noteArea.getX(), noteArea.getBottom() - 17, noteArea.getWidth(), 15, 
                       juce::Justification::centred);
        }
        
        g.setColour(pitch.isActive ? juce::Colour(0xFF50C878) : juce::Colour(0xFF444444));
        g.fillEllipse(noteArea.getRight() - 18.0f, noteArea.getY() + 8.0f, 10.0f, 10.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        auto topRow = bounds.removeFromTop(30);
        enableBtn.setBounds(topRow.removeFromRight(50));
        
        bounds.removeFromTop(10);
        
        auto leftArea = bounds.removeFromLeft(220);
        leftArea.removeFromTop(100);
        
        leftArea.removeFromTop(8);
        centsMeter.setBounds(leftArea.removeFromTop(28));
        
        leftArea.removeFromTop(8);
        semitoneBar.setBounds(leftArea.removeFromTop(40));
        
        leftArea.removeFromTop(8);
        keyboard.setBounds(leftArea.removeFromTop(65));
        
        leftArea.removeFromTop(8);
        guitarStrings.setBounds(leftArea.removeFromTop(40));
        
        bounds.removeFromLeft(20);
        auto rightArea = bounds;
        
        rightArea.removeFromTop(60);
        sensitivityLabel.setBounds(rightArea.removeFromTop(18));
        sensitivitySlider.setBounds(rightArea.removeFromTop(24));
    }

private:
    void timerCallback() override
    {
        auto pitch = processor.getCurrentPitch();
        bool hasNote = (pitch.midiNote >= 0);
        
        centsMeter.update(pitch.cents, hasNote && pitch.isActive);
        semitoneBar.update(pitch.noteIndex, pitch.cents, hasNote);
        keyboard.update(pitch.noteIndex, hasNote);
        guitarStrings.update(pitch.nearestGuitarString, pitch.stringCents, hasNote && pitch.isActive);
        
        repaint();
    }
    
    PitchProcessor& processor;
    
    juce::TextButton enableBtn;
    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel;
    
    CentsMeter centsMeter;
    SemitoneBar semitoneBar;
    PianoKeyboard keyboard;
    GuitarStringDisplay guitarStrings;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchPanel)
};
