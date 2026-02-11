// ==============================================================================
//  DynamicEQPanel.h
//  OnStage - Dynamic EQ / Sidechain Compressor UI
//
//  Features:
//  - Blue pins: Input source (playback)
//  - Green pins: Reductor source (vocals) - sidechain input
//  - Y-axis: -4, -8, -12, -16, -20 dB gain reduction
//  - Band buttons highlight golden when selected
//  - 2x wider layout
//
//  FIX: Band frequency lines now centered on selected frequency
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/DynamicEQProcessor.h"

class PresetManager;
class DynamicEQPanel;

// ==============================================================================
// Band Selector Button - Golden when selected, dark when not
// Always one must be selected (radio group behavior)
// ==============================================================================
class BandSelectorButton : public juce::Button
{
public:
    BandSelectorButton(const juce::String& text, juce::Colour bandColor)
        : juce::Button(text), color(bandColor)
    {
        setClickingTogglesState(false);  // We handle toggle manually
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, 
                     bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2);
        
        bool isOn = getToggleState();
        
        // Background
        if (isOn)
        {
            g.setColour(juce::Colour(0xFFD4AF37));  // Golden
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(juce::Colours::black);
        }
        else
        {
            g.setColour(juce::Colour(0xFF2A2A2A));
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(color);  // Band color when not selected
        }
        
        // Border
        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            g.setColour(juce::Colours::white.withAlpha(0.5f));
        else if (isOn)
            g.setColour(juce::Colour(0xFFB8860B));  // Dark golden border
        else
            g.setColour(juce::Colour(0xFF404040));
        
        g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
        
        // Text
        if (isOn)
            g.setColour(juce::Colours::black);
        else
            g.setColour(color);
        
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(getButtonText(), bounds, juce::Justification::centred);
    }
    
private:
    juce::Colour color;
};

// ==============================================================================
// Band Controls Component
// ==============================================================================
class BandControlsComponent : public juce::Component
{
public:
    BandControlsComponent(DynamicEQProcessor& proc, int bandIndex, DynamicEQPanel* parent)
        : dynEQ(proc), band(bandIndex), parentPanel(parent)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = dynEQ.getParams(band);
        
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
            s->getSlider().onValueChange = [this]() { updateDynamicEQ(); };
            addAndMakeVisible(s.get());
        };
        
        createSlider(duckBandSlider, "Freq", "CC 59", 100.0, 8000.0, params.duckBandHz, " Hz");
        createSlider(qSlider, "Q", "CC 60", 0.1, 10.0, params.q, "");
        createSlider(shapeSlider, "Shape", "CC 61", 0.0, 1.0, params.shape, "");
        createSlider(thresholdSlider, "Thresh", "CC 62", -60.0, 0.0, params.threshold, " dB");
        createSlider(ratioSlider, "Ratio", "CC 65", 1.0, 20.0, params.ratio, ":1");
        createSlider(attackSlider, "Attack", "CC 66", 0.1, 100.0, params.attack, " ms");
        createSlider(releaseSlider, "Release", "CC 67", 10.0, 1000.0, params.release, " ms");
    }
    
    ~BandControlsComponent() override
    {
        duckBandSlider->getSlider().setLookAndFeel(nullptr);
        qSlider->getSlider().setLookAndFeel(nullptr);
        shapeSlider->getSlider().setLookAndFeel(nullptr);
        thresholdSlider->getSlider().setLookAndFeel(nullptr);
        ratioSlider->getSlider().setLookAndFeel(nullptr);
        attackSlider->getSlider().setLookAndFeel(nullptr);
        releaseSlider->getSlider().setLookAndFeel(nullptr);
    }
    
    void resized() override
    {
        auto area = getLocalBounds();
        int numSliders = 7;
        int sliderWidth = 50;
        int spacing = 10;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = (getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);

        duckBandSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        qSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        shapeSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        thresholdSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        attackSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        releaseSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromProcessor()
    {
        auto p = dynEQ.getParams(band);
        duckBandSlider->setValue(p.duckBandHz, juce::dontSendNotification);
        qSlider->setValue(p.q, juce::dontSendNotification);
        shapeSlider->setValue(p.shape, juce::dontSendNotification);
        thresholdSlider->setValue(p.threshold, juce::dontSendNotification);
        ratioSlider->setValue(p.ratio, juce::dontSendNotification);
        attackSlider->setValue(p.attack, juce::dontSendNotification);
        releaseSlider->setValue(p.release, juce::dontSendNotification);
    }
    
    bool isAnySliderDragging() const
    {
        return duckBandSlider->getSlider().isMouseOverOrDragging() ||
               qSlider->getSlider().isMouseOverOrDragging() ||
               shapeSlider->getSlider().isMouseOverOrDragging() ||
               thresholdSlider->getSlider().isMouseOverOrDragging() ||
               ratioSlider->getSlider().isMouseOverOrDragging() ||
               attackSlider->getSlider().isMouseOverOrDragging() ||
               releaseSlider->getSlider().isMouseOverOrDragging();
    }

private:
    void updateDynamicEQ()
    {
        DynamicEQProcessor::BandParams p;
        p.duckBandHz = (float)duckBandSlider->getValue();
        p.q = (float)qSlider->getValue();
        p.shape = (float)shapeSlider->getValue();
        p.threshold = (float)thresholdSlider->getValue();
        p.ratio = (float)ratioSlider->getValue();
        p.attack = (float)attackSlider->getValue();
        p.release = (float)releaseSlider->getValue();
        dynEQ.setParams(band, p);
    }

    DynamicEQProcessor& dynEQ;
    int band;
    DynamicEQPanel* parentPanel;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<VerticalSlider> duckBandSlider, qSlider, shapeSlider;
    std::unique_ptr<VerticalSlider> thresholdSlider, ratioSlider, attackSlider, releaseSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlsComponent)
};

// ==============================================================================
// Dynamic EQ Graph - Accurate dB Scale Animation
// Y-axis: 0, -4, -8, -12, -16, -20 dB
// FIX: Band lines now centered on selected frequency
// ==============================================================================
class DynamicEQGraphComponent : public juce::Component, private juce::Timer
{
public:
    DynamicEQGraphComponent(DynamicEQProcessor& proc) 
        : dynEQProc(proc) 
    {
        startTimerHz(60); 
    }
    
    ~DynamicEQGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto graphBounds = getLocalBounds().toFloat();
        auto& dyn = dynEQProc;
        
        // Margins for labels
        const float leftMargin = 45.0f;
        const float bottomMargin = 25.0f;
        const float topMargin = 15.0f;
        
        auto plotArea = graphBounds.reduced(0);
        plotArea.removeFromLeft(leftMargin);
        plotArea.removeFromBottom(bottomMargin);
        plotArea.removeFromTop(topMargin);
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(graphBounds);
        
        // Draw Y-axis grid and labels (0, -4, -8, -12, -16, -20 dB)
        g.setFont(10.0f);
        const float dbValues[] = { 0.0f, -4.0f, -8.0f, -12.0f, -16.0f, -20.0f };
        const int numDbLines = 6;
        
        for (int i = 0; i < numDbLines; ++i)
        {
            float db = dbValues[i];
            float y = dbToY(db, plotArea);
            
            // Grid line
            if (i == 0)
                g.setColour(juce::Colour(0xFF505050));  // 0dB line brighter
            else
                g.setColour(juce::Colour(0xFF2A2A2A));
            
            g.drawHorizontalLine((int)y, plotArea.getX(), plotArea.getRight());
            
            // Label
            g.setColour(juce::Colour(0xFF808080));
            juce::String label = (db == 0.0f) ? "0" : juce::String((int)db);
            g.drawText(label + " dB", 2, (int)y - 6, (int)leftMargin - 5, 12, 
                       juce::Justification::right);
        }
        
        // Draw frequency axis labels
        const float freqLabels[] = { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 8000.0f };
        const int numFreqLabels = 7;
        
        g.setColour(juce::Colour(0xFF606060));
        for (int i = 0; i < numFreqLabels; ++i)
        {
            float freq = freqLabels[i];
            float x = freqToX(freq, plotArea);
            
            // Vertical grid line
            g.setColour(juce::Colour(0xFF1A1A1A));
            g.drawVerticalLine((int)x, plotArea.getY(), plotArea.getBottom());
            
            // Label
            g.setColour(juce::Colour(0xFF606060));
            juce::String label;
            if (freq >= 1000.0f)
                label = juce::String((int)(freq / 1000.0f)) + "k";
            else
                label = juce::String((int)freq);
            
            g.drawText(label, (int)x - 15, (int)plotArea.getBottom() + 5, 30, 15, 
                       juce::Justification::centred);
        }

        // Draw both bands' reduction bowls
        drawBand(g, dyn, 0, juce::Colour(0xFFD4AF37), plotArea); // Gold - Band 1
        drawBand(g, dyn, 1, juce::Colour(0xFF00CED1), plotArea); // Cyan - Band 2
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(plotArea, 1.0f);
        
        // Title with pin info
        g.setColour(juce::Colour(0xFF808080));
        g.setFont(11.0f);
        g.drawText("Blue pins: Input | Green pins: Sidechain (reductor)", 
                   graphBounds.getX() + leftMargin, graphBounds.getY() + 2, 
                   graphBounds.getWidth() - leftMargin - 10, 12, 
                   juce::Justification::right);
    }
    
    void timerCallback() override { repaint(); }
    
private:
    float freqToX(float freq, const juce::Rectangle<float>& area) const
    {
        // Log scale: 100Hz to 8000Hz
        float logMin = std::log10(100.0f);
        float logMax = std::log10(8000.0f);
        float logFreq = std::log10(juce::jlimit(100.0f, 8000.0f, freq));
        float normalized = (logFreq - logMin) / (logMax - logMin);
        return area.getX() + normalized * area.getWidth();
    }
    
    float dbToY(float db, const juce::Rectangle<float>& area) const
    {
        // 0dB at top, -20dB at bottom
        float normalized = -db / 20.0f;  // 0 to 1
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        return area.getY() + normalized * area.getHeight();
    }
    
    void drawBand(juce::Graphics& g, DynamicEQProcessor& dyn, int bandIndex, 
                  juce::Colour colour, const juce::Rectangle<float>& plotArea)
    {
        auto params = dyn.getParams(bandIndex);
        
        // Get actual gain reduction
        float gainReductionDb = dyn.getCurrentGainReductionDb(bandIndex);
        gainReductionDb = juce::jlimit(0.0f, 20.0f, gainReductionDb);
        
        // Bowl center X position (frequency) - THIS IS THE CENTER
        float bowlCenterX = freqToX(params.duckBandHz, plotArea);
        
        // FIX: Calculate bowl width symmetrically in PIXEL space (not frequency space)
        // This ensures the line is visually centered on the frequency
        float bowlWidthHz = params.duckBandHz / params.q;
        
        // Calculate asymmetric frequency bounds
        float lowerFreq = juce::jmax(100.0f, params.duckBandHz - bowlWidthHz);
        float upperFreq = juce::jmin(8000.0f, params.duckBandHz + bowlWidthHz);
        
        // Convert to pixel positions
        float bowlLeftX = freqToX(lowerFreq, plotArea);
        float bowlRightX = freqToX(upperFreq, plotArea);
        
        // FIX: Make the bowl symmetric around the center frequency in pixel space
        // Take the smaller of the two half-widths to keep it within bounds
        float leftHalfWidth = bowlCenterX - bowlLeftX;
        float rightHalfWidth = bowlRightX - bowlCenterX;
        float bowlWidthPx = juce::jmin(leftHalfWidth, rightHalfWidth);
        bowlWidthPx = juce::jmax(20.0f, bowlWidthPx);
        
        // 0dB Y position (top of the reduction)
        float zeroDbY = dbToY(0.0f, plotArea);
        
        // Bowl depth in pixels based on actual gain reduction
        float bowlDepthY = dbToY(-gainReductionDb, plotArea) - zeroDbY;
        
        // FIX: Draw a vertical center line at the exact frequency position
        g.setColour(colour.withAlpha(0.4f));
        g.drawVerticalLine((int)bowlCenterX, zeroDbY, zeroDbY + bowlDepthY);
        
        // FIX: Calculate bowl start/end CENTERED on bowlCenterX
        int bowlStartX = (int)(bowlCenterX - bowlWidthPx);
        int bowlEndX = (int)(bowlCenterX + bowlWidthPx);
        bowlStartX = juce::jlimit((int)plotArea.getX(), (int)plotArea.getRight(), bowlStartX);
        bowlEndX = juce::jlimit((int)plotArea.getX(), (int)plotArea.getRight(), bowlEndX);
        
        // Draw the bowl path
        juce::Path bowlPath;
        bool pathStarted = false;
        
        for (int x = bowlStartX; x <= bowlEndX; ++x)
        {
            // FIX: normalizedX is -1 at bowlStartX, 0 at bowlCenterX, +1 at bowlEndX
            float normalizedX = (x - bowlCenterX) / bowlWidthPx;
            
            // Gaussian bowl shape - maximum depth at normalizedX = 0 (center)
            float bowlCurve = std::exp(-normalizedX * normalizedX * 2.5f);
            
            // Apply shape parameter (0 = gentle, 1 = aggressive)
            bowlCurve = std::pow(bowlCurve, 1.0f - params.shape * 0.5f);
            
            float y = zeroDbY + (bowlDepthY * bowlCurve);
            
            if (!pathStarted)
            {
                bowlPath.startNewSubPath((float)x, zeroDbY);
                pathStarted = true;
            }
            bowlPath.lineTo((float)x, y);
        }
        
        if (pathStarted)
        {
            bowlPath.lineTo((float)bowlEndX, zeroDbY);
            bowlPath.closeSubPath();
            
            // Fill with gradient
            float alpha = juce::jmap(gainReductionDb, 0.0f, 20.0f, 0.1f, 0.5f);
            g.setColour(colour.withAlpha(alpha));
            g.fillPath(bowlPath);
            
            // Stroke outline
            g.setColour(colour.withAlpha(0.8f));
            g.strokePath(bowlPath, juce::PathStrokeType(2.0f));
        }
        
        // FIX: Draw a small diamond/marker at the exact center frequency
        float markerSize = 6.0f;
        juce::Path marker;
        marker.addTriangle(bowlCenterX, zeroDbY - markerSize,
                           bowlCenterX - markerSize * 0.7f, zeroDbY,
                           bowlCenterX + markerSize * 0.7f, zeroDbY);
        g.setColour(colour);
        g.fillPath(marker);
        
        // Frequency label at top (above the 0dB line)
        g.setColour(colour.withAlpha(0.9f));
        g.setFont(10.0f);
        juce::String freqLabel = (params.duckBandHz < 1000.0f) 
            ? juce::String((int)params.duckBandHz) + " Hz"
            : juce::String(params.duckBandHz / 1000.0f, 1) + " kHz";
        
        // FIX: Center the text on bowlCenterX
        g.drawText(freqLabel, (int)(bowlCenterX - 30), (int)(zeroDbY - 18), 60, 12, 
                   juce::Justification::centred);
        
        // Reduction amount label at bottom of bowl
        if (gainReductionDb > 0.5f)
        {
            g.setColour(colour);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("-" + juce::String(gainReductionDb, 1) + " dB",
                       (int)(bowlCenterX - 25), (int)(zeroDbY + bowlDepthY * 0.5f - 6), 
                       50, 12, juce::Justification::centred);
        }
    }

    DynamicEQProcessor& dynEQProc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQGraphComponent)
};

// ==============================================================================
// Main Dynamic EQ Panel - 2x wider
// ==============================================================================
class DynamicEQPanel : public juce::Component, private juce::Timer
{
public:
    DynamicEQPanel(DynamicEQProcessor& proc, PresetManager& /*presets*/) : dynEQ(proc)
    {
        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 28");
        toggleButton->setToggleState(!dynEQ.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            dynEQ.setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Dynamic EQ / Sidechain", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Band selector buttons - ensure one is always selected
        band1Button = std::make_unique<BandSelectorButton>("Band 1", juce::Colour(0xFFD4AF37));
        band1Button->setToggleState(true, juce::dontSendNotification);  // First one selected by default
        band1Button->onClick = [this]() { 
            if (!band1Button->getToggleState())  // Only act if not already selected
            {
                band1Button->setToggleState(true, juce::dontSendNotification);
                band2Button->setToggleState(false, juce::dontSendNotification);
                selectBand(0);
            }
        };
        addAndMakeVisible(band1Button.get());
        
        band2Button = std::make_unique<BandSelectorButton>("Band 2", juce::Colour(0xFF00CED1));
        band2Button->setToggleState(false, juce::dontSendNotification);
        band2Button->onClick = [this]() { 
            if (!band2Button->getToggleState())  // Only act if not already selected
            {
                band2Button->setToggleState(true, juce::dontSendNotification);
                band1Button->setToggleState(false, juce::dontSendNotification);
                selectBand(1);
            }
        };
        addAndMakeVisible(band2Button.get());
        
        // Band control components
        bandControls[0] = std::make_unique<BandControlsComponent>(dynEQ, 0, this);
        bandControls[1] = std::make_unique<BandControlsComponent>(dynEQ, 1, this);
        addAndMakeVisible(bandControls[0].get());
        addChildComponent(bandControls[1].get());
        
        // Graph
        graphComponent = std::make_unique<DynamicEQGraphComponent>(dynEQ);
        addAndMakeVisible(graphComponent.get());
        
        currentBand = 0;
        
        startTimerHz(15);
    }
    
    ~DynamicEQPanel() override
    {
        stopTimer();
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
        
        // Top row
        auto topRow = area.removeFromTop(40);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        area.removeFromTop(10);
        
        // Band buttons row
        auto buttonRow = area.removeFromTop(35);
        band1Button->setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(10);
        band2Button->setBounds(buttonRow.removeFromLeft(100));
        area.removeFromTop(10);
        
        // Keep original control sizes, graph gets extra space
        int controlsWidth = 500;
        auto leftArea = area.removeFromLeft(controlsWidth);
        
        // Band controls (overlay)
        bandControls[0]->setBounds(leftArea);
        bandControls[1]->setBounds(leftArea);
        
        area.removeFromLeft(20);
        graphComponent->setBounds(area);
    }
    
    void updateFromPreset()
    {
        toggleButton->setToggleState(!dynEQ.isBypassed(), juce::dontSendNotification);
        bandControls[0]->updateFromProcessor();
        bandControls[1]->updateFromProcessor();
    }
    
private:
    void selectBand(int band)
    {
        currentBand = band;
        bandControls[0]->setVisible(band == 0);
        bandControls[1]->setVisible(band == 1);
    }
    
    void timerCallback() override
    {
        bool shouldBeOn = !dynEQ.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
        
        // Update sliders if not dragging
        if (!bandControls[currentBand]->isAnySliderDragging())
            bandControls[currentBand]->updateFromProcessor();
    }
    
    DynamicEQProcessor& dynEQ;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<BandSelectorButton> band1Button;
    std::unique_ptr<BandSelectorButton> band2Button;
    std::unique_ptr<BandControlsComponent> bandControls[2];
    std::unique_ptr<DynamicEQGraphComponent> graphComponent;
    int currentBand = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQPanel)
};
