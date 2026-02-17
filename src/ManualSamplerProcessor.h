// #D:\Workspace\Subterraneum_plugins_daw\src\ManualSamplerProcessor.h
// MANUAL SAMPLER - Armed recording triggered by incoming MIDI note-on
// Records audio input, stops on silence detection after note activity
// File naming: {family}_{NoteName}{Octave}_V{velocity}.wav

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class ManualSamplerProcessor : public juce::AudioProcessor {
public:
    ManualSamplerProcessor();
    ~ManualSamplerProcessor() override;

    const juce::String getName() const override { return "Manual Sampling"; }

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
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // =========================================================================
    // Armed State
    // =========================================================================
    void setArmed(bool armed) { isArmed.store(armed); }
    bool getArmed() const { return isArmed.load(); }

    // =========================================================================
    // Family Name (base filename)
    // =========================================================================
    void setFamilyName(const juce::String& name) { familyName = name; }
    juce::String getFamilyName() const { return familyName; }

    // =========================================================================
    // Recording Folder
    // =========================================================================
    void setRecordingFolder(const juce::File& folder) { recordingFolder = folder; }
    juce::File getRecordingFolder() const { return recordingFolder; }
    void openRecordingFolder() const;

    // =========================================================================
    // Global Default Folder (separate from Recorder)
    // =========================================================================
    static void setGlobalDefaultFolder(const juce::File& folder);
    static juce::File getGlobalDefaultFolder();
    static juce::File getEffectiveDefaultFolder();

    // =========================================================================
    // Silence Detection Settings
    // =========================================================================
    void setSilenceThresholdDb(float db) { silenceThresholdDb.store(db); }
    float getSilenceThresholdDb() const { return silenceThresholdDb.load(); }

    void setSilenceDurationMs(float ms) { silenceDurationMs.store(ms); }
    float getSilenceDurationMs() const { return silenceDurationMs.load(); }

    // =========================================================================
    // Status
    // =========================================================================
    enum State { IDLE, RECORDING, WAITING_SILENCE };
    State getCurrentState() const { return state.load(); }
    bool isCurrentlyRecording() const { return state.load() != IDLE; }
    int getLastRecordedNote() const { return lastRecordedNote.load(); }
    int getLastRecordedVelocity() const { return lastRecordedVelocity.load(); }
    int getTotalFilesRecorded() const { return totalFilesRecorded.load(); }

    // =========================================================================
    // Waveform / Metering (same as Recorder)
    // =========================================================================
    struct WaveformSample {
        float minL, maxL, minR, maxR;
    };

    std::vector<WaveformSample> getWaveformData(int numSamples) const;
    float getLeftLevel() const { return leftLevel.load(); }
    float getRightLevel() const { return rightLevel.load(); }
    double getRecordingLengthSeconds() const;

    // =========================================================================
    // Note Name Helper
    // =========================================================================
    static juce::String midiNoteToName(int noteNumber);

    static constexpr const char* getIdentifier() { return "ManualSampler"; }

private:
    bool startNewRecording(int note, int velocity);
    void stopAndSaveRecording();
    juce::File buildFilePath(int note, int velocity) const;

    // State machine
    std::atomic<State> state { IDLE };
    std::atomic<bool> isArmed { false };

    // Current recording info
    int currentNote = 0;
    int currentVelocity = 0;
    std::atomic<int> lastRecordedNote { -1 };
    std::atomic<int> lastRecordedVelocity { -1 };
    std::atomic<int> totalFilesRecorded { 0 };

    // Silence detection
    std::atomic<float> silenceThresholdDb { -60.0f };
    std::atomic<float> silenceDurationMs { 500.0f };
    int silenceSampleCount = 0;
    bool hasReceivedAudio = false;  // Don't detect silence before sound arrives

    // Family name and folder
    juce::String familyName { "Sample" };
    juce::File recordingFolder;

    // Recording output
    double currentSampleRate = 44100.0;
    std::atomic<int64_t> samplesRecorded { 0 };
    juce::TimeSliceThread writerThread { "ManualSamplerWriter" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;

    // Level metering
    std::atomic<float> leftLevel { 0.0f };
    std::atomic<float> rightLevel { 0.0f };

    // Waveform ring buffer
    static constexpr int waveformBufferSize = 1024;
    std::array<WaveformSample, waveformBufferSize> waveformBuffer;
    std::atomic<int> waveformWritePos { 0 };
    int waveformDownsampleCounter = 0;
    static constexpr int waveformDownsampleFactor = 256;
    float currentMinL = 0.0f, currentMaxL = 0.0f;
    float currentMinR = 0.0f, currentMaxR = 0.0f;

    // Static global default folder
    static juce::File globalDefaultFolder;
    static juce::SpinLock folderLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ManualSamplerProcessor)
};
