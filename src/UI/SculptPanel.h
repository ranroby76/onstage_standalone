// ==============================================================================
//  SculptPanel.h
//  OnStage - Sculpt (Saturation & Tone Shaping) UI
//
//  Features:
//  - 3 mode selector buttons (Tube, Tape, Hybrid)
//  - Drive, Clean Mud, Tame Harsh, Air controls
//  - Frequency response graph
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/SculptProcessor.h"

class PresetManager;

// ==============================================================================
//  Sculpt Mode Selector Button
//  Off: dark gray background, white text, black frame
//  On: golden background, black text, black frame
// ==============================================================================
class SculptModeButton : public juce::Button
{
public:
    SculptModeButton(const juce::String& text)
        : juce::Button(text)
    {
        setClickingTogglesState(false); // We handle toggle manually for radio behavior
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, 
                     bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        bool isOn = getToggleState();
        
        // Background
        if (isOn)
        {
            g.setColour(juce::Colour(0xFFD4AF37)); // Golden
        }
        else
        {
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xFF3A3A3A) : juce::Colour(0xFF2A2A2A)); // Dark gray
        }
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Black frame
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
        
        // Text
        if (isOn)
        {
            g.setColour(juce::Colours::black);
        }
        else
        {
            g.setColour(juce::Colours::white);
        }
        
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(getButtonText(), bounds, juce::Justification::centred);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SculptModeButton)
};

// ==============================================================================
//  Sculpt Graph Component - Shows frequency response
// ==============================================================================
class SculptGraphComponent : public juce::Component, private juce::Timer
{
public:
    SculptGraphComponent(SculptProcessor& proc) : sculptProcessor(proc) 
    { 
        startTimerHz(30); 
    }
    
    ~SculptGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto params = sculptProcessor.getParams();
        
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
        float freqs[] = { 100.0f, 300.0f, 1000.0f, 3500.0f, 10000.0f };
        for (float freq : freqs)
        {
            float x = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f) * bounds.getWidth();
            g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        }
        
        // Draw response curve
        juce::Path responseCurve;
        bool first = true;
        
        for (int x = 0; x < getWidth(); ++x)
        {
            float freq = 20.0f * std::pow(1000.0f, x / (float)getWidth());
            float totalGain = 0.0f;
            
            // Mud cut (300 Hz dip)
            float mudGainDb = -12.0f * params.mudCut;
            totalGain += calculateBellGain(freq, 300.0f, mudGainDb, 1.5f);
            
            // Harsh cut (3.5 kHz dip)
            float harshGainDb = -12.0f * params.harshCut;
            totalGain += calculateBellGain(freq, 3500.0f, harshGainDb, 2.0f);
            
            // Air boost (12 kHz shelf)
            float airGainDb = 10.0f * params.air;
            if (freq > 8000.0f)
            {
                float shelfRatio = (freq - 8000.0f) / 4000.0f;
                shelfRatio = juce::jlimit(0.0f, 1.0f, shelfRatio);
                totalGain += airGainDb * shelfRatio;
            }
            
            float y = juce::jmap(totalGain, 12.0f, -12.0f, 0.0f, (float)getHeight());
            
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
    
    SculptProcessor& sculptProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SculptGraphComponent)
};

// ==============================================================================
//  Main Sculpt Panel
// ==============================================================================
class SculptPanel : public juce::Component, private juce::Timer {
public:
    SculptPanel(SculptProcessor& proc, PresetManager& /*presets*/) 
        : sculptProcessor(proc) 
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = sculptProcessor.getParams(); 

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!sculptProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            sculptProcessor.setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Sculpt", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Description
        addAndMakeVisible(descLabel);
        descLabel.setText("Saturation & Tone Shaping", juce::dontSendNotification);
        descLabel.setFont(juce::Font(12.0f));
        descLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

        // Mode label
        addAndMakeVisible(modeLabel);
        modeLabel.setText("MODE", juce::dontSendNotification);
        modeLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        modeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        modeLabel.setJustificationType(juce::Justification::centredLeft);

        // Create mode selector buttons
        const char* modeNames[] = { "TUBE", "TAPE", "HYBRID" };
        for (int i = 0; i < 3; ++i)
        {
            modeButtons[i] = std::make_unique<SculptModeButton>(modeNames[i]);
            modeButtons[i]->setToggleState(i == static_cast<int>(params.mode), juce::dontSendNotification);
            modeButtons[i]->onClick = [this, i]() { selectMode(i); };
            addAndMakeVisible(modeButtons[i].get());
        }

        // Sliders
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double max, double val, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(n); 
            s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(0.0, max, max/100.0);
            s->setValue(val);
            s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        int ccBase = 90;
        cS(driveSlider, "Drive", ccBase, 1.0, params.drive, "%");
        cS(mudSlider, "Clean Mud", ccBase+1, 1.0, params.mudCut, "%");
        cS(harshSlider, "Tame Harsh", ccBase+2, 1.0, params.harshCut, "%");
        cS(airSlider, "Air", ccBase+3, 1.0, params.air, "%");
        
        // Graph component
        graphComponent = std::make_unique<SculptGraphComponent>(sculptProcessor);
        addAndMakeVisible(graphComponent.get());

        startTimerHz(15);
    }

    ~SculptPanel() override { 
        stopTimer(); 
        driveSlider->getSlider().setLookAndFeel(nullptr);
        mudSlider->getSlider().setLookAndFeel(nullptr);
        harshSlider->getSlider().setLookAndFeel(nullptr);
        airSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override { 
        g.fillAll(juce::Colour(0xFF1A1A1A)); 
        g.setColour(juce::Colour(0xFF404040)); 
        g.drawRect(getLocalBounds(), 2); 
        g.setColour(juce::Colour(0xFF2A2A2A)); 
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override {
        auto area = getLocalBounds().reduced(15);
        
        // Title row
        auto titleRow = area.removeFromTop(30);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        
        // Description
        descLabel.setBounds(area.removeFromTop(18));
        
        area.removeFromTop(8);
        
        // Mode selector row
        auto modeRow = area.removeFromTop(32);
        modeLabel.setBounds(modeRow.removeFromLeft(45));
        modeRow.removeFromLeft(5);
        
        int buttonWidth = 80;
        int buttonSpacing = 10;
        for (int i = 0; i < 3; ++i)
        {
            modeButtons[i]->setBounds(modeRow.removeFromLeft(buttonWidth));
            modeRow.removeFromLeft(buttonSpacing);
        }
        
        area.removeFromTop(15);
        
        // Split: controls on left, graph on right
        int sliderAreaWidth = 360;
        auto sliderArea = area.removeFromLeft(sliderAreaWidth);
        area.removeFromLeft(20); // Gap
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Slider layout
        int sliderW = 80; 
        int gap = 20;
        int startX = sliderArea.getX();
        
        driveSlider->setBounds(startX, sliderArea.getY(), sliderW, sliderArea.getHeight());
        mudSlider->setBounds(startX + sliderW + gap, sliderArea.getY(), sliderW, sliderArea.getHeight());
        harshSlider->setBounds(startX + (sliderW + gap) * 2, sliderArea.getY(), sliderW, sliderArea.getHeight());
        airSlider->setBounds(startX + (sliderW + gap) * 3, sliderArea.getY(), sliderW, sliderArea.getHeight());
    }

    void updateFromPreset() {
        auto p = sculptProcessor.getParams();
        driveSlider->setValue(p.drive, juce::dontSendNotification);
        mudSlider->setValue(p.mudCut, juce::dontSendNotification);
        harshSlider->setValue(p.harshCut, juce::dontSendNotification);
        airSlider->setValue(p.air, juce::dontSendNotification);
        toggleButton->setToggleState(!sculptProcessor.isBypassed(), juce::dontSendNotification);
        
        // Update mode buttons
        int currentMode = static_cast<int>(p.mode);
        for (int i = 0; i < 3; ++i)
            modeButtons[i]->setToggleState(i == currentMode, juce::dontSendNotification);
    }
    
private:
    void selectMode(int modeIndex)
    {
        // Update button states (radio behavior)
        for (int i = 0; i < 3; ++i)
            modeButtons[i]->setToggleState(i == modeIndex, juce::dontSendNotification);
        
        // Update processor
        auto p = sculptProcessor.getParams();
        p.mode = static_cast<SculptProcessor::SaturationMode>(modeIndex);
        sculptProcessor.setParams(p);
    }
    
    void timerCallback() override {
        auto p = sculptProcessor.getParams();
        if (!driveSlider->getSlider().isMouseOverOrDragging()) 
            driveSlider->setValue(p.drive, juce::dontSendNotification);
        if (!mudSlider->getSlider().isMouseOverOrDragging()) 
            mudSlider->setValue(p.mudCut, juce::dontSendNotification);
        if (!harshSlider->getSlider().isMouseOverOrDragging()) 
            harshSlider->setValue(p.harshCut, juce::dontSendNotification);
        if (!airSlider->getSlider().isMouseOverOrDragging()) 
            airSlider->setValue(p.air, juce::dontSendNotification);
        
        // Update mode buttons from processor
        int currentMode = static_cast<int>(p.mode);
        for (int i = 0; i < 3; ++i)
        {
            if (modeButtons[i]->getToggleState() != (i == currentMode))
                modeButtons[i]->setToggleState(i == currentMode, juce::dontSendNotification);
        }
        
        bool shouldBeOn = !sculptProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn) 
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor() {
        SculptProcessor::Params p = sculptProcessor.getParams(); // Preserve current mode
        p.drive = (float)driveSlider->getValue();
        p.mudCut = (float)mudSlider->getValue();
        p.harshCut = (float)harshSlider->getValue();
        p.air = (float)airSlider->getValue();
        sculptProcessor.setParams(p);
    }

    SculptProcessor& sculptProcessor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::Label descLabel;
    juce::Label modeLabel;
    std::unique_ptr<SculptModeButton> modeButtons[3];
    std::unique_ptr<VerticalSlider> driveSlider, mudSlider, harshSlider, airSlider;
    std::unique_ptr<SculptGraphComponent> graphComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SculptPanel)
};