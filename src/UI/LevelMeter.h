#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AudioEngine.h"

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    enum class Source { Input, Output };
    LevelMeter (AudioEngine& eng, Source src) : engine (eng), source (src)
    {
        startTimerHz (60);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey);
        auto r = getLocalBounds().reduced (2);
        g.setColour (juce::Colours::black);
        g.drawRect (r);

        const auto h = (int) juce::jmap (smoothedLevel, 0.0f, 1.0f, 0.0f, (float) r.getHeight());
        juce::Rectangle<int> bar (r.withY (r.getBottom() - h).withHeight (h));
        
        // CHANGED: Use Gold color for the signal
        g.setColour (juce::Colour(0xFFD4AF37));
        g.fillRect (bar);
    }

private:
    void timerCallback() override
    {
        float targetLevel = (source == Source::Input) ? engine.getInputLevel() : engine.getOutputLevel();
        if (targetLevel > smoothedLevel) smoothedLevel += (targetLevel - smoothedLevel) * 0.7f;
        else smoothedLevel += (targetLevel - smoothedLevel) * 0.3f;
        repaint();
    }

    AudioEngine& engine;
    Source source;
    float smoothedLevel { 0.0f };
};