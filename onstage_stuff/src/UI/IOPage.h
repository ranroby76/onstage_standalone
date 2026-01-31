#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"
#include "SignalLed.h"
#include "StyledSlider.h"

// Simple solid container
class SimpleContainer : public juce::Component {
public:
    SimpleContainer() { setOpaque(true); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF202020)); }
};

// Output Item (Matrix Output)
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
    juce::Label nameLabel;
    juce::ToggleButton checkL, checkR;
    SignalLed signalLed;
    int itemIndex;
};

// Input Item (Matrix Input) - FIXED LAYOUT
class InputItemComponent : public juce::Component {
public:
    InputItemComponent(const juce::String& name, int index) : itemIndex(index) {
        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLabel);
        
        auto cfgToggle = [&](juce::ToggleButton& b) {
            b.setButtonText(""); 
            b.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFD4AF37));
            addAndMakeVisible(b);
        };
        cfgToggle(checkV1);
        cfgToggle(checkV2);
        cfgToggle(checkPBL);
        cfgToggle(checkPBR);
        
        gainSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        gainSlider->setRange(0.0, 2.0, 0.01);
        gainSlider->setValue(1.0);
        gainSlider->setTooltip("Input Gain");
        addAndMakeVisible(gainSlider.get());
        
        addAndMakeVisible(signalLed);
    }
    
    void resized() override {
        auto area = getLocalBounds();
        int h = area.getHeight();
        int x = area.getWidth(); // Start from right edge
        
        // 1. LED (Rightmost)
        x -= 30; // Allocate 30px
        signalLed.setBounds(x + 5, (h-20)/2, 20, 20);
        
        x -= 10; // Gap
        
        // 2. Slider
        int sliderW = 100; // Wider slider
        x -= sliderW;
        gainSlider->setBounds(x, 0, sliderW, h);
        
        x -= 25; // Larger Gap between slider and checkboxes
        
        // 3. Matrix Checkboxes
        int colW = 90; // Much wider columns (was 60)
        
        // Playback R
        x -= colW;
        checkPBR.setBounds(x + (colW-24)/2, 0, 24, h); 
        
        // Playback L
        x -= colW;
        checkPBL.setBounds(x + (colW-24)/2, 0, 24, h);
        
        // Mic 2
        x -= colW;
        checkV2.setBounds(x + (colW-24)/2, 0, 24, h);
        
        // Mic 1
        x -= colW;
        checkV1.setBounds(x + (colW-24)/2, 0, 24, h);
        
        // Name Label (Takes remaining space)
        nameLabel.setBounds(10, 0, x - 10, h);
    }
    
    void paint(juce::Graphics& g) override { g.setColour(juce::Colours::black); g.fillRect(0, getHeight()-1, getWidth(), 1); }
    
    juce::Label nameLabel;
    juce::ToggleButton checkV1, checkV2, checkPBL, checkPBR;
    std::unique_ptr<StyledSlider> gainSlider;
    SignalLed signalLed;
    int itemIndex;
};

// --- MAIN PAGE ---
class IOPage : public juce::Component, private juce::Timer {
public: 
    IOPage(AudioEngine& engine, IOSettingsManager& ioSettings);
    ~IOPage() override;
    void paint(juce::Graphics& g) override; 
    void resized() override; 
    void restoreSavedSettings();

private: 
    void timerCallback() override;
    AudioEngine& audioEngine; 
    IOSettingsManager& ioSettingsManager;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    
    // -- LEFT SIDE (1/3) --
    juce::Label asioLabel;
    juce::Label driverLabel;
    juce::ComboBox specificDriverSelector;
    juce::TextButton controlPanelButton;
    juce::Label deviceInfoLabel;
    
    juce::Label logicLabel;
    MidiTooltipToggleButton mic1Mute, mic1Bypass;
    MidiTooltipToggleButton mic2Mute, mic2Bypass;
    
    juce::Label settingsLabel;
    MidiTooltipLabel latencyLabel, vocalBoostLabel;
    std::unique_ptr<StyledSlider> latencySlider, vocalBoostSlider;
    
    juce::Label midiLabel;
    juce::ComboBox midiInputSelector;
    juce::TextButton midiRefreshButton;

    // -- MIDDLE (50%) --
    juce::Label inputsLabel;
    // Matrix Headers
    juce::Label headMic1, headMic2, headPbL, headPbR, headGain; 
    
    juce::Viewport inputsViewport;
    SimpleContainer inputsContainer;
    juce::OwnedArray<InputItemComponent> inputItems;
    
    // -- RIGHT SIDE (25%) --
    juce::Label outputsLabel;
    juce::Viewport outputsViewport;
    SimpleContainer outputsContainer;
    juce::OwnedArray<OutputItemComponent> outputItems;

    void onSpecificDriverChanged(); 
    void updateInputList(); 
    void updateOutputList(); 
    void updateMidiDevices();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOPage)
};