#include "MasterMeter.h"
#include "../AudioEngine.h"

MasterMeter::MasterMeter(AudioEngine& engine)
    : audioEngine(engine), smoothedLeftLevel(0.0f), smoothedRightLevel(0.0f), leftLevel(0.0f), rightLevel(0.0f)
{
    startTimerHz(30);
}

MasterMeter::~MasterMeter() { stopTimer(); }

void MasterMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;

    float separatorWidth = 2.0f;
    float singleMeterWidth = (bounds.getWidth() - separatorWidth) / 2.0f;
    
    auto leftBounds = bounds.removeFromLeft(singleMeterWidth);
    auto separatorBounds = bounds.removeFromLeft(separatorWidth);
    auto rightBounds = bounds;

    auto drawBar = [&](juce::Rectangle<float> area, float level, const juce::String& label) {
        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(area, 3.0f);

        // Gold Bar
        float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
        auto meterHeight = area.getHeight() * clampedLevel;
        auto meterBounds = area.removeFromBottom(meterHeight);

        g.setColour(juce::Colour(0xFFD4AF37)); // Gold
        g.fillRoundedRectangle(meterBounds, 3.0f);

        // Frame
        g.setColour(juce::Colour(0xff333333));
        g.drawRoundedRectangle(area.withHeight(area.getHeight()), 3.0f, 1.0f);

        // Label
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        auto labelArea = area.removeFromBottom(15);
        g.drawText(label, labelArea, juce::Justification::centred);
    };

    drawBar(leftBounds, smoothedLeftLevel, "L");
    
    // Separator
    g.setColour(juce::Colour(0xff404040));
    g.fillRect(separatorBounds);
    
    drawBar(rightBounds, smoothedRightLevel, "R");
}

void MasterMeter::resized() {}

void MasterMeter::timerCallback()
{
    leftLevel = audioEngine.getOutputLevel(0);
    rightLevel = audioEngine.getOutputLevel(1);

    auto smooth = [](float current, float target) {
        if (target > current) return current + (target - current) * 0.7f;
        else return current + (target - current) * 0.3f;
    };

    smoothedLeftLevel = smooth(smoothedLeftLevel, leftLevel);
    smoothedRightLevel = smooth(smoothedRightLevel, rightLevel);
    repaint();
}