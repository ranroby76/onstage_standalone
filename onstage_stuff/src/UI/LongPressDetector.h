// This is the helper class that handles the timer and distance logic.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// A Mix-in class to add Long Press detection to any Component
class LongPressDetector : private juce::Timer
{
public:
    virtual ~LongPressDetector() { stopTimer(); }

    // Call this from your component's mouseDown
    void handleMouseDown(const juce::MouseEvent& e)
    {
        mouseDownTime = e.eventTime;
        mouseDownPos = e.getPosition();
        isLongPressTriggered = false;
        // 800ms is a standard "Long Press" duration
        startTimer(800); 
    }

    // Call this from mouseDrag
    void handleMouseDrag(const juce::MouseEvent& e)
    {
        // If user moves finger/mouse more than 10 pixels, cancel the long press
        if (e.getPosition().getDistanceFrom(mouseDownPos) > 10)
            stopTimer(); 
    }

    // Call this from mouseUp
    void handleMouseUp(const juce::MouseEvent& e)
    {
        stopTimer();
    }

    // Override this in your component to react!
    virtual void onLongPress() = 0;

private:
    void timerCallback() override
    {
        stopTimer();
        isLongPressTriggered = true;
        onLongPress();
    }

    juce::Point<int> mouseDownPos;
    juce::Time mouseDownTime;
protected:
    // Use this flag in mouseUp/mouseDrag to ignore events if long press happened
    bool isLongPressTriggered = false;
};