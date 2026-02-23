// #D:\Workspace\Subterraneum_plugins_daw\src\LatcherProcessor.h
// THE LATCHER - 4x4 MIDI toggle pad controller system tool
// Each pad has: trigger note (input), output note, velocity, MIDI channel
// Toggle ON = send note-on (latched), toggle OFF = send note-off
// Triggered by mouse click or matching incoming MIDI note
// MIDI-only processor (no audio buses)

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

class LatcherProcessor : public juce::AudioProcessor {
public:
    // =========================================================================
    // Constants
    // =========================================================================
    static constexpr int NumPads = 16;  // 4x4 grid
    static constexpr int GridRows = 4;
    static constexpr int GridCols = 4;

    // =========================================================================
    // Pad - one latch pad
    // =========================================================================
    struct Pad {
        int triggerNote  = 60;    // Incoming MIDI note that toggles this pad (0-127)
        int outputNote   = 60;    // MIDI note to send when latched (0-127)
        int velocity     = 100;   // Velocity of the output note (1-127)
        int midiChannel  = 1;     // Output MIDI channel (1-16)
    };

    // =========================================================================
    // Constructor / Destructor
    // =========================================================================
    LatcherProcessor();
    ~LatcherProcessor() override;

    // =========================================================================
    // AudioProcessor overrides
    // =========================================================================
    const juce::String getName() const override { return "Latcher"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
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

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // =========================================================================
    // Pad access
    // =========================================================================
    Pad& getPad(int index) { return pads[juce::jlimit(0, NumPads - 1, index)]; }
    const Pad& getPad(int index) const { return pads[juce::jlimit(0, NumPads - 1, index)]; }

    // =========================================================================
    // Latch state (thread-safe read for UI)
    // =========================================================================
    bool isPadLatched(int index) const {
        if (index >= 0 && index < NumPads) return padLatched[index].load();
        return false;
    }

    // Toggle pad from UI (mouse click) - queues the toggle for audio thread
    void togglePadFromUI(int index);

    // All notes off - unlatch all pads
    void allNotesOff();

    // =========================================================================
    // Identifier for preset save/load
    // =========================================================================
    static constexpr const char* getIdentifier() { return "Latcher"; }

private:
    std::array<Pad, NumPads> pads;
    std::array<std::atomic<bool>, NumPads> padLatched;

    // UI toggle queue: lock-free single-producer (UI), single-consumer (audio)
    // Each bit represents a pending toggle for that pad index
    std::atomic<uint32_t> pendingToggles { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LatcherProcessor)
};
