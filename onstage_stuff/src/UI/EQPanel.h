#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "DualHandleSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/EQProcessor.h"

class EQPanel : public juce::Component, private juce::Timer {
public:
    EQPanel(EQProcessor& processor, int micIdx, const juce::String& micName);
    ~EQPanel() override;
    void paint(juce::Graphics& g) override; void resized() override; void updateFromPreset();
private:
    void timerCallback() override;
    EQProcessor& eqProcessor; int micIndex; std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton; std::unique_ptr<VerticalSlider> lowGainSlider, midGainSlider, highGainSlider, lowQSlider, midQSlider, highQSlider;
    std::unique_ptr<DualHandleSlider> frequencySelector; juce::Label titleLabel, lowLabel, midLabel, highLabel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPanel)
};

inline EQPanel::EQPanel(EQProcessor& processor, int micIdx, const juce::String& micName) : eqProcessor(processor), micIndex(micIdx) {
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    toggleButton = std::make_unique<EffectToggleButton>(); int note = (micIndex == 0) ? 18 : 21; toggleButton->setMidiInfo("MIDI: Note " + juce::String(note));
    addAndMakeVisible(toggleButton.get()); toggleButton->setToggleState(!eqProcessor.isBypassed(), juce::dontSendNotification);
    toggleButton->onClick = [this]() { eqProcessor.setBypassed(!toggleButton->getToggleState()); };
    int baseCC = (micIndex == 0) ? 70 : 80;
    auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double min, double max, double v, const juce::String& suf) {
        s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo("MIDI: CC " + juce::String(cc));
        s->setRange(min, max, (max-min)/200.0); s->setValue(v); s->setTextValueSuffix(suf); s->getSlider().setLookAndFeel(goldenLookAndFeel.get()); addAndMakeVisible(s.get());
    };
    cS(lowGainSlider, "Low Gain", baseCC, -12.0, 12.0, eqProcessor.getLowGain(), " dB"); lowGainSlider->getSlider().onValueChange = [this]() { eqProcessor.setLowGain(lowGainSlider->getValue()); };
    cS(lowQSlider, "Low Q", baseCC+1, 0.1, 10.0, eqProcessor.getLowQ(), ""); lowQSlider->getSlider().onValueChange = [this]() { eqProcessor.setLowQ(lowQSlider->getValue()); };
    cS(midGainSlider, "Mid Gain", baseCC+2, -12.0, 12.0, eqProcessor.getMidGain(), " dB"); midGainSlider->getSlider().onValueChange = [this]() { eqProcessor.setMidGain(midGainSlider->getValue()); };
    cS(midQSlider, "Mid Q", baseCC+3, 0.1, 10.0, eqProcessor.getMidQ(), ""); midQSlider->getSlider().onValueChange = [this]() { eqProcessor.setMidQ(midQSlider->getValue()); };
    cS(highGainSlider, "High Gain", baseCC+4, -12.0, 12.0, eqProcessor.getHighGain(), " dB"); highGainSlider->getSlider().onValueChange = [this]() { eqProcessor.setHighGain(highGainSlider->getValue()); };
    cS(highQSlider, "High Q", baseCC+5, 0.1, 10.0, eqProcessor.getHighQ(), ""); highQSlider->getSlider().onValueChange = [this]() { eqProcessor.setHighQ(highQSlider->getValue()); };
    frequencySelector = std::make_unique<DualHandleSlider>(); frequencySelector->setRange(20.0, 20000.0); frequencySelector->setLeftValue(eqProcessor.getLowFrequency()); frequencySelector->setRightValue(eqProcessor.getHighFrequency());
    addAndMakeVisible(frequencySelector.get()); frequencySelector->onLeftValueChange = [this]() { eqProcessor.setLowFrequency(frequencySelector->getLeftValue()); }; frequencySelector->onRightValueChange = [this]() { eqProcessor.setHighFrequency(frequencySelector->getRightValue()); };
    frequencySelector->setLeftMidiInfo("MIDI: CC " + juce::String((micIndex == 0) ? 68 : 78)); frequencySelector->setRightMidiInfo("MIDI: CC " + juce::String((micIndex == 0) ? 69 : 79));
    titleLabel.setText(micName + " - 3-Band EQ", juce::dontSendNotification); titleLabel.setFont(juce::Font(18.0f, juce::Font::bold)); addAndMakeVisible(titleLabel);
    auto setupL = [&](juce::Label& l, const juce::String& t) { l.setText(t, juce::dontSendNotification); l.setJustificationType(juce::Justification::centred); addAndMakeVisible(l); };
    setupL(lowLabel, "Low"); setupL(midLabel, "Mid"); setupL(highLabel, "High");
    startTimerHz(15);
}
inline EQPanel::~EQPanel() { stopTimer(); lowGainSlider->getSlider().setLookAndFeel(nullptr); lowQSlider->getSlider().setLookAndFeel(nullptr); midGainSlider->getSlider().setLookAndFeel(nullptr); midQSlider->getSlider().setLookAndFeel(nullptr); highGainSlider->getSlider().setLookAndFeel(nullptr); highQSlider->getSlider().setLookAndFeel(nullptr); }
inline void EQPanel::timerCallback() {
    if (!lowGainSlider->getSlider().isMouseOverOrDragging()) lowGainSlider->setValue(eqProcessor.getLowGain(), juce::dontSendNotification);
    if (!lowQSlider->getSlider().isMouseOverOrDragging()) lowQSlider->setValue(eqProcessor.getLowQ(), juce::dontSendNotification);
    if (!midGainSlider->getSlider().isMouseOverOrDragging()) midGainSlider->setValue(eqProcessor.getMidGain(), juce::dontSendNotification);
    if (!midQSlider->getSlider().isMouseOverOrDragging()) midQSlider->setValue(eqProcessor.getMidQ(), juce::dontSendNotification);
    if (!highGainSlider->getSlider().isMouseOverOrDragging()) highGainSlider->setValue(eqProcessor.getHighGain(), juce::dontSendNotification);
    if (!highQSlider->getSlider().isMouseOverOrDragging()) highQSlider->setValue(eqProcessor.getHighQ(), juce::dontSendNotification);
    if (!frequencySelector->isUserDragging()) { frequencySelector->setLeftValue(eqProcessor.getLowFrequency()); frequencySelector->setRightValue(eqProcessor.getHighFrequency()); }
    bool shouldBeOn = !eqProcessor.isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
}
inline void EQPanel::updateFromPreset() {
    lowGainSlider->setValue(eqProcessor.getLowGain(), juce::dontSendNotification); lowQSlider->setValue(eqProcessor.getLowQ(), juce::dontSendNotification); midGainSlider->setValue(eqProcessor.getMidGain(), juce::dontSendNotification); midQSlider->setValue(eqProcessor.getMidQ(), juce::dontSendNotification); highGainSlider->setValue(eqProcessor.getHighGain(), juce::dontSendNotification); highQSlider->setValue(eqProcessor.getHighQ(), juce::dontSendNotification); frequencySelector->setLeftValue(eqProcessor.getLowFrequency()); frequencySelector->setRightValue(eqProcessor.getHighFrequency()); toggleButton->setToggleState(!eqProcessor.isBypassed(), juce::dontSendNotification);
}
inline void EQPanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xFF1A1A1A)); g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2); g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10)); }
inline void EQPanel::resized() {
    auto area = getLocalBounds().reduced(15);
    auto topArea = area.removeFromTop(40);
    toggleButton->setBounds(topArea.removeFromRight(40).withSizeKeepingCentre(40, 40));
    titleLabel.setBounds(topArea);
    area.removeFromTop(10);
    auto freqArea = area.removeFromTop(60); int fW = juce::jmin(600, freqArea.getWidth());
    frequencySelector->setBounds(freqArea.withWidth(fW).withX(freqArea.getX() + (freqArea.getWidth() - fW)/2));
    area.removeFromTop(20);
    int groupW = 140; int spacing = 40; int totalW = (groupW * 3) + (spacing * 2);
    int startX = area.getX() + (area.getWidth() - totalW) / 2;
    auto bandArea = area.withX(startX).withWidth(totalW);
    auto layoutGroup = [&](juce::Label& lbl, VerticalSlider& s1, VerticalSlider& s2) {
        auto gArea = bandArea.removeFromLeft(groupW); bandArea.removeFromLeft(spacing);
        lbl.setBounds(gArea.removeFromTop(20)); s1.setBounds(gArea.removeFromLeft(groupW/2).reduced(2)); s2.setBounds(gArea.reduced(2));
    };
    layoutGroup(lowLabel, *lowGainSlider, *lowQSlider); layoutGroup(midLabel, *midGainSlider, *midQSlider); layoutGroup(highLabel, *highGainSlider, *highQSlider);
}