#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/ExciterProcessor.h"

class PresetManager;

class ExciterPanel : public juce::Component, private juce::Timer {
public:
    ExciterPanel(ExciterProcessor& proc, PresetManager& /*presets*/) : exciter(proc) {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = exciter.getParams();
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!exciter.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { exciter.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Exciter", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n);
            s->setRange(min, max, (max-min)/100.0); s->setValue(v); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateExciter(); };
            addAndMakeVisible(s.get());
        };
        cS(freqSlider, "Freq", 1000.0, 10000.0, params.frequency, " Hz");
        cS(amountSlider, "Drive", 0.0, 24.0, params.amount, " dB");
        cS(mixSlider, "Mix", 0.0, 1.0, params.mix, "");
        startTimerHz(15);
    }
    ~ExciterPanel() override { stopTimer(); freqSlider->getSlider().setLookAndFeel(nullptr); amountSlider->getSlider().setLookAndFeel(nullptr); mixSlider->getSlider().setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF1A1A1A)); g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2); g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10)); }
    void resized() override {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(40);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        area.removeFromTop(10);
        int numSliders = 3; int sliderWidth = 60; int spacing = 40;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);
        freqSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        amountSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        mixSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    void updateFromPreset() { auto p = exciter.getParams(); freqSlider->setValue(p.frequency, juce::dontSendNotification); amountSlider->setValue(p.amount, juce::dontSendNotification); mixSlider->setValue(p.mix, juce::dontSendNotification); toggleButton->setToggleState(!exciter.isBypassed(), juce::dontSendNotification); }
private:
    void timerCallback() override { auto p = exciter.getParams(); if (!freqSlider->getSlider().isMouseOverOrDragging()) freqSlider->setValue(p.frequency, juce::dontSendNotification); if (!amountSlider->getSlider().isMouseOverOrDragging()) amountSlider->setValue(p.amount, juce::dontSendNotification); if (!mixSlider->getSlider().isMouseOverOrDragging()) mixSlider->setValue(p.mix, juce::dontSendNotification); bool shouldBeOn = !exciter.isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification); }
    ExciterProcessor& exciter;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel; std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel; std::unique_ptr<VerticalSlider> freqSlider, amountSlider, mixSlider;
    void updateExciter() { ExciterProcessor::Params p; p.frequency = freqSlider->getValue(); p.amount = amountSlider->getValue(); p.mix = mixSlider->getValue(); exciter.setParams(p); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExciterPanel)
};