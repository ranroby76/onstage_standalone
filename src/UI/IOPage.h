// **Changes:** Added `juce::TextButton midiRefreshButton;`.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"
#include "SignalLed.h"
#include "StyledSlider.h"

// 1. Solid Background Container
class SimpleContainer : public juce::Component
{
public:
    SimpleContainer() { setOpaque(true); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF202020)); }
};

// 2. Output Item Component with SEPARATOR
class OutputItemComponent : public juce::Component
{
public:
    OutputItemComponent(const juce::String& name, int index) : itemIndex(index)
    {
        toggleButton.setButtonText(name);
        toggleButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        toggleButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFD4AF37));
        addAndMakeVisible(toggleButton);

        addAndMakeVisible(ledL);
        addAndMakeVisible(ledR);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto rightSide = area.removeFromRight(40); 
        ledR.setBounds(rightSide.removeFromRight(15).reduced(1));
        rightSide.removeFromRight(5);
        ledL.setBounds(rightSide.removeFromRight(15).reduced(1));
        toggleButton.setBounds(area);
    }
    
    void paint(juce::Graphics& g) override {
        // FIX: Explicit BLACK separator line between items
        g.setColour(juce::Colours::black);
        g.fillRect(0, getHeight() - 1, getWidth(), 1);
    }
    
    juce::ToggleButton toggleButton;
    SignalLed ledL;
    SignalLed ledR;
    int itemIndex;
};

// --- MAIN CLASS ---
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
    
    juce::Viewport leftViewport; 
    SimpleContainer leftContainer; 
    
    juce::Viewport outputViewport; 
    SimpleContainer outputCheckboxContainer;
    
    // ASIO
    juce::Label asioDriverSectionLabel; 
    juce::Label specificDriverLabel; 
    juce::ComboBox specificDriverSelector;
    juce::TextButton controlPanelButton; 
    juce::Label deviceInfoLabel;
    
    // Mics
    juce::Label performersInputsSectionLabel; 
    juce::Label mic1Label, mic2Label;
    juce::ComboBox mic1InputSelector, mic2InputSelector;
    MidiTooltipToggleButton mic1MuteToggle, mic2MuteToggle; 
    MidiTooltipToggleButton mic1BypassToggle, mic2BypassToggle;
    SignalLed mic1Led, mic2Led;
    
    // Backing
    juce::Label backingTrackSectionLabel; 
    juce::Label mediaPlayerLabel;
    MidiTooltipToggleButton mediaPlayerToggle; 
    SignalLed mediaPlayerLed;
    juce::Label backingGainHeaderLabel;
    
    struct PlaybackInputPair { 
        juce::Label label, leftLabel, rightLabel; 
        juce::ComboBox leftSelector, rightSelector; 
        SignalLed leftLed, rightLed; 
        std::unique_ptr<StyledSlider> gainSlider; 
    };
    juce::OwnedArray<PlaybackInputPair> playbackInputPairs; 
    SimpleContainer inputsContainer; 
    
    // Settings
    juce::Label vocalSettingsSectionLabel; 
    MidiTooltipLabel latencyLabel, vocalBoostLabel;
    std::unique_ptr<StyledSlider> latencySlider, vocalBoostSlider;
    
    // MIDI
    juce::Label midiInputLabel; 
    juce::ComboBox midiInputSelector;
    juce::TextButton midiRefreshButton; // NEW BUTTON
    
    // Outputs
    juce::Label outputsSectionLabel; 
    juce::OwnedArray<OutputItemComponent> outputItems;
    
    void onSpecificDriverChanged(); 
    void updateInputDevices(); 
    void updateOutputDevices(); 
    void updateMidiDevices();
    void onPlaybackInputChanged(int pairIndex, bool isLeft);
    
    void validateAndSelectMic(int micIndex, juce::ComboBox& selector, const juce::String& savedName);
    void validateAndSelectBacking(int pairIndex, bool isLeft, juce::ComboBox& selector, const juce::String& savedName);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOPage)
};