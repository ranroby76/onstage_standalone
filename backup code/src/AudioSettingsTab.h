// #D:\Workspace\onstage_colosseum_upgrade\src\AudioSettingsTab.h
// Audio/MIDI settings tab for OnStage
// MIDI CHANNEL DUPLICATION: Select target channels to duplicate hardware MIDI to
// FIX: Added Tempo, Time Signature, and Metronome sections from removed StudioTab

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Style.h"
#include "MidiSelectors.h"

// =============================================================================
// InfoButton — TextButton with right-click tooltip popup
// =============================================================================
class InfoButton : public juce::TextButton
{
public:
    InfoButton(const juce::String& buttonText = "") : juce::TextButton(buttonText) {}
    
    juce::String infoText;
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown() && infoText.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Info",
                infoText,
                "OK");
        }
        else
        {
            TextButton::mouseDown(e);
        }
    }
};

// =============================================================================
// GoldSliderLookAndFeel — Orange/gold themed slider
// =============================================================================
class GoldSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        auto trackBounds = juce::Rectangle<float>((float)x, (float)y + height * 0.4f, 
                                                   (float)width, height * 0.2f);
        
        // Track background
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRoundedRectangle(trackBounds, 3.0f);
        
        // Filled portion
        auto filledWidth = sliderPos - (float)x;
        if (filledWidth > 0)
        {
            g.setColour(juce::Colours::orange.darker(0.2f));
            g.fillRoundedRectangle(trackBounds.withWidth(filledWidth), 3.0f);
        }
        
        // Thumb
        float thumbWidth = 14.0f;
        float thumbHeight = (float)height * 0.8f;
        float thumbX = sliderPos - thumbWidth / 2;
        float thumbY = (float)y + ((float)height - thumbHeight) / 2;
        
        g.setColour(slider.isMouseOverOrDragging() ? juce::Colours::orange : juce::Colours::orange.darker());
        g.fillRoundedRectangle(thumbX, thumbY, thumbWidth, thumbHeight, 4.0f);
        
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRoundedRectangle(thumbX, thumbY, thumbWidth, thumbHeight, 4.0f, 1.0f);
    }
};

// =============================================================================
// GreenSliderLookAndFeel — Green themed slider for metronome
// =============================================================================
class GreenSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        auto trackBounds = juce::Rectangle<float>((float)x, (float)y + height * 0.4f, 
                                                   (float)width, height * 0.2f);
        
        // Track background
        g.setColour(juce::Colour(50, 60, 50));
        g.fillRoundedRectangle(trackBounds, 3.0f);
        
        // Filled portion
        auto filledWidth = sliderPos - (float)x;
        if (filledWidth > 0)
        {
            g.setColour(juce::Colours::green.darker(0.2f));
            g.fillRoundedRectangle(trackBounds.withWidth(filledWidth), 3.0f);
        }
        
        // Thumb
        float thumbWidth = 14.0f;
        float thumbHeight = (float)height * 0.8f;
        float thumbX = sliderPos - thumbWidth / 2;
        float thumbY = (float)y + ((float)height - thumbHeight) / 2;
        
        g.setColour(slider.isMouseOverOrDragging() ? juce::Colours::lightgreen : juce::Colours::green);
        g.fillRoundedRectangle(thumbX, thumbY, thumbWidth, thumbHeight, 4.0f);
        
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRoundedRectangle(thumbX, thumbY, thumbWidth, thumbHeight, 4.0f, 1.0f);
    }
};

// =============================================================================
// AudioSettingsTab — Main audio/MIDI settings component
// =============================================================================
class AudioSettingsTab : public juce::Component,
                         public juce::ChangeListener,
                         public juce::Button::Listener,
                         public juce::ComboBox::Listener,
                         public juce::Slider::Listener
{
public:
    AudioSettingsTab(SubterraneumAudioProcessor& processor);
    ~AudioSettingsTab() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void sliderValueChanged(juce::Slider* slider) override;
    
private:
    SubterraneumAudioProcessor& processor;
    juce::AudioDeviceManager* deviceManager = nullptr;
    
    // Look and feel
    GoldSliderLookAndFeel goldSliderLookAndFeel;
    GreenSliderLookAndFeel greenSliderLookAndFeel;
    
    // ==========================================================================
    // Driver Settings Group
    // ==========================================================================
    juce::GroupComponent driverGroup { "driver", "Driver Settings" };
    juce::Label deviceLabel { "device", "Audio Device:" };
    juce::ComboBox deviceCombo;
    juce::TextButton controlPanelBtn { "Control Panel" };
    juce::TextButton reconnectMidiBtn { "Reconnect MIDI Devices" };
    juce::Label statusLabel { "status", "" };
    
    // Recording folder
    juce::TextButton recordingFolderBtn { "Recording Folder..." };
    juce::Label recordingFolderLabel { "recFolder", "" };
    
    // Sampler folder
    juce::TextButton samplerFolderBtn { "Sampler Folder..." };
    juce::Label samplerFolderLabel { "sampFolder", "" };
    
    // Default patch
    InfoButton saveDefaultBtn { "Save as Default" };
    juce::TextButton clearDefaultBtn { "Clear Default" };
    juce::Label defaultPatchLabel { "defaultPatch", "" };
    
    // ==========================================================================
    // MIDI Inputs Group
    // ==========================================================================
    juce::GroupComponent midiInputsGroup { "midiIn", "MIDI Inputs" };
    juce::Viewport midiInputsViewport;
    juce::Component midiInputsContent;
    
    struct MidiInputRow
    {
        juce::String identifier;
        juce::String deviceName;
        int channelMask = 0xFFFF;
        std::unique_ptr<juce::Label> deviceNameLabel;
        std::unique_ptr<juce::TextButton> channelButton;
    };
    std::vector<std::unique_ptr<MidiInputRow>> midiInputRows;
    
    // ==========================================================================
    // MIDI Outputs Group
    // ==========================================================================
    juce::GroupComponent midiOutputsGroup { "midiOut", "MIDI Outputs" };
    juce::Viewport midiOutputsViewport;
    juce::Component midiOutputsContent;
    
    struct MidiOutputRow
    {
        juce::String identifier;
        juce::String deviceName;
        int channelMask = 0;
        std::unique_ptr<juce::Label> deviceNameLabel;
        std::unique_ptr<juce::TextButton> channelButton;
    };
    std::vector<std::unique_ptr<MidiOutputRow>> midiOutputRows;
    
    // ==========================================================================
    // Tempo Section
    // ==========================================================================
    juce::GroupComponent tempoGroup { "tempo", "Tempo" };
    juce::Label tempoLabel { "tempoLbl", "BPM" };
    juce::Slider tempoSlider;
    juce::Label tempoValueLabel { "tempoVal", "120.0" };
    juce::TextButton tapTempoBtn { "TAP" };
    
    double currentTempo = 120.0;
    std::vector<double> tapTimes;
    double lastTapTime = 0.0;
    
    // ==========================================================================
    // Time Signature Section
    // ==========================================================================
    juce::GroupComponent timeSignatureGroup { "timeSig", "Time Sig" };
    juce::Label timeSigValueLabel { "timeSigVal", "4/4" };
    juce::ComboBox numeratorCombo;
    juce::Label slashLabel { "slash", "/" };
    juce::ComboBox denominatorCombo;
    
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    
    // ==========================================================================
    // Metronome Section
    // ==========================================================================
    juce::GroupComponent metronomeGroup { "metronome", "Metronome" };
    juce::Label metronomeVolumeLabel { "metVol", "Vol" };
    juce::Slider metronomeVolumeSlider;
    juce::ToggleButton metronomeBtn { "Enable" };
    
    // ==========================================================================
    // File Chooser
    // ==========================================================================
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // ==========================================================================
    // Methods
    // ==========================================================================
    void enforceDriverType();
    void updateDeviceList();
    void updateStatusLabel();
    
    void updateMidiInputsList();
    void updateMidiOutputsList();
    void updateMidiInputRowButton(MidiInputRow* row);
    void updateMidiOutputRowButton(MidiOutputRow* row);
    void openMidiInputChannelSelector(MidiInputRow* row);
    void openMidiOutputChannelSelector(MidiOutputRow* row);
    
    void updateTempoDisplay();
    void updateTimeSigDisplay();
    void handleTapTempo();
    
    void selectRecordingFolder();
    void updateRecordingFolderLabel();
    void selectSamplerFolder();
    void updateSamplerFolderLabel();
    
    void saveAsDefault();
    void clearDefault();
    void updateDefaultPatchLabel();
    
    void reconnectMidiDevices();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsTab)
};
