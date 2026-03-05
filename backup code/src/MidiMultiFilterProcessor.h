// MidiMultiFilterProcessor.h
// MIDI Multi Filter - comprehensive MIDI filtering system tool
// Features: Message type filter, Channel filter, Note filter, Delay, Velocity
// Single MIDI output: passed/filtered messages
// Node buttons: E (editor), P (pass-through), X (delete)

#pragma once

#include <JuceHeader.h>
#include <set>
#include <vector>
#include <mutex>

class MidiMultiFilterProcessor : public juce::AudioProcessor
{
public:
    MidiMultiFilterProcessor();
    ~MidiMultiFilterProcessor() override = default;

    // =========================================================================
    // AudioProcessor interface
    // =========================================================================
    const juce::String getName() const override { return "MIDI Multi Filter"; }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    
    // Editor support - returns true so "e" button appears on node
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    // MIDI processing
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // =========================================================================
    // Global pass-through: when ON, all MIDI passes unfiltered
    // =========================================================================
    bool passThrough = false;

    // =========================================================================
    // Tab Enable/Disable - when OFF, filter passes through
    // =========================================================================
    bool messageFilterEnabled = false;
    bool channelFilterEnabled = false;
    bool noteFilterEnabled = false;
    bool delayEnabled = false;
    bool velocityEnabled = false;

    // =========================================================================
    // Tab 1: Message Type Filter
    // When enabled, only checked message types pass through
    // =========================================================================
    bool passNoteOn = true;
    bool passNoteOff = true;
    bool passPolyPressure = true;
    bool passCC = true;
    bool passProgramChange = true;
    bool passChannelPressure = true;
    bool passPitchBend = true;
    bool passSysex = true;

    // =========================================================================
    // Tab 2: Channel Filter
    // When enabled, only checked channels pass through
    // =========================================================================
    std::array<bool, 16> channelPass = { true, true, true, true, true, true, true, true,
                                          true, true, true, true, true, true, true, true };

    // =========================================================================
    // Tab 3: Note Filter
    // Pass Only = only listed notes pass, Filter Out = listed notes blocked
    // =========================================================================
    bool noteFilterPassOnly = true;
    juce::String noteFilterText = "";
    std::set<int> noteFilterSet;
    
    void parseNoteFilterText();

    // =========================================================================
    // Tab 4: MIDI Delay (Note Off delay / gate)
    // Note On passes immediately, Note Off delayed by delayMs
    // =========================================================================
    int delayMs = 500;
    
    // =========================================================================
    // Tab 5: Velocity
    // Min/Max mode: clamp to range, Fixed mode: all notes same velocity
    // =========================================================================
    bool velocityFixedMode = false;
    int velocityMin = 1;
    int velocityMax = 127;
    int velocityFixed = 100;

    // Identifier for serialization
    static constexpr const char* getIdentifier() { return "MidiMultiFilter"; }

private:
    double currentSampleRate = 44100.0;
    
    // Pending Note Offs for delay feature
    struct PendingNoteOff {
        int noteNumber;
        int channel;
        int64_t triggerTimeMs;
    };
    std::vector<PendingNoteOff> pendingNoteOffs;
    std::mutex pendingMutex;
    
    bool shouldPassMessage(const juce::MidiMessage& msg);
    bool shouldPassChannel(int channel);
    bool shouldPassNote(int noteNumber);
    juce::MidiMessage processVelocity(const juce::MidiMessage& msg);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMultiFilterProcessor)
};
