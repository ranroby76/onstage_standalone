#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class IOSettingsManager
{
public:
    IOSettingsManager();
    ~IOSettingsManager() = default;

    void saveDriverType(const juce::String& driverType);
    void saveSpecificDriver(const juce::String& driverName);
    void saveMicInput(int micIndex, const juce::String& inputName);
    void saveOutputs(const juce::StringArray& outputNames);
    
    void saveBackingTrackInput(int index, bool enabled, int mappedInput, 
                               const juce::String& leftSelection = "OFF", 
                               const juce::String& rightSelection = "OFF",
                               float gain = 1.0f);
    
    void saveMediaFolder(const juce::String& path);
    juce::String getMediaFolder() const { return lastMediaFolder; }

    void savePlaylistFolder(const juce::String& path);
    juce::String getPlaylistFolder() const { return lastPlaylistFolder; }

    // Vocal Recording Settings
    void saveVocalSettings(float latencyMs, float boostDb);
    float getLastLatencyMs() const { return lastLatencyMs; }
    float getLastVocalBoostDb() const { return lastVocalBoostDb; }

    // NEW: MIDI Settings
    void saveMidiDevice(const juce::String& deviceName);
    juce::String getLastMidiDevice() const { return lastMidiDevice; }

    bool loadSettings();

    juce::String getLastDriverType() const { return lastDriverType; }
    juce::String getLastSpecificDriver() const { return lastSpecificDriver; }
    juce::String getLastMicInput(int micIndex) const;
    juce::StringArray getLastOutputs() const { return lastOutputs; }

    struct BackingTrackInputState
    {
        bool enabled = false;
        int mappedInput = -1;
        juce::String leftSelection = "OFF";
        juce::String rightSelection = "OFF";
        float gain = 1.0f;
        bool isValid() const { return enabled && mappedInput >= 0; }
    };

    BackingTrackInputState getBackingTrackInput(int index) const;
    bool hasExistingSettings() const;

private:
    juce::File getSettingsFile() const;
    bool saveToFile();

    juce::String lastDriverType;
    juce::String lastSpecificDriver;
    juce::String lastMicInputs[2];
    juce::StringArray lastOutputs;
    BackingTrackInputState backingTrackInputs[9];
    
    juce::String lastMediaFolder = ""; 
    juce::String lastPlaylistFolder = "";
    
    float lastLatencyMs = 0.0f;
    float lastVocalBoostDb = 0.0f;

    // NEW
    juce::String lastMidiDevice = "";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOSettingsManager)
};