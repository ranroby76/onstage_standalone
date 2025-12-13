/*
  ==============================================================================

    AudioEngine.h
    OnStage

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_graphics/juce_graphics.h>
#include "UI/PlaylistDataStructures.h"

// INTERNAL PLAYER DIRECT DEPENDENCY
#include "engine/VLCMediaPlayer.h" 

// DSP Headers
#include "dsp/SimplePitchShifter.h"
#include "dsp/EQProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/ExciterProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/DelayProcessor.h"
#include "dsp/HarmonizerProcessor.h"
#include "dsp/DynamicEQProcessor.h"

class AudioEngine : public juce::AudioIODeviceCallback, private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Device & Format Managers
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    
    // Playback Control
    void stopAllPlayback();
    VLCMediaPlayer& getMediaPlayer() { return *mediaPlayer; } // Direct Access
    std::vector<PlaylistItem>& getPlaylist() { return playlist; }
    
    // Recording
    bool startRecording();
    void stopRecording();
    juce::File getLastRecordingFile() const { return lastRecordingFile; }

    // Logic & Settings
    void updateCrossfadeState();
    void showVideoWindow();
    
    // Pitch Control
    void setBackingTrackPitch(float semitones);
    
    // Metering Getters
    float getInputLevel(int channel) const; 
    float getOutputLevel(int channel) const;
    float getBackingTrackLevel(int channel) const; 

    // --- IO Routing & Settings ---
    juce::StringArray getSpecificDrivers(const juce::String& type);
    juce::StringArray getAvailableInputDevices();
    juce::StringArray getAvailableOutputDevices();
    juce::StringArray getAvailableMidiInputs();
    
    void setSpecificDriver(const juce::String& type, const juce::String& name);
    void openDriverControlPanel();
    void setMidiInput(const juce::String& deviceName);
    
    // Mic Chain Controls
    void setMicMute(int micIndex, bool muted);
    bool isMicMuted(int micIndex) const;
    
    void setFxBypass(int micIndex, bool bypassed);
    bool isFxBypassed(int micIndex) const;
    
    void setMicPreampGain(int micIndex, float gainDb);
    float getMicPreampGain(int micIndex) const;

    // Master / Routing
    void setMasterVolume(float gainDb);
    void setOutputRoute(int outputIndex, int mask); 
    int getOutputRoute(int outputIndex) const;

    // Input Routing (Matrix)
    void setInputRoute(int inputChannelIndex, int mask);
    int getInputRoute(int inputChannelIndex) const;
    
    void setInputGain(int inputChannelIndex, float gain);
    float getInputGain(int inputChannelIndex) const;

    // Settings
    void setLatencyCorrectionMs(float ms);
    void setVocalBoostDb(float db);

    // Crossfade
    void triggerCrossfade(const juce::String& nextPath, double duration, float nextVol, float nextSpeed);

    // --- DSP Accessors ---
    EQProcessor& getEQProcessor(int micIndex);
    CompressorProcessor& getCompressorProcessor(int micIndex);
    ExciterProcessor& getExciterProcessor(int micIndex);
    
    HarmonizerProcessor& getHarmonizerProcessor() { return harmonizer; }
    ReverbProcessor& getReverbProcessor() { return reverb; }
    DelayProcessor& getDelayProcessor() { return delay; }
    DynamicEQProcessor& getDynamicEQProcessor() { return dynamicEQ; }

    // Deprecated helpers (mapped to matrix)
    void setBackingTrackInputMapping(int virtualInputIndex, int realInputChannel);
    void setBackingTrackInputEnabled(int virtualInputIndex, bool enabled);
    void setBackingTrackPairGain(int pairIndex, float gain);
    float getBackingTrackPairGain(int pairIndex) const;
    int getBackingTrackInputChannel(int virtualInputIndex) const;

private:
    void launchEngine();
    void terminateEngine();
    void timerCallback() override;
    
    void processAudio(const float* const* inputChannelData, int numInputChannels,
                      float* const* outputChannelData, int numOutputChannels,
                      int numSamples);

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    
    // INTERNAL PLAYER
    std::unique_ptr<VLCMediaPlayer> mediaPlayer;
    
    std::vector<PlaylistItem> playlist;
    
    struct MicChain {
        float preampGainDb = 0.0f;
        bool muted = false;
        bool fxBypassed = false;
        EQProcessor eq;
        CompressorProcessor comp;
        ExciterProcessor exciter;
    };
    MicChain micChains[2]; 

    HarmonizerProcessor harmonizer;
    ReverbProcessor reverb;
    DelayProcessor delay;
    DynamicEQProcessor dynamicEQ;
    
    SimplePitchShifter pitchShifterL;
    SimplePitchShifter pitchShifterR;

    std::vector<int> outputRoutingMasks; 
    std::vector<int> inputRoutingMasks; 
    std::vector<float> inputGains;      

    std::atomic<float> inputLevelMeters[32]; 
    std::atomic<float> outputLevels[2] { 0.0f, 0.0f };
    std::atomic<float> internalPlayerLevel { 0.0f };
    std::atomic<float> backingLevels[9];

    float masterGain = 1.0f;
    
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;
    juce::TimeSliceThread writerThread { "Audio Recorder Thread" };
    juce::File lastRecordingFile;
    bool isRecording = false;
    float vocalBoostLinear = 1.0f;
    int latencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};