// #D:\Workspace\Subterraneum_plugins_daw\src\MeteringProcessor.cpp
// COMPLETE FIX: ALL getPluginDescription() calls replaced with cachedIsInstrument
// getPluginDescription() freezes some plugins (like SOLO by Taqs.im) when called!

#include "PluginProcessor.h"

void MeteringProcessor::sendAllNotesOffToPlugin() {
    if (!innerPlugin) return;
    
    juce::MidiBuffer panicBuffer;
    for (int ch = 1; ch <= 16; ++ch) {
        panicBuffer.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        panicBuffer.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
        panicBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 120, 0), 0);
        panicBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
    }
    
    juce::AudioBuffer<float> silentBuffer(2, 512);
    silentBuffer.clear();
    
    for (int i = 0; i < 3; ++i) {
        juce::MidiBuffer bufferCopy = panicBuffer;
        try {
            innerPlugin->processBlock(silentBuffer, bufferCopy);
        } catch (...) {}
    }
}

void MeteringProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    if (innerPlugin) {
        // =========================================================================
        // CRITICAL FIX: Use cachedIsInstrument instead of getPluginDescription()
        // getPluginDescription() freezes some plugins when called!
        // =========================================================================
        bool isInst = cachedIsInstrument;
        
        BusesLayout innerLayout;
        
        if (isInst) {
            // INSTRUMENTS: No audio inputs
        } else {
            innerLayout.inputBuses.add(juce::AudioChannelSet::stereo());
            if (hasSidechainBus) {
                innerLayout.inputBuses.add(juce::AudioChannelSet::stereo());
            }
        }
        
        innerLayout.outputBuses.add(juce::AudioChannelSet::stereo());
        innerPlugin->setBusesLayout(innerLayout);
        
        if (!isInst && hasSidechainBus) {
            if (auto* scBus = innerPlugin->getBus(true, 1)) {
                scBus->enable(true);
            }
        }
        
        innerPlugin->setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
        innerPlugin->prepareToPlay(sampleRate, samplesPerBlock);
        
        int maxChannels = juce::jmax(innerPlugin->getTotalNumInputChannels(),
                                      innerPlugin->getTotalNumOutputChannels(), 4);
        internalBuffer.setSize(maxChannels, samplesPerBlock);
    }
    
    // CRITICAL: Reset tracking state on prepare
    lastReceivedNotes.clearQuick();
    previousMidiChannelMask = midiChannelMask.load();
    sentPanicWhenFrozen = false;
    midiInActiveAtomic.store(false);
    midiOutActiveAtomic.store(false);
}

void MeteringProcessor::releaseResources() {
    sendAllNotesOffToPlugin();
    
    if (innerPlugin) innerPlugin->releaseResources(); 
    internalBuffer.setSize(0, 0);
    
    lastReceivedNotes.clearQuick();
    midiInActiveAtomic.store(false);
    midiOutActiveAtomic.store(false);
}

void MeteringProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    if (buffer.getNumSamples() == 0) return;
    
    // =========================================================================
    // CRITICAL FIX: Use cachedIsInstrument instead of getPluginDescription()
    // getPluginDescription() freezes some plugins when called!
    // This is called thousands of times per second - MUST use cached value!
    // =========================================================================
    
    // FIX #1: Use per-instance member variable instead of static
    if (frozen.load()) {
        if (!sentPanicWhenFrozen) {
            sendAllNotesOffToPlugin();
            sentPanicWhenFrozen = true;
            lastReceivedNotes.clearQuick();
        }
        
        // CRITICAL FIX: Use cached value
        if (innerPlugin && cachedIsInstrument) {
            buffer.clear();
        }
        midiMessages.clear();
        midiInActiveAtomic.store(false);
        midiOutActiveAtomic.store(false);
        return;
    } else {
        sentPanicWhenFrozen = false;
    }
    
    // MIDI activity tracking
    if (!midiMessages.isEmpty()) {
        midiInActiveAtomic.store(true);
        midiInDecay = 1.0f;
    } else {
        midiInDecay *= 0.9f;
        if (midiInDecay < 0.01f) midiInActiveAtomic.store(false);
    }

    // =========================================================================
    // FIX #2 & #3: Proper MIDI channel filtering with mask change detection
    // CRITICAL FIX: Use cached value instead of getPluginDescription()
    // =========================================================================
    bool isInstrument = cachedIsInstrument;
    int currentMask = midiChannelMask.load();
    
    if (isInstrument) {
        // Detect mask changes and find notes to kill
        juce::Array<int> notesToKill;
        
        if (currentMask != previousMidiChannelMask) {
            int disabledChannels = previousMidiChannelMask & (~currentMask);
            
            if (disabledChannels != 0) {
                for (int noteKey : lastReceivedNotes) {
                    int noteChannel = (noteKey >> 8) & 0xFF;
                    if ((disabledChannels >> (noteChannel - 1)) & 1) {
                        notesToKill.add(noteKey);
                    }
                }
            }
            
            // If ALL channels disabled, full panic
            if (currentMask == 0) {
                sendAllNotesOffToPlugin();
                lastReceivedNotes.clearQuick();
                midiMessages.clear();
                previousMidiChannelMask = currentMask;
                return;
            }
            
            previousMidiChannelMask = currentMask;
        }
        
        // Filter MIDI based on channel mask
        if (currentMask != 0xFFFF) {
            juce::MidiBuffer filtered;
            
            for (const auto metadata : midiMessages) {
                auto msg = metadata.getMessage();
                
                if (msg.getRawDataSize() > 0) {
                    int statusByte = msg.getRawData()[0];
                    bool isChannelMessage = (statusByte >= 0x80 && statusByte <= 0xEF);
                    
                    if (isChannelMessage) {
                        int ch = msg.getChannel();
                        bool channelAllowed = (currentMask >> (ch - 1)) & 1;
                        
                        if (channelAllowed) {
                            filtered.addEvent(msg, metadata.samplePosition);
                            
                            if (msg.isNoteOn()) {
                                int noteKey = (ch << 8) | msg.getNoteNumber();
                                lastReceivedNotes.addIfNotAlreadyThere(noteKey);
                            }
                            else if (msg.isNoteOff()) {
                                int noteKey = (ch << 8) | msg.getNoteNumber();
                                lastReceivedNotes.removeFirstMatchingValue(noteKey);
                            }
                        }
                    } else {
                        filtered.addEvent(msg, metadata.samplePosition);
                    }
                } else {
                    filtered.addEvent(msg, metadata.samplePosition);
                }
            }
            
            // *** CRITICAL FIX #3 ***
            // Add note-offs AFTER filtering to the filtered buffer!
            for (int noteKey : notesToKill) {
                int noteChannel = (noteKey >> 8) & 0xFF;
                int noteNumber = noteKey & 0xFF;
                filtered.addEvent(juce::MidiMessage::noteOff(noteChannel, noteNumber, (uint8)0), 0);
                lastReceivedNotes.removeFirstMatchingValue(noteKey);
            }
            
            midiMessages.swapWith(filtered);
        } else {
            // All channels enabled - just track notes
            for (const auto metadata : midiMessages) {
                auto msg = metadata.getMessage();
                if (msg.isNoteOn()) {
                    int ch = msg.getChannel();
                    int noteKey = (ch << 8) | msg.getNoteNumber();
                    lastReceivedNotes.addIfNotAlreadyThere(noteKey);
                } else if (msg.isNoteOff()) {
                    int ch = msg.getChannel();
                    int noteKey = (ch << 8) | msg.getNoteNumber();
                    lastReceivedNotes.removeFirstMatchingValue(noteKey);
                }
            }
            
            // Still send note-offs if we have notes to kill
            for (int noteKey : notesToKill) {
                int noteChannel = (noteKey >> 8) & 0xFF;
                int noteNumber = noteKey & 0xFF;
                midiMessages.addEvent(juce::MidiMessage::noteOff(noteChannel, noteNumber, (uint8)0), 0);
                lastReceivedNotes.removeFirstMatchingValue(noteKey);
            }
        }
    }

    // CRITICAL FIX: Use cached value instead of getPluginDescription()
    bool isEffect = innerPlugin && !cachedIsInstrument;
    bool shouldProcess = innerPlugin && !pluginCrashed && !(isEffect && passThrough.load());
    
    if (shouldProcess) {
        try {
            int numSamples = buffer.getNumSamples();
            int bufferChannels = buffer.getNumChannels();
            int innerInputs = innerPlugin->getTotalNumInputChannels();
            int innerOutputs = innerPlugin->getTotalNumOutputChannels();
            
            int neededChannels = juce::jmax(innerInputs, innerOutputs, 4);
            if (internalBuffer.getNumChannels() < neededChannels || 
                internalBuffer.getNumSamples() < numSamples) {
                internalBuffer.setSize(neededChannels, numSamples, false, false, true);
            }
            
            internalBuffer.clear();
            
            if (isEffect) {
                int mainChans = juce::jmin(2, bufferChannels, innerInputs);
                for (int ch = 0; ch < mainChans; ++ch) {
                    internalBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                }
                
                if (hasSidechainBus && sidechainActive.load() && bufferChannels >= 4 && innerInputs >= 4) {
                    for (int ch = 0; ch < 2; ++ch) {
                        int srcCh = 2 + ch;
                        int dstCh = 2 + ch;
                        if (srcCh < bufferChannels && dstCh < innerInputs) {
                            internalBuffer.copyFrom(dstCh, 0, buffer, srcCh, 0, numSamples);
                        }
                    }
                }
            }
            
            int processChannels = juce::jmax(innerInputs, innerOutputs);
            juce::AudioBuffer<float> processBuffer(
                internalBuffer.getArrayOfWritePointers(), 
                processChannels, 
                numSamples
            );
            
            innerPlugin->processBlock(processBuffer, midiMessages);
            
            int outputChans = juce::jmin(2, innerOutputs, bufferChannels);
            for (int ch = 0; ch < outputChans; ++ch) {
                buffer.copyFrom(ch, 0, processBuffer, ch, 0, numSamples);
            }
            
            for (int ch = outputChans; ch < 2 && ch < bufferChannels; ++ch) {
                buffer.clear(ch, 0, numSamples);
            }
            
        } catch (...) {
            pluginCrashed = true;
            buffer.clear();
        }
    }
    
    if (pluginCrashed) {
        buffer.clear();
    }
    
    // CRITICAL FIX: Use cached value instead of getPluginDescription()
    if (innerPlugin && cachedIsInstrument) {
        float g = gain.load();
        if (g != 1.0f) {
            buffer.applyGain(g);
        }
    }

    if (innerPlugin && !midiMessages.isEmpty() && innerPlugin->producesMidi()) {
        midiOutActiveAtomic.store(true);
        midiOutDecay = 1.0f;
    } else {
        midiOutDecay *= 0.9f;
        if (midiOutDecay < 0.01f) midiOutActiveAtomic.store(false);
    }
}
