#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/DelayProcessor.h"

// Delay Traveling Echoes Animation
class DelayGraphComponent : public juce::Component, private juce::Timer {
public:
    struct Pulse {
        float position;    // 0.0 to 1.0 across timeline
        float amplitude;   // Brightness/size
        int channel;       // 0=Left, 1=Right
        int age;          // Frames alive
    };
    
    DelayGraphComponent(DelayProcessor& processor) 
        : delayProcessor(processor) 
    {
        startTimerHz(60); // Smooth animation
        frameCount = 0;
    }
    
    ~DelayGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        auto params = delayProcessor.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Draw channels
        float channelHeight = getHeight() / 2.0f;
        float topChannelY = channelHeight / 2.0f;
        float bottomChannelY = channelHeight + (channelHeight / 2.0f);
        
        // Channel labels
        g.setColour(juce::Colour(0xFF606060));
        g.setFont(10.0f);
        g.drawText("L", 5, topChannelY - 8, 20, 16, juce::Justification::left);
        g.drawText("R", 5, bottomChannelY - 8, 20, 16, juce::Justification::left);
        
        // Center lines
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.drawHorizontalLine((int)topChannelY, 0.0f, (float)getWidth());
        g.drawHorizontalLine((int)bottomChannelY, 0.0f, (float)getWidth());
        
        // Draw pulses
        for (const auto& pulse : pulses) {
            float x = pulse.position * getWidth();
            float y = (pulse.channel == 0) ? topChannelY : bottomChannelY;
            
            // Size based on amplitude
            float size = 8.0f * pulse.amplitude;
            
            // Color with trail effect
            float alpha = pulse.amplitude * 0.8f;
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(alpha));
            
            // Glow
            g.fillEllipse(x - size, y - size, size * 2, size * 2);
            
            // Core
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(alpha * 1.5f));
            g.fillEllipse(x - size/2, y - size/2, size, size);
            
            // Trail
            if (pulse.position > 0.05f) {
                float trailLength = 30.0f * pulse.amplitude;
                juce::ColourGradient trail(
                    juce::Colour(0xFFD4AF37).withAlpha(alpha * 0.6f), x, y,
                    juce::Colour(0xFFD4AF37).withAlpha(0.0f), x - trailLength, y,
                    false
                );
                g.setGradientFill(trail);
                g.fillRect(x - trailLength, y - 2, trailLength, 4.0f);
            }
        }
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override {
        auto params = delayProcessor.getParams();
        frameCount++;
        
        // Spawn new pulses periodically (simulated audio activity)
        if (frameCount % 20 == 0) { // Every 20 frames
            Pulse newPulse;
            newPulse.position = 0.0f;
            newPulse.amplitude = 0.8f + (juce::Random::getSystemRandom().nextFloat() * 0.2f);
            newPulse.channel = (params.stereoWidth > 1.0f && juce::Random::getSystemRandom().nextBool()) ? 1 : 0;
            newPulse.age = 0;
            pulses.push_back(newPulse);
        }
        
        // Update existing pulses
        float speed = 1.0f / (params.delayMs * 0.001f * 60.0f); // Convert delay to frames
        speed = juce::jlimit(0.005f, 0.05f, speed); // Limit speed for visibility
        
        float feedback = 1.0f - params.stage; // Stage controls decay
        
        for (int i = pulses.size() - 1; i >= 0; --i) {
            auto& pulse = pulses[i];
            
            // Move pulse
            pulse.position += speed;
            pulse.age++;
            
            // Spawn echo when pulse reaches delay point
            if (pulse.position >= 1.0f && pulse.amplitude > 0.15f) {
                Pulse echo;
                echo.position = 0.0f;
                echo.amplitude = pulse.amplitude * feedback * params.ratio;
                
                // Ping-pong effect
                if (params.stereoWidth > 1.0f) {
                    echo.channel = 1 - pulse.channel; // Switch channel
                } else {
                    echo.channel = pulse.channel;
                }
                
                echo.age = 0;
                
                if (echo.amplitude > 0.1f) {
                    pulses.push_back(echo);
                }
            }
            
            // Remove dead pulses
            if (pulse.position > 1.2f || pulse.amplitude < 0.05f) {
                pulses.erase(pulses.begin() + i);
            }
        }
        
        // Limit pulse count
        if (pulses.size() > 100) {
            pulses.erase(pulses.begin(), pulses.begin() + 20);
        }
        
        repaint();
    }
    
private:
    DelayProcessor& delayProcessor;
    std::vector<Pulse> pulses;
    int frameCount;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayGraphComponent)
};

class DelayPanel : public juce::Component, private juce::Timer {
public:
    DelayPanel(DelayProcessor& processor) : delayProcessor(processor) {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = delayProcessor.getParams();
        
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 27");
        toggleButton->setToggleState(!delayProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            delayProcessor.setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Stereo Delay", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, const juce::String& m, 
                      double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(n);
            s->setMidiInfo(m);
            s->setRange(min, max, (max-min)/200.0);
            s->setValue(v);
            s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateDelay(); };
            addAndMakeVisible(s.get());
        };
        
        cS(delayTimeSlider, "Time", "MIDI: CC 47", 1.0, 2000.0, params.delayMs, " ms");
        cS(ratioSlider, "Ratio", "MIDI: CC 48", 0.0, 1.0, params.ratio, "");
        cS(stageSlider, "Stage", "MIDI: CC 49", 0.0, 1.0, params.stage, "");
        cS(mixSlider, "Mix", "MIDI: CC 29", 0.0, 1.0, params.mix, "");
        cS(widthSlider, "Width", "MIDI: CC 50", 0.0, 2.0, params.stereoWidth, "");
        cS(lowCutSlider, "LowCut", "MIDI: CC 51", 20.0, 2000.0, params.lowCutHz, " Hz");
        cS(highCutSlider, "HighCut", "MIDI: CC 52", 2000.0, 20000.0, params.highCutHz, " Hz");
        
        // Add graph component
        graphComponent = std::make_unique<DelayGraphComponent>(delayProcessor);
        addAndMakeVisible(graphComponent.get());
        
        startTimerHz(15);
    }
    
    ~DelayPanel() override {
        stopTimer();
        delayTimeSlider->getSlider().setLookAndFeel(nullptr);
        ratioSlider->getSlider().setLookAndFeel(nullptr);
        stageSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
        widthSlider->getSlider().setLookAndFeel(nullptr);
        lowCutSlider->getSlider().setLookAndFeel(nullptr);
        highCutSlider->getSlider().setLookAndFeel(nullptr);
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
        int sliderAreaWidth = 480;
        auto sliderArea = area.removeFromLeft(sliderAreaWidth);
        area.removeFromLeft(20); // Gap
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout sliders
        int numSliders = 7;
        int sliderWidth = 60;
        int spacing = 12;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = sliderArea.getX();
        auto sArea = sliderArea.withX(startX).withWidth(totalW);
        
        delayTimeSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        stageSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        mixSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        widthSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        lowCutSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        highCutSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromPreset() {
        auto params = delayProcessor.getParams();
        toggleButton->setToggleState(!delayProcessor.isBypassed(), juce::dontSendNotification);
        delayTimeSlider->setValue(params.delayMs, juce::dontSendNotification);
        ratioSlider->setValue(params.ratio, juce::dontSendNotification);
        stageSlider->setValue(params.stage, juce::dontSendNotification);
        mixSlider->setValue(params.mix, juce::dontSendNotification);
        widthSlider->setValue(params.stereoWidth, juce::dontSendNotification);
        lowCutSlider->setValue(params.lowCutHz, juce::dontSendNotification);
        highCutSlider->setValue(params.highCutHz, juce::dontSendNotification);
    }
    
private:
    void timerCallback() override {
        auto params = delayProcessor.getParams();
        
        if (!delayTimeSlider->getSlider().isMouseOverOrDragging())
            delayTimeSlider->setValue(params.delayMs, juce::dontSendNotification);
        if (!ratioSlider->getSlider().isMouseOverOrDragging())
            ratioSlider->setValue(params.ratio, juce::dontSendNotification);
        if (!stageSlider->getSlider().isMouseOverOrDragging())
            stageSlider->setValue(params.stage, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(params.mix, juce::dontSendNotification);
        if (!widthSlider->getSlider().isMouseOverOrDragging())
            widthSlider->setValue(params.stereoWidth, juce::dontSendNotification);
        if (!lowCutSlider->getSlider().isMouseOverOrDragging())
            lowCutSlider->setValue(params.lowCutHz, juce::dontSendNotification);
        if (!highCutSlider->getSlider().isMouseOverOrDragging())
            highCutSlider->setValue(params.highCutHz, juce::dontSendNotification);
        
        bool shouldBeOn = !delayProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void updateDelay() {
        DelayProcessor::Params p;
        p.delayMs = delayTimeSlider->getValue();
        p.ratio = ratioSlider->getValue();
        p.stage = stageSlider->getValue();
        p.mix = mixSlider->getValue();
        p.stereoWidth = widthSlider->getValue();
        p.lowCutHz = lowCutSlider->getValue();
        p.highCutHz = highCutSlider->getValue();
        delayProcessor.setParams(p);
    }
    
    DelayProcessor& delayProcessor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> delayTimeSlider, ratioSlider, stageSlider, mixSlider, widthSlider, lowCutSlider, highCutSlider;
    std::unique_ptr<DelayGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayPanel)
};
