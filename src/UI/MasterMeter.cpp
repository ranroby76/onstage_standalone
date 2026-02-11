#include "MasterMeter.h"
#include "../AudioEngine.h"

MasterMeter::MasterMeter (AudioEngine& engine)
    : audioEngine (engine)
{
    startTimerHz (60);   // 60 fps for smooth pumping
}

MasterMeter::~MasterMeter() { stopTimer(); }

// ==============================================================================
void MasterMeter::drawMeterBar (juce::Graphics& g,
                                 juce::Rectangle<float> area,
                                 float level,
                                 float peak,
                                 const juce::String& label)
{
    // --- L / R label at bottom ---------------------------------------------------
    constexpr float labelH = 14.0f;
    auto labelArea = area.removeFromBottom (labelH);
    area.removeFromBottom (2.0f);   // tiny gap between label and bar

    // --- Bar background ----------------------------------------------------------
    g.setColour (juce::Colour (0xFF111111));
    g.fillRoundedRectangle (area, 3.0f);

    // --- Segmented fill (bottom-up): green → yellow → red ------------------------
    float clamped = juce::jlimit (0.0f, 1.0f, level);

    if (clamped > 0.0f)
    {
        float barH   = area.getHeight();
        float barBot = area.getBottom();
        float barX   = area.getX();
        float barW   = area.getWidth();

        // Thresholds expressed in normalised 0-1
        constexpr float greenEnd  = 0.75f;
        constexpr float yellowEnd = 0.90f;

        // Green segment: 0 → min(level, 0.75)
        float greenTop = juce::jmin (clamped, greenEnd);
        if (greenTop > 0.0f)
        {
            float h = barH * greenTop;
            juce::Rectangle<float> seg (barX, barBot - h, barW, h);
            g.setColour (juce::Colour (0xFF00CC44));       // vivid green
            g.fillRoundedRectangle (seg, 3.0f);
        }

        // Yellow segment: 0.75 → min(level, 0.90)
        if (clamped > greenEnd)
        {
            float yellowTop = juce::jmin (clamped, yellowEnd);
            float yBot = barBot - barH * greenEnd;
            float yTop = barBot - barH * yellowTop;
            juce::Rectangle<float> seg (barX, yTop, barW, yBot - yTop);
            g.setColour (juce::Colour (0xFFDDCC00));       // warm yellow
            g.fillRect (seg);
        }

        // Red segment: 0.90 → level
        if (clamped > yellowEnd)
        {
            float rBot = barBot - barH * yellowEnd;
            float rTop = barBot - barH * clamped;
            juce::Rectangle<float> seg (barX, rTop, barW, rBot - rTop);
            g.setColour (juce::Colour (0xFFDD2222));       // bright red
            g.fillRect (seg);
        }
    }

    // --- Peak hold indicator (thin horizontal line) ------------------------------
    float clampedPeak = juce::jlimit (0.0f, 1.0f, peak);
    if (clampedPeak > 0.01f)
    {
        float peakY = area.getBottom() - area.getHeight() * clampedPeak;
        // Choose colour matching the zone the peak sits in
        juce::Colour peakColour = (clampedPeak > 0.90f) ? juce::Colour (0xFFFF4444)
                                : (clampedPeak > 0.75f) ? juce::Colour (0xFFEEDD22)
                                                        : juce::Colour (0xFF44EE66);
        g.setColour (peakColour);
        g.fillRect (area.getX() + 1.0f, peakY, area.getWidth() - 2.0f, 2.0f);
    }

    // --- Subtle frame ------------------------------------------------------------
    g.setColour (juce::Colour (0xFF333333));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    // --- Label -------------------------------------------------------------------
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (label, labelArea, juce::Justification::centred, false);
}

// ==============================================================================
void MasterMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    constexpr float separatorW = 3.0f;
    float singleW = (bounds.getWidth() - separatorW) / 2.0f;

    auto leftBounds  = bounds.removeFromLeft (singleW);
    bounds.removeFromLeft (separatorW);          // gap
    auto rightBounds = bounds;

    drawMeterBar (g, leftBounds,  smoothedLeftLevel,  peakLeft,  "L");
    drawMeterBar (g, rightBounds, smoothedRightLevel, peakRight, "R");
}

void MasterMeter::resized() {}

// ==============================================================================
void MasterMeter::timerCallback()
{
    leftLevel  = audioEngine.getOutputLevel (0);
    rightLevel = audioEngine.getOutputLevel (1);

    // Smooth rise/fall (fast attack, moderate release)
    auto smooth = [] (float current, float target) -> float
    {
        if (target > current)
            return current + (target - current) * 0.6f;   // attack
        else
            return current + (target - current) * 0.15f;  // release (slower = smoother tail)
    };

    smoothedLeftLevel  = smooth (smoothedLeftLevel,  leftLevel);
    smoothedRightLevel = smooth (smoothedRightLevel, rightLevel);

    // Peak hold logic
    auto updatePeak = [] (float level, float& peak, int& holdCounter)
    {
        if (level >= peak)
        {
            peak = level;
            holdCounter = peakHoldFrames;
        }
        else if (holdCounter > 0)
        {
            --holdCounter;
        }
        else
        {
            peak *= peakDecayRate;
            if (peak < 0.005f)
                peak = 0.0f;
        }
    };

    updatePeak (smoothedLeftLevel,  peakLeft,  peakHoldCounterL);
    updatePeak (smoothedRightLevel, peakRight, peakHoldCounterR);

    repaint();
}
