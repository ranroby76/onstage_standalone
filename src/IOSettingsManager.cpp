// **Fix:** Added logging to help verify if `json` is actually loading.

#include "IOSettingsManager.h"
#include "AppLogger.h"
#include <juce_core/juce_core.h>

IOSettingsManager::IOSettingsManager()
{
    lastDriverType = "";
    lastSpecificDriver = "";
    lastMediaFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getFullPathName();
    lastPlaylistFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getFullPathName();
    
    lastLatencyMs = 0.0f;
    lastVocalBoostDb = 0.0f;
    lastMidiDevice = "";
    
    for (int i = 0; i < 2; ++i)
        lastMicInputs[i] = "OFF";
    for (int i = 0; i < 9; ++i)
    {
        backingTrackInputs[i].enabled = false;
        backingTrackInputs[i].mappedInput = -1;
        backingTrackInputs[i].leftSelection = "OFF";
        backingTrackInputs[i].rightSelection = "OFF";
        backingTrackInputs[i].gain = 1.0f;
    }
}

void IOSettingsManager::saveDriverType(const juce::String& driverType) { lastDriverType = driverType;
    saveToFile(); }
void IOSettingsManager::saveSpecificDriver(const juce::String& driverName) { lastSpecificDriver = driverName; saveToFile();
}
void IOSettingsManager::saveMicInput(int micIndex, const juce::String& inputName) { if (micIndex >= 0 && micIndex < 2) { lastMicInputs[micIndex] = inputName; saveToFile();
} }
void IOSettingsManager::saveOutputs(const juce::StringArray& outputNames) { lastOutputs = outputNames; saveToFile();
}

void IOSettingsManager::saveBackingTrackInput(int index, bool enabled, int mappedInput, 
                                              const juce::String& leftSelection, 
                                              const juce::String& rightSelection,
                                              float gain) 
{
    if (index >= 0 && index < 9) {
        backingTrackInputs[index].enabled = enabled;
        backingTrackInputs[index].mappedInput = mappedInput;
        backingTrackInputs[index].leftSelection = leftSelection;
        backingTrackInputs[index].rightSelection = rightSelection;
        backingTrackInputs[index].gain = gain;
        saveToFile();
    }
}

void IOSettingsManager::saveMediaFolder(const juce::String& path) { lastMediaFolder = path; saveToFile(); }
void IOSettingsManager::savePlaylistFolder(const juce::String& path) { lastPlaylistFolder = path; saveToFile(); }
void IOSettingsManager::saveVocalSettings(float latencyMs, float boostDb) { lastLatencyMs = latencyMs; lastVocalBoostDb = boostDb; saveToFile(); }
void IOSettingsManager::saveMidiDevice(const juce::String& deviceName) { lastMidiDevice = deviceName; saveToFile(); }

bool IOSettingsManager::loadSettings()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile()) {
        LOG_INFO("IOSettingsManager: Settings file not found at " + file.getFullPathName());
        return false;
    }
    
    LOG_INFO("IOSettingsManager: Loading settings from " + file.getFullPathName());

    try {
        auto json = juce::JSON::parse(file);
        if (auto* obj = json.getDynamicObject())
        {
            lastDriverType = obj->getProperty("driverType").toString();
            lastSpecificDriver = obj->getProperty("specificDriver").toString();
            
            if (obj->hasProperty("mediaFolder")) lastMediaFolder = obj->getProperty("mediaFolder").toString();
            if (obj->hasProperty("playlistFolder")) lastPlaylistFolder = obj->getProperty("playlistFolder").toString();
            if (obj->hasProperty("latencyMs")) lastLatencyMs = (float)obj->getProperty("latencyMs");
            if (obj->hasProperty("vocalBoostDb")) lastVocalBoostDb = (float)obj->getProperty("vocalBoostDb");
            if (obj->hasProperty("midiDevice")) lastMidiDevice = obj->getProperty("midiDevice").toString();
            
            if (auto* mics = obj->getProperty("micInputs").getArray())
                for (int i = 0; i < juce::jmin(2, mics->size()); ++i)
                    lastMicInputs[i] = mics->getReference(i).toString();
                    
            if (auto* outputs = obj->getProperty("outputs").getArray()) {
                lastOutputs.clear();
                for (auto& output : *outputs) lastOutputs.add(output.toString());
            }
            
            if (auto* backing = obj->getProperty("backingTrackInputs").getArray()) {
                for (int i = 0; i < juce::jmin(9, backing->size()); ++i) {
                    if (auto* btObj = backing->getReference(i).getDynamicObject()) {
                        backingTrackInputs[i].enabled = btObj->getProperty("enabled");
                        backingTrackInputs[i].mappedInput = btObj->getProperty("mappedInput");
                        backingTrackInputs[i].leftSelection = btObj->getProperty("leftSelection").toString();
                        backingTrackInputs[i].rightSelection = btObj->getProperty("rightSelection").toString();
                        if (btObj->hasProperty("gain")) backingTrackInputs[i].gain = (float)btObj->getProperty("gain");
                        else backingTrackInputs[i].gain = 1.0f;
                    }
                }
            }
            LOG_INFO("IOSettingsManager: Load Complete. Driver=" + lastSpecificDriver);
            return true;
        }
    }
    catch (...) {
        LOG_ERROR("IOSettingsManager: Failed to parse JSON");
    }
    return false;
}

juce::String IOSettingsManager::getLastMicInput(int micIndex) const { if (micIndex >= 0 && micIndex < 2) return lastMicInputs[micIndex]; return "OFF";
}
IOSettingsManager::BackingTrackInputState IOSettingsManager::getBackingTrackInput(int index) const { if (index >= 0 && index < 9) return backingTrackInputs[index]; return BackingTrackInputState();
}
bool IOSettingsManager::hasExistingSettings() const { return getSettingsFile().existsAsFile(); }
juce::File IOSettingsManager::getSettingsFile() const {
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto onstageDir = appDataDir.getChildFile("OnStage");
    if (!onstageDir.exists()) onstageDir.createDirectory();
    return onstageDir.getChildFile("io_settings.json");
}

bool IOSettingsManager::saveToFile()
{
    auto file = getSettingsFile();
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("driverType", lastDriverType);
    obj->setProperty("specificDriver", lastSpecificDriver);
    obj->setProperty("mediaFolder", lastMediaFolder);
    obj->setProperty("playlistFolder", lastPlaylistFolder);
    obj->setProperty("latencyMs", lastLatencyMs);
    obj->setProperty("vocalBoostDb", lastVocalBoostDb);
    obj->setProperty("midiDevice", lastMidiDevice);

    juce::Array<juce::var> mics;
    for (int i = 0; i < 2; ++i) mics.add(lastMicInputs[i]);
    obj->setProperty("micInputs", mics);
    
    juce::Array<juce::var> outputs;
    for (auto& output : lastOutputs) outputs.add(output);
    obj->setProperty("outputs", outputs);

    juce::Array<juce::var> backing;
    for (int i = 0; i < 9; ++i) {
        juce::DynamicObject::Ptr btObj = new juce::DynamicObject();
        btObj->setProperty("enabled", backingTrackInputs[i].enabled);
        btObj->setProperty("mappedInput", backingTrackInputs[i].mappedInput);
        btObj->setProperty("leftSelection", backingTrackInputs[i].leftSelection);
        btObj->setProperty("rightSelection", backingTrackInputs[i].rightSelection);
        btObj->setProperty("gain", backingTrackInputs[i].gain);
        backing.add(btObj.get());
    }
    obj->setProperty("backingTrackInputs", backing);

    // Use juce::var() to convert DynamicObject::Ptr correctly
    file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    return true;
}