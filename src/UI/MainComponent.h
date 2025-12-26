#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../AudioEngine.h"
#include "../PresetManager.h"
#include "../IOSettingsManager.h"
#include "HeaderBar.h"
#include "StyledSlider.h" 
#include "MasterMeter.h" 

class IOPage;
class VocalsPage;
class MediaPage;

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void restoreIOSettings();

private:
    void timerCallback() override; 

    AudioEngine audioEngine;
    PresetManager presetManager { audioEngine };
    IOSettingsManager ioSettingsManager;
    
    // Task 1: L&F for Tabs
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;

    HeaderBar header;
    juce::TabbedComponent mainTabs { juce::TabbedButtonBar::TabsAtTop };

    std::unique_ptr<IOPage> ioPage;
    std::unique_ptr<VocalsPage> vocalsPage;
    std::unique_ptr<MediaPage> mediaPage;
    
    StyledSlider masterVolumeSlider;
    MidiTooltipLabel masterVolumeLabel;
    MasterMeter masterMeter;
    
    juce::Label sloganLabel;  // NEW: Slogan display

    // Recorder
    MidiTooltipTextButton recordButton;
    juce::TextButton downloadWavButton;
    bool isRecording = false;
    bool recordFlickerState = false;
    bool showDownloads = false;

    void handleRecordClick();
    void startRecording();
    void stopRecording();
    void downloadRecording();
    void exportAudioFile(const juce::File& source, const juce::File& dest);
    float sliderValueToDb(double sliderValue);
    void savePreset();
    void loadPreset();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};