#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

class ExciterPanel : public juce::Component, private juce::Timer {
public:
    ExciterPanel(AudioEngine& engine, int micIndex, const juce::String& micName) : audioEngine(engine), micIdx(micIndex) {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& e = audioEngine.getExciterProcessor(micIdx); auto params = e.getParams();
        toggleButton = std::make_unique<EffectToggleButton>();
        int note = (micIdx == 0) ? 17 : 20; toggleButton->setMidiInfo("MIDI: Note " + juce::String(note));
        toggleButton->setToggleState(!e.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { audioEngine.getExciterProcessor(micIdx).setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());
        addAndMakeVisible(titleLabel);
        titleLabel.setText(micName + " - Exciter", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(min, max, (max-min)/100.0); s->setValue(v); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateExciter(); };
            addAndMakeVisible(s.get());
        };
        int baseFreqCC = (micIdx == 0) ? 53 : 63; int baseDriveCC = (micIdx == 0) ? 54 : 64; int baseMixCC = (micIdx == 0) ? 39 : 40;
        cS(freqSlider, "Freq", baseFreqCC, 1000.0, 10000.0, params.frequency, " Hz");
        cS(amountSlider, "Drive", baseDriveCC, 0.0, 24.0, params.amount, " dB");
        cS(mixSlider, "Mix", baseMixCC, 0.0, 1.0, params.mix, "");
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
        int numSliders = 3; int sliderWidth = 60; int spacing = 40; // Increased spacing for sparse panel
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);
        freqSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        amountSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        mixSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    void updateFromPreset() { auto& e = audioEngine.getExciterProcessor(micIdx); auto p = e.getParams(); freqSlider->setValue(p.frequency, juce::dontSendNotification); amountSlider->setValue(p.amount, juce::dontSendNotification); mixSlider->setValue(p.mix, juce::dontSendNotification); toggleButton->setToggleState(!e.isBypassed(), juce::dontSendNotification); }
private:
    void timerCallback() override { auto p = audioEngine.getExciterProcessor(micIdx).getParams(); if (!freqSlider->getSlider().isMouseOverOrDragging()) freqSlider->setValue(p.frequency, juce::dontSendNotification); if (!amountSlider->getSlider().isMouseOverOrDragging()) amountSlider->setValue(p.amount, juce::dontSendNotification); if (!mixSlider->getSlider().isMouseOverOrDragging()) mixSlider->setValue(p.mix, juce::dontSendNotification); bool shouldBeOn = !audioEngine.getExciterProcessor(micIdx).isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification); }
    AudioEngine& audioEngine; int micIdx;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel; std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel; std::unique_ptr<VerticalSlider> freqSlider, amountSlider, mixSlider;
    void updateExciter() { ExciterProcessor::Params p; p.frequency = freqSlider->getValue(); p.amount = amountSlider->getValue(); p.mix = mixSlider->getValue(); audioEngine.getExciterProcessor(micIdx).setParams(p); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExciterPanel)
};