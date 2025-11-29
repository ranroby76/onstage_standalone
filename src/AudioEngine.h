#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/EQProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/ExciterProcessor.h"
#include "dsp/HarmonizerProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/DelayProcessor.h"
#include "dsp/DynamicEQProcessor.h"
#include "engine/VLCMediaPlayer.h"

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // AudioIODeviceCallback overrides
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    // MidiInputCallback override
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    void prepareToPlay(double sampleRate, int samplesPerBlockExpected);
    void releaseResources();

    // Audio device management
    juce::StringArray getAvailableDriverTypes();
    juce::StringArray getSpecificDrivers(const juce::String& driverType);
    void setDriverType(const juce::String& driverType);
    void setSpecificDriver(const juce::String& driverType, const juce::String& specificDriver);
    void openDriverControlPanel();
    
    juce::StringArray getAvailableInputDevices() const;
    juce::StringArray getAvailableOutputDevices() const;

    // MIDI Device Management
    juce::StringArray getAvailableMidiInputs();
    void setMidiInput(const juce::String& deviceName);
    juce::String getCurrentMidiInputName() const { return currentMidiDevice; }

    // Metering
    float getOutputLevel(int channelIndex) const {
        if (channelIndex == 0) return masterLevel[0].load();
        if (channelIndex == 1) return masterLevel[1].load();
        return 0.0f;
    }
    
    float getInputLevel(int micIndex = 0) const {
        if (micIndex >= 0 && micIndex < 2) return micLevel[micIndex].load();
        return 0.0f;
    }

    float getBackingTrackLevel(int index) const {
        if (index >= 0 && index < 9) return backingTrackLevels[index].load();
        return 0.0f;
    }

    // I/O Configuration
    void setMicInputChannel(int micIndex, int channelIndex);
    void setOutputChannels(const juce::Array<int>& channelPairs);
    void selectInputDevice(int micIndex, const juce::String& channelName);
    void selectOutputDevices(const juce::StringArray& outputNames);

    // NEW: Explicit Output Channel Control
    void setOutputChannelEnabled(int channelIndex, bool enabled);
    bool isOutputChannelEnabled(int channelIndex) const;

    // Backing Tracks
    void setBackingTrackInputMapping(int index, int channelIndex);
    void setBackingTrackInputEnabled(int index, bool enabled);
    int getBackingTrackInputChannel(int btIndex) const;
    bool isBackingTrackInputEnabled(int btIndex) const;

    void setBackingTrackPairGain(int pairIndex, float gainLinear) {
        if (pairIndex >= 0 && pairIndex < 4)
            backingTrackPairGains[pairIndex].store(gainLinear);
    }
    float getBackingTrackPairGain(int pairIndex) const {
        if (pairIndex >= 0 && pairIndex < 4)
            return backingTrackPairGains[pairIndex].load();
        return 1.0f;
    }

    void setLatencyCorrectionMs(float ms) { latencyCorrectionMs = ms; }
    float getLatencyCorrectionMs() const { return latencyCorrectionMs; }
    
    void setVocalBoostDb(float db) { vocalBoostDb = db; }
    float getVocalBoostDb() const { return vocalBoostDb; }

    // DSP Accessors
    EQProcessor& getEQProcessor(int channel) { return eqProcessor[juce::jlimit(0, 1, channel)]; }
    CompressorProcessor& getCompressorProcessor(int channel) { return compressorProcessor[juce::jlimit(0, 1, channel)]; }
    ExciterProcessor& getExciterProcessor(int channel) { return exciterProcessor[juce::jlimit(0, 1, channel)]; }
    DynamicEQProcessor& getDynamicEQProcessor() { return dynamicEQProcessor; }
    HarmonizerProcessor& getHarmonizerProcessor() { return harmonizerProcessor; }
    ReverbProcessor& getReverbProcessor() { return reverbProcessor; }
    DelayProcessor& getDelayProcessor() { return delayProcessor; }
    
    // Get active player
    VLCMediaPlayer& getMediaPlayer();
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    void setMicPreampGain(int micIndex, float gainDb) {
        if (micIndex >= 0 && micIndex < 2) micPreampGain[micIndex] = gainDb;
    }

    float getMicPreampGain(int micIndex) const {
        if (micIndex >= 0 && micIndex < 2) return micPreampGain[micIndex];
        return 0.0f;
    }

    void setMicMute(int micIndex, bool shouldMute) {
        if (micIndex >= 0 && micIndex < 2) micMute[micIndex] = shouldMute;
    }
    bool isMicMuted(int micIndex) const {
        if (micIndex >= 0 && micIndex < 2) return micMute[micIndex];
        return false;
    }

    void setFxBypass(int channel, bool shouldBypass);
    bool isFxBypassed(int channel) const;

    void setMediaPlayerVolume(float volume) { mediaPlayerVolume = juce::jlimit(0.0f, 1.0f, volume); }
    float getMediaPlayerVolume() const { return mediaPlayerVolume; }

    void setMasterVolume(float volumeDb) { masterVolumeDb = volumeDb; }
    float getMasterVolume() const { return masterVolumeDb; }
    
    bool startRecording();
    void stopRecording();
    bool isRecording() const;
    juce::File getLastRecordingFile() const { return tempRecordingFile; }
    
    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    juce::String getCurrentDriverName() const;
    juce::String getCurrentDriverType() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;

    // Crossfade Control
    void triggerCrossfade(const juce::String& nextFile, double fadeDurationSec, float nextVol, float nextRate);
    bool isCrossfading() const { return isCrossfadingActive; }
    void finalizeCrossfade();
    void stopAllPlayback();
    void cancelCrossfade();
    
    // NEW: Called by UI to handle cleanup after fade finishes
    void updateCrossfadeState();

private:
    void processMicChannelFX(juce::AudioBuffer<float>& stereoBuffer, int channel, int numSamples);
    void processGlobalVocalFX(juce::AudioBuffer<float>& summedBuffer, int numSamples);

    juce::AudioDeviceManager deviceManager;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    bool micMute[2] { false, false };
    bool micFxBypass[2] { false, false };
    float micPreampGain[2] { 0.0f, 0.0f };

    float mediaPlayerVolume = 1.0f;
    float masterVolumeDb = 0.0f;
    
    float latencyCorrectionMs = 0.0f;
    float vocalBoostDb = 0.0f;

    juce::String currentMidiDevice;

    int selectedMicInputChannel[2] = {-1, -1};
    
    // Output Routing
    juce::BigInteger activeOutputChannels;

    std::atomic<float> micLevel[2] { 0.0f, 0.0f };
    std::atomic<float> masterLevel[2] { 0.0f, 0.0f };
    std::atomic<float> backingTrackLevels[9];
    std::atomic<float> backingTrackPairGains[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    bool backingTrackInputEnabled[9] { false };
    int backingTrackInputMapping[9] { -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    juce::AudioBuffer<float> backingTrackBuffer;
    juce::AudioBuffer<float> btDelayBuffer;
    int btDelayWritePos = 0;

    juce::AudioBuffer<float> vocalSidechainBuffer;
    juce::AudioBuffer<float> vocalsMixBuffer;
    juce::AudioBuffer<float> vocalsBypassBuffer;
    juce::AudioBuffer<float> micChannelBuffer;
    juce::AudioBuffer<float> masterMixBuffer;

    ExciterProcessor exciterProcessor[2];
    EQProcessor eqProcessor[2];
    CompressorProcessor compressorProcessor[2];
    HarmonizerProcessor harmonizerProcessor;
    ReverbProcessor reverbProcessor;
    DelayProcessor delayProcessor;
    DynamicEQProcessor dynamicEQProcessor;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highpass[2];
    juce::dsp::Gain<float> gain[2];

    // TWO PLAYERS
    VLCMediaPlayer players[2];
    std::atomic<int> activePlayerIndex { 0 };

    std::atomic<bool> isCrossfadingActive { false };
    std::atomic<bool> shouldStopOldPlayer { false }; // NEW: Signal UI to stop old VLC player
    double crossfadeDuration = 0.0;
    double crossfadeTimeElapsed = 0.0;

    juce::AudioFormatManager formatManager;
    juce::File tempRecordingFile;
    std::atomic<bool> isRecordingActive { false };

    class RecorderThread : public juce::Thread
    {
    public:
        RecorderThread(juce::AudioFormatWriter* w) 
            : Thread("Recorder Thread"), writer(w) 
        {
            fifo.setTotalSize(131072);
            buffer.setSize(2, 131072);
        }

        ~RecorderThread() override
        {
            stopThread(2000);
            writer.reset(); 
        }

        void run() override
        {
            while (!threadShouldExit())
            {
                const int samplesReady = fifo.getNumReady();
                if (samplesReady > 0)
                {
                    const int numToSend = juce::jmin(samplesReady, 4096);
                    int start1, size1, start2, size2;
                    fifo.prepareToRead(numToSend, start1, size1, start2, size2);

                    if (size1 > 0) writer->writeFromAudioSampleBuffer(buffer, start1, size1);
                    if (size2 > 0) writer->writeFromAudioSampleBuffer(buffer, start2, size2);

                    fifo.finishedRead(numToSend);
                }
                else
                {
                    wait(5);
                }
            }
            flushRemaining();
        }

        void pushAudio(const juce::AudioBuffer<float>& input, int numSamples)
        {
            int start1, size1, start2, size2;
            fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

            if (size1 > 0)
            {
                for (int i = 0; i < 2; ++i)
                    buffer.copyFrom(i, start1, input, i, 0, size1);
            }
            if (size2 > 0)
            {
                for (int i = 0; i < 2; ++i)
                    buffer.copyFrom(i, start2, input, i, size1, size2);
            }
            fifo.finishedWrite(size1 + size2);
        }

    private:
        void flushRemaining()
        {
            int samplesReady = fifo.getNumReady();
            while (samplesReady > 0)
            {
                int numToSend = juce::jmin(samplesReady, 4096);
                int start1, size1, start2, size2;
                fifo.prepareToRead(numToSend, start1, size1, start2, size2);
                
                if (size1 > 0) writer->writeFromAudioSampleBuffer(buffer, start1, size1);
                if (size2 > 0) writer->writeFromAudioSampleBuffer(buffer, start2, size2);
                
                fifo.finishedRead(numToSend);
                samplesReady = fifo.getNumReady();
            }
        }

        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::AbstractFifo fifo { 131072 };
        juce::AudioBuffer<float> buffer;
    };

    std::unique_ptr<RecorderThread> recorderThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
#endif