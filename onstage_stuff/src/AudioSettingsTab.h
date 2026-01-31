#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "MidiSelectors.h"
#include "PluginProcessor.h"

class AudioSettingsTab : public juce::Component, 
                         public juce::ChangeListener, 
                         public juce::ComboBox::Listener, 
                         public juce::Button::Listener {
public: 
    AudioSettingsTab(SubterraneumAudioProcessor& p);
    ~AudioSettingsTab() override;
    void paint(juce::Graphics& g) override; 
    void resized() override; 
    void changeListenerCallback(juce::ChangeBroadcaster*) override; 
    void comboBoxChanged(juce::ComboBox*) override; 
    void buttonClicked(juce::Button*) override;
    
private: 
    SubterraneumAudioProcessor& processor; 
    juce::AudioDeviceManager* deviceManager = nullptr; 
    
    // Driver Settings
    juce::GroupComponent driverGroup { "driverGroup", "Driver Settings" };
    
    // Platform-specific label text set in constructor
    #if JUCE_WINDOWS
    juce::Label deviceLabel { "device", "ASIO Device:" };
    #elif JUCE_MAC
    juce::Label deviceLabel { "device", "Audio Device:" };
    #else
    juce::Label deviceLabel { "device", "Audio Device:" };
    #endif
    
    juce::ComboBox deviceCombo; 
    juce::TextButton controlPanelBtn { "Control Panel" };
    juce::Label statusLabel { "status", "" };
    
    // MIDI Settings
    juce::GroupComponent midiGroup { "midiGroup", "MIDI Inputs" }; 
    juce::Viewport midiViewport; 
    juce::Component midiContent;
    struct MidiRow { 
        juce::String identifier; 
        juce::String deviceName;
        std::unique_ptr<juce::Label> deviceNameLabel; 
        std::unique_ptr<juce::TextButton> channelButton;
        int channelMask = 0;
    }; 
    std::vector<std::unique_ptr<MidiRow>> midiRows; 
    
    void enforceDriverType(); 
    void updateDeviceList(); 
    void updateMidiList();
    void updateStatusLabel();
    void openMidiChannelSelector(MidiRow* row);
    void updateMidiRowButton(MidiRow* row);
};
