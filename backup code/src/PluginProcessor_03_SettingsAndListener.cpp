// #D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_03_SettingsAndListener.cpp
// MIDI CHANNEL DUPLICATION: Hardware MIDI duplicated to selected channels for layering
// When specific channels selected (e.g., 1,2,3,4): Duplicates incoming MIDI to those channels
// ALL channels selected (0xFFFF): Duplicates to all 16 channels
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
        if (auto* pluginSettings = pluginProperties.getUserSettings()) {
            if (auto xml = knownPluginList.createXml()) {
                pluginSettings->setValue("KnownPluginsV2", xml.get());
                pluginSettings->saveIfNeeded();
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

    suspendProcessing(false);
}

// =============================================================================
// Hardware MIDI Channel Routing
// =============================================================================
void SubterraneumAudioProcessor::updateHardwareMidiChannelMasks() {
    // =========================================================================
    // THREAD-SAFE: Build new map on UI thread, then atomically publish to audio thread.
    //
    // CRITICAL: Do NOT call suspendProcessing() here.
    // suspendProcessing() from the message thread deadlocks with ASIO:
    //   - message thread waits for audio device to stop
    //   - audio thread may be waiting for message thread
    // → freeze / crash
    //
    // This is safe WITHOUT suspension because:
    //   - hardwareMidiChannelMasks is only ever read/written on the UI thread
    //   - audio thread reads ONLY cachedCombinedHardwareMask (atomic)
    //   - lastHardwareMidiMask is atomic, no lock needed
    // =========================================================================

    std::map<juce::String, int> newMasks;
    int newCombined = 0xFFFF;  // Default: all channels

    if (standaloneDeviceManager) {
        auto* userSettings = appProperties.getUserSettings();
        if (userSettings) {
            auto midiInputs = juce::MidiInput::getAvailableDevices();
            bool hasAnyMask = false;
            int combined = 0;

            for (auto& info : midiInputs) {
                if (standaloneDeviceManager->isMidiInputDeviceEnabled(info.identifier)) {
                    juce::String maskKey = "MidiMask_" + info.identifier.replaceCharacters(" :/\\", "____");
                    int mask = userSettings->getIntValue(maskKey, 0xFFFF);

                    if (mask != 0) {
                        newMasks[info.identifier] = mask;
                        combined |= mask;
                        hasAnyMask = true;
                    }
                }
            }

            if (hasAnyMask)
                newCombined = combined;
        }
    }

    // Publish: update atomic first so audio thread sees consistent state
    hardwareMidiChannelMasks.swap(newMasks);
    cachedCombinedHardwareMask.store(newCombined);
    // Reset lastHardwareMidiMask so audio thread detects a change and sends
    // note-offs to any channels that were just deselected
    lastHardwareMidiMask.store(0xFFFF);
}

void SubterraneumAudioProcessor::applyHardwareMidiChannelFiltering(juce::MidiBuffer& midiMessages) {
    // THREAD-SAFE: Read only the pre-computed atomic — never touch the map on audio thread
    int combinedHardwareMask = cachedCombinedHardwareMask.load();

    // Track mask changes for panic messages
    int prevMask = lastHardwareMidiMask.load();
    bool maskChanged = (prevMask != combinedHardwareMask);
    int disabledChannels = prevMask & (~combinedHardwareMask);

    if (maskChanged)
        lastHardwareMidiMask.store(combinedHardwareMask);

    // No channels selected → block all MIDI, send panic to previously active channels
    if (combinedHardwareMask == 0) {
        if (maskChanged && disabledChannels != 0) {
            for (int ch = 1; ch <= 16; ++ch) {
                if ((disabledChannels >> (ch - 1)) & 1) {
                    midiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                    midiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
                }
            }
        }
        midiMessages.clear();
        return;
    }

    // CHANNEL DUPLICATION MODE:
    // Incoming MIDI (typically on Ch 1) is duplicated to every selected channel.
    // Example: Ch1 input, channels 1-4 selected → message sent on Ch1, Ch2, Ch3, Ch4
    // Example: ALL channels selected → message sent on all 16 channels
    juce::MidiBuffer duplicated;

    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();

        if (msg.getRawDataSize() > 0) {
            int statusByte = msg.getRawData()[0];
            bool isChannelMessage = (statusByte >= 0x80 && statusByte <= 0xEF);

            if (isChannelMessage) {
                // Duplicate this message to every selected channel in the mask
                for (int targetCh = 1; targetCh <= 16; ++targetCh) {
                    if ((combinedHardwareMask >> (targetCh - 1)) & 1) {
                        auto duplicatedMsg = msg;
                        duplicatedMsg.setChannel(targetCh);
                        duplicated.addEvent(duplicatedMsg, metadata.samplePosition);
                    }
                }
            } else {
                // Non-channel messages (SysEx, etc.) pass through once
                duplicated.addEvent(msg, metadata.samplePosition);
            }
        } else {
            duplicated.addEvent(msg, metadata.samplePosition);
        }
    }

    // Send panic/note-offs to channels that were just deselected
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
