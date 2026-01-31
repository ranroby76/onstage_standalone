#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==============================================================================
// MIDI Input Channel Selector (16-channel routing for MIDI inputs in Audio Settings)
// ==============================================================================
class MidiInputChannelSelector : public juce::Component, public juce::Button::Listener {
public: 
    MidiInputChannelSelector(const juce::String& deviceName, int currentMask, std::function<void(int)> onMaskChanged);
    ~MidiInputChannelSelector() override; 
    void paint(juce::Graphics& g) override; 
    void resized() override; 
    void buttonClicked(juce::Button* b) override;
private: 
    juce::String midiDeviceName;
    std::function<void(int)> maskChangedCallback;
    juce::Label titleLabel; 
    juce::TextButton closeButton { "X" };
    juce::TextButton allButton { "ALL" };
    juce::TextButton noneButton { "NONE" };
    juce::OwnedArray<juce::TextButton> channelButtons; 
    int channelMask = 0;
};

// ==============================================================================
// MIDI Channel Selector (for VSTi nodes in rack - SINGLE CHANNEL SELECT)
// ==============================================================================
class MidiChannelSelector : public juce::Component, public juce::Button::Listener {
public: 
    MidiChannelSelector(MeteringProcessor* processor, std::function<void()> onUpdate);
    ~MidiChannelSelector() override; 
    void paint(juce::Graphics& g) override; 
    void resized() override; 
    void buttonClicked(juce::Button* b) override;
private: 
    MeteringProcessor* meteringProc;
    std::function<void()> updateCallback;
    juce::Label titleLabel; 
    juce::TextButton closeButton { "X" }; 
    juce::OwnedArray<juce::TextButton> channelButtons; 
};