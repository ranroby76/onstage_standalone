// **Changes:** 1.  **Right-Click:** `mouseDown` checks right-click, shows tooltip, and **returns immediately**. `mouseUp` also blocks right-click to prevent toggle logic. 2.  **Visuals:** `paintButton` now uses `0xFFD4AF37` (Gold) background with **Black** text when ON. <!-- end list -->

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h" 

class EffectToggleButton : public juce::ToggleButton
{
public:
    EffectToggleButton()
    {
        setButtonText("");
        setToggleState(true, juce::dontSendNotification);
    }
    
    void setMidiInfo(const juce::String& info) { midiInfo = info; }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo);
            return; // STRICT BLOCK: Do not call base class
        }
        juce::ToggleButton::mouseDown(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            return; // STRICT BLOCK
        }
        juce::ToggleButton::mouseUp(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        juce::ToggleButton::mouseDrag(e);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat();
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto squareBounds = bounds.withSizeKeepingCentre(size, size);
        
        bool isOn = getToggleState();
        
        // FIX: ON = Gold Background, OFF = Dark Grey
        g.setColour(isOn ? juce::Colour(0xFFD4AF37) : juce::Colour(0xFF404040));
        g.fillRoundedRectangle(squareBounds, 3.0f);
        
        // Border
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(squareBounds, 3.0f, 2.0f);
        
        // FIX: ON = Black Text, OFF = White Text
        g.setColour(isOn ? juce::Colours::black : juce::Colours::white);
        g.setFont(juce::Font(size * 0.35f, juce::Font::bold));
        g.drawText("ON", squareBounds, juce::Justification::centred);
    }
    
private:
    juce::String midiInfo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectToggleButton)
};