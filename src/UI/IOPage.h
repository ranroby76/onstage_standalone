#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"
#include "SignalLed.h"
#include "StyledSlider.h"

class SimpleContainer : public juce::Component {
public:
    SimpleContainer() { setOpaque(true); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF202020)); }
};

class OutputItemComponent : public juce::Component {
public:
    OutputItemComponent(const juce::String& name, int index) : itemIndex(index) {
        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLabel);
        checkL.setButtonText("L"); checkL.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFD4AF37));
        checkR.setButtonText("R"); checkR.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFD4AF37));
        addAndMakeVisible(checkL); addAndMakeVisible(checkR); addAndMakeVisible(signalLed);
    }
    void resized() override {
        auto area = getLocalBounds();
        auto rightSide = area.removeFromRight(100);
        signalLed.setBounds(rightSide.removeFromRight(20).reduced(3));
        checkR.setBounds(rightSide.removeFromRight(40));
        checkL.setBounds(rightSide.removeFromRight(40));
        nameLabel.setBounds(area.reduced(5, 0));
    }
    void paint(juce::Graphics& g) override { g.setColour(juce::Colours::black); g.fillRect(0, getHeight()-1, getWidth(), 1); }
    juce::Label nameLabel; juce::ToggleButton checkL, checkR; SignalLed signalLed; int itemIndex;
};

class InputItemComponent : public juce::Component {
public:
    InputItemComponent(const juce::String& name, int index) : itemIndex(index) {
        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLabel);
        auto cfg = [&](juce::ToggleButton& b) { b.setButtonText(""); b.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFD4AF37)); addAndMakeVisible(b); };
        cfg(checkV1); cfg(checkV2); cfg(checkPBL); cfg(checkPBR);
        gainSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        gainSlider->setRange(0.0, 1.0, 0.001); addAndMakeVisible(gainSlider.get());
        addAndMakeVisible(signalLed);
    }
    void resized() override {
        auto area = getLocalBounds();
        int h = area.getHeight();
        int x = area.getWidth();
        x -= 30; signalLed.setBounds(x + 5, (h-20)/2, 20, 20);
        x -= 10; int sliderW = 100; x -= sliderW; gainSlider->setBounds(x, 0, sliderW, h);
        x -= 25; int colW = 90;
        x -= colW; checkPBR.setBounds(x + (colW-24)/2, 0, 24, h);
        x -= colW; checkPBL.setBounds(x + (colW-24)/2, 0, 24, h);
        x -= colW; checkV2.setBounds(x + (colW-24)/2, 0, 24, h);
        x -= colW; checkV1.setBounds(x + (colW-24)/2, 0, 24, h);
        nameLabel.setBounds(10, 0, x - 10, h);
    }
    void paint(juce::Graphics& g) override { g.setColour(juce::Colours::black); g.fillRect(0, getHeight()-1, getWidth(), 1); }
    juce::Label nameLabel; juce::ToggleButton checkV1, checkV2, checkPBL, checkPBR;
    std::unique_ptr<StyledSlider> gainSlider; SignalLed signalLed; int itemIndex;
};

class IOPage : public juce::Component, private juce::Timer {
public:
    IOPage(AudioEngine& engine, IOSettingsManager& settings);
    ~IOPage() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void restoreSavedSettings();
private:
    void timerCallback() override;
    void onSpecificDriverChanged();
    void updateInputList(); void updateOutputList(); void updateMidiDevices();
    static float sliderValueToGain(float v); static float gainToSliderValue(float g);
    AudioEngine& audioEngine; IOSettingsManager& ioSettingsManager;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    juce::Label asioLabel, driverLabel, deviceInfoLabel, logicLabel, settingsLabel, midiLabel, inputsLabel, outputsLabel;
    juce::Label latencyLabel, vocalBoostLabel;
    juce::ComboBox specificDriverSelector, midiInputSelector;
    juce::TextButton controlPanelButton, midiRefreshButton;
    MidiTooltipToggleButton mic1Mute, mic1Bypass, mic2Mute, mic2Bypass;
    std::unique_ptr<StyledSlider> latencySlider, vocalBoostSlider;
    juce::Label headMic1, headMic2, headPbL, headPbR, headGain;
    juce::Viewport inputsViewport, outputsViewport;
    SimpleContainer inputsContainer, outputsContainer;
    juce::OwnedArray<InputItemComponent> inputItems;
    juce::OwnedArray<OutputItemComponent> outputItems;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOPage)
};