
// #D:\Workspace\Subterraneum_plugins_daw\src\MidiMonitorProcessor.cpp
// MIDI Monitor - Real-time MIDI event display
// FIX: Simple numeric format (144, 1, 60, 100) instead of text
// FIX: Change tracking for efficient repainting

#include "MidiMonitorProcessor.h"

MidiMonitorProcessor::MidiMonitorProcessor()
    : AudioProcessor(BusesProperties())  // No audio buses, MIDI only
{
    // Initialize all event slots as inactive
    for (auto& event : midiEvents) {
        event.isActive = false;
    }
}

void MidiMonitorProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {
    // Nothing to prepare - we just process MIDI
}

void MidiMonitorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    // Clear audio buffer (we don't process audio)
    buffer.clear();
    
    bool anyChanges = false;
    
    // Process all incoming MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        
        // Process Note On/Off and CC messages
        if (message.isNoteOn() || message.isNoteOff())
        {
            int channel = message.getChannel();  // 1-16
            int noteNumber = message.getNoteNumber();  // 0-127
            int velocity = message.getVelocity();  // 0-127
            
            bool isNoteOff = message.isNoteOff();
            
            juce::ScopedLock lock(midiEventsLock);
            
            if (channel >= 1 && channel <= 16)
            {
                auto& event = midiEvents[channel - 1];
                event.isActive = true;
                event.isNoteOn = !isNoteOff;
                event.isCC = false;
                event.channel = channel;
                event.noteNumber = noteNumber;
                event.velocity = velocity;
                
                anyChanges = true;
            }
        }
        else if (message.isController())
        {
            int channel = message.getChannel();
            int ccNum = message.getControllerNumber();
            int ccVal = message.getControllerValue();
            
            juce::ScopedLock lock(midiEventsLock);
            
            if (channel >= 1 && channel <= 16)
            {
                auto& event = midiEvents[channel - 1];
                event.isActive = true;
                event.isCC = true;
                event.isNoteOn = false;
                event.channel = channel;
                event.ccNumber = ccNum;
                event.ccValue = ccVal;
                
                anyChanges = true;
            }
        }
    }
    
    // FIX: Set changed flag only if new MIDI arrived
    if (anyChanges) {
        midiEventsChanged.store(true);
    }
    
    // MIDI is consumed (not passed through)
    midiMessages.clear();
}

bool MidiMonitorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // No audio buses required
    return layouts.getMainInputChannelSet().isDisabled() &&
           layouts.getMainOutputChannelSet().isDisabled();
}

std::array<MidiMonitorProcessor::MidiEventInfo, 16> MidiMonitorProcessor::getMidiEvents() const {
    juce::ScopedLock lock(midiEventsLock);
    return midiEvents;  // Returns copy (thread-safe)
}

bool MidiMonitorProcessor::hasActivity() const {
    juce::ScopedLock lock(midiEventsLock);
    for (const auto& event : midiEvents)
    {
        if (event.isActive)
            return true;
    }
    return false;
}

void MidiMonitorProcessor::getStateInformation(juce::MemoryBlock& /*destData*/) {
    // No state to save (display only)
}

void MidiMonitorProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/) {
    // No state to load
}




