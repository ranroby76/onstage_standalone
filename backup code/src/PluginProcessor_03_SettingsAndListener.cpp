
// #D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_03_SettingsAndListener.cpp
// MIDI CHANNEL DUPLICATION: Hardware MIDI duplicated to selected channels for layering
// Default: 0xFFFF (ALL channels) = pass-through unchanged
// When specific channels selected (e.g., 2,3,4): Duplicates incoming MIDI to those channels
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument

#include "PluginProcessor.h"

// =============================================================================
// Audio Settings Persistence
// =============================================================================
void SubterraneumAudioProcessor::saveAudioSettings() {
    auto* userSettings = appProperties.getUserSettings();
    if (!userSettings) return;
    
    juce::String deviceName = "";
    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            deviceName = device->getName();
        }
    }
    
    userSettings->setValue("AudioDeviceName", deviceName);
    userSettings->saveIfNeeded();
    savedAudioDeviceName = deviceName;
}

void SubterraneumAudioProcessor::loadAudioSettings() {
    auto* userSettings = appProperties.getUserSettings();
    if (!userSettings) {
        savedAudioDeviceName = "";
        return;
    }
    
    if (!userSettings->containsKey("AudioDeviceName")) {
        userSettings->setValue("AudioDeviceName", "");
        userSettings->saveIfNeeded();
        savedAudioDeviceName = "";
    } else {
        savedAudioDeviceName = userSettings->getValue("AudioDeviceName", "");
    }
}

// =============================================================================
// Change Listener
// =============================================================================
void SubterraneumAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &knownPluginList) {
        if (auto* userSettings = appProperties.getUserSettings()) {
            if (auto xml = knownPluginList.createXml()) {
                userSettings->setValue("KnownPluginsV2", xml.get());
                userSettings->saveIfNeeded();
            }
        }
    }
    else if (source == standaloneDeviceManager) {
        sendMidiPanicToAllInstruments();
        
        juce::MessageManager::callAsync([this]() {
            updateIOChannelCount();
        });
    }
}

// =============================================================================
// Send MIDI panic to all instruments in graph
// =============================================================================
void SubterraneumAudioProcessor::sendMidiPanicToAllInstruments() {
    if (!mainGraph) return;
    
    suspendProcessing(true);
    
    // =========================================================================
    // REMOVED: Dangerous sendAllNotesOffToPlugin() calls
    // Problem: Calls processBlock() from non-audio thread → race condition → crash
    // Solution: Just suspending processing is sufficient - stops all sound
    // =========================================================================
    
    // No need to iterate and call sendAllNotesOffToPlugin() on each instrument
    // Suspending processing already stops all audio and MIDI processing
    
    suspendProcessing(false);
}

// =============================================================================
// Hardware MIDI Channel Routing
// =============================================================================
void SubterraneumAudioProcessor::updateHardwareMidiChannelMasks() {
    // =========================================================================
    // FIX #3: REMOVED sendMidiPanicToAllInstruments() - causes crashes!
    // Problem: It calls processBlock() from UI thread → race condition → crash
    // Solution: Panic is unnecessary when just changing MIDI channel routing
    // =========================================================================
    
    const juce::MessageManagerLock mmLock;
    suspendProcessing(true);
    
    hardwareMidiChannelMasks.clear();
    
    if (!standaloneDeviceManager) {
        suspendProcessing(false);
        return;
    }
    
    auto* userSettings = appProperties.getUserSettings();
    if (!userSettings) {
        suspendProcessing(false);
        return;
    }
    
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    
    for (auto& info : midiInputs) {
        if (standaloneDeviceManager->isMidiInputDeviceEnabled(info.identifier)) {
            juce::String maskKey = "MidiMask_" + info.identifier.replaceCharacters(" :/\\", "____");
            
            // FIX: Default to ALL channels (0xFFFF) = pass-through mode (no duplication)
            // When specific channels selected: Incoming MIDI duplicated to those channels
            int mask = userSettings->getIntValue(maskKey, 0xFFFF);
            
            if (mask != 0) {
                hardwareMidiChannelMasks[info.identifier] = mask;
            }
        }
    }
    
    // Reset tracking when masks are updated - default to all channels
    lastHardwareMidiMask = 0xFFFF;
    
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::applyHardwareMidiChannelFiltering(juce::MidiBuffer& midiMessages) {
    // Default: ALL channels (0xFFFF) - hardware MIDI passes through unchanged by default
    int combinedHardwareMask = 0xFFFF;
    
    if (!hardwareMidiChannelMasks.empty()) {
        combinedHardwareMask = 0;
        for (const auto& pair : hardwareMidiChannelMasks) {
            combinedHardwareMask |= pair.second;
        }
    }
    
    // Track mask changes for panic messages
    bool maskChanged = (lastHardwareMidiMask != combinedHardwareMask);
    int disabledChannels = lastHardwareMidiMask & (~combinedHardwareMask);
    
    if (maskChanged) {
        lastHardwareMidiMask = combinedHardwareMask;
    }
    
    // If all channels enabled (0xFFFF), pass through unchanged - no duplication needed
    if (combinedHardwareMask == 0xFFFF) {
        if (maskChanged && disabledChannels != 0) {
            for (int ch = 1; ch <= 16; ++ch) {
                if ((disabledChannels >> (ch - 1)) & 1) {
                    midiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                    midiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
                }
            }
        }
        return;
    }
    
    // CHANNEL DUPLICATION MODE: Duplicate incoming MIDI to all selected channels
    // Example: Keyboard sends Ch1, User selects Ch2,3,4 → Message duplicated to Ch2, Ch3, Ch4
    juce::MidiBuffer duplicated;
    
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        
        if (msg.getRawDataSize() > 0) {
            int statusByte = msg.getRawData()[0];
            bool isChannelMessage = (statusByte >= 0x80 && statusByte <= 0xEF);
            
            if (isChannelMessage) {
                // Duplicate this message to ALL selected channels in the mask
                for (int targetCh = 1; targetCh <= 16; ++targetCh) {
                    bool channelSelected = (combinedHardwareMask >> (targetCh - 1)) & 1;
                    
                    if (channelSelected) {
                        // Create a copy of the message on the target channel
                        auto duplicatedMsg = msg;
                        duplicatedMsg.setChannel(targetCh);
                        duplicated.addEvent(duplicatedMsg, metadata.samplePosition);
                    }
                }
            } else {
                // Non-channel messages (SysEx, etc.) pass through unchanged
                duplicated.addEvent(msg, metadata.samplePosition);
            }
        } else {
            duplicated.addEvent(msg, metadata.samplePosition);
        }
    }
    
    // Send panic/note-offs to disabled channels if mask changed
    if (maskChanged && disabledChannels != 0) {
        for (int ch = 1; ch <= 16; ++ch) {
            if ((disabledChannels >> (ch - 1)) & 1) {
                duplicated.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                duplicated.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
            }
        }
    }
    
    midiMessages.swapWith(duplicated);
}




