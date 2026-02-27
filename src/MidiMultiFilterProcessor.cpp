// MidiMultiFilterProcessor.cpp
// MIDI Multi Filter implementation
// FIX: No audio buses - MIDI only (matches Latcher/CCStepperProcessor pattern)
// FIX: Removed rejected/filtered output buffer - single MIDI output only
// NEW: passThrough flag for P button bypass

#include "MidiMultiFilterProcessor.h"
#include "MidiMultiFilterEditorComponent.h"

MidiMultiFilterProcessor::MidiMultiFilterProcessor()
    : AudioProcessor(BusesProperties())  // No audio buses - MIDI only
{
    // Default: all filters disabled (full pass-through)
}

void MidiMultiFilterProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
}

bool MidiMultiFilterProcessor::isBusesLayoutSupported(const BusesLayout& /*layouts*/) const
{
    // Accept any layout — we're MIDI-only, audio buses are irrelevant
    return true;
}

juce::AudioProcessorEditor* MidiMultiFilterProcessor::createEditor()
{
    return new MidiMultiFilterEditor(*this);
}

void MidiMultiFilterProcessor::parseNoteFilterText()
{
    noteFilterSet.clear();
    
    juce::StringArray tokens;
    tokens.addTokens(noteFilterText, ",", "");
    
    for (const auto& token : tokens)
    {
        juce::String t = token.trim();
        if (t.isEmpty()) continue;
        
        if (t.contains("-"))
        {
            // Range: "23-35"
            int dashPos = t.indexOf("-");
            int start = t.substring(0, dashPos).trim().getIntValue();
            int end = t.substring(dashPos + 1).trim().getIntValue();
            
            start = juce::jlimit(0, 127, start);
            end = juce::jlimit(0, 127, end);
            
            if (start > end) std::swap(start, end);
            
            for (int n = start; n <= end; ++n)
                noteFilterSet.insert(n);
        }
        else
        {
            // Single note: "60"
            int note = t.getIntValue();
            if (note >= 0 && note <= 127)
                noteFilterSet.insert(note);
        }
    }
}

bool MidiMultiFilterProcessor::shouldPassMessage(const juce::MidiMessage& msg)
{
    if (!messageFilterEnabled)
        return true;  // Filter disabled = pass all
    
    if (msg.isNoteOn())           return passNoteOn;
    if (msg.isNoteOff())          return passNoteOff;
    if (msg.isAftertouch())       return passPolyPressure;
    if (msg.isController())       return passCC;
    if (msg.isProgramChange())    return passProgramChange;
    if (msg.isChannelPressure())  return passChannelPressure;
    if (msg.isPitchWheel())       return passPitchBend;
    if (msg.isSysEx())            return passSysex;
    
    return true;  // Unknown message types pass through
}

bool MidiMultiFilterProcessor::shouldPassChannel(int channel)
{
    if (!channelFilterEnabled)
        return true;  // Filter disabled = pass all
    
    // channel is 1-16 from MIDI, array is 0-15
    int idx = channel - 1;
    if (idx < 0 || idx >= 16)
        return true;
    
    return channelPass[idx];
}

bool MidiMultiFilterProcessor::shouldPassNote(int noteNumber)
{
    if (!noteFilterEnabled)
        return true;  // Filter disabled = pass all
    
    if (noteFilterSet.empty())
        return true;  // No notes specified = pass all
    
    bool inSet = noteFilterSet.count(noteNumber) > 0;
    
    if (noteFilterPassOnly)
        return inSet;      // Pass only = must be in set
    else
        return !inSet;     // Filter out = must NOT be in set
}

juce::MidiMessage MidiMultiFilterProcessor::processVelocity(const juce::MidiMessage& msg)
{
    if (!velocityEnabled)
        return msg;
    
    if (!msg.isNoteOn())
        return msg;
    
    int velocity = msg.getVelocity();
    
    if (velocityFixedMode)
    {
        // Fixed velocity mode
        velocity = velocityFixed;
    }
    else
    {
        // Min/Max limiter mode
        if (velocity < velocityMin)
            velocity = velocityMin;
        if (velocity > velocityMax)
            velocity = velocityMax;
    }
    
    velocity = juce::jlimit(1, 127, velocity);
    
    return juce::MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(), (juce::uint8)velocity);
}

void MidiMultiFilterProcessor::processBlock(juce::AudioBuffer<float>& /*buffer*/, juce::MidiBuffer& midiMessages)
{
    // Pass-through mode: skip all filtering
    if (passThrough)
        return;
    
    juce::MidiBuffer passedMessages;
    
    int64_t currentTimeMs = juce::Time::currentTimeMillis();
    
    // Process pending Note Offs from delay feature
    if (delayEnabled)
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        
        auto it = pendingNoteOffs.begin();
        while (it != pendingNoteOffs.end())
        {
            if (currentTimeMs >= it->triggerTimeMs)
            {
                // Time to send Note Off
                auto noteOff = juce::MidiMessage::noteOff(it->channel, it->noteNumber);
                passedMessages.addEvent(noteOff, 0);
                it = pendingNoteOffs.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    
    // Process incoming messages
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        bool shouldPass = true;
        
        // 1. Message type filter
        if (!shouldPassMessage(msg))
        {
            shouldPass = false;
        }
        
        // 2. Channel filter (for channel messages only)
        if (shouldPass && msg.getChannel() > 0)
        {
            if (!shouldPassChannel(msg.getChannel()))
            {
                shouldPass = false;
            }
        }
        
        // 3. Note filter (for note messages only)
        if (shouldPass && (msg.isNoteOn() || msg.isNoteOff()))
        {
            if (!shouldPassNote(msg.getNoteNumber()))
            {
                shouldPass = false;
            }
        }
        
        // 4. Delay processing (for Note On/Off)
        if (shouldPass && delayEnabled)
        {
            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                // Schedule a Note Off for later
                std::lock_guard<std::mutex> lock(pendingMutex);
                PendingNoteOff pending;
                pending.noteNumber = msg.getNoteNumber();
                pending.channel = msg.getChannel();
                pending.triggerTimeMs = currentTimeMs + delayMs;
                pendingNoteOffs.push_back(pending);
                
                // Pass the Note On through (after velocity processing)
            }
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                // Block original Note Off - we'll generate our own after delay
                continue;
            }
        }
        
        // 5. Velocity processing
        juce::MidiMessage processedMsg = msg;
        if (shouldPass && msg.isNoteOn())
        {
            processedMsg = processVelocity(msg);
        }
        
        // Route to output (only passed messages)
        if (shouldPass)
        {
            passedMessages.addEvent(processedMsg, metadata.samplePosition);
        }
    }
    
    // Replace input with passed messages
    midiMessages.swapWith(passedMessages);
}

void MidiMultiFilterProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("MidiMultiFilterState");
    
    // Pass-through
    state.setProperty("passThrough", passThrough, nullptr);
    
    // Tab enables
    state.setProperty("messageFilterEnabled", messageFilterEnabled, nullptr);
    state.setProperty("channelFilterEnabled", channelFilterEnabled, nullptr);
    state.setProperty("noteFilterEnabled", noteFilterEnabled, nullptr);
    state.setProperty("delayEnabled", delayEnabled, nullptr);
    state.setProperty("velocityEnabled", velocityEnabled, nullptr);
    
    // Message filter
    state.setProperty("passNoteOn", passNoteOn, nullptr);
    state.setProperty("passNoteOff", passNoteOff, nullptr);
    state.setProperty("passPolyPressure", passPolyPressure, nullptr);
    state.setProperty("passCC", passCC, nullptr);
    state.setProperty("passProgramChange", passProgramChange, nullptr);
    state.setProperty("passChannelPressure", passChannelPressure, nullptr);
    state.setProperty("passPitchBend", passPitchBend, nullptr);
    state.setProperty("passSysex", passSysex, nullptr);
    
    // Channel filter
    for (int i = 0; i < 16; ++i)
        state.setProperty("ch" + juce::String(i + 1), channelPass[i], nullptr);
    
    // Note filter
    state.setProperty("noteFilterPassOnly", noteFilterPassOnly, nullptr);
    state.setProperty("noteFilterText", noteFilterText, nullptr);
    
    // Delay
    state.setProperty("delayMs", delayMs, nullptr);
    
    // Velocity
    state.setProperty("velocityFixedMode", velocityFixedMode, nullptr);
    state.setProperty("velocityMin", velocityMin, nullptr);
    state.setProperty("velocityMax", velocityMax, nullptr);
    state.setProperty("velocityFixed", velocityFixed, nullptr);
    
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void MidiMultiFilterProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, sizeInBytes);
    if (!state.isValid()) return;
    
    // Pass-through
    passThrough = state.getProperty("passThrough", false);
    
    // Tab enables
    messageFilterEnabled = state.getProperty("messageFilterEnabled", false);
    channelFilterEnabled = state.getProperty("channelFilterEnabled", false);
    noteFilterEnabled = state.getProperty("noteFilterEnabled", false);
    delayEnabled = state.getProperty("delayEnabled", false);
    velocityEnabled = state.getProperty("velocityEnabled", false);
    
    // Message filter
    passNoteOn = state.getProperty("passNoteOn", true);
    passNoteOff = state.getProperty("passNoteOff", true);
    passPolyPressure = state.getProperty("passPolyPressure", true);
    passCC = state.getProperty("passCC", true);
    passProgramChange = state.getProperty("passProgramChange", true);
    passChannelPressure = state.getProperty("passChannelPressure", true);
    passPitchBend = state.getProperty("passPitchBend", true);
    passSysex = state.getProperty("passSysex", true);
    
    // Channel filter
    for (int i = 0; i < 16; ++i)
        channelPass[i] = state.getProperty("ch" + juce::String(i + 1), true);
    
    // Note filter
    noteFilterPassOnly = state.getProperty("noteFilterPassOnly", true);
    noteFilterText = state.getProperty("noteFilterText", "").toString();
    parseNoteFilterText();
    
    // Delay
    delayMs = state.getProperty("delayMs", 500);
    
    // Velocity
    velocityFixedMode = state.getProperty("velocityFixedMode", false);
    velocityMin = state.getProperty("velocityMin", 1);
    velocityMax = state.getProperty("velocityMax", 127);
    velocityFixed = state.getProperty("velocityFixed", 100);
}
