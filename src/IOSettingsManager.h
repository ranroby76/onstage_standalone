// D:\Workspace\ONSTAGE_WIRED\src\IOSettingsManager.h

#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>

class IOSettingsManager
{
public:
    IOSettingsManager();
    ~IOSettingsManager() = default;

    void saveDriverType(const juce::String& driverType);
    void saveSpecificDriver(const juce::String& driverName);
    
    // Mic Mute/Bypass state (Logic state only, physical routing is now in InputMatrix)
    void saveMicMute(int micIndex, bool shouldMute);
    void saveMicBypass(int micIndex, bool shouldBypass);

    // Routing Maps
    void saveOutputRouting(const std::map<juce::String, int>& routingMap);
    
    // Input Matrix Routing
    // Key: Input Name
    // Value: Pair(Mask, Gain)
    void saveInputRouting(const std::map<juce::String, std::pair<int, float>>& routingMap);
    std::map<juce::String, std::pair<int, float>> getInputRouting() const;

    void saveMediaFolder(const juce::String& path);
    juce::String getMediaFolder() const { return lastMediaFolder; }

    void savePlaylistFolder(const juce::String& path);
    juce::String getPlaylistFolder() const { return lastPlaylistFolder; }

    // Recording folder setting
    void saveRecordingFolder(const juce::String& path);
    juce::String getRecordingFolder() const { return lastRecordingFolder; }

    // Vocal Recording Settings
    void saveVocalSettings(float latencyMs, float boostDb);
    float getLastLatencyMs() const { return lastLatencyMs; }
    float getLastVocalBoostDb() const { return lastVocalBoostDb; }

    // MIDI Settings - supports multiple devices
    void saveMidiDevices(const juce::StringArray& deviceIdentifiers);
    juce::StringArray getLastMidiDevices() const { return lastMidiDevices; }
    
    // Legacy single device (kept for compatibility)
    void saveMidiDevice(const juce::String& deviceName);
    juce::String getLastMidiDevice() const { return lastMidiDevice; }

    bool loadSettings();

    juce::String getLastDriverType() const { return lastDriverType; }
    juce::String getLastSpecificDriver() const { return lastSpecificDriver; }
    
    struct MicSettings {
        bool isMuted = false;
        bool isBypassed = false;
    };
    MicSettings getMicSettings(int index) const;

    // Output Routing getter
    std::map<juce::String, int> getOutputRouting() const { return outputRoutingMap; }

    bool hasExistingSettings() const;

private:
    juce::File getSettingsFile() const;
    bool saveToFile();

    juce::String lastDriverType;
    juce::String lastSpecificDriver;
    
    MicSettings micSettings[2];
    
    std::map<juce::String, int> outputRoutingMap;
    std::map<juce::String, std::pair<int, float>> inputRoutingMap;

    juce::String lastMediaFolder = ""; 
    juce::String lastPlaylistFolder = "";
    juce::String lastRecordingFolder = "";  // NEW: Recording folder setting
    
    float lastLatencyMs = 0.0f;
    float lastVocalBoostDb = 0.0f;
    juce::String lastMidiDevice = "";
    juce::StringArray lastMidiDevices;  // Multiple MIDI device identifiers
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOSettingsManager)
};
