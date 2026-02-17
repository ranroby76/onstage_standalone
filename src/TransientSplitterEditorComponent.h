
// #D:\Workspace\Subterraneum_plugins_daw\src\TransientSplitterEditorComponent.h
// TRANSIENT SPLITTER EDITOR - Popup for E button
// All parameters, meters, and mode toggles

#pragma once

#include <JuceHeader.h>
#include "TransientSplitterProcessor.h"

class TransientSplitterEditorComponent : public juce::Component, private juce::Timer {
public:
    TransientSplitterEditorComponent(TransientSplitterProcessor* proc)
        : processor(proc)
    {
        setSize(440, 560);
        
        // Title
        titleLabel.setText("Transient Splitter", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // === Detection Section ===
        addSection("DETECTION", detectionLabel);
        
        addSlider(sensitivitySlider, sensitivityLabel, "Sensitivity", 0.0, 1.0, 0.01, processor->sensitivity.load());
        sensitivitySlider.setTextValueSuffix("");
        sensitivitySlider.onValueChange = [this]() { processor->sensitivity.store((float)sensitivitySlider.getValue()); };
        
        addSlider(decaySlider, decayLabel, "Decay", 1.0, 500.0, 1.0, processor->decay.load());
        decaySlider.setTextValueSuffix(" ms");
        decaySlider.setSkewFactorFromMidPoint(50.0);
        decaySlider.onValueChange = [this]() { processor->decay.store((float)decaySlider.getValue()); };
        
        addSlider(holdSlider, holdLabel, "Hold Time", 0.0, 100.0, 0.5, processor->holdTime.load());
        holdSlider.setTextValueSuffix(" ms");
        holdSlider.onValueChange = [this]() { processor->holdTime.store((float)holdSlider.getValue()); };
        
        addSlider(smoothSlider, smoothLabel, "Smoothing", 0.1, 50.0, 0.1, processor->smoothing.load());
        smoothSlider.setTextValueSuffix(" ms");
        smoothSlider.setSkewFactorFromMidPoint(5.0);
        smoothSlider.onValueChange = [this]() { processor->smoothing.store((float)smoothSlider.getValue()); };
        
        // === Frequency Focus Section ===
        addSection("FREQUENCY FOCUS (Detection Only)", freqLabel);
        
        addSlider(hpSlider, hpLabel, "High-Pass", 20.0, 20000.0, 1.0, processor->focusHPFreq.load());
        hpSlider.setTextValueSuffix(" Hz");
        hpSlider.setSkewFactorFromMidPoint(500.0);
        hpSlider.onValueChange = [this]() { processor->focusHPFreq.store((float)hpSlider.getValue()); };
        
        addSlider(lpSlider, lpLabel, "Low-Pass", 20.0, 20000.0, 1.0, processor->focusLPFreq.load());
        lpSlider.setTextValueSuffix(" Hz");
        lpSlider.setSkewFactorFromMidPoint(2000.0);
        lpSlider.onValueChange = [this]() { processor->focusLPFreq.store((float)lpSlider.getValue()); };
        
        // === Output Section ===
        addSection("OUTPUT", outputLabel);
        
        addSlider(tGainSlider, tGainLabel, "Transient Gain", -60.0, 12.0, 0.1, processor->transientGainDb.load());
        tGainSlider.setTextValueSuffix(" dB");
        tGainSlider.onValueChange = [this]() { processor->transientGainDb.store((float)tGainSlider.getValue()); };
        
        addSlider(sGainSlider, sGainLabel, "Sustain Gain", -60.0, 12.0, 0.1, processor->sustainGainDb.load());
        sGainSlider.setTextValueSuffix(" dB");
        sGainSlider.onValueChange = [this]() { processor->sustainGainDb.store((float)sGainSlider.getValue()); };
        
        addSlider(balanceSlider, balanceLabel, "Balance", -1.0, 1.0, 0.01, processor->balance.load());
        balanceSlider.setTextValueSuffix("");
        balanceSlider.onValueChange = [this]() { processor->balance.store((float)balanceSlider.getValue()); };
        
        // === Mode Toggles ===
        addSection("MODE", modeLabel);
        
        stereoLinkBtn.setButtonText("Stereo Link");
        stereoLinkBtn.setClickingTogglesState(true);
        stereoLinkBtn.setToggleState(processor->stereoLinked.load(), juce::dontSendNotification);
        stereoLinkBtn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0, 180, 255));
        stereoLinkBtn.onClick = [this]() { processor->stereoLinked.store(stereoLinkBtn.getToggleState()); };
        addAndMakeVisible(stereoLinkBtn);
        
        gateModeBtn.setButtonText("Gate Mode (Hard Split)");
        gateModeBtn.setClickingTogglesState(true);
        gateModeBtn.setToggleState(processor->gateMode.load(), juce::dontSendNotification);
        gateModeBtn.setColour(juce::ToggleButton::tickColourId, juce::Colour(255, 180, 0));
        gateModeBtn.onClick = [this]() { processor->gateMode.store(gateModeBtn.getToggleState()); };
        addAndMakeVisible(gateModeBtn);
        
        invertModeBtn.setButtonText("Invert (Swap Transient/Sustain)");
        invertModeBtn.setClickingTogglesState(true);
        invertModeBtn.setToggleState(processor->invertMode.load(), juce::dontSendNotification);
        invertModeBtn.setColour(juce::ToggleButton::tickColourId, juce::Colour(255, 80, 80));
        invertModeBtn.onClick = [this]() { processor->invertMode.store(invertModeBtn.getToggleState()); };
        addAndMakeVisible(invertModeBtn);
        
        startTimerHz(20); // 20fps meter refresh
    }
    
    ~TransientSplitterEditorComponent() override { stopTimer(); }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        
        titleLabel.setBounds(area.removeFromTop(26));
        area.removeFromTop(6);
        
        // Meters at top
        auto meterArea = area.removeFromTop(50);
        // Drawn in paint()
        meterBounds = meterArea;
        area.removeFromTop(6);
        
        // Detection
        detectionLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        layoutSlider(area, sensitivitySlider, sensitivityLabel);
        layoutSlider(area, decaySlider, decayLabel);
        layoutSlider(area, holdSlider, holdLabel);
        layoutSlider(area, smoothSlider, smoothLabel);
        area.removeFromTop(4);
        
        // Frequency Focus
        freqLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        layoutSlider(area, hpSlider, hpLabel);
        layoutSlider(area, lpSlider, lpLabel);
        area.removeFromTop(4);
        
        // Output
        outputLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        layoutSlider(area, tGainSlider, tGainLabel);
        layoutSlider(area, sGainSlider, sGainLabel);
        layoutSlider(area, balanceSlider, balanceLabel);
        area.removeFromTop(4);
        
        // Mode toggles
        modeLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        stereoLinkBtn.setBounds(area.removeFromTop(22));
        gateModeBtn.setBounds(area.removeFromTop(22));
        invertModeBtn.setBounds(area.removeFromTop(22));
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(35, 35, 40));
        
        if (meterBounds.isEmpty()) return;
        
        auto area = meterBounds.toFloat();
        
        // Activity LED
        float ledSize = 10.0f;
        float ledX = area.getX();
        float ledY = area.getY() + 2.0f;
        float activity = processor->transientActivity.load();
        g.setColour(juce::Colour::fromHSV(0.0f, 1.0f - activity * 0.3f, 0.3f + activity * 0.7f, 1.0f));
        g.fillEllipse(ledX, ledY, ledSize, ledSize);
        g.setColour(juce::Colour(100, 100, 110));
        g.drawEllipse(ledX, ledY, ledSize, ledSize, 1.0f);
        
        g.setColour(juce::Colour(160, 160, 180));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("TRANSIENT", (int)(ledX + ledSize + 4), (int)(ledY - 1), 70, 14, juce::Justification::centredLeft);
        
        // Meter bars
        float meterX = area.getX();
        float meterW = area.getWidth();
        float barH = 12.0f;
        float barY = area.getY() + 18.0f;
        float halfW = (meterW - 10.0f) / 2.0f;
        
        // Transient meters (cyan)
        drawMeter(g, meterX, barY, halfW, barH, 
                  processor->transientRmsL.load(), juce::Colour(0, 200, 255), "T-L");
        drawMeter(g, meterX, barY + barH + 2, halfW, barH,
                  processor->transientRmsR.load(), juce::Colour(0, 200, 255), "T-R");
        
        // Sustain meters (amber)
        float rightX = meterX + halfW + 10.0f;
        drawMeter(g, rightX, barY, halfW, barH,
                  processor->sustainRmsL.load(), juce::Colour(255, 180, 50), "S-L");
        drawMeter(g, rightX, barY + barH + 2, halfW, barH,
                  processor->sustainRmsR.load(), juce::Colour(255, 180, 50), "S-R");
    }
    
    void timerCallback() override { repaint(meterBounds); }

private:
    TransientSplitterProcessor* processor;
    juce::Rectangle<int> meterBounds;
    
    juce::Label titleLabel;
    juce::Label detectionLabel, freqLabel, outputLabel, modeLabel;
    
    juce::Slider sensitivitySlider, decaySlider, holdSlider, smoothSlider;
    juce::Label sensitivityLabel, decayLabel, holdLabel, smoothLabel;
    
    juce::Slider hpSlider, lpSlider;
    juce::Label hpLabel, lpLabel;
    
    juce::Slider tGainSlider, sGainSlider, balanceSlider;
    juce::Label tGainLabel, sGainLabel, balanceLabel;
    
    juce::ToggleButton stereoLinkBtn, gateModeBtn, invertModeBtn;
    
    void addSection(const juce::String& text, juce::Label& label)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        label.setColour(juce::Label::textColourId, juce::Colour(120, 180, 255));
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    }
    
    void addSlider(juce::Slider& slider, juce::Label& label, const juce::String& name,
                   double min, double max, double step, double value)
    {
        label.setText(name, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(11.0f)));
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(label);
        
        slider.setRange(min, max, step);
        slider.setValue(value, juce::dontSendNotification);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(80, 80, 100));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(180, 180, 200));
        addAndMakeVisible(slider);
    }
    
    void layoutSlider(juce::Rectangle<int>& area, juce::Slider& slider, juce::Label& label)
    {
        auto row = area.removeFromTop(24);
        label.setBounds(row.removeFromLeft(120));
        slider.setBounds(row);
        area.removeFromTop(2);
    }
    
    void drawMeter(juce::Graphics& g, float x, float y, float w, float h,
                   float rms, juce::Colour colour, const juce::String& label)
    {
        // Background
        g.setColour(juce::Colour(25, 25, 30));
        g.fillRoundedRectangle(x, y, w, h, 2.0f);
        
        // Level bar (convert RMS to dB, map to 0-1)
        float db = juce::Decibels::gainToDecibels(rms, -60.0f);
        float normalized = juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        
        if (normalized > 0.001f) {
            g.setColour(colour.withAlpha(0.8f));
            g.fillRoundedRectangle(x + 1, y + 1, (w - 2) * normalized, h - 2, 1.5f);
        }
        
        // Label
        g.setColour(juce::Colour(180, 180, 190));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(label, (int)x + 3, (int)y, 24, (int)h, juce::Justification::centredLeft);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransientSplitterEditorComponent)
};


