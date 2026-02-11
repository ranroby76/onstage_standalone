// ==============================================================================
//  CompressorPanel.h
//  OnStage - Compressor UI with 5 type selectors
//
//  Features:
//  - 5 compressor type buttons (Opto, FET, VCA, Vintage, Peak)
//  - Threshold, Ratio, Attack, Release, Makeup controls
//  - Compression curve graph with animated indicator
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/CompressorProcessor.h"

class PresetManager;

// ==============================================================================
//  Compressor Type Selector Button
//  Off: dark gray background, white text, black frame
//  On: golden background, black text, black frame
// ==============================================================================
class CompressorTypeButton : public juce::Button
{
public:
    CompressorTypeButton(const juce::String& text)
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorTypeButton)
};

// ==============================================================================
//  Compressor Graph Component
// ==============================================================================
class CompressorGraphComponent : public juce::Component, private juce::Timer
{
public:
    CompressorGraphComponent(CompressorProcessor& proc) : compressor(proc) 
    { 
        startTimerHz(30); 
    }
    
    ~CompressorGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        auto params = compressor.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Grid lines
        g.setColour(juce::Colour(0xFF2A2A2A));
        for (int i = 1; i < 6; ++i) {
            float ratio = i / 6.0f;
            g.drawHorizontalLine((int)(bounds.getHeight() * ratio), bounds.getX(), bounds.getRight());
            g.drawVerticalLine((int)(bounds.getWidth() * ratio), bounds.getY(), bounds.getBottom());
        }
        
        // Axis labels
        g.setColour(juce::Colour(0xFF606060));
        g.setFont(10.0f);
        g.drawText("0dB", bounds.getRight() - 30, bounds.getY() + 2, 28, 12, juce::Justification::right);
        g.drawText("-60dB", bounds.getRight() - 35, bounds.getBottom() - 14, 33, 12, juce::Justification::right);
        g.drawText("In", bounds.getX() + 2, bounds.getBottom() - 14, 20, 12, juce::Justification::left);
        
        // Draw compression curve
        juce::Path curve;
        bool first = true;
        
        for (int x = 0; x < getWidth(); ++x) {
            float inputDb = juce::jmap((float)x, 0.0f, (float)getWidth(), -60.0f, 0.0f);
            float outputDb = calculateOutputLevel(inputDb, params.thresholdDb, params.ratio);
            
            outputDb += params.makeupDb;
            outputDb = juce::jlimit(-60.0f, 0.0f, outputDb);
            
            float y = juce::jmap(outputDb, 0.0f, -60.0f, 0.0f, (float)getHeight());
            
            if (first) {
                curve.startNewSubPath((float)x, y);
                first = false;
            } else {
                curve.lineTo((float)x, y);
            }
        }
        
        g.setColour(juce::Colour(0xFFD4AF37));
        g.strokePath(curve, juce::PathStrokeType(2.0f));
        
        // Draw threshold line
        float thresholdX = juce::jmap(params.thresholdDb, -60.0f, 0.0f, 0.0f, (float)getWidth());
        g.setColour(juce::Colour(0xFF8B7000));
        g.drawVerticalLine((int)thresholdX, 0.0f, (float)getHeight());
        
        // Draw 1:1 reference line
        g.setColour(juce::Colour(0xFF404040));
        g.drawLine(0, getHeight(), getWidth(), 0, 1.0f);
        
        // Draw moving circle showing current compression
        float currentInputDb = compressor.getCurrentInputLevelDb();
        currentInputDb = juce::jlimit(-60.0f, 0.0f, currentInputDb);
        
        float currentOutputDb = calculateOutputLevel(currentInputDb, params.thresholdDb, params.ratio);
        currentOutputDb += params.makeupDb;
        currentOutputDb = juce::jlimit(-60.0f, 0.0f, currentOutputDb);
        
        float circleX = juce::jmap(currentInputDb, -60.0f, 0.0f, 0.0f, (float)getWidth());
        float circleY = juce::jmap(currentOutputDb, 0.0f, -60.0f, 0.0f, (float)getHeight());
        
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.3f));
        g.fillEllipse(circleX - 12, circleY - 12, 24, 24);
        
        g.setColour(juce::Colour(0xFFD4AF37));
        g.fillEllipse(circleX - 6, circleY - 6, 12, 12);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override { repaint(); }
    
private:
    float calculateOutputLevel(float inputDb, float thresholdDb, float ratio) {
        if (inputDb <= thresholdDb)
            return inputDb;
        
        float overThreshold = inputDb - thresholdDb;
        float compressed = overThreshold / ratio;
        return thresholdDb + compressed;
    }
    
    CompressorProcessor& compressor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorGraphComponent)
};

// ==============================================================================
//  Main Compressor Panel
// ==============================================================================
class CompressorPanel : public juce::Component, private juce::Timer {
public:
    CompressorPanel(CompressorProcessor& proc, PresetManager& /*presets*/)
        : compressor(proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = compressor.getParams();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!compressor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            compressor.setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Compressor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Type label
        addAndMakeVisible(typeLabel);
        typeLabel.setText("TYPE", juce::dontSendNotification);
        typeLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        typeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        typeLabel.setJustificationType(juce::Justification::centredLeft);

        // Create type selector buttons
        const char* typeNames[] = { "OPTO", "FET", "VCA", "VINTAGE", "PEAK" };
        for (int i = 0; i < 5; ++i)
        {
            typeButtons[i] = std::make_unique<CompressorTypeButton>(typeNames[i]);
            typeButtons[i]->setToggleState(i == static_cast<int>(params.type), juce::dontSendNotification);
            typeButtons[i]->onClick = [this, i]() { selectType(i); };
            addAndMakeVisible(typeButtons[i].get());
        }

        // Sliders
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, 
                      double min, double max, double step, double val, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(n);
            s->setRange(min, max, step);
            s->setValue(val);
            s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateCompressor(); };
            addAndMakeVisible(s.get());
        };
        
        cS(thresholdSlider, "Threshold", -60.0, 0.0, 0.6, params.thresholdDb, " dB");
        cS(ratioSlider, "Ratio", 1.0, 20.0, 0.19, params.ratio, ":1");
        cS(attackSlider, "Attack", 0.1, 100.0, 0.999, params.attackMs, " ms");
        cS(releaseSlider, "Release", 10.0, 1000.0, 9.9, params.releaseMs, " ms");
        cS(makeupSlider, "Makeup", 0.0, 24.0, 0.24, params.makeupDb, " dB");
        
        // Graph
        graphComponent = std::make_unique<CompressorGraphComponent>(compressor);
        addAndMakeVisible(graphComponent.get());

        startTimerHz(15);
    }
    
    ~CompressorPanel() override {
        stopTimer();
        thresholdSlider->getSlider().setLookAndFeel(nullptr);
        ratioSlider->getSlider().setLookAndFeel(nullptr);
        attackSlider->getSlider().setLookAndFeel(nullptr);
        releaseSlider->getSlider().setLookAndFeel(nullptr);
        makeupSlider->getSlider().setLookAndFeel(nullptr);
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
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        
        area.removeFromTop(5);
        
        // Type selector row
        auto typeRow = area.removeFromTop(32);
        typeLabel.setBounds(typeRow.removeFromLeft(40));
        typeRow.removeFromLeft(5);
        
        int buttonWidth = 70;
        int buttonSpacing = 8;
        for (int i = 0; i < 5; ++i)
        {
            typeButtons[i]->setBounds(typeRow.removeFromLeft(buttonWidth));
            typeRow.removeFromLeft(buttonSpacing);
        }
        
        area.removeFromTop(15);

        // Split: controls on left, graph on right
        int sliderAreaWidth = 400;
        auto sliderArea = area.removeFromLeft(sliderAreaWidth);
        area.removeFromLeft(20);
        
        graphComponent->setBounds(area);
        
        // Sliders layout
        int numSliders = 5;
        int sliderWidth = 60;
        int spacing = 20;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = sliderArea.getX();
        auto sArea = sliderArea.withX(startX).withWidth(totalW);
        
        thresholdSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        attackSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        releaseSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        makeupSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromPreset() {
        auto p = compressor.getParams();
        thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        ratioSlider->setValue(p.ratio, juce::dontSendNotification);
        attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
        makeupSlider->setValue(p.makeupDb, juce::dontSendNotification);
        toggleButton->setToggleState(!compressor.isBypassed(), juce::dontSendNotification);
        
        // Update type buttons
        int currentType = static_cast<int>(p.type);
        for (int i = 0; i < 5; ++i)
            typeButtons[i]->setToggleState(i == currentType, juce::dontSendNotification);
        
        repaint();
    }
    
private:
    void selectType(int typeIndex)
    {
        // Update button states (radio behavior)
        for (int i = 0; i < 5; ++i)
            typeButtons[i]->setToggleState(i == typeIndex, juce::dontSendNotification);
        
        // Update processor
        auto p = compressor.getParams();
        p.type = static_cast<CompressorProcessor::Type>(typeIndex);
        compressor.setParams(p);
    }
    
    void timerCallback() override {
        auto p = compressor.getParams();
        
        if (!thresholdSlider->getSlider().isMouseOverOrDragging())
            thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        if (!ratioSlider->getSlider().isMouseOverOrDragging())
            ratioSlider->setValue(p.ratio, juce::dontSendNotification);
        if (!attackSlider->getSlider().isMouseOverOrDragging())
            attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        if (!releaseSlider->getSlider().isMouseOverOrDragging())
            releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
        if (!makeupSlider->getSlider().isMouseOverOrDragging())
            makeupSlider->setValue(p.makeupDb, juce::dontSendNotification);
        
        // Update type buttons from processor (in case changed externally)
        int currentType = static_cast<int>(p.type);
        for (int i = 0; i < 5; ++i)
        {
            if (typeButtons[i]->getToggleState() != (i == currentType))
                typeButtons[i]->setToggleState(i == currentType, juce::dontSendNotification);
        }
        
        bool shouldBeOn = !compressor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void updateCompressor() {
        CompressorProcessor::Params p = compressor.getParams(); // Preserve current type
        p.thresholdDb = (float)thresholdSlider->getValue();
        p.ratio = (float)ratioSlider->getValue();
        p.attackMs = (float)attackSlider->getValue();
        p.releaseMs = (float)releaseSlider->getValue();
        p.makeupDb = (float)makeupSlider->getValue();
        compressor.setParams(p);
    }
    
    CompressorProcessor& compressor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::Label typeLabel;
    std::unique_ptr<CompressorTypeButton> typeButtons[5];
    std::unique_ptr<VerticalSlider> thresholdSlider, ratioSlider, attackSlider, releaseSlider, makeupSlider;
    std::unique_ptr<CompressorGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPanel)
};