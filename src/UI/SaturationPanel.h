// ==============================================================================
//  SaturationPanel.h
//  OnStage - Multimode Saturation UI
//
//  Three tonal modes with standard table-like mode selector buttons:
//  - Tape: Vintage tape machine warmth
//  - Tube: Culture Vulture style harmonics
//  - Digital: Bitcrushing and sample rate reduction
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/SaturationProcessor.h"

class PresetManager;

// ==============================================================================
// Saturation Mode Button (standard selector style - matches Compressor/Delay)
// ==============================================================================
class SaturationModeButton : public juce::Component
{
public:
    SaturationModeButton(const juce::String& label) : buttonLabel(label)
    {
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        // Background: golden when selected, dark gray when off
        juce::Colour bgColor;
        if (isSelected)
            bgColor = juce::Colour(0xFFD4AF37);  // Golden when selected
        else if (isMouseOver())
            bgColor = juce::Colour(0xFF3A3A3A);  // Lighter gray on hover
        else
            bgColor = juce::Colour(0xFF2A2A2A);  // Dark gray
        
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Black border
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        // Text: black when selected, white when off
        g.setColour(isSelected ? juce::Colours::black : juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(buttonLabel, bounds, juce::Justification::centred);
    }
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && onClick)
            onClick();
    }
    
    void setSelected(bool shouldBeSelected)
    {
        if (isSelected != shouldBeSelected)
        {
            isSelected = shouldBeSelected;
            repaint();
        }
    }
    
    bool getSelected() const { return isSelected; }
    
    std::function<void()> onClick;
    
private:
    juce::String buttonLabel;
    bool isSelected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturationModeButton)
};

// ==============================================================================
// Saturation Graph Component - Visualizes the transfer function
// ==============================================================================
class SaturationGraphComponent : public juce::Component, private juce::Timer
{
public:
    SaturationGraphComponent(SaturationProcessor& proc) : satProcessor(proc)
    {
        startTimerHz(30);
    }
    
    ~SaturationGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto params = satProcessor.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Grid
        g.setColour(juce::Colour(0xFF2A2A2A));
        float centerX = bounds.getCentreX();
        float centerY = bounds.getCentreY();
        g.drawLine(bounds.getX(), centerY, bounds.getRight(), centerY, 1.0f);
        g.drawLine(centerX, bounds.getY(), centerX, bounds.getBottom(), 1.0f);
        
        // Draw linear reference line (diagonal)
        g.setColour(juce::Colour(0xFF404040));
        g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 1.0f);
        
        // Draw transfer function curve
        juce::Path curve;
        bool first = true;
        
        float driveGain = 1.0f + params.drive * (params.mode == SaturationProcessor::Mode::Tube ? 15.0f : 8.0f);
        
        for (float px = 0; px < bounds.getWidth(); px += 2.0f)
        {
            // Map pixel to input range (-1 to +1)
            float input = (px / bounds.getWidth()) * 2.0f - 1.0f;
            float driven = input * driveGain;
            
            // Apply saturation based on current mode
            float output = 0.0f;
            switch (params.mode)
            {
                case SaturationProcessor::Mode::Tape:
                    output = simulateTape(driven, params.tapeBias, params.tapeCompression);
                    break;
                case SaturationProcessor::Mode::Tube:
                    output = simulateTube(driven, params.tubeBias, params.tubeOddEven);
                    break;
                case SaturationProcessor::Mode::Digital:
                    output = simulateDigital(driven, params.bitDepth);
                    break;
            }
            
            // Clamp output for display
            output = juce::jlimit(-1.0f, 1.0f, output);
            
            // Map output to pixel Y
            float py = centerY - (output * centerY * 0.9f);
            
            if (first)
            {
                curve.startNewSubPath(px + bounds.getX(), py);
                first = false;
            }
            else
            {
                curve.lineTo(px + bounds.getX(), py);
            }
        }
        
        // Draw the curve with glow effect
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.3f));
        g.strokePath(curve, juce::PathStrokeType(4.0f));
        g.setColour(juce::Colour(0xFFD4AF37));
        g.strokePath(curve, juce::PathStrokeType(2.0f));
        
        // Mode indicator
        g.setColour(juce::Colour(0xFF888888));
        g.setFont(11.0f);
        juce::String modeText;
        switch (params.mode)
        {
            case SaturationProcessor::Mode::Tape:    modeText = "TAPE"; break;
            case SaturationProcessor::Mode::Tube:    modeText = "TUBE"; break;
            case SaturationProcessor::Mode::Digital: modeText = "DIGITAL"; break;
        }
        g.drawText(modeText, bounds.reduced(8).removeFromTop(16), juce::Justification::topRight);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    // Simplified saturation curves for visualization
    float simulateTape(float x, float bias, float compression)
    {
        float asymmetry = 0.1f + bias * 0.2f;
        if (x > 0.0f)
            x = std::tanh(x * (1.0f + asymmetry));
        else
            x = std::tanh(x * (1.0f - asymmetry));
        
        if (compression > 0.01f)
        {
            float absX = std::abs(x);
            float compGain = 1.0f / (1.0f + compression * absX * 2.0f);
            x *= compGain;
        }
        
        return x * 0.7f;
    }
    
    float simulateTube(float x, float bias, float oddEven)
    {
        float saturated;
        
        if (bias < 0.5f)
        {
            float triodeFactor = 1.0f - bias * 2.0f;
            float even = x + 0.25f * x * x - 0.1f * x * x * x;
            float odd = std::tanh(x * 1.5f);
            saturated = even * (1.0f - oddEven) * triodeFactor + odd * oddEven + std::tanh(x) * (1.0f - triodeFactor);
        }
        else
        {
            float pentodeFactor = (bias - 0.5f) * 2.0f;
            float hard = juce::jlimit(-1.0f, 1.0f, x * 1.2f);
            float soft = std::tanh(x * 2.0f) * 0.8f;
            saturated = soft * (1.0f - pentodeFactor * 0.5f) + hard * pentodeFactor * 0.5f;
        }
        
        return std::tanh(saturated * 0.9f) * 0.75f;
    }
    
    float simulateDigital(float x, float bitDepth)
    {
        x = juce::jlimit(-1.0f, 1.0f, x);
        int bits = juce::jlimit(2, 16, (int)bitDepth);
        float quantLevels = std::pow(2.0f, (float)bits);
        float quantStep = 2.0f / quantLevels;
        return std::floor(x / quantStep + 0.5f) * quantStep;
    }
    
    SaturationProcessor& satProcessor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturationGraphComponent)
};

// ==============================================================================
// Main Saturation Panel
// ==============================================================================
class SaturationPanel : public juce::Component, private juce::Timer
{
public:
    SaturationPanel(SaturationProcessor& proc, PresetManager& /*presets*/) 
        : satProcessor(proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = satProcessor.getParams();
        
        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!satProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() {
            satProcessor.setBypassed(!toggleButton->getToggleState());
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Saturation", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        
        // Mode selector buttons (table-like row)
        auto createModeButton = [this](std::unique_ptr<SaturationModeButton>& btn,
                                        const juce::String& name, SaturationProcessor::Mode mode)
        {
            btn = std::make_unique<SaturationModeButton>(name);
            btn->onClick = [this, mode]() { selectMode(mode); };
            addAndMakeVisible(btn.get());
        };
        
        createModeButton(tapeButton, "TAPE", SaturationProcessor::Mode::Tape);
        createModeButton(tubeButton, "TUBE", SaturationProcessor::Mode::Tube);
        createModeButton(digitalButton, "DIGITAL", SaturationProcessor::Mode::Digital);
        
        updateModeButtons();
        
        // Common sliders helper
        auto createSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                                const juce::String& midi, double min, double max, double value,
                                const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setMidiInfo(midi);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };
        
        // Common controls (always visible)
        createSlider(driveSlider, "Drive", "CC 60", 0.0, 1.0, params.drive, "");
        createSlider(toneSlider, "Tone", "CC 61", 0.0, 1.0, params.tone, "");
        createSlider(mixSlider, "Mix", "CC 62", 0.0, 1.0, params.mix, "");
        createSlider(outputSlider, "Output", "CC 63", -12.0, 12.0, params.outputDb, " dB");
        
        // Tape-specific controls
        createSlider(tapeCompSlider, "Compress", "CC 64", 0.0, 1.0, params.tapeCompression, "");
        createSlider(tapeBiasSlider, "Bias", "CC 65", 0.0, 1.0, params.tapeBias, "");
        
        // Tube-specific controls
        createSlider(tubeOddEvenSlider, "Odd/Even", "CC 66", 0.0, 1.0, params.tubeOddEven, "");
        createSlider(tubeBiasSlider, "Tri/Pent", "CC 67", 0.0, 1.0, params.tubeBias, "");
        
        // Digital-specific controls
        createSlider(bitDepthSlider, "Bits", "CC 68", 2.0, 16.0, params.bitDepth, "");
        createSlider(sampleRateDivSlider, "Downsamp", "CC 69", 1.0, 64.0, params.sampleRateDiv, "x");
        
        // Set logarithmic skew for sample rate reduction
        sampleRateDivSlider->getSlider().setSkewFactor(0.5);
        
        // Initially show/hide mode-specific sliders
        updateSliderVisibility();
        
        // Graph component
        graphComponent = std::make_unique<SaturationGraphComponent>(satProcessor);
        addAndMakeVisible(graphComponent.get());
        
        startTimerHz(15);
    }
    
    ~SaturationPanel() override
    {
        stopTimer();
        driveSlider->getSlider().setLookAndFeel(nullptr);
        toneSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
        outputSlider->getSlider().setLookAndFeel(nullptr);
        tapeCompSlider->getSlider().setLookAndFeel(nullptr);
        tapeBiasSlider->getSlider().setLookAndFeel(nullptr);
        tubeOddEvenSlider->getSlider().setLookAndFeel(nullptr);
        tubeBiasSlider->getSlider().setLookAndFeel(nullptr);
        bitDepthSlider->getSlider().setLookAndFeel(nullptr);
        sampleRateDivSlider->getSlider().setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
        
        // "MODE" label above selector buttons
        g.setColour(juce::Colour(0xFF888888));
        g.setFont(11.0f);
        auto area = getLocalBounds().reduced(15);
        area.removeFromTop(40);
        g.drawText("MODE", 15, area.getY() + 2, 40, 16, juce::Justification::centredLeft);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Title row
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        
        // Mode selector row (table-like)
        auto modeRow = area.removeFromTop(32);
        modeRow.removeFromLeft(50);  // Space for "MODE" label
        
        int buttonWidth = 80;
        int buttonSpacing = 10;
        tapeButton->setBounds(modeRow.removeFromLeft(buttonWidth));
        modeRow.removeFromLeft(buttonSpacing);
        tubeButton->setBounds(modeRow.removeFromLeft(buttonWidth));
        modeRow.removeFromLeft(buttonSpacing);
        digitalButton->setBounds(modeRow.removeFromLeft(buttonWidth));
        
        area.removeFromTop(15);
        
        // Controls area
        int controlAreaWidth = 500;
        auto controlArea = area.removeFromLeft(controlAreaWidth);
        area.removeFromLeft(20);
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout sliders
        int sliderWidth = 65;
        int spacing = 12;
        
        auto params = satProcessor.getParams();
        
        // Common sliders (always visible)
        driveSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        toneSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        mixSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        outputSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing + 20);  // Extra gap before mode-specific
        
        // Mode-specific sliders
        switch (params.mode)
        {
            case SaturationProcessor::Mode::Tape:
                tapeCompSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
                controlArea.removeFromLeft(spacing);
                tapeBiasSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
                break;
                
            case SaturationProcessor::Mode::Tube:
                tubeOddEvenSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
                controlArea.removeFromLeft(spacing);
                tubeBiasSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
                break;
                
            case SaturationProcessor::Mode::Digital:
                bitDepthSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
                controlArea.removeFromLeft(spacing);
                sampleRateDivSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
                break;
        }
    }
    
    void updateFromPreset()
    {
        auto p = satProcessor.getParams();
        toggleButton->setToggleState(!satProcessor.isBypassed(), juce::dontSendNotification);
        driveSlider->setValue(p.drive, juce::dontSendNotification);
        toneSlider->setValue(p.tone, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
        outputSlider->setValue(p.outputDb, juce::dontSendNotification);
        tapeCompSlider->setValue(p.tapeCompression, juce::dontSendNotification);
        tapeBiasSlider->setValue(p.tapeBias, juce::dontSendNotification);
        tubeOddEvenSlider->setValue(p.tubeOddEven, juce::dontSendNotification);
        tubeBiasSlider->setValue(p.tubeBias, juce::dontSendNotification);
        bitDepthSlider->setValue(p.bitDepth, juce::dontSendNotification);
        sampleRateDivSlider->setValue(p.sampleRateDiv, juce::dontSendNotification);
        updateModeButtons();
        updateSliderVisibility();
    }

private:
    void timerCallback() override
    {
        auto p = satProcessor.getParams();
        
        if (!driveSlider->getSlider().isMouseOverOrDragging())
            driveSlider->setValue(p.drive, juce::dontSendNotification);
        if (!toneSlider->getSlider().isMouseOverOrDragging())
            toneSlider->setValue(p.tone, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(p.mix, juce::dontSendNotification);
        if (!outputSlider->getSlider().isMouseOverOrDragging())
            outputSlider->setValue(p.outputDb, juce::dontSendNotification);
        if (!tapeCompSlider->getSlider().isMouseOverOrDragging())
            tapeCompSlider->setValue(p.tapeCompression, juce::dontSendNotification);
        if (!tapeBiasSlider->getSlider().isMouseOverOrDragging())
            tapeBiasSlider->setValue(p.tapeBias, juce::dontSendNotification);
        if (!tubeOddEvenSlider->getSlider().isMouseOverOrDragging())
            tubeOddEvenSlider->setValue(p.tubeOddEven, juce::dontSendNotification);
        if (!tubeBiasSlider->getSlider().isMouseOverOrDragging())
            tubeBiasSlider->setValue(p.tubeBias, juce::dontSendNotification);
        if (!bitDepthSlider->getSlider().isMouseOverOrDragging())
            bitDepthSlider->setValue(p.bitDepth, juce::dontSendNotification);
        if (!sampleRateDivSlider->getSlider().isMouseOverOrDragging())
            sampleRateDivSlider->setValue(p.sampleRateDiv, juce::dontSendNotification);
        
        bool shouldBeOn = !satProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void selectMode(SaturationProcessor::Mode mode)
    {
        auto p = satProcessor.getParams();
        if (p.mode != mode)
        {
            p.mode = mode;
            satProcessor.setParams(p);
            updateModeButtons();
            updateSliderVisibility();
            resized();
            repaint();
        }
    }
    
    void updateModeButtons()
    {
        auto mode = satProcessor.getParams().mode;
        tapeButton->setSelected(mode == SaturationProcessor::Mode::Tape);
        tubeButton->setSelected(mode == SaturationProcessor::Mode::Tube);
        digitalButton->setSelected(mode == SaturationProcessor::Mode::Digital);
    }
    
    void updateSliderVisibility()
    {
        auto mode = satProcessor.getParams().mode;
        
        // Hide all mode-specific sliders
        tapeCompSlider->setVisible(false);
        tapeBiasSlider->setVisible(false);
        tubeOddEvenSlider->setVisible(false);
        tubeBiasSlider->setVisible(false);
        bitDepthSlider->setVisible(false);
        sampleRateDivSlider->setVisible(false);
        
        // Show sliders for current mode
        switch (mode)
        {
            case SaturationProcessor::Mode::Tape:
                tapeCompSlider->setVisible(true);
                tapeBiasSlider->setVisible(true);
                break;
                
            case SaturationProcessor::Mode::Tube:
                tubeOddEvenSlider->setVisible(true);
                tubeBiasSlider->setVisible(true);
                break;
                
            case SaturationProcessor::Mode::Digital:
                bitDepthSlider->setVisible(true);
                sampleRateDivSlider->setVisible(true);
                break;
        }
    }
    
    void updateProcessor()
    {
        SaturationProcessor::Params p = satProcessor.getParams();
        p.drive = (float)driveSlider->getValue();
        p.tone = (float)toneSlider->getValue();
        p.mix = (float)mixSlider->getValue();
        p.outputDb = (float)outputSlider->getValue();
        p.tapeCompression = (float)tapeCompSlider->getValue();
        p.tapeBias = (float)tapeBiasSlider->getValue();
        p.tubeOddEven = (float)tubeOddEvenSlider->getValue();
        p.tubeBias = (float)tubeBiasSlider->getValue();
        p.bitDepth = (float)bitDepthSlider->getValue();
        p.sampleRateDiv = (float)sampleRateDivSlider->getValue();
        satProcessor.setParams(p);
    }
    
    SaturationProcessor& satProcessor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    
    // Mode selector buttons (table-like row)
    std::unique_ptr<SaturationModeButton> tapeButton;
    std::unique_ptr<SaturationModeButton> tubeButton;
    std::unique_ptr<SaturationModeButton> digitalButton;
    
    // Common sliders (always visible)
    std::unique_ptr<VerticalSlider> driveSlider;
    std::unique_ptr<VerticalSlider> toneSlider;
    std::unique_ptr<VerticalSlider> mixSlider;
    std::unique_ptr<VerticalSlider> outputSlider;
    
    // Tape-specific sliders
    std::unique_ptr<VerticalSlider> tapeCompSlider;
    std::unique_ptr<VerticalSlider> tapeBiasSlider;
    
    // Tube-specific sliders
    std::unique_ptr<VerticalSlider> tubeOddEvenSlider;
    std::unique_ptr<VerticalSlider> tubeBiasSlider;
    
    // Digital-specific sliders
    std::unique_ptr<VerticalSlider> bitDepthSlider;
    std::unique_ptr<VerticalSlider> sampleRateDivSlider;
    
    std::unique_ptr<SaturationGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturationPanel)
};
