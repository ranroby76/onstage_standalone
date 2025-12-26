#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

// Compressor Gain Reduction Graph with Moving Circle
class CompressorGraphComponent : public juce::Component, private juce::Timer {
public:
    CompressorGraphComponent(AudioEngine& engine, int micIdx) 
        : audioEngine(engine), micIndex(micIdx) 
    {
        startTimerHz(60); // Smooth circle movement
    }
    
    ~CompressorGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        auto& comp = audioEngine.getCompressorProcessor(micIndex);
        auto params = comp.getParams();
        
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
            
            // Add makeup gain
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
        
        // Draw curve
        g.setColour(juce::Colour(0xFFD4AF37));
        g.strokePath(curve, juce::PathStrokeType(2.0f));
        
        // Draw threshold line
        float thresholdX = juce::jmap(params.thresholdDb, -60.0f, 0.0f, 0.0f, (float)getWidth());
        g.setColour(juce::Colour(0xFF8B7000));
        g.drawVerticalLine((int)thresholdX, 0.0f, (float)getHeight());
        
        // Draw 1:1 reference line (no compression)
        g.setColour(juce::Colour(0xFF404040));
        g.drawLine(0, getHeight(), getWidth(), 0, 1.0f);
        
        // Draw moving circle showing current compression
        float currentInputDb = comp.getCurrentInputLevelDb();
        currentInputDb = juce::jlimit(-60.0f, 0.0f, currentInputDb);
        
        float currentOutputDb = calculateOutputLevel(currentInputDb, params.thresholdDb, params.ratio);
        currentOutputDb += params.makeupDb;
        currentOutputDb = juce::jlimit(-60.0f, 0.0f, currentOutputDb);
        
        float circleX = juce::jmap(currentInputDb, -60.0f, 0.0f, 0.0f, (float)getWidth());
        float circleY = juce::jmap(currentOutputDb, 0.0f, -60.0f, 0.0f, (float)getHeight());
        
        // Glow effect
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.3f));
        g.fillEllipse(circleX - 12, circleY - 12, 24, 24);
        
        // Circle
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
            return inputDb; // No compression
        
        float overThreshold = inputDb - thresholdDb;
        float compressed = overThreshold / ratio;
        return thresholdDb + compressed;
    }
    
    AudioEngine& audioEngine;
    int micIndex;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorGraphComponent)
};

class CompressorPanel : public juce::Component, private juce::Timer {
public:
    CompressorPanel(AudioEngine& engine, int micIndex, const juce::String& micName)
        : audioEngine(engine), micIdx(micIndex)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& c = audioEngine.getCompressorProcessor(micIdx);
        auto params = c.getParams();

        toggleButton = std::make_unique<EffectToggleButton>();
        int note = (micIdx == 0) ? 19 : 22;
        toggleButton->setMidiInfo("MIDI: Note " + juce::String(note));
        toggleButton->setToggleState(!c.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            audioEngine.getCompressorProcessor(micIdx).setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        addAndMakeVisible(titleLabel);
        titleLabel.setText(micName + " - Compressor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, 
                      double min, double max, double step, double val, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(n);
            s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(min, max, step);
            s->setValue(val);
            s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateCompressor(); };
            addAndMakeVisible(s.get());
        };
        
        int base = (micIdx == 0) ? 32 : 42;
        cS(thresholdSlider, "Threshold", base, -60.0, 0.0, 0.6, params.thresholdDb, " dB");
        cS(ratioSlider, "Ratio", base+1, 1.0, 20.0, 0.19, params.ratio, ":1");
        cS(attackSlider, "Attack", base+2, 0.1, 100.0, 0.999, params.attackMs, " ms");
        cS(releaseSlider, "Release", base+3, 10.0, 1000.0, 9.9, params.releaseMs, " ms");
        cS(makeupSlider, "Makeup", base+4, 0.0, 24.0, 0.24, params.makeupDb, " dB");
        
        // Add graph component
        graphComponent = std::make_unique<CompressorGraphComponent>(audioEngine, micIdx);
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
        auto titleRow = area.removeFromTop(40);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        area.removeFromTop(10);

        // Left-aligned sliders, graph on right
        int sliderAreaWidth = 400;
        auto sliderArea = area.removeFromLeft(sliderAreaWidth);
        area.removeFromLeft(20); // Gap
        
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
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        attackSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        releaseSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        makeupSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromPreset() {
        auto& c = audioEngine.getCompressorProcessor(micIdx);
        auto p = c.getParams();
        thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        ratioSlider->setValue(p.ratio, juce::dontSendNotification);
        attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
        makeupSlider->setValue(p.makeupDb, juce::dontSendNotification);
        toggleButton->setToggleState(!c.isBypassed(), juce::dontSendNotification);
        repaint();
    }
    
private:
    void timerCallback() override {
        auto& c = audioEngine.getCompressorProcessor(micIdx);
        auto p = c.getParams();
        
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
        
        bool shouldBeOn = !c.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void updateCompressor() {
        CompressorProcessor::Params p;
        p.thresholdDb = thresholdSlider->getValue();
        p.ratio = ratioSlider->getValue();
        p.attackMs = attackSlider->getValue();
        p.releaseMs = releaseSlider->getValue();
        p.makeupDb = makeupSlider->getValue();
        audioEngine.getCompressorProcessor(micIdx).setParams(p);
    }
    
    AudioEngine& audioEngine;
    int micIdx;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> thresholdSlider, ratioSlider, attackSlider, releaseSlider, makeupSlider;
    std::unique_ptr<CompressorGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPanel)
};
