#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

class DynamicEQPanel : public juce::Component, private juce::Timer {
public:
    DynamicEQPanel(AudioEngine& engine) : audioEngine(engine) {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>(); auto& d = audioEngine.getDynamicEQProcessor(); auto params = d.getParams();
        toggleButton = std::make_unique<EffectToggleButton>(); toggleButton->setMidiInfo("MIDI: Note 28");
        toggleButton->setToggleState(!d.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { audioEngine.getDynamicEQProcessor().setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());
        addAndMakeVisible(titleLabel); titleLabel.setText("Sidechain Compressor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold)); titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37)); titleLabel.setJustificationType(juce::Justification::centredLeft);
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, const juce::String& m, double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo(m);
            s->setRange(min, max, (max-min)/100.0); s->setValue(v); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get()); s->getSlider().onValueChange = [this]() { updateDynamicEQ(); };
            addAndMakeVisible(s.get());
        };
        cS(duckBandSlider, "Duck Band", "MIDI: CC 59", 100.0, 8000.0, params.duckBandHz, " Hz");
        cS(qSlider, "Q", "MIDI: CC 60", 0.1, 10.0, params.q, "");
        cS(shapeSlider, "Shape", "MIDI: CC 61", 0.0, 1.0, params.shape, "");
        cS(thresholdSlider, "Threshold", "MIDI: CC 62", -60.0, 0.0, params.threshold, " dB");
        cS(ratioSlider, "Ratio", "MIDI: CC 65", 1.0, 20.0, params.ratio, ":1");
        cS(attackSlider, "Attack", "MIDI: CC 66", 0.1, 100.0, params.attack, " ms");
        cS(releaseSlider, "Release", "MIDI: CC 67", 10.0, 1000.0, params.release, " ms");
        startTimerHz(15);
    }
    ~DynamicEQPanel() override { stopTimer(); duckBandSlider->getSlider().setLookAndFeel(nullptr); qSlider->getSlider().setLookAndFeel(nullptr); shapeSlider->getSlider().setLookAndFeel(nullptr); thresholdSlider->getSlider().setLookAndFeel(nullptr); ratioSlider->getSlider().setLookAndFeel(nullptr); attackSlider->getSlider().setLookAndFeel(nullptr); releaseSlider->getSlider().setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF1A1A1A)); g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2); g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10)); }
    void resized() override {
        auto area = getLocalBounds().reduced(15);
        auto topRow = area.removeFromTop(40);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        area.removeFromTop(10);
        int numSliders = 7; int sliderWidth = 60; int spacing = 20; 
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);
        duckBandSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        qSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        shapeSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        thresholdSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        attackSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        releaseSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    void updateFromPreset() { auto& d = audioEngine.getDynamicEQProcessor(); auto p = d.getParams(); toggleButton->setToggleState(!d.isBypassed(), juce::dontSendNotification); duckBandSlider->setValue(p.duckBandHz, juce::dontSendNotification); qSlider->setValue(p.q, juce::dontSendNotification); shapeSlider->setValue(p.shape, juce::dontSendNotification); thresholdSlider->setValue(p.threshold, juce::dontSendNotification); ratioSlider->setValue(p.ratio, juce::dontSendNotification); attackSlider->setValue(p.attack, juce::dontSendNotification); releaseSlider->setValue(p.release, juce::dontSendNotification); }
private:
    void timerCallback() override { auto p = audioEngine.getDynamicEQProcessor().getParams(); if (!duckBandSlider->getSlider().isMouseOverOrDragging()) duckBandSlider->setValue(p.duckBandHz, juce::dontSendNotification); if (!qSlider->getSlider().isMouseOverOrDragging()) qSlider->setValue(p.q, juce::dontSendNotification); if (!shapeSlider->getSlider().isMouseOverOrDragging()) shapeSlider->setValue(p.shape, juce::dontSendNotification); if (!thresholdSlider->getSlider().isMouseOverOrDragging()) thresholdSlider->setValue(p.threshold, juce::dontSendNotification); if (!ratioSlider->getSlider().isMouseOverOrDragging()) ratioSlider->setValue(p.ratio, juce::dontSendNotification); if (!attackSlider->getSlider().isMouseOverOrDragging()) attackSlider->setValue(p.attack, juce::dontSendNotification); if (!releaseSlider->getSlider().isMouseOverOrDragging()) releaseSlider->setValue(p.release, juce::dontSendNotification); bool shouldBeOn = !audioEngine.getDynamicEQProcessor().isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification); }
    AudioEngine& audioEngine; std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel; std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel; std::unique_ptr<VerticalSlider> duckBandSlider, qSlider, shapeSlider, thresholdSlider, ratioSlider, attackSlider, releaseSlider;
    void updateDynamicEQ() { DynamicEQProcessor::Params p; p.duckBandHz = duckBandSlider->getValue(); p.q = qSlider->getValue(); p.shape = shapeSlider->getValue(); p.threshold = thresholdSlider->getValue(); p.ratio = ratioSlider->getValue(); p.attack = attackSlider->getValue(); p.release = releaseSlider->getValue(); audioEngine.getDynamicEQProcessor().setParams(p); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQPanel)
};