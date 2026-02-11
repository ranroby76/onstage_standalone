// ==============================================================================
//  AudioEngine.h
//  OnStage — Core audio engine with graph-based routing
//
//  Owns the AudioDeviceManager, media player, and OnStageGraph.
//  The audioDeviceIOCallback feeds hardware audio through the graph.
//
//  INTEGRATION CHANGE:  The old hardcoded mic-chain arrays are gone.
//  All routing is now handled by OnStageGraph (juce::AudioProcessorGraph).
//  The WiringCanvas provides the visual interface for the graph.
// ==============================================================================

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

#include "AppLogger.h"
#include "IOSettingsManager.h"

// --- Platform media player typedef -------------------------------------------
#if JUCE_WINDOWS
  #include "engine/VLCMediaPlayer_Desktop.h"
  using MediaPlayerType = VLCMediaPlayer_Desktop;
#elif JUCE_MAC
  #include "engine/AVFMediaPlayer_Mac.h"
  using MediaPlayerType = AVFMediaPlayer_Mac;
#else
  #include "engine/NullMediaPlayer.h"
  using MediaPlayerType = NullMediaPlayer;
#endif

// Forward declarations
class OnStageGraph;
class PresetManager;

// ==============================================================================
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // --- Lifecycle -----------------------------------------------------------
    void initialise();
    void shutdown();

    // --- AudioIODeviceCallback -----------------------------------------------
    void audioDeviceIOCallbackWithContext (
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // --- ChangeListener (device changes) -------------------------------------
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // --- Accessors -----------------------------------------------------------
    juce::AudioDeviceManager&  getDeviceManager()   { return deviceManager; }
    juce::AudioFormatManager&  getFormatManager()   { return formatManager; }
    MediaPlayerType&           getMediaPlayer()     { return mediaPlayer; }
    IOSettingsManager&         getIOSettings()      { return ioSettings; }

    OnStageGraph&              getGraph()            { return *graph; }
    const OnStageGraph&        getGraph() const      { return *graph; }

    // --- Master volume -------------------------------------------------------
    void  setMasterVolume (float linearGain);
    float getMasterVolume() const { return masterVolume.load (std::memory_order_relaxed); }

    // --- Metering (thread-safe reads from UI) --------------------------------
    float getInputLevel  (int channel) const;
    float getOutputLevel (int channel) const;

    // --- Playback control ----------------------------------------------------
    void stopAllPlayback() { mediaPlayer.stop(); }

    // --- Recording -----------------------------------------------------------
    void startRecording (const juce::File& outputFile);
    void stopRecording();
    bool isRecording() const { return recording.load (std::memory_order_relaxed); }

    // --- Graph state persistence ---------------------------------------------
    void saveGraphState (const juce::File& file);
    void loadGraphState (const juce::File& file, PresetManager& presetMgr);

private:
    // Audio system
    juce::AudioDeviceManager  deviceManager;
    juce::AudioFormatManager  formatManager;
    IOSettingsManager         ioSettings;

    // Media player (platform-specific)
    MediaPlayerType           mediaPlayer;

    // The node graph (replaces old mic chain)
    std::unique_ptr<OnStageGraph> graph;

    // Master output volume
    std::atomic<float> masterVolume { 1.0f };

    // Metering arrays (read by UI, written in audio callback)
    static constexpr int kMaxChannels = 32;
    std::atomic<float> inputLevels  [kMaxChannels] {};
    std::atomic<float> outputLevels [kMaxChannels] {};

    // Recording
    std::atomic<bool>                       recording { false };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::TimeSliceThread                   writerThread { "RecordingThread" };
    juce::CriticalSection                   writerLock;

    // Current device config (cached for re-prepare)
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    int    currentNumInputs  = 0;
    int    currentNumOutputs = 0;

    // Helper: route audio through graph
    void processAudio (const float* const* inputs,  int numIns,
                       float* const* outputs, int numOuts,
                       int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};