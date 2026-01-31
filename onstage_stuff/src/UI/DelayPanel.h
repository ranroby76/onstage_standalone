#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/DelayProcessor.h"

class DelayPanel : public juce::Component, private juce::Timer {
public:
    DelayPanel(DelayProcessor& processor) : delayProcessor(processor) {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>(); auto params = delayProcessor.getParams();
        toggleButton = std::make_unique<EffectToggleButton>(); toggleButton->setMidiInfo("MIDI: Note 27");
        toggleButton->setToggleState(!delayProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { delayProcessor.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());
        addAndMakeVisible(titleLabel); titleLabel.setText("Stereo Delay", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold)); titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37)); titleLabel.setJustificationType(juce::Justification::centredLeft);
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, const juce::String& m, double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo(m);
            s->setRange(min, max, (max-min)/200.0); s->setValue(v); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get()); s->getSlider().onValueChange = [this]() { updateDelay(); };
            addAndMakeVisible(s.get());
        };
        cS(delayTimeSlider, "Time", "MIDI: CC 47", 1.0, 2000.0, params.delayMs, " ms");
        cS(ratioSlider, "Ratio", "MIDI: CC 48", 0.0, 1.0, params.ratio, "");
        cS(stageSlider, "Stage", "MIDI: CC 49", 0.0, 1.0, params.stage, "");
        cS(mixSlider, "Mix", "MIDI: CC 29", 0.0, 1.0, params.mix, "");
        cS(widthSlider, "Width", "MIDI: CC 50", 0.0, 2.0, params.stereoWidth, "");
        cS(lowCutSlider, "LowCut", "MIDI: CC 51", 20.0, 2000.0, params.lowCutHz, " Hz");
        cS(highCutSlider, "HighCut", "MIDI: CC 52", 2000.0, 20000.0, params.highCutHz, " Hz");
        startTimerHz(15);
    }
    ~DelayPanel() override { stopTimer(); delayTimeSlider->getSlider().setLookAndFeel(nullptr); ratioSlider->getSlider().setLookAndFeel(nullptr); stageSlider->getSlider().setLookAndFeel(nullptr); mixSlider->getSlider().setLookAndFeel(nullptr); widthSlider->getSlider().setLookAndFeel(nullptr); lowCutSlider->getSlider().setLookAndFeel(nullptr); highCutSlider->getSlider().setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF1A1A1A)); g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2); g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10)); }
    void resized() override {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(40);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        area.removeFromTop(10);
        int numSliders = 7; int sliderWidth = 60; int spacing = 20; 
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);
        delayTimeSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        stageSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        mixSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        widthSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        lowCutSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        highCutSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    void updateFromPreset() { auto params = delayProcessor.getParams(); toggleButton->setToggleState(!delayProcessor.isBypassed(), juce::dontSendNotification); delayTimeSlider->setValue(params.delayMs, juce::dontSendNotification); ratioSlider->setValue(params.ratio, juce::dontSendNotification); stageSlider->setValue(params.stage, juce::dontSendNotification); mixSlider->setValue(params.mix, juce::dontSendNotification); widthSlider->setValue(params.stereoWidth, juce::dontSendNotification); lowCutSlider->setValue(params.lowCutHz, juce::dontSendNotification); highCutSlider->setValue(params.highCutHz, juce::dontSendNotification); }
private:
    void timerCallback() override { auto params = delayProcessor.getParams(); if (!delayTimeSlider->getSlider().isMouseOverOrDragging()) delayTimeSlider->setValue(params.delayMs, juce::dontSendNotification); if (!ratioSlider->getSlider().isMouseOverOrDragging()) ratioSlider->setValue(params.ratio, juce::dontSendNotification); if (!stageSlider->getSlider().isMouseOverOrDragging()) stageSlider->setValue(params.stage, juce::dontSendNotification); if (!mixSlider->getSlider().isMouseOverOrDragging()) mixSlider->setValue(params.mix, juce::dontSendNotification); if (!widthSlider->getSlider().isMouseOverOrDragging()) widthSlider->setValue(params.stereoWidth, juce::dontSendNotification); if (!lowCutSlider->getSlider().isMouseOverOrDragging()) lowCutSlider->setValue(params.lowCutHz, juce::dontSendNotification); if (!highCutSlider->getSlider().isMouseOverOrDragging()) highCutSlider->setValue(params.highCutHz, juce::dontSendNotification); bool shouldBeOn = !delayProcessor.isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification); }
    DelayProcessor& delayProcessor; std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel; std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel; std::unique_ptr<VerticalSlider> delayTimeSlider, ratioSlider, stageSlider, mixSlider, widthSlider, lowCutSlider, highCutSlider;
    void updateDelay() { DelayProcessor::Params p; p.delayMs = delayTimeSlider->getValue(); p.ratio = ratioSlider->getValue(); p.stage = stageSlider->getValue(); p.mix = mixSlider->getValue(); p.stereoWidth = widthSlider->getValue(); p.lowCutHz = lowCutSlider->getValue(); p.highCutHz = highCutSlider->getValue(); delayProcessor.setParams(p); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayPanel)
};