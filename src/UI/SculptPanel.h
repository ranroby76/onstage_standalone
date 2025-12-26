#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

// Sculpt Frequency Response Graph
class SculptGraphComponent : public juce::Component, private juce::Timer {
public:
    SculptGraphComponent(AudioEngine& engine, int micIdx) 
        : audioEngine(engine), micIndex(micIdx) 
    {
        startTimerHz(30);
    }
    
    ~SculptGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        auto& sculpt = audioEngine.getSculptProcessor(micIndex);
        auto params = sculpt.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Grid lines
        g.setColour(juce::Colour(0xFF2A2A2A));
        for (int i = 1; i < 5; ++i) {
            float y = bounds.getHeight() * i / 5.0f;
            g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
        }
        
        // Draw frequency response curve
        juce::Path responseCurve;
        bool first = true;
        
        for (int x = 0; x < getWidth(); ++x) {
            // Logarithmic frequency mapping (20Hz to 20kHz)
            float freq = 20.0f * std::pow(1000.0f, x / (float)getWidth());
            
            float totalGain = 0.0f;
            
            // Mud cut at 300Hz (bell dip)
            float mudDb = -12.0f * params.mudCut;
            totalGain += calculateBellGain(freq, 300.0f, mudDb, 2.0f);
            
            // Harsh cut at 3.5kHz (bell dip)
            float harshDb = -12.0f * params.harshCut;
            totalGain += calculateBellGain(freq, 3500.0f, harshDb, 3.0f);
            
            // Air boost at 12kHz (high shelf)
            float airDb = 10.0f * params.air;
            totalGain += calculateHighShelfGain(freq, 12000.0f, airDb);
            
            // Map gain to Y position (-15dB to +15dB range)
            float y = juce::jmap(totalGain, 15.0f, -15.0f, 0.0f, (float)getHeight());
            
            if (first) {
                responseCurve.startNewSubPath((float)x, y);
                first = false;
            } else {
                responseCurve.lineTo((float)x, y);
            }
        }
        
        // Draw the curve
        g.setColour(juce::Colour(0xFFD4AF37));
        g.strokePath(responseCurve, juce::PathStrokeType(2.0f));
        
        // 0dB reference line
        float zeroY = getHeight() / 2.0f;
        g.setColour(juce::Colour(0xFF404040));
        g.drawHorizontalLine((int)zeroY, 0.0f, (float)getWidth());
        
        // Frequency markers
        g.setColour(juce::Colour(0xFF606060));
        g.setFont(9.0f);
        g.drawText("300Hz", juce::jmap(std::log10(300.0f / 20.0f) / std::log10(1000.0f), 0.0f, 1.0f, 0.0f, (float)getWidth()) - 15, 
                   bounds.getBottom() - 12, 30, 10, juce::Justification::centred);
        g.drawText("3.5kHz", juce::jmap(std::log10(3500.0f / 20.0f) / std::log10(1000.0f), 0.0f, 1.0f, 0.0f, (float)getWidth()) - 15, 
                   bounds.getBottom() - 12, 30, 10, juce::Justification::centred);
        g.drawText("12kHz", juce::jmap(std::log10(12000.0f / 20.0f) / std::log10(1000.0f), 0.0f, 1.0f, 0.0f, (float)getWidth()) - 15, 
                   bounds.getBottom() - 12, 30, 10, juce::Justification::centred);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override { repaint(); }
    
private:
    float calculateBellGain(float freq, float centerFreq, float gainDb, float q) {
        if (std::abs(gainDb) < 0.01f) return 0.0f;
        
        float ratio = freq / centerFreq;
        float logRatio = std::log2(ratio);
        float bandwidth = 1.0f / q;
        
        // Gaussian-like bell curve
        float response = std::exp(-logRatio * logRatio / (bandwidth * bandwidth));
        return gainDb * response;
    }
    
    float calculateHighShelfGain(float freq, float cornerFreq, float gainDb) {
        if (std::abs(gainDb) < 0.01f) return 0.0f;
        
        // Smooth shelf transition
        float ratio = freq / cornerFreq;
        float shelf = 1.0f / (1.0f + std::pow(ratio, -4.0f)); // 4th order shelf
        return gainDb * shelf;
    }
    
    AudioEngine& audioEngine;
    int micIndex;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SculptGraphComponent)
};

class SculptPanel : public juce::Component, private juce::Timer {
public:
    SculptPanel(AudioEngine& engine, int micIndex, const juce::String& micName) 
        : audioEngine(engine), micIdx(micIndex) 
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& s = audioEngine.getSculptProcessor(micIdx); 
        auto params = s.getParams(); 

        toggleButton = std::make_unique<EffectToggleButton>();
        int note = (micIdx == 0) ? 25 : 30; 
        toggleButton->setMidiInfo("MIDI: Note " + juce::String(note)); 
        toggleButton->setToggleState(!s.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            audioEngine.getSculptProcessor(micIdx).setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        addAndMakeVisible(titleLabel);
        titleLabel.setText(micName + " - Sculpt", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(descLabel);
        descLabel.setText("Saturation & Tone Shaping", juce::dontSendNotification);
        descLabel.setFont(juce::Font(14.0f));
        descLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

        // Saturation Mode Selector
        addAndMakeVisible(modeSelector);
        modeSelector.addItem("Tube", 1);
        modeSelector.addItem("Tape", 2);
        modeSelector.addItem("Hybrid", 3);
        modeSelector.setSelectedId((int)params.mode + 1, juce::dontSendNotification);
        modeSelector.onChange = [this]() { updateProcessor(); };
        
        addAndMakeVisible(modeLabel);
        modeLabel.setText("Mode", juce::dontSendNotification);
        modeLabel.setFont(juce::Font(12.0f));
        modeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        modeLabel.setJustificationType(juce::Justification::centredLeft);

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

        int ccBase = (micIdx == 0) ? 90 : 95;
        cS(driveSlider, "Drive", ccBase, 1.0, params.drive, "%");
        cS(mudSlider, "Clean Mud", ccBase+1, 1.0, params.mudCut, "%");
        cS(harshSlider, "Tame Harsh", ccBase+2, 1.0, params.harshCut, "%");
        cS(airSlider, "Air", ccBase+3, 1.0, params.air, "%");
        
        // Add graph component
        graphComponent = std::make_unique<SculptGraphComponent>(audioEngine, micIdx);
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
        auto topRow = area.removeFromTop(30);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        
        descLabel.setBounds(area.removeFromTop(20));
        
        // Mode selector row
        auto modeRow = area.removeFromTop(30);
        modeLabel.setBounds(modeRow.removeFromLeft(50));
        modeSelector.setBounds(modeRow.removeFromLeft(120).reduced(0, 2));
        
        area.removeFromTop(10);
        
        // Left-aligned sliders, graph on right
        int sliderAreaWidth = 360;
        auto sliderArea = area.removeFromLeft(sliderAreaWidth);
        area.removeFromLeft(20); // Gap
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout sliders
        int sliderW = 80; 
        int gap = 20;
        int startX = sliderArea.getX();
        
        driveSlider->setBounds(startX, sliderArea.getY(), sliderW, sliderArea.getHeight());
        mudSlider->setBounds(startX + sliderW + gap, sliderArea.getY(), sliderW, sliderArea.getHeight());
        harshSlider->setBounds(startX + (sliderW + gap) * 2, sliderArea.getY(), sliderW, sliderArea.getHeight());
        airSlider->setBounds(startX + (sliderW + gap) * 3, sliderArea.getY(), sliderW, sliderArea.getHeight());
    }

    void updateFromPreset() {
        auto p = audioEngine.getSculptProcessor(micIdx).getParams();
        driveSlider->setValue(p.drive, juce::dontSendNotification);
        mudSlider->setValue(p.mudCut, juce::dontSendNotification);
        harshSlider->setValue(p.harshCut, juce::dontSendNotification);
        airSlider->setValue(p.air, juce::dontSendNotification);
        modeSelector.setSelectedId((int)p.mode + 1, juce::dontSendNotification);
        toggleButton->setToggleState(!audioEngine.getSculptProcessor(micIdx).isBypassed(), juce::dontSendNotification);
    }
    
private:
    void timerCallback() override {
        auto p = audioEngine.getSculptProcessor(micIdx).getParams();
        if (!driveSlider->getSlider().isMouseOverOrDragging()) 
            driveSlider->setValue(p.drive, juce::dontSendNotification);
        if (!mudSlider->getSlider().isMouseOverOrDragging()) 
            mudSlider->setValue(p.mudCut, juce::dontSendNotification);
        if (!harshSlider->getSlider().isMouseOverOrDragging()) 
            harshSlider->setValue(p.harshCut, juce::dontSendNotification);
        if (!airSlider->getSlider().isMouseOverOrDragging()) 
            airSlider->setValue(p.air, juce::dontSendNotification);
        
        if (!modeSelector.isMouseOver(true))
            modeSelector.setSelectedId((int)p.mode + 1, juce::dontSendNotification);
        
        bool shouldBeOn = !audioEngine.getSculptProcessor(micIdx).isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn) 
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor() {
        SculptProcessor::Params p;
        p.drive = (float)driveSlider->getValue();
        p.mudCut = (float)mudSlider->getValue();
        p.harshCut = (float)harshSlider->getValue();
        p.air = (float)airSlider->getValue();
        p.mode = (SculptProcessor::SaturationMode)(modeSelector.getSelectedId() - 1);
        audioEngine.getSculptProcessor(micIdx).setParams(p);
    }

    AudioEngine& audioEngine;
    int micIdx;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::Label descLabel;
    juce::Label modeLabel;
    juce::ComboBox modeSelector;
    std::unique_ptr<VerticalSlider> driveSlider, mudSlider, harshSlider, airSlider;
    std::unique_ptr<SculptGraphComponent> graphComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SculptPanel)
};