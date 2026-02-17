
// #D:\Workspace\Subterraneum_plugins_daw\src\MidiPlayerProcessor.h
// MIDI FILE PLAYER - System tool for playing .mid files with full transport
// Features: Play/Pause/Stop/Loop, position slider with seek, tempo override,
//           section markers, controller state snapshots for clean seeking
// Outputs all 16 MIDI channels simultaneously

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <map>

class MidiPlayerProcessor : public juce::AudioProcessor {
public:
    MidiPlayerProcessor();
    ~MidiPlayerProcessor() override;

    const juce::String getName() const override { return "MIDI Player"; }

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

    bool acceptsMidi() const override { return true; }   // For external transport control
    bool producesMidi() const override { return true; }   // Outputs all MIDI channels
    double getTailLengthSeconds() const override { return 0.0; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // =========================================================================
    // File Loading
    // =========================================================================
    bool loadFile(const juce::File& file);
    bool hasFileLoaded() const { return fileLoaded.load(); }
    juce::String getFileName() const { return loadedFileName; }
    juce::File getLoadedFile() const { return loadedFile; }

    // =========================================================================
    // Transport Controls
    // =========================================================================
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    enum TransportState { STOPPED, PLAYING, PAUSED };
    TransportState getTransportState() const { return transportState.load(); }
    bool isPlaying() const { return transportState.load() == PLAYING; }
    bool isPaused() const { return transportState.load() == PAUSED; }
    bool isStopped() const { return transportState.load() == STOPPED; }

    // =========================================================================
    // Loop
    // =========================================================================
    void setLooping(bool enabled) { looping.store(enabled); }
    bool isLooping() const { return looping.load(); }

    // =========================================================================
    // Position / Seek
    // =========================================================================
    double getPositionNormalized() const;    // 0.0 to 1.0
    void setPositionNormalized(double pos);  // Seek to normalized position
    void seekToTick(double tick);

    double getCurrentTimeSeconds() const;
    double getTotalTimeSeconds() const { return totalDurationSeconds; }
    double getCurrentTick() const { return currentTickPosition.load(); }
    double getTotalTicks() const { return totalTicks; }

    // =========================================================================
    // Tempo
    // =========================================================================
    void setSyncToMaster(bool enabled) { syncToMaster.store(enabled); }
    bool isSyncToMaster() const { return syncToMaster.load(); }
    
    void setTempoOverride(bool enabled) { tempoOverrideEnabled.store(enabled); }
    bool isTempoOverrideEnabled() const { return tempoOverrideEnabled.load(); }
    void setTempoOverrideBpm(double bpm) { tempoOverrideBpm.store(juce::jlimit(20.0, 400.0, bpm)); }
    double getTempoOverrideBpm() const { return tempoOverrideBpm.load(); }
    double getCurrentBpm() const { return currentBpm.load(); }
    double getFileBpm() const { return fileBpm; }

    // =========================================================================
    // Markers / Sections
    // =========================================================================
    struct Marker {
        double tick;
        double timeSeconds;
        juce::String name;
    };

    const std::vector<Marker>& getMarkers() const { return markers; }
    void jumpToMarker(int markerIndex);

    // =========================================================================
    // Track Info
    // =========================================================================
    int getNumTracks() const { return numTracks; }
    int getTicksPerBeat() const { return ticksPerBeat; }
    int getMidiType() const { return midiType; }

    // =========================================================================
    // Active channel indicator (for UI - which channels have recent activity)
    // =========================================================================
    bool isChannelActive(int ch) const { return (ch >= 0 && ch < 16) ? channelActive[ch].load() : false; }

    // =========================================================================
    // Channel Mute (per-channel mute from E button popup)
    // =========================================================================
    void setChannelMuted(int ch, bool muted) { if (ch >= 0 && ch < 16) channelMuted[ch].store(muted); }
    bool isChannelMuted(int ch) const { return (ch >= 0 && ch < 16) ? channelMuted[ch].load() : false; }

    // =========================================================================
    // Channel Info (instruments table for popup)
    // =========================================================================
    struct ChannelInfo {
        int channel = 0;         // 0-15
        int program = -1;        // -1 = unused
        int bankMSB = 0;
        int bankLSB = 0;
        int noteCount = 0;
        juce::String instrumentName;
    };
    
    const std::vector<ChannelInfo>& getChannelInfoTable() const { return channelInfoTable; }
    
    static juce::String getGMInstrumentName(int program);

    static constexpr const char* getIdentifier() { return "MidiPlayer"; }

private:
    // =========================================================================
    // Controller State Snapshot (for clean seeking)
    // Per-channel state at any point in time
    // =========================================================================
    struct ChannelState {
        int program = 0;
        int bankMSB = 0;         // CC0
        int bankLSB = 0;         // CC32
        int volume = 100;        // CC7
        int pan = 64;            // CC10
        int expression = 127;    // CC11
        int modWheel = 0;        // CC1
        int sustain = 0;         // CC64
        int pitchBend = 8192;    // Center = 8192
        int reverb = 0;          // CC91
        int chorus = 0;          // CC93
    };

    void buildMergedSequence();
    void buildTempoMap();
    void buildControllerSnapshots();
    void extractMarkers();
    void buildChannelInfoTable();
    void sendAllNotesOff(juce::MidiBuffer& buffer, int sampleOffset);
    void sendControllerSnapshot(juce::MidiBuffer& buffer, int sampleOffset, double atTick);
    double tickToSeconds(double tick) const;
    double secondsToTick(double seconds) const;
    double getTempoAtTick(double tick) const;

    // =========================================================================
    // File Data
    // =========================================================================
    std::atomic<bool> fileLoaded { false };
    juce::File loadedFile;
    juce::String loadedFileName;
    int midiType = 0;
    int numTracks = 0;
    int ticksPerBeat = 480;
    double totalTicks = 0.0;
    double totalDurationSeconds = 0.0;
    double fileBpm = 120.0;

    // Merged sequence (all tracks combined, sorted by time)
    juce::MidiMessageSequence mergedSequence;

    // Tempo map: tick → microseconds per quarter note
    struct TempoEvent {
        double tick;
        double microsecondsPerQuarterNote;
        double bpm;
        double timeSeconds;  // Absolute time of this tempo event
    };
    std::vector<TempoEvent> tempoMap;

    // Markers
    std::vector<Marker> markers;
    
    // Channel info table (built on file load)
    std::vector<ChannelInfo> channelInfoTable;

    // =========================================================================
    // Playback State
    // =========================================================================
    std::atomic<TransportState> transportState { STOPPED };
    std::atomic<double> currentTickPosition { 0.0 };
    std::atomic<bool> looping { false };
    std::atomic<bool> needsSeekSnapshot { false };
    std::atomic<double> seekTargetTick { 0.0 };

    // Tempo
    std::atomic<bool> syncToMaster { true };
    std::atomic<bool> tempoOverrideEnabled { false };
    std::atomic<double> tempoOverrideBpm { 120.0 };
    std::atomic<double> currentBpm { 120.0 };

    // Playback tracking
    int lastEventIndex = 0;  // Index into mergedSequence for next event to play
    double currentSampleRate = 44100.0;

    // Channel activity tracking (for UI)
    std::atomic<bool> channelActive[16];
    float channelDecay[16] = {};
    
    // Channel mute state (from E button popup)
    std::atomic<bool> channelMuted[16];

    // File chooser (must persist for async callback)
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiPlayerProcessor)
};




