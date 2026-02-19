// D:\Workspace\ONSTAGE_WIRED\src\dsp\RecorderProcessor.h
// RECORDER SYSTEM TOOL - Independent stereo recording with sync capability
// Streams directly to disk, GL-powered waveform visualization
// OnStage Version: 24-bit recording at device sample rate
//
// FIXES APPLIED:
//   1. Metering/waveform only runs when recording or UI is actively watching
//   2. backgroundWriter race condition fixed with CriticalSection
//   3. writerThread started once in ctor, stopped once in dtor (no per-session churn)
//   4. Waveform ring buffer uses waveformLock consistently (no torn reads)
//   5. Level metering decays to true zero (floor applied)
//   6. Static registry guarded with safer shutdown check

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

// =============================================================================
// RecorderProcessor - Stereo audio recorder (termination point)
// =============================================================================
class RecorderProcessor : public juce::AudioProcessor
{
public:
    RecorderProcessor();
    ~RecorderProcessor() override;

    const juce::String getName() const override { return "Recorder"; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    int  getNumPrograms() override                          { return 1; }
    int  getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // =========================================================================
    // Recording Control
    // =========================================================================
    bool startRecording();
    void stopRecording();
    bool isCurrentlyRecording() const { return isRecording.load (std::memory_order_relaxed); }

    // =========================================================================
    // Sync Mode
    // =========================================================================
    void setSyncMode (bool enabled) { syncMode.store (enabled, std::memory_order_relaxed); }
    bool isSyncMode() const         { return syncMode.load (std::memory_order_relaxed); }

    void triggerSyncedRecording();
    void triggerSyncedStop();

    // =========================================================================
    // Name / Identification
    // =========================================================================
    void setRecorderName (const juce::String& name)  { recorderName = name; }
    juce::String getRecorderName() const             { return recorderName; }

    // =========================================================================
    // Recording Folder (per-instance)
    // =========================================================================
    void setRecordingFolder (const juce::File& folder) { recordingFolder = folder; }
    juce::File getRecordingFolder() const              { return recordingFolder; }

    // =========================================================================
    // Global Default Recording Folder (shared across all recorders)
    // =========================================================================
    static void setGlobalDefaultFolder (const juce::File& folder);
    static juce::File getGlobalDefaultFolder();
    static juce::File getEffectiveDefaultFolder();

    void openRecordingFolder() const;

    // =========================================================================
    // Recording Info
    // =========================================================================
    double getRecordingLengthSeconds() const;
    juce::File getLastRecordingFile() const { return lastRecordingFile; }
    bool hasRecording() const               { return lastRecordingFile.existsAsFile(); }

    // =========================================================================
    // Waveform Data for GL Visualization
    // =========================================================================
    struct WaveformSample
    {
        float minL, maxL;
        float minR, maxR;
    };

    // FIX #1: UI calls this to signal it's actively displaying the waveform.
    //         When no UI is watching and we're not recording, metering is skipped.
    void setWaveformActive (bool active) { waveformActive.store (active, std::memory_order_relaxed); }

    std::vector<WaveformSample> getWaveformData (int numSamples) const;

    float getLeftLevel()  const { return leftLevel.load (std::memory_order_relaxed); }
    float getRightLevel() const { return rightLevel.load (std::memory_order_relaxed); }

    // =========================================================================
    // Static Sync Manager
    // =========================================================================
    static void registerRecorder   (RecorderProcessor* recorder);
    static void unregisterRecorder (RecorderProcessor* recorder);
    static void startAllSyncedRecorders();
    static void stopAllSyncedRecorders();

    static constexpr const char* getIdentifier() { return "Recorder"; }

private:
    // Recording state
    std::atomic<bool> isRecording { false };
    std::atomic<bool> syncMode    { true };

    // FIX #1: Skip metering when nobody cares
    std::atomic<bool> waveformActive { false };

    // Recording output
    juce::File recordingFolder;
    juce::File lastRecordingFile;

    // FIX #3: Thread lives for the entire processor lifetime (no per-session start/stop)
    juce::TimeSliceThread writerThread { "RecorderWriter" };

    // FIX #2: Guard backgroundWriter with a CriticalSection so stopRecording()
    //         on the message thread and processBlock() on the audio thread
    //         cannot race on the pointer.
    juce::CriticalSection writerLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;

    // Name
    juce::String recorderName { "Untitled" };

    // Audio info
    double currentSampleRate = 44100.0;
    std::atomic<int64_t> samplesRecorded { 0 };

    // Level metering
    std::atomic<float> leftLevel  { 0.0f };
    std::atomic<float> rightLevel { 0.0f };

    // Waveform ring buffer for visualization
    static constexpr int waveformBufferSize = 1024;
    std::array<WaveformSample, waveformBufferSize> waveformBuffer;
    std::atomic<int> waveformWritePos { 0 };
    mutable juce::SpinLock waveformLock;  // FIX #4: Actually used now

    // Downsampling for waveform
    int waveformDownsampleCounter = 0;
    static constexpr int waveformDownsampleFactor = 256;
    float currentMinL = 0.0f, currentMaxL = 0.0f;
    float currentMinR = 0.0f, currentMaxR = 0.0f;

    // FIX #6: Static registry with shutdown guard
    static std::atomic<bool> registryShutdown;
    static juce::Array<RecorderProcessor*> syncedRecorders;
    static juce::SpinLock registryLock;

    // Static global default folder
    static juce::File globalDefaultFolder;
    static juce::SpinLock folderLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecorderProcessor)
};
