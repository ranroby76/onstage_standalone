// ==============================================================================
//  GatePanel.h
//  OnStage - Noise Gate UI
//
//  Features:
//  - Threshold, Attack, Hold, Release, Range controls
//  - Gate state visualization (open/closed indicator)
//  - Gain reduction meter
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/GateProcessor.h"

class PresetManager;

// Gate State Visualization
class GateGraphComponent : public juce::Component, private juce::Timer
{
public:
    GateGraphComponent(GateProcessor& proc) : gateProcessor(proc)
    {
        startTimerHz(60);
    }
    
    ~GateGraphComponent() override { stopTimer(); }
    
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
        
        auto params = gateProcessor.getParams();
        float gateState = gateProcessor.getGateState();
        
        // Draw threshold line
        float thresholdY = juce::jmap(params.thresholdDb, 0.0f, -80.0f, 0.0f, bounds.getHeight());
        g.setColour(juce::Colour(0xFF8B7000));
        g.drawHorizontalLine((int)thresholdY, bounds.getX(), bounds.getRight());
        
        // Threshold label
        g.setColour(juce::Colour(0xFF888888));
        g.setFont(10.0f);
        g.drawText(juce::String((int)params.thresholdDb) + " dB", 
                   bounds.getRight() - 45, thresholdY - 12, 40, 12, 
                   juce::Justification::right);
        
        // Gate state indicator (large circle)
        float centerX = bounds.getCentreX();
        float centerY = bounds.getCentreY();
        float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.3f;
        
        // Outer ring
        g.setColour(juce::Colour(0xFF404040));
        g.drawEllipse(centerX - radius - 2, centerY - radius - 2, 
                      (radius + 2) * 2, (radius + 2) * 2, 2.0f);
        
        // Inner fill based on gate state
        juce::Colour gateColor = juce::Colour(0xFFD4AF37).interpolatedWith(
            juce::Colour(0xFF2A2A2A), 1.0f - gateState);
        
        g.setColour(gateColor);
        g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
        
        // Gate state text
        g.setColour(gateState > 0.5f ? juce::Colours::black : juce::Colours::grey);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(gateState > 0.5f ? "OPEN" : "CLOSED", 
                   bounds.withTrimmedTop(bounds.getHeight() * 0.4f).withTrimmedBottom(bounds.getHeight() * 0.4f),
                   juce::Justification::centred);
        
        // Gain reduction bar on the right
        float reductionDb = gateProcessor.getCurrentGainReductionDb();
        float reductionNorm = juce::jmap(reductionDb, 0.0f, -80.0f, 0.0f, 1.0f);
        
        auto meterBounds = bounds.removeFromRight(20).reduced(2, 10);
        
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRect(meterBounds);
        
        float meterHeight = meterBounds.getHeight() * reductionNorm;
        auto reductionRect = meterBounds.withHeight(meterHeight);
        
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.8f));
        g.fillRect(reductionRect);
        
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(meterBounds, 1.0f);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds().toFloat(), 1.0f);
    }
    
    void timerCallback() override { repaint(); }
    
private:
    GateProcessor& gateProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GateGraphComponent)
};

class GatePanel : public juce::Component, private juce::Timer
{
public:
    GatePanel(GateProcessor& proc, PresetManager& /*presets*/)
        : gateProcessor(proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = gateProcessor.getParams();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!gateProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            gateProcessor.setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Noise Gate", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Create sliders
        auto createSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                                const juce::String& midi, double min, double max, double val,
                                const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setMidiInfo(midi);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(val);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };
        
        createSlider(thresholdSlider, "Threshold", "CC 70", -80.0, 0.0, params.thresholdDb, " dB");
        createSlider(attackSlider, "Attack", "CC 71", 0.1, 50.0, params.attackMs, " ms");
        createSlider(holdSlider, "Hold", "CC 72", 0.0, 500.0, params.holdMs, " ms");
        createSlider(releaseSlider, "Release", "CC 73", 10.0, 1000.0, params.releaseMs, " ms");
        createSlider(rangeSlider, "Range", "CC 74", -80.0, 0.0, params.rangeDb, " dB");
        
        // Graph component
        graphComponent = std::make_unique<GateGraphComponent>(gateProcessor);
        addAndMakeVisible(graphComponent.get());

        startTimerHz(15);
    }
    
    ~GatePanel() override
    {
        stopTimer();
        thresholdSlider->getSlider().setLookAndFeel(nullptr);
        attackSlider->getSlider().setLookAndFeel(nullptr);
        holdSlider->getSlider().setLookAndFeel(nullptr);
        releaseSlider->getSlider().setLookAndFeel(nullptr);
        rangeSlider->getSlider().setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(40);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        area.removeFromTop(10);

        // Controls on left (original sizes)
        int sliderAreaWidth = 400;
        auto sliderArea = area.removeFromLeft(sliderAreaWidth);
        area.removeFromLeft(20);
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout sliders
        int numSliders = 5;
        int sliderWidth = 60;
        int spacing = 20;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = sliderArea.getX();
        auto sArea = sliderArea.withX(startX).withWidth(totalW);
        
        thresholdSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        attackSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        holdSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        releaseSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        rangeSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromPreset()
    {
        auto p = gateProcessor.getParams();
        thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        holdSlider->setValue(p.holdMs, juce::dontSendNotification);
        releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
        rangeSlider->setValue(p.rangeDb, juce::dontSendNotification);
        toggleButton->setToggleState(!gateProcessor.isBypassed(), juce::dontSendNotification);
    }
    
private:
    void timerCallback() override
    {
        auto p = gateProcessor.getParams();
        
        if (!thresholdSlider->getSlider().isMouseOverOrDragging())
            thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        if (!attackSlider->getSlider().isMouseOverOrDragging())
            attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        if (!holdSlider->getSlider().isMouseOverOrDragging())
            holdSlider->setValue(p.holdMs, juce::dontSendNotification);
        if (!releaseSlider->getSlider().isMouseOverOrDragging())
            releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
        if (!rangeSlider->getSlider().isMouseOverOrDragging())
            rangeSlider->setValue(p.rangeDb, juce::dontSendNotification);
        
        bool shouldBeOn = !gateProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void updateProcessor()
    {
        GateProcessor::Params p;
        p.thresholdDb = (float)thresholdSlider->getValue();
        p.attackMs = (float)attackSlider->getValue();
        p.holdMs = (float)holdSlider->getValue();
        p.releaseMs = (float)releaseSlider->getValue();
        p.rangeDb = (float)rangeSlider->getValue();
        gateProcessor.setParams(p);
    }
    
    GateProcessor& gateProcessor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> thresholdSlider, attackSlider, holdSlider, releaseSlider, rangeSlider;
    std::unique_ptr<GateGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GatePanel)
};
