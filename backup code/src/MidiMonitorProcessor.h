#pragma once

#include <JuceHeader.h>
#include <array>

// =============================================================================
// MidiMonitorProcessor - Visual MIDI activity monitor module
// Shows real-time MIDI messages in simple numeric format: status, channel, data1, data2
// Example: "144, 1, 60, 100" (Note On, Ch 1, Note 60, Vel 100)
// CC:      "CC 74, Ch 1, Val 64"
// 6x height, 2x width - displays up to 16 channels simultaneously
// MIDI input only, no audio, no MIDI output (MIDI is consumed for display)
// =============================================================================
class MidiMonitorProcessor : public juce::AudioProcessor {
public:
    // Structure to hold MIDI event info for display
    struct MidiEventInfo {
        bool isActive = false;          // Is this slot showing an event?
        bool isNoteOn = false;          // true = Note On, false = Note Off
        bool isCC = false;              // true = CC message
        int channel = 1;                // MIDI channel 1-16
        int noteNumber = 60;            // MIDI note 0-127
        int velocity = 0;               // Velocity 0-127
        int ccNumber = 0;               // CC# 0-127 (when isCC)
        int ccValue = 0;                // CC value 0-127 (when isCC)
        
        juce::String toString() const {
            if (isCC)
            {
                return juce::String::formatted("CC %d, Ch %d, Val %d",
                                              ccNumber, channel, ccValue);
            }
            int statusByte = isNoteOn ? 144 : 128;
            return juce::String::formatted("%d, %d, %d, %d", 
                                          statusByte,
                                          channel, 
                                          noteNumber, 
                                          velocity);
        }
    };

    MidiMonitorProcessor();
    ~MidiMonitorProcessor() override = default;

    const juce::String getName() const override { return "MIDI Monitor"; }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    // MIDI input only (no audio, no MIDI output)
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    
    // Get MIDI events for display (one per channel, up to 16)
    std::array<MidiEventInfo, 16> getMidiEvents() const;
    
    // Check if any MIDI activity is happening
    bool hasActivity() const;
    
    // Track if MIDI events have changed since last read (for efficient repainting)
    bool hasChanged() const { return midiEventsChanged.load(); }
    void clearChanged() { midiEventsChanged.store(false); }
    
    static constexpr const char* getIdentifier() { return "MidiMonitor"; }
    
private:
    // Thread-safe storage for MIDI events (one slot per MIDI channel 1-16)
    std::array<MidiEventInfo, 16> midiEvents;
    juce::CriticalSection midiEventsLock;
    
    // Track when MIDI data changes for efficient repainting
    std::atomic<bool> midiEventsChanged { false };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMonitorProcessor)
};