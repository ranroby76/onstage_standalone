#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

class ReverbPanel : public juce::Component, private juce::Timer {
public:
    ReverbPanel(AudioEngine& engine) : audioEngine(engine) {
        lastIrDirectory = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>(); auto& r = audioEngine.getReverbProcessor(); auto params = r.getParams();
        toggleButton = std::make_unique<EffectToggleButton>(); toggleButton->setMidiInfo("MIDI: Note 26"); 
        toggleButton->setToggleState(!r.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { audioEngine.getReverbProcessor().setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());
        addAndMakeVisible(titleLabel); titleLabel.setText("Convolution Reverb", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold)); titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37)); titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(loadButton); loadButton.setButtonText("Load IR File"); loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF404040)); loadButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFD4AF37)); loadButton.onClick = [this]() { openIrFile(); };
        addAndMakeVisible(irNameLabel); irNameLabel.setText(r.getCurrentIrName(), juce::dontSendNotification); irNameLabel.setJustificationType(juce::Justification::centred); irNameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey); irNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF202020)); irNameLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xFF404040));
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(min, max, (max-min)/100.0); s->setValue(v); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get()); s->getSlider().onValueChange = [this]() { updateReverb(); };
            addAndMakeVisible(s.get());
        };
        cS(wetSlider, "Wet Level", 28, 0.0, 10.0, params.wetGain, "");
        cS(lowCutSlider, "Low Cut", 37, 20.0, 1000.0, params.lowCutHz, " Hz");
        cS(highCutSlider, "High Cut", 38, 1000.0, 20000.0, params.highCutHz, " Hz");
        startTimerHz(15);
    }
    ~ReverbPanel() override { stopTimer(); wetSlider->getSlider().setLookAndFeel(nullptr); lowCutSlider->getSlider().setLookAndFeel(nullptr); highCutSlider->getSlider().setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF1A1A1A)); g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2); g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10)); }
    void resized() override {
        auto area = getLocalBounds().reduced(15);
        auto topRow = area.removeFromTop(40);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        area.removeFromTop(10);
        auto irArea = area.removeFromLeft(140);
        loadButton.setBounds(irArea.removeFromTop(30).reduced(5)); irArea.removeFromTop(5); irNameLabel.setBounds(irArea.removeFromTop(30).reduced(5));
        int numSliders = 3; int sliderWidth = 60; int spacing = 40;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);
        wetSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        lowCutSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        highCutSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    void updateFromPreset() { auto& r = audioEngine.getReverbProcessor(); auto p = r.getParams(); toggleButton->setToggleState(!r.isBypassed(), juce::dontSendNotification); wetSlider->setValue(p.wetGain, juce::dontSendNotification); lowCutSlider->setValue(p.lowCutHz, juce::dontSendNotification); highCutSlider->setValue(p.highCutHz, juce::dontSendNotification); irNameLabel.setText(r.getCurrentIrName(), juce::dontSendNotification); }
private:
    void timerCallback() override { auto p = audioEngine.getReverbProcessor().getParams(); if (!wetSlider->getSlider().isMouseOverOrDragging()) wetSlider->setValue(p.wetGain, juce::dontSendNotification); if (!lowCutSlider->getSlider().isMouseOverOrDragging()) lowCutSlider->setValue(p.lowCutHz, juce::dontSendNotification); if (!highCutSlider->getSlider().isMouseOverOrDragging()) highCutSlider->setValue(p.highCutHz, juce::dontSendNotification); bool shouldBeOn = !audioEngine.getReverbProcessor().isBypassed(); if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification); }
    void updateReverb() { ReverbProcessor::Params p = audioEngine.getReverbProcessor().getParams(); p.wetGain = wetSlider->getValue(); p.lowCutHz = lowCutSlider->getValue(); p.highCutHz = highCutSlider->getValue(); audioEngine.getReverbProcessor().setParams(p); }
    void openIrFile() { auto chooser = std::make_shared<juce::FileChooser>("Load Impulse Response", lastIrDirectory, "*.wav;*.aiff;*.flac"); chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser& fc) { auto file = fc.getResult(); if (file.existsAsFile()) { lastIrDirectory = file.getParentDirectory(); auto p = audioEngine.getReverbProcessor().getParams(); p.irFilePath = file.getFullPathName(); audioEngine.getReverbProcessor().setParams(p); irNameLabel.setText(file.getFileNameWithoutExtension(), juce::dontSendNotification); } }); }
    AudioEngine& audioEngine; std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel; std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel; juce::TextButton loadButton; juce::Label irNameLabel; juce::File lastIrDirectory; std::unique_ptr<VerticalSlider> wetSlider, lowCutSlider, highCutSlider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbPanel)
};