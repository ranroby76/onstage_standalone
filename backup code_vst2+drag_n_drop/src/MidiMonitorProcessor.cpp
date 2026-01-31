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
        
        // Only process Note On and Note Off messages
        if (message.isNoteOn() || message.isNoteOff())
        {
            int channel = message.getChannel();  // 1-16
            int noteNumber = message.getNoteNumber();  // 0-127
            int velocity = message.getVelocity();  // 0-127
            
            // JUCE's isNoteOff() returns true for both:
            // - Real note-off messages (0x80)
            // - Note-on with velocity=0 (0x90 with vel=0)
            bool isNoteOff = message.isNoteOff();
            
            // Update the event slot for this channel
            juce::ScopedLock lock(midiEventsLock);
            
            if (channel >= 1 && channel <= 16)
            {
                // FIX #4: Sample-and-hold - just update the slot, no fade-out
                auto& event = midiEvents[channel - 1];  // 0-indexed array
                event.isActive = true;
                event.isNoteOn = !isNoteOff;
                event.channel = channel;
                event.noteNumber = noteNumber;
                event.velocity = velocity;
                
                anyChanges = true;  // Mark that we have new data
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
