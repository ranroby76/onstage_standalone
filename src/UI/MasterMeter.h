#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../AudioEngine.h"

class MasterMeter : public juce::Component, public juce::Timer
{
public:
    MasterMeter(AudioEngine& engine);
    ~MasterMeter() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioEngine& audioEngine;
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;
    float smoothedLeftLevel = 0.0f;
    float smoothedRightLevel = 0.0f;

    // Peak hold
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    int   peakHoldCounterL = 0;
    int   peakHoldCounterR = 0;
    static constexpr int peakHoldFrames = 30;   // ~0.5 s at 60 Hz
    static constexpr float peakDecayRate = 0.97f;

    void drawMeterBar (juce::Graphics& g, juce::Rectangle<float> area,
                       float level, float peak, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterMeter)
};
