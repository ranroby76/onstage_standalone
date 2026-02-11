// ==============================================================================
//  EQPanel.h
//  OnStage - 9-Band Parametric EQ UI with frequency response graph
//  Redesigned with gain bars and rectangle knobs for freq/Q
//  FIX: Adjacent band frequency limiting
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EffectToggleButton.h"
#include "../dsp/EQProcessor.h"

class PresetManager;

// ==============================================================================
//  EQ Frequency Response Graph Component
// ==============================================================================
class EQGraphComponent : public juce::Component, private juce::Timer
{
public:
    EQGraphComponent(EQProcessor& processor) : eqProcessor(processor) { startTimerHz(30); }
    ~EQGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Grid lines
        g.setColour(juce::Colour(0xFF2A2A2A));
        for (int i = 1; i < 5; ++i)
        {
            float y = bounds.getHeight() * i / 5.0f;
            g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
        }
        
        // Frequency markers
        g.setColour(juce::Colour(0xFF333333));
        float freqs[] = { 100.0f, 1000.0f, 10000.0f };
        for (float freq : freqs)
        {
            float x = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f) * bounds.getWidth();
            g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        }
        
        // Frequency labels
        g.setColour(juce::Colour(0xFF555555));
        g.setFont(10.0f);
        g.drawText("100", (int)(std::log10(100.0f / 20.0f) / std::log10(20000.0f / 20.0f) * bounds.getWidth()) - 15, 
                   (int)bounds.getBottom() - 14, 30, 12, juce::Justification::centred);
        g.drawText("1k", (int)(std::log10(1000.0f / 20.0f) / std::log10(20000.0f / 20.0f) * bounds.getWidth()) - 15, 
                   (int)bounds.getBottom() - 14, 30, 12, juce::Justification::centred);
        g.drawText("10k", (int)(std::log10(10000.0f / 20.0f) / std::log10(20000.0f / 20.0f) * bounds.getWidth()) - 15, 
                   (int)bounds.getBottom() - 14, 30, 12, juce::Justification::centred);
        
        // Draw response curve
        juce::Path responseCurve;
        bool first = true;
        
        for (int x = 0; x < getWidth(); ++x)
        {
            float freq = 20.0f * std::pow(1000.0f, x / (float)getWidth());
            float totalGain = 0.0f;
            
            for (int band = 0; band < EQProcessor::kNumBands; ++band)
            {
                float bandFreq = eqProcessor.getBandFrequency(band);
                float bandGain = eqProcessor.getBandGain(band);
                float bandQ = eqProcessor.getBandQ(band);
                
                totalGain += calculateBellGain(freq, bandFreq, bandGain, bandQ);
            }
            
            float y = juce::jmap(totalGain, 15.0f, -15.0f, 0.0f, (float)getHeight());
            
            if (first)
            {
                responseCurve.startNewSubPath((float)x, y);
                first = false;
            }
            else
            {
                responseCurve.lineTo((float)x, y);
            }
        }
        
        // Glow effect
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.3f));
        g.strokePath(responseCurve, juce::PathStrokeType(4.0f));
        
        g.setColour(juce::Colour(0xFFD4AF37));
        g.strokePath(responseCurve, juce::PathStrokeType(2.0f));
        
        // Zero line
        float zeroY = getHeight() / 2.0f;
        g.setColour(juce::Colour(0xFF505050));
        g.drawHorizontalLine((int)zeroY, 0.0f, (float)getWidth());
        
        // dB labels
        g.setColour(juce::Colour(0xFF555555));
        g.setFont(9.0f);
        g.drawText("+15", 2, 2, 25, 12, juce::Justification::centredLeft);
        g.drawText("0", 2, (int)zeroY - 6, 20, 12, juce::Justification::centredLeft);
        g.drawText("-15", 2, getHeight() - 14, 25, 12, juce::Justification::centredLeft);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override { repaint(); }
    
private:
    float calculateBellGain(float freq, float centerFreq, float gainDb, float q)
    {
        if (std::abs(gainDb) < 0.01f) return 0.0f;
        float ratio = freq / centerFreq;
        float logRatio = std::log2(ratio);
        float bandwidth = 1.0f / q;
        float response = std::exp(-logRatio * logRatio / (bandwidth * bandwidth));
        return gainDb * response;
    }
    
    EQProcessor& eqProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQGraphComponent)
};

// ==============================================================================
//  Gain Bar - Vertical bar that responds to click and drag
// ==============================================================================
class EQGainBar : public juce::Component
{
public:
    EQGainBar(int bandIdx) : bandIndex(bandIdx)
    {
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Background
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(bounds, 3.0f);
        
        // Border
        g.setColour(isMouseOver() ? juce::Colour(0xFF555555) : juce::Colour(0xFF333333));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        
        // Center line (0 dB)
        float centerY = bounds.getCentreY();
        g.setColour(juce::Colour(0xFF444444));
        g.drawHorizontalLine((int)centerY, bounds.getX() + 2, bounds.getRight() - 2);
        
        // Calculate bar position
        float normalizedValue = (gain + 15.0f) / 30.0f; // -15 to +15 -> 0 to 1
        
        // Draw the bar from center
        juce::Colour barColour = juce::Colour(0xFFD4AF37);
        if (gain > 0)
        {
            // Positive gain - bar goes up from center
            float barTop = centerY - (gain / 15.0f) * (bounds.getHeight() / 2.0f);
            g.setColour(barColour.withAlpha(0.8f));
            g.fillRoundedRectangle(bounds.getX() + 3, barTop, bounds.getWidth() - 6, centerY - barTop, 2.0f);
            
            // Glow
            g.setColour(barColour.withAlpha(0.3f));
            g.fillRoundedRectangle(bounds.getX() + 1, barTop - 2, bounds.getWidth() - 2, centerY - barTop + 4, 3.0f);
        }
        else if (gain < 0)
        {
            // Negative gain - bar goes down from center
            float barBottom = centerY + (-gain / 15.0f) * (bounds.getHeight() / 2.0f);
            g.setColour(barColour.withAlpha(0.6f));
            g.fillRoundedRectangle(bounds.getX() + 3, centerY, bounds.getWidth() - 6, barBottom - centerY, 2.0f);
        }
        
        // Value indicator line
        float indicatorY = bounds.getY() + bounds.getHeight() * (1.0f - normalizedValue);
        g.setColour(juce::Colours::white);
        g.fillRect(bounds.getX() + 2, indicatorY - 1, bounds.getWidth() - 4, 2.0f);
        
        // Band number at bottom
        g.setColour(juce::Colour(0xFFD4AF37));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(juce::String(bandIndex + 1), bounds.toNearestInt(), juce::Justification::centredBottom);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        updateValueFromMouse(e.position.y);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        updateValueFromMouse(e.position.y);
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        // Reset to 0 dB on double-click
        gain = 0.0f;
        if (onValueChange)
            onValueChange(gain);
        repaint();
    }
    
    void setValue(float newGain, bool notify = true)
    {
        newGain = juce::jlimit(-15.0f, 15.0f, newGain);
        if (std::abs(gain - newGain) > 0.01f)
        {
            gain = newGain;
            if (notify && onValueChange)
                onValueChange(gain);
            repaint();
        }
    }
    
    float getValue() const { return gain; }
    
    std::function<void(float)> onValueChange;
    
private:
    void updateValueFromMouse(float mouseY)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        float normalizedValue = 1.0f - (mouseY - bounds.getY()) / bounds.getHeight();
        normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
        float newGain = normalizedValue * 30.0f - 15.0f; // 0 to 1 -> -15 to +15
        setValue(newGain);
    }
    
    int bandIndex;
    float gain = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQGainBar)
};

// ==============================================================================
//  Rectangle Knob - Click & drag up/down with triangle indicators
//  FIX: Added updateLimits() for dynamic min/max adjustment
// ==============================================================================
class RectangleKnob : public juce::Component
{
public:
    RectangleKnob()
    {
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        // Background
        bool hover = isMouseOver() || isDragging;
        g.setColour(hover ? juce::Colour(0xFF2A2A2A) : juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(hover ? juce::Colour(0xFFD4AF37) : juce::Colour(0xFF444444));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        // Left triangle (pointing up)
        juce::Path leftTriangle;
        float triSize = 6.0f;
        float leftX = bounds.getX() + 5.0f;
        float triY = bounds.getCentreY();
        leftTriangle.addTriangle(leftX, triY + triSize/2, 
                                  leftX + triSize, triY + triSize/2, 
                                  leftX + triSize/2, triY - triSize/2);
        g.setColour(juce::Colour(0xFF666666));
        g.fillPath(leftTriangle);
        
        // Right triangle (pointing down)
        juce::Path rightTriangle;
        float rightX = bounds.getRight() - 5.0f - triSize;
        rightTriangle.addTriangle(rightX, triY - triSize/2, 
                                   rightX + triSize, triY - triSize/2, 
                                   rightX + triSize/2, triY + triSize/2);
        g.fillPath(rightTriangle);
        
        // Value text
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(getDisplayText(), bounds, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        isDragging = true;
        lastMouseY = e.position.y;
        dragStartValue = value;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            float delta = lastMouseY - e.position.y; // Up = positive
            float sensitivity = (currentMaxValue - currentMinValue) / 150.0f;
            
            if (useLogScale)
            {
                // Logarithmic scaling for frequency
                float logMin = std::log10(currentMinValue);
                float logMax = std::log10(currentMaxValue);
                float logCurrent = std::log10(value);
                float logDelta = delta * (logMax - logMin) / 150.0f;
                float newLogValue = juce::jlimit(logMin, logMax, logCurrent + logDelta);
                setValue(std::pow(10.0f, newLogValue));
            }
            else
            {
                setValue(value + delta * sensitivity);
            }
            
            lastMouseY = e.position.y;
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        setValue(defaultValue);
    }
    
    void setRange(float min, float max, float def, bool logarithmic = false)
    {
        absoluteMinValue = min;
        absoluteMaxValue = max;
        currentMinValue = min;
        currentMaxValue = max;
        defaultValue = def;
        useLogScale = logarithmic;
        value = juce::jlimit(currentMinValue, currentMaxValue, value);
    }
    
    // FIX: Update dynamic limits based on adjacent bands
    void updateLimits(float newMin, float newMax)
    {
        // Clamp to absolute range
        currentMinValue = juce::jmax(absoluteMinValue, newMin);
        currentMaxValue = juce::jmin(absoluteMaxValue, newMax);
        
        // Ensure current value is still within new limits
        if (value < currentMinValue || value > currentMaxValue)
        {
            value = juce::jlimit(currentMinValue, currentMaxValue, value);
            repaint();
        }
    }
    
    void setValue(float newValue, bool notify = true)
    {
        newValue = juce::jlimit(currentMinValue, currentMaxValue, newValue);
        if (std::abs(value - newValue) > 0.001f)
        {
            value = newValue;
            if (notify && onValueChange)
                onValueChange(value);
            repaint();
        }
    }
    
    float getValue() const { return value; }
    float getMinLimit() const { return currentMinValue; }
    float getMaxLimit() const { return currentMaxValue; }
    
    void setDisplayMode(int mode) { displayMode = mode; } // 0 = Hz, 1 = Q
    
    std::function<void(float)> onValueChange;
    
private:
    juce::String getDisplayText() const
    {
        if (displayMode == 0) // Frequency
        {
            if (value >= 1000.0f)
                return juce::String(value / 1000.0f, 1) + "k";
            else
                return juce::String((int)value);
        }
        else // Q
        {
            return juce::String(value, 2);
        }
    }
    
    float value = 1000.0f;
    float absoluteMinValue = 20.0f;
    float absoluteMaxValue = 20000.0f;
    float currentMinValue = 20.0f;
    float currentMaxValue = 20000.0f;
    float defaultValue = 1000.0f;
    bool useLogScale = false;
    int displayMode = 0; // 0 = Hz, 1 = Q
    
    bool isDragging = false;
    float lastMouseY = 0.0f;
    float dragStartValue = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RectangleKnob)
};

// ==============================================================================
//  Main EQ Panel
// ==============================================================================
class EQPanel : public juce::Component, private juce::Timer
{
public:
    EQPanel(EQProcessor& processor, PresetManager& /*presets*/)
        : eqProcessor(processor)
    {
        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!eqProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() {
            eqProcessor.setBypassed(!toggleButton->getToggleState());
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        titleLabel.setText("9-Band EQ", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        addAndMakeVisible(titleLabel);
        
        // Labels for rows
        gainLabel.setText("GAIN", juce::dontSendNotification);
        gainLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        gainLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        gainLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(gainLabel);
        
        freqLabel.setText("FREQ", juce::dontSendNotification);
        freqLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        freqLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        freqLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(freqLabel);
        
        qLabel.setText("Q", juce::dontSendNotification);
        qLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        qLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        qLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(qLabel);
        
        // Default frequencies for 9 bands
        float defaultFreqs[] = { 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
        
        // Create band controls
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            // Gain bars
            gainBars[i] = std::make_unique<EQGainBar>(i);
            gainBars[i]->setValue(eqProcessor.getBandGain(i), false);
            gainBars[i]->onValueChange = [this, i](float val) {
                eqProcessor.setBandGain(i, val);
            };
            addAndMakeVisible(gainBars[i].get());
            
            // Frequency knobs
            freqKnobs[i] = std::make_unique<RectangleKnob>();
            freqKnobs[i]->setRange(20.0f, 20000.0f, defaultFreqs[i], true);
            freqKnobs[i]->setValue(eqProcessor.getBandFrequency(i), false);
            freqKnobs[i]->setDisplayMode(0); // Hz
            freqKnobs[i]->onValueChange = [this, i](float val) {
                eqProcessor.setBandFrequency(i, val);
                updateFrequencyLimits();  // FIX: Update adjacent band limits
            };
            addAndMakeVisible(freqKnobs[i].get());
            
            // Q knobs
            qKnobs[i] = std::make_unique<RectangleKnob>();
            qKnobs[i]->setRange(0.1f, 10.0f, 1.0f, false);
            qKnobs[i]->setValue(eqProcessor.getBandQ(i), false);
            qKnobs[i]->setDisplayMode(1); // Q
            qKnobs[i]->onValueChange = [this, i](float val) {
                eqProcessor.setBandQ(i, val);
            };
            addAndMakeVisible(qKnobs[i].get());
        }
        
        // Graph
        graphComponent = std::make_unique<EQGraphComponent>(eqProcessor);
        addAndMakeVisible(graphComponent.get());
        
        // Initialize frequency limits
        updateFrequencyLimits();
        
        startTimerHz(15);
    }
    
    ~EQPanel() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        
        // Left panel background
        auto leftPanel = getLocalBounds().reduced(10).removeFromLeft(getWidth() / 2 - 20);
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(leftPanel.toFloat(), 5.0f);
        
        // Divider line
        int dividerX = getWidth() / 2;
        g.setColour(juce::Colour(0xFF333333));
        g.drawVerticalLine(dividerX, 60.0f, (float)getHeight() - 10.0f);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Title row
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        
        area.removeFromTop(10);
        
        // Split into left (controls) and right (graph)
        int dividerX = area.getWidth() / 2;
        auto controlsArea = area.removeFromLeft(dividerX - 10);
        area.removeFromLeft(20); // Gap
        auto graphArea = area;
        
        // Graph fills right side
        graphComponent->setBounds(graphArea);
        
        // Controls layout
        constexpr int labelWidth = 35;
        constexpr int barWidth = 45;
        constexpr int barSpacing = 6;
        constexpr int knobHeight = 28;
        constexpr int rowSpacing = 8;
        
        // Calculate starting X to align bars
        int barsStartX = controlsArea.getX() + labelWidth + 10;
        
        // Gain bars row (takes most of the height)
        int gainBarHeight = controlsArea.getHeight() - (knobHeight * 2) - (rowSpacing * 3) - 10;
        auto gainRow = controlsArea.removeFromTop(gainBarHeight);
        gainLabel.setBounds(gainRow.removeFromLeft(labelWidth));
        
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            int x = barsStartX + i * (barWidth + barSpacing);
            gainBars[i]->setBounds(x, gainRow.getY(), barWidth, gainBarHeight);
        }
        
        controlsArea.removeFromTop(rowSpacing);
        
        // Frequency knobs row
        auto freqRow = controlsArea.removeFromTop(knobHeight);
        freqLabel.setBounds(freqRow.removeFromLeft(labelWidth));
        
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            int x = barsStartX + i * (barWidth + barSpacing);
            freqKnobs[i]->setBounds(x, freqRow.getY(), barWidth, knobHeight);
        }
        
        controlsArea.removeFromTop(rowSpacing);
        
        // Q knobs row
        auto qRow = controlsArea.removeFromTop(knobHeight);
        qLabel.setBounds(qRow.removeFromLeft(labelWidth));
        
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            int x = barsStartX + i * (barWidth + barSpacing);
            qKnobs[i]->setBounds(x, qRow.getY(), barWidth, knobHeight);
        }
    }
    
    void updateFromPreset()
    {
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            gainBars[i]->setValue(eqProcessor.getBandGain(i), false);
            freqKnobs[i]->setValue(eqProcessor.getBandFrequency(i), false);
            qKnobs[i]->setValue(eqProcessor.getBandQ(i), false);
        }
        toggleButton->setToggleState(!eqProcessor.isBypassed(), juce::dontSendNotification);
        updateFrequencyLimits();
    }
    
private:
    // FIX: Update frequency limits for all bands based on adjacent bands
    void updateFrequencyLimits()
    {
        constexpr float absoluteMin = 20.0f;
        constexpr float absoluteMax = 20000.0f;
        constexpr float minGap = 1.0f;  // Minimum 1 Hz gap between bands
        
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            float minLimit = absoluteMin;
            float maxLimit = absoluteMax;
            
            // Get limit from band below (must be > lower band's frequency)
            if (i > 0)
            {
                float lowerFreq = freqKnobs[i - 1]->getValue();
                minLimit = lowerFreq + minGap;
            }
            
            // Get limit from band above (must be < upper band's frequency)
            if (i < EQProcessor::kNumBands - 1)
            {
                float upperFreq = freqKnobs[i + 1]->getValue();
                maxLimit = upperFreq - minGap;
            }
            
            freqKnobs[i]->updateLimits(minLimit, maxLimit);
        }
    }
    
    void timerCallback() override
    {
        for (int i = 0; i < EQProcessor::kNumBands; ++i)
        {
            if (!gainBars[i]->isMouseOverOrDragging())
                gainBars[i]->setValue(eqProcessor.getBandGain(i), false);
            if (!freqKnobs[i]->isMouseOverOrDragging())
                freqKnobs[i]->setValue(eqProcessor.getBandFrequency(i), false);
            if (!qKnobs[i]->isMouseOverOrDragging())
                qKnobs[i]->setValue(eqProcessor.getBandQ(i), false);
        }
        
        bool shouldBeOn = !eqProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    EQProcessor& eqProcessor;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::Label gainLabel, freqLabel, qLabel;
    
    std::unique_ptr<EQGainBar> gainBars[EQProcessor::kNumBands];
    std::unique_ptr<RectangleKnob> freqKnobs[EQProcessor::kNumBands];
    std::unique_ptr<RectangleKnob> qKnobs[EQProcessor::kNumBands];
    
    std::unique_ptr<EQGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPanel)
};