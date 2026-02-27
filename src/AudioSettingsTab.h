// AudioSettingsTab.h
// FIX: Added Tempo, Time Signature, and Metronome sections from removed StudioTab
// FIX: Added timeSigValueLabel to display current time signature
// FIX: Added recording folder selection button
// FIX: Green slider for metronome

#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "MidiSelectors.h"
#include "PluginProcessor.h"
#include "ManualSamplerProcessor.h"
#include "AutoSamplerProcessor.h"

// Custom LookAndFeel for gold sliders (moved from StudioTab)
class GoldSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldSliderLookAndFeel()
    {
        goldColor = juce::Colour(0xffFFD700);        // Gold
        darkGoldColor = juce::Colour(0xff9B7A00);    // Dark gold
        blackColor = juce::Colours::black;
    }
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                         const juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) override
    {
        auto trackWidth = juce::jmin(6.0f, (float)height * 0.25f);
        
        auto trackBounds = juce::Rectangle<float>((float)x, (float)y + (float)height * 0.5f - trackWidth * 0.5f,
                                                   (float)width, trackWidth);
        
        // Draw "off" rail (dark gold)
        g.setColour(darkGoldColor);
        g.fillRoundedRectangle(trackBounds, trackWidth * 0.5f);
        
        // Draw "on" rail (gold) - from start to thumb position
        auto onTrack = trackBounds.withWidth(sliderPos - trackBounds.getX());
        g.setColour(goldColor);
        g.fillRoundedRectangle(onTrack, trackWidth * 0.5f);
        
        // Draw thumb (gold circle with black center)
        float thumbRadius = 12.0f;
        float thumbX = sliderPos;
        float thumbY = y + height * 0.5f;
        
        // Outer gold circle
        g.setColour(goldColor);
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);
        
        // Inner black circle
        float innerRadius = thumbRadius * 0.5f;
        g.setColour(blackColor);
        g.fillEllipse(thumbX - innerRadius, thumbY - innerRadius, innerRadius * 2, innerRadius * 2);
    }
    
private:
    juce::Colour goldColor;
    juce::Colour darkGoldColor;
    juce::Colour blackColor;
};

// Green slider for metronome (matches metronome frame color)
class GreenSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) override
    {
        auto trackWidth = juce::jmin(6.0f, (float)height * 0.25f);
        
        auto trackBounds = juce::Rectangle<float>((float)x, (float)y + (float)height * 0.5f - trackWidth * 0.5f,
                                                   (float)width, trackWidth);
        
        // Track background (dark green)
        g.setColour(juce::Colour(40, 80, 40));
        g.fillRoundedRectangle(trackBounds, trackWidth * 0.5f);
        
        // Filled portion (light green)
        auto onTrack = trackBounds.withWidth(sliderPos - trackBounds.getX());
        g.setColour(juce::Colours::lightgreen);
        g.fillRoundedRectangle(onTrack, trackWidth * 0.5f);
        
        // Draw thumb (green circle with darker center)
        float thumbRadius = 12.0f;
        float thumbX = sliderPos;
        float thumbY = y + height * 0.5f;
        
        // Outer green circle
        g.setColour(juce::Colours::lightgreen);
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);
        
        // Inner highlight
        float innerRadius = thumbRadius * 0.5f;
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillEllipse(thumbX - innerRadius, thumbY - innerRadius, innerRadius * 2, innerRadius * 2);
    }
};

class AudioSettingsTab : public juce::Component, 
                         public juce::ChangeListener, 
                         public juce::ComboBox::Listener, 
                         public juce::Button::Listener,
                         public juce::Slider::Listener {
public: 
    AudioSettingsTab(SubterraneumAudioProcessor& p);
    ~AudioSettingsTab() override;
    void paint(juce::Graphics& g) override; 
    void resized() override; 
    void changeListenerCallback(juce::ChangeBroadcaster*) override; 
    void comboBoxChanged(juce::ComboBox*) override; 
    void buttonClicked(juce::Button*) override;
    void sliderValueChanged(juce::Slider*) override;
    
    // Get current tempo/time signature for MIDI clock
    double getTempo() const { return currentTempo; }
    int getTimeSigNumerator() const { return timeSigNumerator; }
    int getTimeSigDenominator() const { return timeSigDenominator; }
    
private: 
    SubterraneumAudioProcessor& processor; 
    juce::AudioDeviceManager* deviceManager = nullptr; 
    
    // =========================================================================
    // Driver Settings
    // =========================================================================
    juce::GroupComponent driverGroup { "driverGroup", "Driver Settings" };
    
    #if JUCE_WINDOWS
    juce::Label deviceLabel { "device", "ASIO Device:" };
    #elif JUCE_MAC
    juce::Label deviceLabel { "device", "Audio Device:" };
    #else
    juce::Label deviceLabel { "device", "Audio Device:" };
    #endif
    
    juce::ComboBox deviceCombo; 
    juce::TextButton controlPanelBtn { "Control Panel" };
    juce::TextButton reconnectMidiBtn { "Reconnect MIDI Devices" };
    juce::Label statusLabel { "status", "" };
    
    // Recording folder selection (aligned right in driver settings)
    juce::TextButton recordingFolderBtn { "Set Recording Folder..." };
    juce::Label recordingFolderLabel { "recFolder", "" };
    
    // Sampler folder selection (next to recording folder)
    juce::TextButton samplerFolderBtn { "Set Sampler Folder..." };
    juce::Label samplerFolderLabel { "sampFolder", "" };
    
    // Default patch auto-load
    // Save as Default button with right-click info popup
    class InfoButton : public juce::TextButton {
    public:
        using juce::TextButton::TextButton;
        juce::String infoText;
        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isRightButtonDown() && infoText.isNotEmpty()) {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Save as Default",
                    infoText,
                    "OK");
                return;
            }
            juce::TextButton::mouseDown(e);
        }
    };
    InfoButton saveDefaultBtn { "Save as Default" };
    juce::TextButton clearDefaultBtn { "Clear Default" };
    juce::Label defaultPatchLabel { "defaultPatch", "" };
    
    // =========================================================================
    // MIDI Settings - Split into Inputs and Outputs
    // =========================================================================
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
    
    // MIDI Outputs
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
    
    // =========================================================================
    // Tempo Section (moved from StudioTab)
    // =========================================================================
    juce::GroupComponent tempoGroup { "tempoGroup", "Master Tempo" };
    
    juce::Slider tempoSlider;
    juce::Label tempoLabel { "tempo", "BPM" };
    juce::Label tempoValueLabel { "tempoValue", "120.0" };
    
    juce::TextButton tapTempoBtn { "TAP" };
    std::vector<double> tapTimes;
    double lastTapTime = 0.0;
    
    double currentTempo = 120.0;
    
    // =========================================================================
    // Time Signature Section (moved from StudioTab)
    // =========================================================================
    juce::GroupComponent timeSignatureGroup { "timeSignatureGroup", "Time Signature" };
    
    juce::Label timeSigValueLabel { "timeSigValue", "4/4" };
    juce::ComboBox numeratorCombo;
    juce::Label slashLabel { "slash", "/" };
    juce::ComboBox denominatorCombo;
    
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    
    // =========================================================================
    // Metronome Section (moved from StudioTab)
    // =========================================================================
    juce::GroupComponent metronomeGroup { "metronomeGroup", "Metronome" };
    juce::ToggleButton metronomeBtn { "Enable" };
    juce::Slider metronomeVolumeSlider;
    juce::Label metronomeVolumeLabel { "metVol", "Volume" };
    
    // Custom gold look and feel for sliders
    GoldSliderLookAndFeel goldSliderLookAndFeel;
    
    // Green slider look and feel for metronome
    GreenSliderLookAndFeel greenSliderLookAndFeel;
    
    // Helper methods
    void enforceDriverType(); 
    void updateDeviceList(); 
    void updateMidiInputsList();
    void updateMidiOutputsList();
    void updateStatusLabel();
    void openMidiInputChannelSelector(MidiInputRow* row);
    void openMidiOutputChannelSelector(MidiOutputRow* row);
    void updateMidiInputRowButton(MidiInputRow* row);
    void updateMidiOutputRowButton(MidiOutputRow* row);
    
    // Tempo helpers
    void updateTempoDisplay();
    void handleTapTempo();
    void updateTimeSigDisplay();
    
    // Recording folder helpers
    void selectRecordingFolder();
    void updateRecordingFolderLabel();
    
    // Sampler folder helpers
    void selectSamplerFolder();
    void updateSamplerFolderLabel();
    
    // Default patch helpers
    void saveAsDefault();
    void clearDefault();
    void updateDefaultPatchLabel();
    
    // MIDI reconnection
    void reconnectMidiDevices();
};
