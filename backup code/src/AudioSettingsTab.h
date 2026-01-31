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
    
    // MIDI Settings - Split into Inputs and Outputs
    juce::GroupComponent midiInputsGroup { "midiInputsGroup", "MIDI Inputs" };
    juce::GroupComponent midiOutputsGroup { "midiOutputsGroup", "MIDI Outputs" };
    
    // MIDI Inputs
    juce::Viewport midiInputsViewport; 
    juce::Component midiInputsContent;
    struct MidiInputRow { 
        juce::String identifier; 
        juce::String deviceName;
        std::unique_ptr<juce::Label> deviceNameLabel; 
        std::unique_ptr<juce::TextButton> channelButton;
        int channelMask = 0;
    }; 
    std::vector<std::unique_ptr<MidiInputRow>> midiInputRows;
    
    // MIDI Outputs (NEW)
    juce::Viewport midiOutputsViewport;
    juce::Component midiOutputsContent;
    struct MidiOutputRow {
        juce::String identifier;
        juce::String deviceName;
        std::unique_ptr<juce::Label> deviceNameLabel;
        std::unique_ptr<juce::TextButton> channelButton;
        int channelMask = 0;
    };
    std::vector<std::unique_ptr<MidiOutputRow>> midiOutputRows;
    
    void enforceDriverType(); 
    void updateDeviceList(); 
    void updateMidiInputsList();
    void updateMidiOutputsList();
    void updateStatusLabel();
    void openMidiInputChannelSelector(MidiInputRow* row);
    void openMidiOutputChannelSelector(MidiOutputRow* row);
    void updateMidiInputRowButton(MidiInputRow* row);
    void updateMidiOutputRowButton(MidiOutputRow* row);
};

