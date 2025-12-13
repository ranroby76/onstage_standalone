#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class SignalLed : public juce::Component
{
public:
    SignalLed() { setOpaque(false); }

    void setOn(bool shouldBeOn)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        // Draw centered
        auto bounds = getLocalBounds().toFloat().reduced(3.0f);
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre(size, size);
        
        // Base color (Bright Green if ON, Dark Green if OFF)
        g.setColour(isOn ? juce::Colour(0xFF00FF00) : juce::Colour(0xFF002200));
        g.fillEllipse(r);
        
        // Glow effect when ON
        if (isOn)
        {
            g.setGradientFill(juce::ColourGradient(
                juce::Colours::white.withAlpha(0.8f), r.getCentre(),
                juce::Colour(0xFF00FF00).withAlpha(0.0f), r.getTopLeft(),
                true));
            g.fillEllipse(r);
        }
        
        // Border
        g.setColour(juce::Colours::black);
        g.drawEllipse(r, 1.0f);
    }

private:
    bool isOn = false;
};