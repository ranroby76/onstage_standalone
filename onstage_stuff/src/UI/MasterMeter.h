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
    float smoothedLeftLevel = 0.0f;   // Smoothed left channel
    float smoothedRightLevel = 0.0f;  // Smoothed right channel
    
    juce::Colour getLevelColour(float level) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterMeter)
};