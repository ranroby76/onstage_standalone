// D:\Workspace\ONSTAGE_WIRED\src\UI\IOPage.h

// ==============================================================================
//  IOPage.h
//  OnStage — Audio device settings page (ASIO-only)
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "StyledSlider.h"

class AudioEngine;
class IOSettingsManager;

// ==============================================================================
//  MidiDeviceRow — single checkbox + label inside the popup
// ==============================================================================
class MidiDeviceRow : public juce::Component
{
public:
    MidiDeviceRow (const juce::String& name, const juce::String& id, bool enabled)
        : deviceId (id)
    {
        addAndMakeVisible (toggle);
        toggle.setButtonText (name);
        toggle.setToggleState (enabled, juce::dontSendNotification);
        toggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        toggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xFFD4AF37));
    }

    void resized() override { toggle.setBounds (getLocalBounds()); }

    juce::ToggleButton toggle;
    juce::String deviceId;
};

// ==============================================================================
//  MidiSelectorContent — the content component placed inside the popup window
// ==============================================================================
class MidiSelectorContent : public juce::Component
{
public:
    MidiSelectorContent (juce::AudioDeviceManager& dm)
        : deviceManager (dm)
    {
        rebuild();
    }

    void rebuild()
    {
        rows.clear();
        auto devices = juce::MidiInput::getAvailableDevices();

        for (auto& d : devices)
        {
            bool on = deviceManager.isMidiInputDeviceEnabled (d.identifier);
            auto* row = rows.add (new MidiDeviceRow (d.name, d.identifier, on));
            addAndMakeVisible (row);

            row->toggle.onClick = [this, id = d.identifier, row]
            {
                deviceManager.setMidiInputDeviceEnabled (id, row->toggle.getToggleState());
                if (onSelectionChanged)
                    onSelectionChanged();
            };
        }

        if (rows.isEmpty())
        {
            noDevicesLabel.setText ("No MIDI devices found.", juce::dontSendNotification);
            noDevicesLabel.setFont (juce::Font (13.0f, juce::Font::italic));
            noDevicesLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));
            noDevicesLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (noDevicesLabel);
        }

        setSize (320, juce::jmax (60, (int) rows.size() * 30 + 16));
        resized();
    }

    void resized() override
    {
        if (rows.isEmpty())
        {
            noDevicesLabel.setBounds (getLocalBounds());
            return;
        }

        auto area = getLocalBounds().reduced (8);
        for (auto* row : rows)
            row->setBounds (area.removeFromTop (30));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF252525));
    }

    std::function<void()> onSelectionChanged;

private:
    juce::AudioDeviceManager& deviceManager;
    juce::OwnedArray<MidiDeviceRow> rows;
    juce::Label noDevicesLabel;
};

// ==============================================================================
//  MidiPopupWindow — DocumentWindow with close callback support
// ==============================================================================
class MidiPopupWindow : public juce::DocumentWindow
{
public:
    MidiPopupWindow (const juce::String& name, juce::Colour bg, int buttons)
        : juce::DocumentWindow (name, bg, buttons)
    {
    }

    void closeButtonPressed() override
    {
        if (onClose)
            onClose();
    }

    std::function<void()> onClose;
};

// ==============================================================================
//  IOPage
// ==============================================================================
class IOPage : public juce::Component,
               public juce::ChangeListener,
               public juce::Timer
{
public:
    IOPage (AudioEngine& engine, IOSettingsManager& settings);
    ~IOPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // ChangeListener — device manager notifications
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Timer — refresh device info
    void timerCallback() override;

private:
    AudioEngine& audioEngine;
    IOSettingsManager& ioSettingsManager;

    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;

    // --- ASIO driver selector ------------------------------------------------
    juce::Label      sectionAudioLabel;
    juce::Label      driverLabel;
    juce::ComboBox   driverSelector;
    juce::TextButton controlPanelButton { "Control Panel" };

    // --- Device info display -------------------------------------------------
    juce::Label sampleRateLabel;
    juce::Label bufferSizeLabel;
    juce::Label latencyLabel;
    juce::Label inputCountLabel;
    juce::Label outputCountLabel;

    // --- Channel lists (read-only, with scrolling) ---------------------------
    juce::Label      sectionInputsLabel;
    juce::Label      sectionOutputsLabel;
    juce::TextEditor inputChannelList;
    juce::TextEditor outputChannelList;

    // --- MIDI Input (multi-select popup) -------------------------------------
    juce::Label      sectionMidiLabel;
    juce::TextButton midiSelectButton { "Select MIDI Inputs..." };
    juce::Label      midiSummaryLabel;
    std::unique_ptr<MidiPopupWindow> midiPopup;

    // --- Recording Folder (NEW) ----------------------------------------------
    juce::Label      sectionRecordingLabel;
    juce::TextButton recordingFolderButton { "Set Default Recording Folder..." };
    juce::Label      recordingFolderPathLabel;

    // --- Routing notice ------------------------------------------------------
    juce::Label routingNotice;

    // --- Internal helpers ----------------------------------------------------
    void populateDriverList();
    void onDriverChanged();
    void openDeviceWithAllChannels (const juce::String& deviceName);
    void updateDeviceInfo();
    void openMidiPopup();
    void closeMidiPopup();
    void updateMidiSummary();
    
    // Recording folder helpers (NEW)
    void chooseRecordingFolder();
    void updateRecordingFolderDisplay();
    std::shared_ptr<juce::FileChooser> recordingFolderChooser;
    
    // Settings persistence
    void restoreSavedSettings();
    void saveMidiSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IOPage)
};
