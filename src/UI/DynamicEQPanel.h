#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

// Forward Declaration
class DynamicEQPanel;

// Component to hold controls for a single band
class BandControlsComponent : public juce::Component
{
public:
    BandControlsComponent(AudioEngine& engine, int bandIndex, DynamicEQPanel* parent)
        : audioEngine(engine), band(bandIndex), parentPanel(parent)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& d = audioEngine.getDynamicEQProcessor();
        auto params = d.getParams(band);
        
        juce::String bandName = "Band " + juce::String(band + 1);
        
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, const juce::String& m, 
                      double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(n);
            s->setMidiInfo(m);
            s->setRange(min, max, (max-min)/100.0);
            s->setValue(v);
            s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateDynamicEQ(); };
            addAndMakeVisible(s.get());
        };
        
        cS(duckBandSlider, "Duck Band", "MIDI: CC 59", 100.0, 8000.0, params.duckBandHz, " Hz");
        cS(qSlider, "Q", "MIDI: CC 60", 0.1, 10.0, params.q, "");
        cS(shapeSlider, "Shape", "MIDI: CC 61", 0.0, 1.0, params.shape, "");
        cS(thresholdSlider, "Threshold", "MIDI: CC 62", -60.0, 0.0, params.threshold, " dB");
        cS(ratioSlider, "Ratio", "MIDI: CC 65", 1.0, 20.0, params.ratio, ":1");
        cS(attackSlider, "Attack", "MIDI: CC 66", 0.1, 100.0, params.attack, " ms");
        cS(releaseSlider, "Release", "MIDI: CC 67", 10.0, 1000.0, params.release, " ms");
    }
    
    ~BandControlsComponent() override {
        duckBandSlider->getSlider().setLookAndFeel(nullptr);
        qSlider->getSlider().setLookAndFeel(nullptr);
        shapeSlider->getSlider().setLookAndFeel(nullptr);
        thresholdSlider->getSlider().setLookAndFeel(nullptr);
        ratioSlider->getSlider().setLookAndFeel(nullptr);
        attackSlider->getSlider().setLookAndFeel(nullptr);
        releaseSlider->getSlider().setLookAndFeel(nullptr);
    }
    
    void resized() override {
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
    
    void updateFromProcessor() {
        auto& d = audioEngine.getDynamicEQProcessor();
        auto p = d.getParams(band);
        duckBandSlider->setValue(p.duckBandHz, juce::dontSendNotification);
        qSlider->setValue(p.q, juce::dontSendNotification);
        shapeSlider->setValue(p.shape, juce::dontSendNotification);
        thresholdSlider->setValue(p.threshold, juce::dontSendNotification);
        ratioSlider->setValue(p.ratio, juce::dontSendNotification);
        attackSlider->setValue(p.attack, juce::dontSendNotification);
        releaseSlider->setValue(p.release, juce::dontSendNotification);
    }

private:
    void updateDynamicEQ() {
        DynamicEQProcessor::BandParams p;
        p.duckBandHz = duckBandSlider->getValue();
        p.q = qSlider->getValue();
        p.shape = shapeSlider->getValue();
        p.threshold = thresholdSlider->getValue();
        p.ratio = ratioSlider->getValue();
        p.attack = attackSlider->getValue();
        p.release = releaseSlider->getValue();
        audioEngine.getDynamicEQProcessor().setParams(band, p);
    }

    AudioEngine& audioEngine;
    int band;
    DynamicEQPanel* parentPanel;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<VerticalSlider> duckBandSlider, qSlider, shapeSlider, thresholdSlider, ratioSlider, attackSlider, releaseSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlsComponent)
    friend class DynamicEQPanel;
};

// Dynamic EQ Graph with Animated Pumping Bowls (Dual Band)
class DynamicEQGraphComponent : public juce::Component, private juce::Timer {
public:
    DynamicEQGraphComponent(AudioEngine& engine) 
        : audioEngine(engine) 
    {
        startTimerHz(60); 
    }
    
    ~DynamicEQGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto graphBounds = getLocalBounds().toFloat();
        auto& dyn = audioEngine.getDynamicEQProcessor();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(graphBounds);
        
        // Grid lines
        g.setColour(juce::Colour(0xFF2A2A2A));
        for (int i = 1; i < 5; ++i) {
            float y = graphBounds.getHeight() * i / 5.0f;
            g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
        }
        
        // Draw baseline (0dB)
        float zeroY = getHeight() / 2.0f;
        g.setColour(juce::Colour(0xFF404040));
        g.drawHorizontalLine((int)zeroY, 0.0f, (float)getWidth());

        // Draw both bands
        drawBand(g, dyn, 0, juce::Colour(0xFFD4AF37), zeroY, graphBounds); // Gold
        drawBand(g, dyn, 1, juce::Colour(0xFF00CED1), zeroY, graphBounds); // Cyan
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(graphBounds, 1.0f);
    }
    
    void timerCallback() override { repaint(); }
    
private:
    void drawBand(juce::Graphics& g, DynamicEQProcessor& dyn, int bandIndex, juce::Colour colour, float zeroY, juce::Rectangle<float> graphBounds)
    {
        auto params = dyn.getParams(bandIndex);
        
        // Calculate bowl position
        float bowlFreqX = juce::jmap(std::log10(params.duckBandHz / 20.0f) / std::log10(1000.0f), 
                                      0.0f, 1.0f, 0.0f, (float)getWidth());
        
        float gainReductionDb = dyn.getCurrentGainReductionDb(bandIndex);
        gainReductionDb = juce::jlimit(0.0f, 24.0f, gainReductionDb);
        
        float bowlWidthHz = params.duckBandHz / params.q;
        float bowlWidthX = (getWidth() / 3.0f) / params.q; 
        
        float maxDepthDb = 20.0f; 
        float bowlDepthDb = (gainReductionDb / 24.0f) * maxDepthDb; 
        float bowlDepthY = (bowlDepthDb / maxDepthDb) * (getHeight() * 0.4f); 
        
        juce::Path bowlPath;
        int bowlStartX = (int)(bowlFreqX - bowlWidthX);
        int bowlEndX = (int)(bowlFreqX + bowlWidthX);
        bowlStartX = juce::jlimit(0, getWidth(), bowlStartX);
        bowlEndX = juce::jlimit(0, getWidth(), bowlEndX);
        
        for (int x = bowlStartX; x <= bowlEndX; ++x) {
            float normalizedX = (x - bowlStartX) / (float)(bowlEndX - bowlStartX);
            if (bowlEndX == bowlStartX) normalizedX = 0.5f;
            normalizedX = (normalizedX - 0.5f) * 2.0f; 
            
            float bowlCurve = std::exp(-normalizedX * normalizedX * 3.0f); 
            float y = zeroY + (bowlDepthY * bowlCurve * params.shape); 
            
            if (x == bowlStartX) bowlPath.startNewSubPath((float)x, zeroY);
            bowlPath.lineTo((float)x, y);
        }
        
        bowlPath.lineTo((float)bowlEndX, zeroY);
        bowlPath.closeSubPath();
        
        float alpha = juce::jmap(gainReductionDb, 0.0f, 24.0f, 0.1f, 0.4f);
        g.setColour(colour.withAlpha(alpha));
        g.fillPath(bowlPath);
        
        g.setColour(colour.withAlpha(0.8f));
        g.strokePath(bowlPath, juce::PathStrokeType(2.0f));
        
        // Label
        g.setColour(colour.withAlpha(0.5f));
        g.setFont(10.0f);
        juce::String freqLabel = (params.duckBandHz < 1000.0f) 
            ? juce::String((int)params.duckBandHz) + "Hz"
            : juce::String(params.duckBandHz / 1000.0f, 1) + "kHz";
        
        // JUCE 8 FIX: Use Rectangle<int> for drawText bounds to be safe
        juce::Rectangle<int> textRect((int)(bowlFreqX - 25), (int)(graphBounds.getBottom() - 12), 50, 10);
        g.drawText(freqLabel, textRect, juce::Justification::centred);
    }

    AudioEngine& audioEngine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQGraphComponent)
};

class DynamicEQPanel : public juce::Component, private juce::Timer {
public:
    DynamicEQPanel(AudioEngine& engine) : audioEngine(engine) {
        auto& d = audioEngine.getDynamicEQProcessor();
        
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 28");
        toggleButton->setToggleState(!d.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            audioEngine.getDynamicEQProcessor().setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Sidechain Compressor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Tabbed Component for Bands
        tabbedComponent = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::Orientation::TabsAtTop);
        tabbedComponent->setTabBarDepth(30);
        tabbedComponent->addTab("Band 1", juce::Colours::transparentBlack, new BandControlsComponent(engine, 0, this), true);
        tabbedComponent->addTab("Band 2", juce::Colours::transparentBlack, new BandControlsComponent(engine, 1, this), true);
        addAndMakeVisible(tabbedComponent.get());
        
        // Add graph component
        graphComponent = std::make_unique<DynamicEQGraphComponent>(audioEngine);
        addAndMakeVisible(graphComponent.get());
        
        startTimerHz(15);
    }
    
    ~DynamicEQPanel() override {
        stopTimer();
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
        auto topRow = area.removeFromTop(40);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        area.removeFromTop(10);
        
        // Layout: Tabs Left, Graph Right
        auto leftArea = area.removeFromLeft(500); // Width for tabs
        tabbedComponent->setBounds(leftArea);
        
        area.removeFromLeft(20);
        graphComponent->setBounds(area);
    }
    
    void updateFromPreset() {
        auto& d = audioEngine.getDynamicEQProcessor();
        toggleButton->setToggleState(!d.isBypassed(), juce::dontSendNotification);
        
        // Update all tabs
        for (int i = 0; i < tabbedComponent->getNumTabs(); ++i)
        {
            if (auto* bandComp = dynamic_cast<BandControlsComponent*>(tabbedComponent->getTabContentComponent(i)))
            {
                bandComp->updateFromProcessor();
            }
        }
    }
    
private:
    void timerCallback() override {
        auto& d = audioEngine.getDynamicEQProcessor();
        bool shouldBeOn = !d.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
            
        // Sync sliders from processor (handles external changes/preset loads)
        for (int i = 0; i < tabbedComponent->getNumTabs(); ++i)
        {
            if (auto* bandComp = dynamic_cast<BandControlsComponent*>(tabbedComponent->getTabContentComponent(i)))
            {
                // Only update if not interacting
                if (!bandComp->duckBandSlider->getSlider().isMouseOverOrDragging() &&
                    !bandComp->qSlider->getSlider().isMouseOverOrDragging() &&
                    !bandComp->shapeSlider->getSlider().isMouseOverOrDragging() &&
                    !bandComp->thresholdSlider->getSlider().isMouseOverOrDragging() &&
                    !bandComp->ratioSlider->getSlider().isMouseOverOrDragging() &&
                    !bandComp->attackSlider->getSlider().isMouseOverOrDragging() &&
                    !bandComp->releaseSlider->getSlider().isMouseOverOrDragging())
                {
                    bandComp->updateFromProcessor();
                }
            }
        }
    }
    
    AudioEngine& audioEngine;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<juce::TabbedComponent> tabbedComponent;
    std::unique_ptr<DynamicEQGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQPanel)
};