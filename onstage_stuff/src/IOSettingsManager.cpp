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
    
    micSettings[0].isMuted = false; micSettings[0].isBypassed = false;
    micSettings[1].isMuted = false; micSettings[1].isBypassed = false;
}

void IOSettingsManager::saveDriverType(const juce::String& driverType) { lastDriverType = driverType; saveToFile(); }
void IOSettingsManager::saveSpecificDriver(const juce::String& driverName) { lastSpecificDriver = driverName; saveToFile(); }

void IOSettingsManager::saveMicMute(int micIndex, bool shouldMute) {
    if (micIndex >= 0 && micIndex < 2) { micSettings[micIndex].isMuted = shouldMute; saveToFile(); }
}
void IOSettingsManager::saveMicBypass(int micIndex, bool shouldBypass) {
    if (micIndex >= 0 && micIndex < 2) { micSettings[micIndex].isBypassed = shouldBypass; saveToFile(); }
}

void IOSettingsManager::saveOutputRouting(const std::map<juce::String, int>& routingMap) {
    outputRoutingMap = routingMap;
    saveToFile();
}

void IOSettingsManager::saveInputRouting(const std::map<juce::String, std::pair<int, float>>& routingMap) {
    inputRoutingMap = routingMap;
    saveToFile();
}

std::map<juce::String, std::pair<int, float>> IOSettingsManager::getInputRouting() const {
    return inputRoutingMap;
}

void IOSettingsManager::saveMediaFolder(const juce::String& path) { lastMediaFolder = path; saveToFile(); }
void IOSettingsManager::savePlaylistFolder(const juce::String& path) { lastPlaylistFolder = path; saveToFile(); }
void IOSettingsManager::saveVocalSettings(float latencyMs, float boostDb) { lastLatencyMs = latencyMs; lastVocalBoostDb = boostDb; saveToFile(); }
void IOSettingsManager::saveMidiDevice(const juce::String& deviceName) { lastMidiDevice = deviceName; saveToFile(); }

bool IOSettingsManager::loadSettings()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile()) return false;
    
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

            if (auto* mics = obj->getProperty("micSettings").getArray()) {
                for (int i = 0; i < juce::jmin(2, mics->size()); ++i) {
                    if (auto* micObj = mics->getReference(i).getDynamicObject()) {
                        micSettings[i].isMuted = micObj->getProperty("mute");
                        micSettings[i].isBypassed = micObj->getProperty("bypass");
                    }
                }
            }

            outputRoutingMap.clear();
            if (auto* outputs = obj->getProperty("outputRouting").getArray()) {
                for (auto& item : *outputs) {
                    if (auto* routeObj = item.getDynamicObject()) {
                        outputRoutingMap[routeObj->getProperty("name").toString()] = (int)routeObj->getProperty("mask");
                    }
                }
            }
            
            inputRoutingMap.clear();
            if (auto* inputs = obj->getProperty("inputRouting").getArray()) {
                for (auto& item : *inputs) {
                    if (auto* routeObj = item.getDynamicObject()) {
                        juce::String name = routeObj->getProperty("name").toString();
                        int mask = (int)routeObj->getProperty("mask");
                        float gain = routeObj->hasProperty("gain") ? (float)routeObj->getProperty("gain") : 1.0f;
                        inputRoutingMap[name] = { mask, gain };
                    }
                }
            }
            return true;
        }
    }
    catch (...) {}
    return false;
}

IOSettingsManager::MicSettings IOSettingsManager::getMicSettings(int index) const {
    if (index >= 0 && index < 2) return micSettings[index];
    return MicSettings();
}

bool IOSettingsManager::hasExistingSettings() const { return getSettingsFile().existsAsFile(); }

juce::File IOSettingsManager::getSettingsFile() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("OnStage").getChildFile("io_settings.json");
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
    for (int i = 0; i < 2; ++i) {
        juce::DynamicObject::Ptr micObj = new juce::DynamicObject();
        micObj->setProperty("mute", micSettings[i].isMuted);
        micObj->setProperty("bypass", micSettings[i].isBypassed);
        mics.add(micObj.get());
    }
    obj->setProperty("micSettings", mics);

    juce::Array<juce::var> outputArr;
    for (auto const& [name, mask] : outputRoutingMap) {
        juce::DynamicObject::Ptr routeObj = new juce::DynamicObject();
        routeObj->setProperty("name", name);
        routeObj->setProperty("mask", mask);
        outputArr.add(routeObj.get());
    }
    obj->setProperty("outputRouting", outputArr);
    
    juce::Array<juce::var> inputArr;
    for (auto const& [name, pair] : inputRoutingMap) {
        juce::DynamicObject::Ptr routeObj = new juce::DynamicObject();
        routeObj->setProperty("name", name);
        routeObj->setProperty("mask", pair.first);
        routeObj->setProperty("gain", pair.second);
        inputArr.add(routeObj.get());
    }
    obj->setProperty("inputRouting", inputArr);

    if (!file.getParentDirectory().exists()) file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    return true;
}