// D:\Workspace\ONSTAGE_WIRED\src\dsp\RecorderProcessor.h
// RECORDER SYSTEM TOOL - Independent stereo recording with sync capability
// Streams directly to disk, GL-powered waveform visualization
// OnStage Version: 24-bit/44100Hz recording

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

// =============================================================================
// RecorderProcessor - Stereo audio recorder (termination point)
// =============================================================================
class RecorderProcessor : public juce::AudioProcessor {
public:
    RecorderProcessor();
    ~RecorderProcessor() override;

    const juce::String getName() const override { return "Recorder"; }
    
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
    
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    
    // =========================================================================
    // Recording Control
    // =========================================================================
    bool startRecording();
    void stopRecording();
    bool isCurrentlyRecording() const { return isRecording.load(); }
    
    // =========================================================================
    // Sync Mode - When enabled, this recorder will start when any synced recorder starts
    // =========================================================================
    void setSyncMode(bool enabled) { syncMode.store(enabled); }
    bool isSyncMode() const { return syncMode.load(); }
    
    // Called by sync manager to trigger recording on all synced recorders
    void triggerSyncedRecording();
    void triggerSyncedStop();
    
    // =========================================================================
    // Name / Identification
    // =========================================================================
    void setRecorderName(const juce::String& name) { recorderName = name; }
    juce::String getRecorderName() const { return recorderName; }
    
    // =========================================================================
    // Recording Folder (per-instance)
    // =========================================================================
    void setRecordingFolder(const juce::File& folder) { recordingFolder = folder; }
    juce::File getRecordingFolder() const { return recordingFolder; }
    
    // =========================================================================
    // Global Default Recording Folder (shared across all recorders)
    // =========================================================================
    static void setGlobalDefaultFolder(const juce::File& folder);
    static juce::File getGlobalDefaultFolder();
    static juce::File getEffectiveDefaultFolder();  // Returns user-set folder or app default
    
    // Open the recording folder in system file explorer
    void openRecordingFolder() const;
    
    // =========================================================================
    // Recording Info
    // =========================================================================
    double getRecordingLengthSeconds() const;
    juce::File getLastRecordingFile() const { return lastRecordingFile; }
    bool hasRecording() const { return lastRecordingFile.existsAsFile(); }
    
    // =========================================================================
    // Waveform Data for GL Visualization
    // =========================================================================
    struct WaveformSample {
        float minL, maxL;  // Left channel min/max
        float minR, maxR;  // Right channel min/max
    };
    
    // Get recent waveform data for display (thread-safe)
    std::vector<WaveformSample> getWaveformData(int numSamples) const;
    
    // Get current input levels (for live meter)
    float getLeftLevel() const { return leftLevel.load(); }
    float getRightLevel() const { return rightLevel.load(); }
    
    // =========================================================================
    // Static Sync Manager
    // =========================================================================
    static void registerRecorder(RecorderProcessor* recorder);
    static void unregisterRecorder(RecorderProcessor* recorder);
    static void startAllSyncedRecorders();
    static void stopAllSyncedRecorders();
    
    static constexpr const char* getIdentifier() { return "Recorder"; }
    
private:
    // Recording state
    std::atomic<bool> isRecording { false };
    std::atomic<bool> syncMode { true };  // Default to sync mode
    
    // Recording output
    juce::File recordingFolder;
    juce::File lastRecordingFile;
    juce::TimeSliceThread writerThread { "RecorderWriter" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;
    
    // Name
    juce::String recorderName { "Untitled" };
    
    // Audio info
    double currentSampleRate = 44100.0;
    std::atomic<int64_t> samplesRecorded { 0 };
    
    // Level metering
    std::atomic<float> leftLevel { 0.0f };
    std::atomic<float> rightLevel { 0.0f };
    
    // Waveform ring buffer for visualization
    static constexpr int waveformBufferSize = 1024;
    std::array<WaveformSample, waveformBufferSize> waveformBuffer;
    std::atomic<int> waveformWritePos { 0 };
    mutable juce::SpinLock waveformLock;
    
    // Downsampling for waveform (collect min/max over N samples)
    int waveformDownsampleCounter = 0;
    static constexpr int waveformDownsampleFactor = 256;
    float currentMinL = 0.0f, currentMaxL = 0.0f;
    float currentMinR = 0.0f, currentMaxR = 0.0f;
    
    // Static sync registry
    static juce::Array<RecorderProcessor*> syncedRecorders;
    static juce::SpinLock registryLock;
    
    // Static global default folder
    static juce::File globalDefaultFolder;
    static juce::SpinLock folderLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecorderProcessor)
};
