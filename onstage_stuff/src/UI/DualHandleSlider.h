// **Changes:** The text drawing logic was already there, but I've slightly adjusted the Y offset to ensure it doesn't get clipped and uses a distinct font.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"

class DualHandleSlider : public juce::Component
{
public:
    DualHandleSlider() { setInterceptsMouseClicks(true, false); }
    
    void setRange(double minimum, double maximum) { minValue = minimum; maxValue = maximum; }
    void setLeftValue(double value) { 
        leftValue = juce::jlimit(minValue, maxValue, value);
        if (leftValue > rightValue - 100.0) leftValue = rightValue - 100.0; 
        repaint();
    }
    void setRightValue(double value) { 
        rightValue = juce::jlimit(minValue, maxValue, value);
        if (rightValue < leftValue + 100.0) rightValue = leftValue + 100.0; 
        repaint();
    }
    double getLeftValue() const { return leftValue; }
    double getRightValue() const { return rightValue; }
    
    void setLeftMidiInfo(const juce::String& info) { leftMidiInfo = info; }
    void setRightMidiInfo(const juce::String& info) { rightMidiInfo = info; }
    
    std::function<void()> onLeftValueChange;
    std::function<void()> onRightValueChange;
    bool isUserDragging() const { return isMouseOverOrDragging() && isMouseButtonDown(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float trackY = bounds.getCentreY() - 10; // Shift track up slightly to make room for text
        float trackHeight = 6.0f;
        
        g.setColour(juce::Colour(0xFF202020));
        g.fillRoundedRectangle(bounds.getX(), trackY - trackHeight/2, bounds.getWidth(), trackHeight, trackHeight/2);
        
        float leftPos = valueToPosition(leftValue);
        float rightPos = valueToPosition(rightValue);
        
        g.setColour(juce::Colour(0xFF4A90E2));
        g.fillRoundedRectangle(bounds.getX(), trackY - trackHeight/2, leftPos - bounds.getX(), trackHeight, trackHeight/2);
        g.setColour(juce::Colour(0xFF7ED321));
        g.fillRoundedRectangle(leftPos, trackY - trackHeight/2, rightPos - leftPos, trackHeight, trackHeight/2);
        g.setColour(juce::Colour(0xFFD0021B));
        g.fillRoundedRectangle(rightPos, trackY - trackHeight/2, bounds.getRight() - rightPos, trackHeight, trackHeight/2);
        
        drawHandle(g, leftPos, trackY, true);
        drawHandle(g, rightPos, trackY, false);
        
        // FIX: Ensure text is distinct
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        // Position text clearly below handles
        g.drawText(formatFrequency(leftValue), (int)(leftPos - 30), (int)(trackY + 20), 60, 20, juce::Justification::centred);
        g.drawText(formatFrequency(rightValue), (int)(rightPos - 30), (int)(trackY + 20), 60, 20, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        float clickX = (float)e.getPosition().getX();
        float leftPos = valueToPosition(leftValue);
        float rightPos = valueToPosition(rightValue);
        draggingLeft = (std::abs(clickX - leftPos) < std::abs(clickX - rightPos));
        if (e.mods.isRightButtonDown()) {
            if (draggingLeft && !leftMidiInfo.isEmpty()) showMidiTooltip(this, leftMidiInfo);
            else if (!draggingLeft && !rightMidiInfo.isEmpty()) showMidiTooltip(this, rightMidiInfo);
            return;
        }
        mouseDrag(e);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        float val = positionToValue((float)e.getPosition().getX());
        if (draggingLeft) { setLeftValue(val); if (onLeftValueChange) onLeftValueChange(); }
        else { setRightValue(val); if (onRightValueChange) onRightValueChange(); }
    }
    
private:
    double minValue = 20.0, maxValue = 20000.0, leftValue = 300.0, rightValue = 3000.0;
    bool draggingLeft = false;
    juce::String leftMidiInfo, rightMidiInfo;
    
    void drawHandle(juce::Graphics& g, float x, float y, bool isLeft)
    {
        g.setColour(juce::Colours::black);
        g.fillEllipse(x - 18, y - 18, 36, 36);
        g.setColour(juce::Colour(0xFFD4AF37)); g.fillEllipse(x - 10.8f, y - 10.8f, 21.6f, 21.6f);
    }
    
    float valueToPosition(double value) const {
        double p = (value - minValue) / (maxValue - minValue);
        p = std::log(1.0 + p * 9.0) / std::log(10.0);
        return getLocalBounds().getX() + (float)p * getLocalBounds().getWidth();
    }
    double positionToValue(float position) const {
        float p = (position - getLocalBounds().getX()) / getLocalBounds().getWidth();
        return minValue + ((std::pow(10.0, juce::jlimit(0.0f, 1.0f, p)) - 1.0) / 9.0) * (maxValue - minValue);
    }
    juce::String formatFrequency(double f) const { return f >= 1000.0 ?
        juce::String(f/1000.0, 1) + " kHz" : juce::String((int)f) + " Hz"; }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualHandleSlider)
};