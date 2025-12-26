#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "DualHandleSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/EQProcessor.h"

// EQ Frequency Response Graph Component
class EQGraphComponent : public juce::Component, private juce::Timer {
public:
    EQGraphComponent(EQProcessor& processor) : eqProcessor(processor) { startTimerHz(30); }
    ~EQGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
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
        float lowFreq = eqProcessor.getLowFrequency();
        float highFreq = eqProcessor.getHighFrequency();
        float midFreq = std::sqrt(lowFreq * highFreq);
        
        float lowGain = eqProcessor.getLowGain();
        float midGain = eqProcessor.getMidGain();
        float highGain = eqProcessor.getHighGain();
        
        float lowQ = eqProcessor.getLowQ();
        float midQ = eqProcessor.getMidQ();
        float highQ = eqProcessor.getHighQ();
        
        bool first = true;
        for (int x = 0; x < getWidth(); ++x) {
            float freq = 20.0f * std::pow(1000.0f, x / (float)getWidth());
            
            // Calculate combined gain at this frequency
            float totalGain = 0.0f;
            
            // Low band (LOW SHELF)
            totalGain += calculateLowShelfGain(freq, lowFreq, lowGain);
            
            // Mid band (BELL filter)
            totalGain += calculateBellGain(freq, midFreq, midGain, midQ);
            
            // High band (HIGH SHELF)
            totalGain += calculateHighShelfGain(freq, highFreq, highGain);
            
            // Map gain to Y position (-12dB to +12dB)
            float y = juce::jmap(totalGain, 12.0f, -12.0f, 0.0f, (float)getHeight());
            
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
    
    float calculateLowShelfGain(float freq, float cornerFreq, float gainDb) {
        if (std::abs(gainDb) < 0.01f) return 0.0f;
        
        // Low shelf: boost/cut below corner frequency
        float ratio = freq / cornerFreq;
        float shelf = 1.0f / (1.0f + std::pow(ratio, 4.0f)); // 4th order shelf
        return gainDb * shelf;
    }
    
    float calculateHighShelfGain(float freq, float cornerFreq, float gainDb) {
        if (std::abs(gainDb) < 0.01f) return 0.0f;
        
        // High shelf: boost/cut above corner frequency
        float ratio = freq / cornerFreq;
        float shelf = 1.0f / (1.0f + std::pow(ratio, -4.0f)); // 4th order shelf
        return gainDb * shelf;
    }
    
    EQProcessor& eqProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQGraphComponent)
};

class EQPanel : public juce::Component, private juce::Timer {
public:
    EQPanel(EQProcessor& processor, int micIdx, const juce::String& micName);
    ~EQPanel() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateFromPreset();
    
private:
    void timerCallback() override;
    EQProcessor& eqProcessor;
    int micIndex;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    std::unique_ptr<VerticalSlider> lowGainSlider, midGainSlider, highGainSlider, lowQSlider, midQSlider, highQSlider;
    std::unique_ptr<DualHandleSlider> frequencySelector;
    std::unique_ptr<EQGraphComponent> graphComponent;  // NEW: Graph visualization
    juce::Label titleLabel, lowLabel, midLabel, highLabel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPanel)
};

inline EQPanel::EQPanel(EQProcessor& processor, int micIdx, const juce::String& micName) 
    : eqProcessor(processor), micIndex(micIdx) 
{
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    toggleButton = std::make_unique<EffectToggleButton>();
    int note = (micIndex == 0) ? 18 : 21;
    toggleButton->setMidiInfo("MIDI: Note " + juce::String(note));
    addAndMakeVisible(toggleButton.get());
    toggleButton->setToggleState(!eqProcessor.isBypassed(), juce::dontSendNotification);
    toggleButton->onClick = [this]() { eqProcessor.setBypassed(!toggleButton->getToggleState()); };
    
    int baseCC = (micIndex == 0) ? 70 : 80;
    auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double min, double max, double v, const juce::String& suf) {
        s = std::make_unique<VerticalSlider>();
        s->setLabelText(n);
        s->setMidiInfo("MIDI: CC " + juce::String(cc));
        s->setRange(min, max, (max-min)/200.0);
        s->setValue(v);
        s->setTextValueSuffix(suf);
        s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
        addAndMakeVisible(s.get());
    };
    
    cS(lowGainSlider, "Low Gain", baseCC, -12.0, 12.0, eqProcessor.getLowGain(), " dB");
    lowGainSlider->getSlider().onValueChange = [this]() { eqProcessor.setLowGain(lowGainSlider->getValue()); };
    
    cS(lowQSlider, "Low Q", baseCC+1, 0.1, 10.0, eqProcessor.getLowQ(), "");
    lowQSlider->getSlider().onValueChange = [this]() { eqProcessor.setLowQ(lowQSlider->getValue()); };
    
    cS(midGainSlider, "Mid Gain", baseCC+2, -12.0, 12.0, eqProcessor.getMidGain(), " dB");
    midGainSlider->getSlider().onValueChange = [this]() { eqProcessor.setMidGain(midGainSlider->getValue()); };
    
    cS(midQSlider, "Mid Q", baseCC+3, 0.1, 10.0, eqProcessor.getMidQ(), "");
    midQSlider->getSlider().onValueChange = [this]() { eqProcessor.setMidQ(midQSlider->getValue()); };
    
    cS(highGainSlider, "High Gain", baseCC+4, -12.0, 12.0, eqProcessor.getHighGain(), " dB");
    highGainSlider->getSlider().onValueChange = [this]() { eqProcessor.setHighGain(highGainSlider->getValue()); };
    
    cS(highQSlider, "High Q", baseCC+5, 0.1, 10.0, eqProcessor.getHighQ(), "");
    highQSlider->getSlider().onValueChange = [this]() { eqProcessor.setHighQ(highQSlider->getValue()); };
    
    frequencySelector = std::make_unique<DualHandleSlider>();
    frequencySelector->setRange(20.0, 20000.0);
    frequencySelector->setLeftValue(eqProcessor.getLowFrequency());
    frequencySelector->setRightValue(eqProcessor.getHighFrequency());
    addAndMakeVisible(frequencySelector.get());
    frequencySelector->onLeftValueChange = [this]() { eqProcessor.setLowFrequency(frequencySelector->getLeftValue()); };
    frequencySelector->onRightValueChange = [this]() { eqProcessor.setHighFrequency(frequencySelector->getRightValue()); };
    frequencySelector->setLeftMidiInfo("MIDI: CC " + juce::String((micIndex == 0) ? 68 : 78));
    frequencySelector->setRightMidiInfo("MIDI: CC " + juce::String((micIndex == 0) ? 69 : 79));
    
    titleLabel.setText(micName + " - 3-Band EQ", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);
    
    auto setupL = [&](juce::Label& l, const juce::String& t) {
        l.setText(t, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };
    setupL(lowLabel, "Low");
    setupL(midLabel, "Mid");
    setupL(highLabel, "High");
    
    // NEW: Add graph component
    graphComponent = std::make_unique<EQGraphComponent>(eqProcessor);
    addAndMakeVisible(graphComponent.get());
    
    startTimerHz(15);
}

inline EQPanel::~EQPanel() {
    stopTimer();
    lowGainSlider->getSlider().setLookAndFeel(nullptr);
    lowQSlider->getSlider().setLookAndFeel(nullptr);
    midGainSlider->getSlider().setLookAndFeel(nullptr);
    midQSlider->getSlider().setLookAndFeel(nullptr);
    highGainSlider->getSlider().setLookAndFeel(nullptr);
    highQSlider->getSlider().setLookAndFeel(nullptr);
}

inline void EQPanel::timerCallback() {
    if (!lowGainSlider->getSlider().isMouseOverOrDragging())
        lowGainSlider->setValue(eqProcessor.getLowGain(), juce::dontSendNotification);
    if (!lowQSlider->getSlider().isMouseOverOrDragging())
        lowQSlider->setValue(eqProcessor.getLowQ(), juce::dontSendNotification);
    if (!midGainSlider->getSlider().isMouseOverOrDragging())
        midGainSlider->setValue(eqProcessor.getMidGain(), juce::dontSendNotification);
    if (!midQSlider->getSlider().isMouseOverOrDragging())
        midQSlider->setValue(eqProcessor.getMidQ(), juce::dontSendNotification);
    if (!highGainSlider->getSlider().isMouseOverOrDragging())
        highGainSlider->setValue(eqProcessor.getHighGain(), juce::dontSendNotification);
    if (!highQSlider->getSlider().isMouseOverOrDragging())
        highQSlider->setValue(eqProcessor.getHighQ(), juce::dontSendNotification);
    if (!frequencySelector->isUserDragging()) {
        frequencySelector->setLeftValue(eqProcessor.getLowFrequency());
        frequencySelector->setRightValue(eqProcessor.getHighFrequency());
    }
    bool shouldBeOn = !eqProcessor.isBypassed();
    if (toggleButton->getToggleState() != shouldBeOn)
        toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
}

inline void EQPanel::updateFromPreset() {
    lowGainSlider->setValue(eqProcessor.getLowGain(), juce::dontSendNotification);
    lowQSlider->setValue(eqProcessor.getLowQ(), juce::dontSendNotification);
    midGainSlider->setValue(eqProcessor.getMidGain(), juce::dontSendNotification);
    midQSlider->setValue(eqProcessor.getMidQ(), juce::dontSendNotification);
    highGainSlider->setValue(eqProcessor.getHighGain(), juce::dontSendNotification);
    highQSlider->setValue(eqProcessor.getHighQ(), juce::dontSendNotification);
    frequencySelector->setLeftValue(eqProcessor.getLowFrequency());
    frequencySelector->setRightValue(eqProcessor.getHighFrequency());
    toggleButton->setToggleState(!eqProcessor.isBypassed(), juce::dontSendNotification);
}

inline void EQPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1A1A1A));
    g.setColour(juce::Colour(0xFF404040));
    g.drawRect(getLocalBounds(), 2);
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.fillRect(getLocalBounds().reduced(10));
}

inline void EQPanel::resized() {
    auto area = getLocalBounds().reduced(15);
    auto topArea = area.removeFromTop(40);
    toggleButton->setBounds(topArea.removeFromRight(40).withSizeKeepingCentre(40, 40));
    titleLabel.setBounds(topArea);
    
    area.removeFromTop(10);
    auto freqArea = area.removeFromTop(60);
    int fW = juce::jmin(600, freqArea.getWidth());
    frequencySelector->setBounds(freqArea.withWidth(fW).withX(freqArea.getX() + (freqArea.getWidth() - fW)/2));
    
    area.removeFromTop(20);
    
    // NEW LAYOUT: Sliders on left, graph on right
    int sliderAreaWidth = 450;  // Fixed width for sliders
    auto sliderArea = area.removeFromLeft(sliderAreaWidth);
    area.removeFromLeft(20);  // Gap
    
    // Graph takes remaining space
    graphComponent->setBounds(area);
    
    // Layout sliders in their area
    int groupW = 140;
    int spacing = 15;
    int totalW = (groupW * 3) + (spacing * 2);
    int startX = sliderArea.getX() + (sliderArea.getWidth() - totalW) / 2;
    auto bandArea = sliderArea.withX(startX).withWidth(totalW);
    
    auto layoutGroup = [&](juce::Label& lbl, VerticalSlider& s1, VerticalSlider& s2) {
        auto gArea = bandArea.removeFromLeft(groupW);
        bandArea.removeFromLeft(spacing);
        lbl.setBounds(gArea.removeFromTop(20));
        s1.setBounds(gArea.removeFromLeft(groupW/2).reduced(2));
        s2.setBounds(gArea.reduced(2));
    };
    
    layoutGroup(lowLabel, *lowGainSlider, *lowQSlider);
    layoutGroup(midLabel, *midGainSlider, *midQSlider);
    layoutGroup(highLabel, *highGainSlider, *highQSlider);
}