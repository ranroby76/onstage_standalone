// #D:\Workspace\Subterraneum_plugins_daw\src\AutoSamplerProcessor.h
// AUTO SAMPLING - Automated multi-note sampling with MIDI output
// Architecture: MIDI-only output node. Taps audio from end of connected chain.
//   User connects MIDI OUT -> VSTi -> Effects -> [Connector] -> Audio Output
//   Auto Sampling follows the chain, taps audio from the last plugin node.
// Text syntax: [notes], [velocities], [durations_sec] (duration optional)

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>
#include <set>

// Forward declarations
class MeteringProcessor;
class SubterraneumAudioProcessor;

class AutoSamplerProcessor : public juce::AudioProcessor {
public:
    AutoSamplerProcessor(juce::AudioProcessorGraph* graph, SubterraneumAudioProcessor* mainProc);
    ~AutoSamplerProcessor() override;

    const juce::String getName() const override { return "Auto Sampling"; }

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

    bool acceptsMidi() const override { return false; }    // No MIDI input
    bool producesMidi() const override { return true; }    // MIDI output to VSTi
    double getTailLengthSeconds() const override { return 0.0; }

    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }

    // =========================================================================
    // Note/Velocity/Duration Triple
    // =========================================================================
    struct NoteVelocityPair {
        int note;
        int velocity;
        float durationSeconds = 0.0f;  // 0 = use global noteHoldMs
    };

    // =========================================================================
    // Text Syntax Parser
    // Syntax per line: [notes], [velocities], [durations_sec] (duration optional)
    //   [23-45], [127]              -> range x single, global hold time
    //   [23,36,56], [67,88,120]     -> same-length lists = zip = 3 files
    //   [48-72], [32,64,96,127]     -> range x multiple = cross product
    //   [55,60,65], [127,126,127], [4.3,5.1,5.2]  -> zip with per-note duration
    // =========================================================================
    static std::vector<NoteVelocityPair> parseNoteList(const juce::String& text);

    // =========================================================================
    // Text Editor Content (for E button popup)
    // =========================================================================
    void setEditorText(const juce::String& text) { editorText = text; }
    juce::String getEditorText() const { return editorText; }

    // =========================================================================
    // Family Name
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
    // Global Default Folder (shared with Manual Sampler via separate static)
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
    // Note Hold Time (global default - per-note duration overrides this)
    // =========================================================================
    void setNoteHoldMs(float ms) { noteHoldMs.store(ms); }
    float getNoteHoldMs() const { return noteHoldMs.load(); }

    // =========================================================================
    // Auto-Sampling Control
    // =========================================================================
    bool startAutoSampling();    // Parse text, find chain, begin auto-walk
    void stopAutoSampling();     // Abort immediately
    bool isRunning() const { return autoRunning.load(); }

    // =========================================================================
    // Connection Status
    // =========================================================================
    bool hasValidChain() const { return tapSource != nullptr; }
    juce::String getLastError() const { return lastError; }

    // =========================================================================
    // Status
    // =========================================================================
    enum State { IDLE, STARTING_NOTE, RECORDING_NOTE, WAITING_SILENCE, DONE };
    State getCurrentState() const { return state.load(); }
    int getCurrentNoteIndex() const { return currentIndex.load(); }
    int getTotalNotes() const { return totalNotes.load(); }
    int getCurrentNote() const { return activeNote.load(); }
    int getCurrentVelocity() const { return activeVelocity.load(); }

    // =========================================================================
    // Waveform / Metering (from tap source audio)
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

    // =========================================================================
    // SFZ Generation
    // =========================================================================
    void setCreateSfz(bool enabled) { createSfz.store(enabled); }
    bool getCreateSfz() const { return createSfz.load(); }
    
    void setFillGap(bool enabled) { fillGap.store(enabled); }
    bool getFillGap() const { return fillGap.load(); }
    
    void generateSfzFile();

    static constexpr const char* getIdentifier() { return "AutoSampler"; }

private:
    bool startNewRecording(int note, int velocity);
    void stopAndSaveCurrentRecording();
    juce::File buildFilePath(int note, int velocity) const;

    static std::vector<int> parseGroup(const juce::String& group);
    static std::vector<float> parseFloatGroup(const juce::String& group);

    // =========================================================================
    // Chain Walking - find the last plugin in the MIDI target's audio chain
    // =========================================================================
    MeteringProcessor* findChainEnd();

    // =========================================================================
    // Graph references (set at construction, used for chain walking)
    // =========================================================================
    juce::AudioProcessorGraph* graphRef = nullptr;
    SubterraneumAudioProcessor* mainProcessorRef = nullptr;

    // =========================================================================
    // Audio Tap Source - the MeteringProcessor we read audio from
    // =========================================================================
    MeteringProcessor* tapSource = nullptr;
    juce::String lastError;

    // =========================================================================
    // Auto-Sampling State Machine
    // =========================================================================
    std::atomic<State> state { IDLE };
    std::atomic<bool> autoRunning { false };

    std::vector<NoteVelocityPair> noteList;
    std::atomic<int> currentIndex { 0 };
    std::atomic<int> totalNotes { 0 };
    std::atomic<int> activeNote { -1 };
    std::atomic<int> activeVelocity { -1 };

    // Note hold tracking
    int noteHoldSampleCount = 0;
    int currentNoteHoldSamples = 0;
    bool noteOnSent = false;
    bool noteOffSent = false;

    // Silence detection
    std::atomic<float> silenceThresholdDb { -60.0f };
    std::atomic<float> silenceDurationMs { 500.0f };
    std::atomic<float> noteHoldMs { 2000.0f };
    int silenceSampleCount = 0;
    bool hasReceivedAudio = false;

    // Family name and folder
    juce::String familyName { "Sample" };
    juce::String editorText;
    juce::File recordingFolder;

    // Recording output
    double currentSampleRate = 44100.0;
    std::atomic<int64_t> samplesRecorded { 0 };
    juce::TimeSliceThread writerThread { "AutoSamplerWriter" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;

    // Level metering (from tap source audio)
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

    // SFZ generation settings
    std::atomic<bool> createSfz { false };
    std::atomic<bool> fillGap { false };
    
    // Safe async callback flag — shared_ptr so the lambda outlives this object
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>>(true);

    // Static global default folder
    static juce::File globalDefaultFolder;
    static juce::SpinLock folderLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoSamplerProcessor)
};
