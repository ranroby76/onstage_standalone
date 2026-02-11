// ==============================================================================
//  DeEsserPanel.h
//  OnStage - De-Esser UI for reducing sibilance (s, z, sh sounds)
//
//  Features:
//  - Mode selector: Wideband / Split-Band
//  - Frequency spectrum visualization with sibilance band highlighted
//  - Real-time gain reduction meter
//  - Listen mode toggle to hear what's being reduced
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/DeEsserProcessor.h"

class PresetManager;

// ==============================================================================
// De-Esser Mode Button (standard selector style)
// ==============================================================================
class DeEsserModeButton : public juce::Component
{
public:
    DeEsserModeButton(const juce::String& label) : buttonLabel(label)
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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserModeButton)
};

// ==============================================================================
// Listen Mode Toggle Button
// ==============================================================================
class ListenModeButton : public juce::Component
{
public:
    ListenModeButton()
    {
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Background
        juce::Colour bgColor;
        if (isActive)
            bgColor = juce::Colour(0xFFFF6B6B);  // Red when listening
        else if (isMouseOver())
            bgColor = juce::Colour(0xFF4A4A4A);
        else
            bgColor = juce::Colour(0xFF3A3A3A);
        
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(isActive ? juce::Colour(0xFFCC5555) : juce::Colour(0xFF505050));
        g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
        
        // Headphone icon (simple representation)
        auto iconArea = bounds.reduced(8);
        g.setColour(isActive ? juce::Colours::white : juce::Colour(0xFFAAAAAA));
        
        // Draw headphone shape
        float cx = iconArea.getCentreX();
        float cy = iconArea.getCentreY();
        float size = juce::jmin(iconArea.getWidth(), iconArea.getHeight()) * 0.4f;
        
        // Arc for headband
        juce::Path headband;
        headband.addArc(cx - size, cy - size * 0.3f, size * 2, size * 1.5f, 
                        juce::MathConstants<float>::pi * 1.2f, 
                        juce::MathConstants<float>::pi * 1.8f, true);
        g.strokePath(headband, juce::PathStrokeType(2.0f));
        
        // Ear cups
        g.fillRoundedRectangle(cx - size - 3, cy + size * 0.3f, 6, 10, 2);
        g.fillRoundedRectangle(cx + size - 3, cy + size * 0.3f, 6, 10, 2);
        
        // Text
        g.setFont(9.0f);
        g.drawText("LISTEN", bounds.removeFromBottom(14), juce::Justification::centred);
    }
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked())
        {
            isActive = !isActive;
            if (onToggle)
                onToggle(isActive);
            repaint();
        }
    }
    
    void setActive(bool active)
    {
        if (isActive != active)
        {
            isActive = active;
            repaint();
        }
    }
    
    bool getActive() const { return isActive; }
    
    std::function<void(bool)> onToggle;
    
private:
    bool isActive = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ListenModeButton)
};

// ==============================================================================
// De-Esser Graph Component - Frequency response and gain reduction
// ==============================================================================
class DeEsserGraphComponent : public juce::Component, private juce::Timer
{
public:
    DeEsserGraphComponent(DeEsserProcessor& proc) : deEsser(proc)
    {
        startTimerHz(30);
    }
    
    ~DeEsserGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto params = deEsser.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Margins
        const float leftMargin = 35.0f;
        const float bottomMargin = 20.0f;
        const float topMargin = 10.0f;
        
        auto plotArea = bounds;
        plotArea.removeFromLeft(leftMargin);
        plotArea.removeFromBottom(bottomMargin);
        plotArea.removeFromTop(topMargin);
        
        // Draw frequency grid
        const float freqLabels[] = { 2000.0f, 4000.0f, 6000.0f, 8000.0f, 10000.0f, 14000.0f };
        const int numFreqLabels = 6;
        
        g.setFont(9.0f);
        for (int i = 0; i < numFreqLabels; ++i)
        {
            float freq = freqLabels[i];
            float x = freqToX(freq, plotArea);
            
            // Vertical grid line
            g.setColour(juce::Colour(0xFF1A1A1A));
            g.drawVerticalLine((int)x, plotArea.getY(), plotArea.getBottom());
            
            // Label
            g.setColour(juce::Colour(0xFF606060));
            juce::String label = (freq >= 10000.0f) 
                ? juce::String((int)(freq / 1000.0f)) + "k"
                : juce::String((int)(freq / 1000.0f)) + "k";
            g.drawText(label, (int)x - 15, (int)plotArea.getBottom() + 3, 30, 15, 
                       juce::Justification::centred);
        }
        
        // Draw dB grid (0, -6, -12, -18 dB)
        const float dbValues[] = { 0.0f, -6.0f, -12.0f, -18.0f };
        for (int i = 0; i < 4; ++i)
        {
            float db = dbValues[i];
            float y = dbToY(db, plotArea);
            
            g.setColour(juce::Colour(i == 0 ? 0xFF404040 : 0xFF1A1A1A));
            g.drawHorizontalLine((int)y, plotArea.getX(), plotArea.getRight());
            
            g.setColour(juce::Colour(0xFF606060));
            g.drawText(juce::String((int)db), 2, (int)y - 6, (int)leftMargin - 5, 12,
                       juce::Justification::right);
        }
        
        // Draw sibilance detection band (highlighted area)
        float bandCenterX = freqToX(params.frequency, plotArea);
        float bandWidth = plotArea.getWidth() * 0.15f / params.bandwidth * params.range;
        bandWidth = juce::jlimit(20.0f, plotArea.getWidth() * 0.4f, bandWidth);
        
        // Gradient fill for sibilance band
        juce::ColourGradient bandGradient(
            juce::Colour(0xFFD4AF37).withAlpha(0.0f), bandCenterX - bandWidth, plotArea.getCentreY(),
            juce::Colour(0xFFD4AF37).withAlpha(0.3f), bandCenterX, plotArea.getCentreY(),
            false);
        bandGradient.addColour(0.5, juce::Colour(0xFFD4AF37).withAlpha(0.3f));
        bandGradient.addColour(1.0, juce::Colour(0xFFD4AF37).withAlpha(0.0f));
        
        g.setGradientFill(bandGradient);
        g.fillRect(bandCenterX - bandWidth, plotArea.getY(), bandWidth * 2, plotArea.getHeight());
        
        // Center line for frequency
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.8f));
        g.drawVerticalLine((int)bandCenterX, plotArea.getY(), plotArea.getBottom());
        
        // Draw gain reduction curve (animated)
        float gainReductionDb = deEsser.getCurrentGainReductionDb();
        gainReductionDb = juce::jlimit(-20.0f, 0.0f, gainReductionDb);
        
        if (gainReductionDb < -0.5f)
        {
            // Draw the reduction "dip" at the sibilance frequency
            juce::Path reductionPath;
            
            float dipCenterY = dbToY(gainReductionDb, plotArea);
            float zeroY = dbToY(0.0f, plotArea);
            
            reductionPath.startNewSubPath(plotArea.getX(), zeroY);
            
            // Smooth curve to the dip
            for (float x = plotArea.getX(); x <= plotArea.getRight(); x += 2.0f)
            {
                float normalizedX = (x - bandCenterX) / bandWidth;
                float curve = std::exp(-normalizedX * normalizedX * 2.0f);
                float y = zeroY + (dipCenterY - zeroY) * curve;
                reductionPath.lineTo(x, y);
            }
            
            reductionPath.lineTo(plotArea.getRight(), zeroY);
            
            // Fill under curve
            juce::Path fillPath = reductionPath;
            fillPath.lineTo(plotArea.getRight(), plotArea.getY());
            fillPath.lineTo(plotArea.getX(), plotArea.getY());
            fillPath.closeSubPath();
            
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.15f));
            g.fillPath(fillPath);
            
            // Stroke the curve
            g.setColour(juce::Colour(0xFFD4AF37));
            g.strokePath(reductionPath, juce::PathStrokeType(2.0f));
        }
        else
        {
            // Draw flat 0dB line when not reducing
            float zeroY = dbToY(0.0f, plotArea);
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.5f));
            g.drawHorizontalLine((int)zeroY, plotArea.getX(), plotArea.getRight());
        }
        
        // Draw gain reduction meter on the right
        auto meterArea = bounds.removeFromRight(25).reduced(5, topMargin + 5);
        meterArea.removeFromBottom(bottomMargin - 5);
        
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRect(meterArea);
        
        // Meter fill (from top down for reduction)
        float meterLevel = juce::jmap(gainReductionDb, -20.0f, 0.0f, 1.0f, 0.0f);
        meterLevel = juce::jlimit(0.0f, 1.0f, meterLevel);
        
        if (meterLevel > 0.01f)
        {
            float fillHeight = meterArea.getHeight() * meterLevel;
            auto fillRect = meterArea.withHeight(fillHeight);
            
            // Color gradient from yellow to red
            juce::Colour meterColor = (meterLevel < 0.5f) 
                ? juce::Colour(0xFFD4AF37)
                : juce::Colour(0xFFD4AF37).interpolatedWith(juce::Colours::red, (meterLevel - 0.5f) * 2.0f);
            
            g.setColour(meterColor);
            g.fillRect(fillRect);
        }
        
        // Meter border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(meterArea, 1.0f);
        
        // Reduction value text
        g.setColour(juce::Colour(0xFFD4AF37));
        g.setFont(10.0f);
        g.drawText(juce::String(gainReductionDb, 1) + " dB", 
                   meterArea.getX() - 35, meterArea.getBottom() + 2, 60, 12,
                   juce::Justification::centred);
        
        // Frequency label at center
        g.setColour(juce::Colour(0xFFD4AF37));
        g.setFont(11.0f);
        juce::String freqText = (params.frequency >= 10000.0f)
            ? juce::String(params.frequency / 1000.0f, 1) + " kHz"
            : juce::String((int)params.frequency) + " Hz";
        g.drawText(freqText, (int)bandCenterX - 30, (int)plotArea.getY() + 5, 60, 14,
                   juce::Justification::centred);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(plotArea.expanded(leftMargin, 0).withTrimmedRight(30), 1.0f);
        
        // Mode indicator
        g.setColour(juce::Colour(0xFF808080));
        g.setFont(10.0f);
        juce::String modeText = (params.mode == DeEsserProcessor::Mode::Wideband) 
            ? "WIDEBAND" : "SPLIT-BAND";
        g.drawText(modeText, plotArea.getX(), plotArea.getY() + 2, 70, 12,
                   juce::Justification::left);
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    float freqToX(float freq, const juce::Rectangle<float>& area) const
    {
        // Linear scale from 2kHz to 16kHz
        float normalized = (freq - 2000.0f) / (16000.0f - 2000.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        return area.getX() + normalized * area.getWidth();
    }
    
    float dbToY(float db, const juce::Rectangle<float>& area) const
    {
        // 0dB at top, -20dB at bottom
        float normalized = -db / 20.0f;
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        return area.getY() + normalized * area.getHeight();
    }
    
    DeEsserProcessor& deEsser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserGraphComponent)
};

// ==============================================================================
// Main De-Esser Panel
// ==============================================================================
class DeEsserPanel : public juce::Component, private juce::Timer
{
public:
    DeEsserPanel(DeEsserProcessor& proc, PresetManager& /*presets*/) : deEsser(proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = deEsser.getParams();
        
        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!deEsser.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() {
            deEsser.setBypassed(!toggleButton->getToggleState());
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("De-Esser", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        
        // Subtitle
        addAndMakeVisible(subtitleLabel);
        subtitleLabel.setText("Sibilance Reduction", juce::dontSendNotification);
        subtitleLabel.setFont(juce::Font(11.0f));
        subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        subtitleLabel.setJustificationType(juce::Justification::centredLeft);
        
        // Mode selector buttons
        widebandButton = std::make_unique<DeEsserModeButton>("WIDEBAND");
        widebandButton->onClick = [this]() { selectMode(DeEsserProcessor::Mode::Wideband); };
        addAndMakeVisible(widebandButton.get());
        
        splitBandButton = std::make_unique<DeEsserModeButton>("SPLIT-BAND");
        splitBandButton->onClick = [this]() { selectMode(DeEsserProcessor::Mode::SplitBand); };
        addAndMakeVisible(splitBandButton.get());
        
        updateModeButtons();
        
        // Listen mode button
        listenButton = std::make_unique<ListenModeButton>();
        listenButton->setActive(params.listenMode);
        listenButton->onToggle = [this](bool active) {
            auto p = deEsser.getParams();
            p.listenMode = active;
            deEsser.setParams(p);
        };
        addAndMakeVisible(listenButton.get());
        
        // Sliders
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
        
        createSlider(frequencySlider, "Frequency", "CC 70", 2000.0, 16000.0, params.frequency, " Hz");
        createSlider(bandwidthSlider, "Width", "CC 71", 0.5, 4.0, params.bandwidth, "");
        createSlider(thresholdSlider, "Threshold", "CC 72", -60.0, 0.0, params.threshold, " dB");
        createSlider(reductionSlider, "Reduction", "CC 73", 0.0, 20.0, params.reduction, " dB");
        createSlider(attackSlider, "Attack", "CC 74", 0.1, 10.0, params.attack, " ms");
        createSlider(releaseSlider, "Release", "CC 75", 10.0, 200.0, params.release, " ms");
        createSlider(rangeSlider, "Range", "CC 76", 0.5, 2.0, params.range, "x");
        
        // Set logarithmic skew for frequency
        frequencySlider->getSlider().setSkewFactor(0.5);
        
        // Graph component
        graphComponent = std::make_unique<DeEsserGraphComponent>(deEsser);
        addAndMakeVisible(graphComponent.get());
        
        startTimerHz(15);
    }
    
    ~DeEsserPanel() override
    {
        stopTimer();
        frequencySlider->getSlider().setLookAndFeel(nullptr);
        bandwidthSlider->getSlider().setLookAndFeel(nullptr);
        thresholdSlider->getSlider().setLookAndFeel(nullptr);
        reductionSlider->getSlider().setLookAndFeel(nullptr);
        attackSlider->getSlider().setLookAndFeel(nullptr);
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
        
        // "MODE" label
        auto area = getLocalBounds().reduced(15);
        area.removeFromTop(55);
        g.setColour(juce::Colour(0xFF888888));
        g.setFont(11.0f);
        g.drawText("MODE", 15, area.getY() + 5, 40, 16, juce::Justification::centredLeft);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Title row
        auto titleRow = area.removeFromTop(22);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        
        // Subtitle
        auto subtitleRow = area.removeFromTop(16);
        subtitleLabel.setBounds(subtitleRow);
        
        area.removeFromTop(8);
        
        // Mode selector row
        auto modeRow = area.removeFromTop(32);
        modeRow.removeFromLeft(50);  // Space for "MODE" label
        
        int buttonWidth = 90;
        int buttonSpacing = 10;
        widebandButton->setBounds(modeRow.removeFromLeft(buttonWidth));
        modeRow.removeFromLeft(buttonSpacing);
        splitBandButton->setBounds(modeRow.removeFromLeft(buttonWidth));
        modeRow.removeFromLeft(buttonSpacing + 20);
        
        // Listen button
        listenButton->setBounds(modeRow.removeFromLeft(50).withHeight(40));
        
        area.removeFromTop(15);
        
        // FIX: Calculate control area based on actual slider needs
        int sliderWidth = 60;  // Reduced from 65
        int spacing = 10;      // Reduced from 12
        int groupGap = 20;     // Gap between slider groups
        
        // Total: 7 sliders + 4 spacings + 2 group gaps
        int controlAreaWidth = (sliderWidth * 7) + (spacing * 4) + (groupGap * 2);
        
        auto controlArea = area.removeFromLeft(controlAreaWidth);
        area.removeFromLeft(20);  // Gap before graph
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout sliders in 3 groups: [Freq, Width, Range] [Thresh, Reduc] [Atk, Rel]
        frequencySlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        bandwidthSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        rangeSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(groupGap);
        
        thresholdSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        reductionSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(groupGap);
        
        attackSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
        controlArea.removeFromLeft(spacing);
        releaseSlider->setBounds(controlArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromPreset()
    {
        auto p = deEsser.getParams();
        toggleButton->setToggleState(!deEsser.isBypassed(), juce::dontSendNotification);
        frequencySlider->setValue(p.frequency, juce::dontSendNotification);
        bandwidthSlider->setValue(p.bandwidth, juce::dontSendNotification);
        thresholdSlider->setValue(p.threshold, juce::dontSendNotification);
        reductionSlider->setValue(p.reduction, juce::dontSendNotification);
        attackSlider->setValue(p.attack, juce::dontSendNotification);
        releaseSlider->setValue(p.release, juce::dontSendNotification);
        rangeSlider->setValue(p.range, juce::dontSendNotification);
        listenButton->setActive(p.listenMode);
        updateModeButtons();
    }

private:
    void timerCallback() override
    {
        auto p = deEsser.getParams();
        
        if (!frequencySlider->getSlider().isMouseOverOrDragging())
            frequencySlider->setValue(p.frequency, juce::dontSendNotification);
        if (!bandwidthSlider->getSlider().isMouseOverOrDragging())
            bandwidthSlider->setValue(p.bandwidth, juce::dontSendNotification);
        if (!thresholdSlider->getSlider().isMouseOverOrDragging())
            thresholdSlider->setValue(p.threshold, juce::dontSendNotification);
        if (!reductionSlider->getSlider().isMouseOverOrDragging())
            reductionSlider->setValue(p.reduction, juce::dontSendNotification);
        if (!attackSlider->getSlider().isMouseOverOrDragging())
            attackSlider->setValue(p.attack, juce::dontSendNotification);
        if (!releaseSlider->getSlider().isMouseOverOrDragging())
            releaseSlider->setValue(p.release, juce::dontSendNotification);
        if (!rangeSlider->getSlider().isMouseOverOrDragging())
            rangeSlider->setValue(p.range, juce::dontSendNotification);
        
        bool shouldBeOn = !deEsser.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void selectMode(DeEsserProcessor::Mode mode)
    {
        auto p = deEsser.getParams();
        if (p.mode != mode)
        {
            p.mode = mode;
            deEsser.setParams(p);
            updateModeButtons();
        }
    }
    
    void updateModeButtons()
    {
        auto mode = deEsser.getParams().mode;
        widebandButton->setSelected(mode == DeEsserProcessor::Mode::Wideband);
        splitBandButton->setSelected(mode == DeEsserProcessor::Mode::SplitBand);
    }
    
    void updateProcessor()
    {
        DeEsserProcessor::Params p = deEsser.getParams();
        p.frequency = (float)frequencySlider->getValue();
        p.bandwidth = (float)bandwidthSlider->getValue();
        p.threshold = (float)thresholdSlider->getValue();
        p.reduction = (float)reductionSlider->getValue();
        p.attack = (float)attackSlider->getValue();
        p.release = (float)releaseSlider->getValue();
        p.range = (float)rangeSlider->getValue();
        deEsser.setParams(p);
    }
    
    DeEsserProcessor& deEsser;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    
    // Mode selector buttons
    std::unique_ptr<DeEsserModeButton> widebandButton;
    std::unique_ptr<DeEsserModeButton> splitBandButton;
    
    // Listen mode button
    std::unique_ptr<ListenModeButton> listenButton;
    
    // Sliders
    std::unique_ptr<VerticalSlider> frequencySlider;
    std::unique_ptr<VerticalSlider> bandwidthSlider;
    std::unique_ptr<VerticalSlider> thresholdSlider;
    std::unique_ptr<VerticalSlider> reductionSlider;
    std::unique_ptr<VerticalSlider> attackSlider;
    std::unique_ptr<VerticalSlider> releaseSlider;
    std::unique_ptr<VerticalSlider> rangeSlider;
    
    std::unique_ptr<DeEsserGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserPanel)
};