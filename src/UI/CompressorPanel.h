#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

class CompressorPanel : public juce::Component, private juce::Timer {
public:
    CompressorPanel(AudioEngine& engine, int micIndex, const juce::String& micName)
        : audioEngine(engine), micIdx(micIndex)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& c = audioEngine.getCompressorProcessor(micIdx); auto params = c.getParams();

        toggleButton = std::make_unique<EffectToggleButton>();
        int note = (micIdx == 0) ? 19 : 22; toggleButton->setMidiInfo("MIDI: Note " + juce::String(note));
        toggleButton->setToggleState(!c.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { audioEngine.getCompressorProcessor(micIdx).setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());
        addAndMakeVisible(titleLabel);
        titleLabel.setText(micName + " - Compressor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double min, double max, double val, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(min, max, (max-min)/200.0); s->setValue(val); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateCompressor(); };
            addAndMakeVisible(s.get());
        };
        int base = (micIdx == 0) ? 32 : 42;
        cS(thresholdSlider, "Threshold", base, -60.0, 0.0, params.thresholdDb, " dB");
        cS(ratioSlider, "Ratio", base+1, 1.0, 20.0, params.ratio, ":1");
        cS(attackSlider, "Attack", base+2, 0.1, 100.0, params.attackMs, " ms"); attackSlider->setSkewFactor(10.0);
        cS(releaseSlider, "Release", base+3, 10.0, 1000.0, params.releaseMs, " ms"); releaseSlider->setSkewFactor(100.0);
        cS(makeupSlider, "Makeup", base+4, 0.0, 24.0, params.makeupDb, " dB");

        startTimerHz(15);
    }
    ~CompressorPanel() override { stopTimer(); thresholdSlider->getSlider().setLookAndFeel(nullptr); ratioSlider->getSlider().setLookAndFeel(nullptr); attackSlider->getSlider().setLookAndFeel(nullptr); releaseSlider->getSlider().setLookAndFeel(nullptr); makeupSlider->getSlider().setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF1A1A1A)); g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2); g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10)); }
    
    void resized() override {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(40);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        area.removeFromTop(10);

        int numSliders = 5; int sliderWidth = 60; int spacing = 20;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);
        
        thresholdSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        ratioSlider->setBounds(sArea.removeFromLeft(sliderWidth));     sArea.removeFromLeft(spacing);
        attackSlider->setBounds(sArea.removeFromLeft(sliderWidth));    sArea.removeFromLeft(spacing);
        releaseSlider->setBounds(sArea.removeFromLeft(sliderWidth));   sArea.removeFromLeft(spacing);
        makeupSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    void updateFromPreset() { auto& c = audioEngine.getCompressorProcessor(micIdx); auto p = c.getParams(); thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification); ratioSlider->setValue(p.ratio, juce::dontSendNotification); attackSlider->setValue(p.attackMs, juce::dontSendNotification); releaseSlider->setValue(p.releaseMs, juce::dontSendNotification); makeupSlider->setValue(p.makeupDb, juce::dontSendNotification); toggleButton->setToggleState(!c.isBypassed(), juce::dontSendNotification); repaint(); }
private:
    void timerCallback() override { auto& c = audioEngine.getCompressorProcessor(micIdx); auto p = c.getParams();
        if (!thresholdSlider->getSlider().isMouseOverOrDragging()) thresholdSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        if (!ratioSlider->getSlider().isMouseOverOrDragging()) ratioSlider->setValue(p.ratio, juce::dontSendNotification);
        if (!attackSlider->getSlider().isMouseOverOrDragging()) attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        if (!releaseSlider->getSlider().isMouseOverOrDragging()) releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
        if (!makeupSlider->getSlider().isMouseOverOrDragging()) makeupSlider->setValue(p.makeupDb, juce::dontSendNotification);
        bool shouldBeOn = !c.isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    AudioEngine& audioEngine; int micIdx;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel; std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> thresholdSlider, ratioSlider, attackSlider, releaseSlider, makeupSlider;
    void updateCompressor() { CompressorProcessor::Params p; p.thresholdDb = thresholdSlider->getValue(); p.ratio = ratioSlider->getValue(); p.attackMs = attackSlider->getValue(); p.releaseMs = releaseSlider->getValue(); p.makeupDb = makeupSlider->getValue(); audioEngine.getCompressorProcessor(micIdx).setParams(p); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPanel)
};