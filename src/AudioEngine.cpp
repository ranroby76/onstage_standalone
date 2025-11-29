#include "AudioEngine.h"
#include "AppLogger.h"
#include "IOSettingsManager.h"
#include "../RegistrationManager.h" 
#include <juce_gui_basics/juce_gui_basics.h> 
#include <cstring>

using namespace juce;

AudioEngine::AudioEngine()
{
    LOG_INFO("=== AudioEngine Constructor START ===");
    
    // FIX: Enable Internal Media Player by default
    backingTrackInputEnabled[0] = true;

    try {
        LOG_INFO("Step 1: Registering audio formats");
        formatManager.registerBasicFormats();

        // --- STRICT REQUIREMENT: START IN "OFF" STATE ---
        LOG_INFO("Step 2: Enforcing OFF state (Closing Audio Device)...");
        deviceManager.closeAudioDevice();
        
        // Step 3: Set Safe Defaults
        currentSampleRate = 44100.0;
        currentBlockSize = 512;
        
        masterLevel[0].store(0.0f);
        masterLevel[1].store(0.0f);
        micLevel[0].store(0.0f); 
        micLevel[1].store(0.0f);
        for(int i=0; i<9; ++i) 
            backingTrackLevels[i].store(0.0f);

        LOG_INFO("Step 4: Adding audio callback");
        deviceManager.addAudioCallback(this);
        
        LOG_INFO("=== AudioEngine Constructor COMPLETE (State: OFF) ===");
    }
    catch (const std::exception& e) {
        LOG_ERROR("EXCEPTION in AudioEngine constructor: " + String(e.what()));
        throw;
    }
    catch (...) {
        LOG_ERROR("UNKNOWN EXCEPTION in AudioEngine constructor");
        throw;
    }
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback(this);
    if (currentMidiDevice.isNotEmpty())
        deviceManager.removeMidiInputDeviceCallback(currentMidiDevice, this);
    stopRecording();
    releaseResources();
    deviceManager.closeAudioDevice();
}

void AudioEngine::setOutputChannelEnabled(int channelIndex, bool enabled)
{
    activeOutputChannels.setBit(channelIndex, enabled);
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        setup.useDefaultOutputChannels = false;
        setup.outputChannels = activeOutputChannels;
        
        auto activeIns = device->getActiveInputChannels();
        if (!activeIns.isZero()) {
            setup.useDefaultInputChannels = false;
            setup.inputChannels = activeIns;
        } else {
            setup.useDefaultInputChannels = true;
        }
        
        deviceManager.setAudioDeviceSetup(setup, true);
    }
}

bool AudioEngine::isOutputChannelEnabled(int channelIndex) const
{
    return activeOutputChannels[channelIndex];
}

void AudioEngine::openDriverControlPanel()
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        if (device->hasControlPanel()) device->showControlPanel();
        else NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "ASIO Control Panel", "This driver does not support opening the Control Panel directly.");
    }
}

// ==============================================================================
// MIDI HANDLING - COMPLETE IMPLEMENTATION
// ==============================================================================

static float mapMidi(int midiVal, float min, float max) {
    float norm = (float)midiVal / 127.0f;
    return min + (norm * (max - min));
}

void AudioEngine::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (message.isController()) {
        int cc = message.getControllerNumber();
        int val = message.getControllerValue();
        
        if (cc == 7) setMasterVolume(mapMidi(val, -100.0f, 0.0f));
        else if (cc == 24) setLatencyCorrectionMs(mapMidi(val, 0.0f, 500.0f));
        else if (cc == 25) setVocalBoostDb(mapMidi(val, 0.0f, 24.0f));
        else if (cc >= 20 && cc <= 23) { 
            setBackingTrackPairGain(cc - 20, mapMidi(val, 0.0f, 2.0f)); 
        }
        // Mic 1 Comp
        else if (cc >= 32 && cc <= 36) {
            auto params = compressorProcessor[0].getParams();
            if (cc == 32) params.thresholdDb = mapMidi(val, -60.0f, 0.0f);
            else if (cc == 33) params.ratio = mapMidi(val, 1.0f, 20.0f);
            else if (cc == 34) params.attackMs = mapMidi(val, 0.1f, 100.0f);
            else if (cc == 35) params.releaseMs = mapMidi(val, 10.0f, 1000.0f);
            else if (cc == 36) params.makeupDb = mapMidi(val, 0.0f, 24.0f);
            compressorProcessor[0].setParams(params);
        }
        // Mic 2 Comp
        else if (cc >= 42 && cc <= 46) {
            auto params = compressorProcessor[1].getParams();
            if (cc == 42) params.thresholdDb = mapMidi(val, -60.0f, 0.0f);
            else if (cc == 43) params.ratio = mapMidi(val, 1.0f, 20.0f);
            else if (cc == 44) params.attackMs = mapMidi(val, 0.1f, 100.0f);
            else if (cc == 45) params.releaseMs = mapMidi(val, 10.0f, 1000.0f);
            else if (cc == 46) params.makeupDb = mapMidi(val, 0.0f, 24.0f);
            compressorProcessor[1].setParams(params);
        }
        // Exciter
        else if (cc == 53 || cc == 54 || cc == 39) { // Mic 1
            auto params = exciterProcessor[0].getParams();
            if (cc == 53) params.frequency = mapMidi(val, 1000.0f, 10000.0f);
            else if (cc == 54) params.amount = mapMidi(val, 0.0f, 24.0f);
            else if (cc == 39) params.mix = mapMidi(val, 0.0f, 1.0f);
            exciterProcessor[0].setParams(params);
        }
        else if (cc == 63 || cc == 64 || cc == 40) { // Mic 2
            auto params = exciterProcessor[1].getParams();
            if (cc == 63) params.frequency = mapMidi(val, 1000.0f, 10000.0f);
            else if (cc == 64) params.amount = mapMidi(val, 0.0f, 24.0f);
            else if (cc == 40) params.mix = mapMidi(val, 0.0f, 1.0f);
            exciterProcessor[1].setParams(params);
        }
        // EQ
        else if ((cc >= 68 && cc <= 75)) { // Mic 1
            auto& eq = eqProcessor[0];
            if (cc == 70) eq.setLowGain(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 71) eq.setLowQ(mapMidi(val, 0.1f, 10.0f));
            else if (cc == 72) eq.setMidGain(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 73) eq.setMidQ(mapMidi(val, 0.1f, 10.0f));
            else if (cc == 74) eq.setHighGain(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 75) eq.setHighQ(mapMidi(val, 0.1f, 10.0f));
            else if (cc == 68) eq.setLowFrequency(mapMidi(val, 20.0f, 20000.0f));
            else if (cc == 69) eq.setHighFrequency(mapMidi(val, 20.0f, 20000.0f));
        }
        else if ((cc >= 78 && cc <= 85)) { // Mic 2
            auto& eq = eqProcessor[1];
            if (cc == 80) eq.setLowGain(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 81) eq.setLowQ(mapMidi(val, 0.1f, 10.0f));
            else if (cc == 82) eq.setMidGain(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 83) eq.setMidQ(mapMidi(val, 0.1f, 10.0f));
            else if (cc == 84) eq.setHighGain(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 85) eq.setHighQ(mapMidi(val, 0.1f, 10.0f));
            else if (cc == 78) eq.setLowFrequency(mapMidi(val, 20.0f, 20000.0f));
            else if (cc == 79) eq.setHighFrequency(mapMidi(val, 20.0f, 20000.0f));
        }
        // Reverb
        else if (cc == 28 || cc == 37 || cc == 38) {
            auto p = reverbProcessor.getParams();
            if (cc == 28) p.wetGain = mapMidi(val, 0.0f, 10.0f);
            else if (cc == 37) p.lowCutHz = mapMidi(val, 20.0f, 1000.0f);
            else if (cc == 38) p.highCutHz = mapMidi(val, 1000.0f, 20000.0f);
            reverbProcessor.setParams(p);
        }
        // Delay
        else if ((cc >= 47 && cc <= 52) || cc == 29) {
            auto p = delayProcessor.getParams();
            if (cc == 47) p.delayMs = mapMidi(val, 1.0f, 2000.0f);
            else if (cc == 48) p.ratio = mapMidi(val, 0.0f, 1.0f);
            else if (cc == 49) p.stage = mapMidi(val, 0.0f, 1.0f);
            else if (cc == 50) p.stereoWidth = mapMidi(val, 0.0f, 2.0f);
            else if (cc == 51) p.lowCutHz = mapMidi(val, 20.0f, 2000.0f);
            else if (cc == 52) p.highCutHz = mapMidi(val, 2000.0f, 20000.0f);
            else if (cc == 29) p.mix = mapMidi(val, 0.0f, 1.0f);
            delayProcessor.setParams(p);
        }
        // Harmonizer
        else if ((cc >= 55 && cc <= 58) || cc == 30) {
            auto p = harmonizerProcessor.getParams();
            if (cc == 55) p.voices[0].fixedSemitones = floor(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 56) p.voices[0].gainDb = mapMidi(val, -24.0f, 12.0f);
            else if (cc == 57) p.voices[1].fixedSemitones = floor(mapMidi(val, -12.0f, 12.0f));
            else if (cc == 58) p.voices[1].gainDb = mapMidi(val, -24.0f, 12.0f);
            else if (cc == 30) p.wetDb = mapMidi(val, -24.0f, 12.0f);
            harmonizerProcessor.setParams(p);
        }
        // Dynamic EQ
        else if ((cc >= 59 && cc <= 62) || (cc >= 65 && cc <= 67)) {
            auto p = dynamicEQProcessor.getParams();
            if (cc == 59) p.duckBandHz = mapMidi(val, 100.0f, 8000.0f);
            else if (cc == 60) p.q = mapMidi(val, 0.1f, 10.0f);
            else if (cc == 61) p.shape = mapMidi(val, 0.0f, 1.0f);
            else if (cc == 62) p.threshold = mapMidi(val, -60.0f, 0.0f);
            else if (cc == 65) p.ratio = mapMidi(val, 1.0f, 20.0f);
            else if (cc == 66) p.attack = mapMidi(val, 0.1f, 100.0f);
            else if (cc == 67) p.release = mapMidi(val, 10.0f, 1000.0f);
            dynamicEQProcessor.setParams(p);
        }
    }
    else if (message.isNoteOn()) {
        int note = message.getNoteNumber();
        if (note == 15) { auto& p = getMediaPlayer(); if (p.isPlaying()) p.pause(); else p.play(); }
        else if (note == 16) { stopAllPlayback(); }
        else if (note == 10) setMicMute(0, !isMicMuted(0)); 
        else if (note == 17) exciterProcessor[0].setBypassed(!exciterProcessor[0].isBypassed());
        else if (note == 18) eqProcessor[0].setBypassed(!eqProcessor[0].isBypassed());
        else if (note == 19) compressorProcessor[0].setBypassed(!compressorProcessor[0].isBypassed());
        else if (note == 20) exciterProcessor[1].setBypassed(!exciterProcessor[1].isBypassed());
        else if (note == 21) eqProcessor[1].setBypassed(!eqProcessor[1].isBypassed());
        else if (note == 22) compressorProcessor[1].setBypassed(!compressorProcessor[1].isBypassed());
        else if (note == 23) harmonizerProcessor.setBypassed(!harmonizerProcessor.isBypassed());
        else if (note == 24) { auto p = harmonizerProcessor.getParams(); p.voices[0].enabled = !p.voices[0].enabled; harmonizerProcessor.setParams(p); }
        else if (note == 25) { auto p = harmonizerProcessor.getParams(); p.voices[1].enabled = !p.voices[1].enabled; harmonizerProcessor.setParams(p); }
        else if (note == 26) reverbProcessor.setBypassed(!reverbProcessor.isBypassed());
        else if (note == 27) delayProcessor.setBypassed(!delayProcessor.isBypassed());
        else if (note == 28) dynamicEQProcessor.setBypassed(!dynamicEQProcessor.isBypassed());
    }
}

juce::StringArray AudioEngine::getAvailableMidiInputs()
{
    juce::StringArray names;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& device : devices) names.add(device.name);
    return names;
}

void AudioEngine::setMidiInput(const juce::String& deviceName)
{
    if (currentMidiDevice.isNotEmpty()) {
        deviceManager.removeMidiInputDeviceCallback(currentMidiDevice, this);
        deviceManager.setMidiInputDeviceEnabled(currentMidiDevice, false);
    }
    if (deviceName.isEmpty() || deviceName == "OFF") { currentMidiDevice = ""; return;
    }
    
    auto list = juce::MidiInput::getAvailableDevices();
    for (auto& info : list) {
        if (info.name == deviceName) {
            deviceManager.setMidiInputDeviceEnabled(info.identifier, true);
            deviceManager.addMidiInputDeviceCallback(info.identifier, this);
            currentMidiDevice = deviceName;
            return;
        }
    }
    currentMidiDevice = "";
}

bool AudioEngine::startRecording()
{
    if (!RegistrationManager::getInstance().isProMode())
        return false;

    stopRecording();
    tempRecordingFile = File::getSpecialLocation(File::tempDirectory).getChildFile("onstage_recording.wav");
    if (tempRecordingFile.exists()) tempRecordingFile.deleteFile();
    double liveRate = (currentSampleRate > 0) ? currentSampleRate : 44100.0;
    WavAudioFormat wavFormat;
    auto* stream = new FileOutputStream(tempRecordingFile);
    if (stream->failedToOpen()) { delete stream; return false; }

    auto* fileWriter = wavFormat.createWriterFor(stream, liveRate, 2, 24, {}, 0);
    if (fileWriter) {
        recorderThread = std::make_unique<RecorderThread>(fileWriter);
        recorderThread->startThread();
        isRecordingActive = true;
        return true;
    }
    return false;
}

void AudioEngine::stopRecording() { isRecordingActive = false; if (recorderThread) recorderThread.reset();
}
bool AudioEngine::isRecording() const { return isRecordingActive; }

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlockExpected)
{
    LOG_INFO("AudioEngine: prepareToPlay (" + String(sampleRate) + " Hz)");
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlockExpected;
    if (currentSampleRate <= 0.0) currentSampleRate = 44100.0;

    dsp::ProcessSpec stereoSpec { currentSampleRate, (uint32)samplesPerBlockExpected, 2 };
    for (int i = 0; i < 2; ++i) {
        eqProcessor[i].prepare(stereoSpec); eqProcessor[i].reset();
        compressorProcessor[i].prepare(stereoSpec);
        compressorProcessor[i].reset();
        exciterProcessor[i].prepare(stereoSpec); exciterProcessor[i].reset();
    }
    harmonizerProcessor.prepare(stereoSpec); harmonizerProcessor.reset();
    try { reverbProcessor.prepare(stereoSpec); reverbProcessor.reset();
    } catch (...) {}
    delayProcessor.prepare(currentSampleRate, samplesPerBlockExpected, 2); delayProcessor.reset();
    dynamicEQProcessor.prepare(stereoSpec); dynamicEQProcessor.reset();
    
    int safeBlockSize = jmax(samplesPerBlockExpected, 4096);
    backingTrackBuffer.setSize(2, safeBlockSize);
    vocalSidechainBuffer.setSize(2, safeBlockSize);
    vocalsMixBuffer.setSize(2, safeBlockSize);
    vocalsBypassBuffer.setSize(2, safeBlockSize);
    micChannelBuffer.setSize(2, safeBlockSize);
    masterMixBuffer.setSize(2, safeBlockSize);
    btDelayBuffer.setSize(2, 192000); btDelayBuffer.clear(); btDelayWritePos = 0;

    players[0].prepareToPlay(safeBlockSize, currentSampleRate);
    players[1].prepareToPlay(safeBlockSize, currentSampleRate);
    dsp::ProcessSpec monoSpec = stereoSpec; monoSpec.numChannels = 1;
    for (int i = 0; i < 2; ++i) { highpass[i].prepare(monoSpec); highpass[i].reset(); gain[i].prepare(monoSpec);
    gain[i].setGainLinear(1.0f); }
}

void AudioEngine::releaseResources()
{
    backingTrackBuffer.setSize(0, 0); btDelayBuffer.setSize(0, 0);
    vocalSidechainBuffer.setSize(0, 0); vocalsMixBuffer.setSize(0, 0);
    vocalsBypassBuffer.setSize(0, 0); micChannelBuffer.setSize(0, 0);
    masterMixBuffer.setSize(0, 0);
    players[0].releaseResources(); players[1].releaseResources();
    for (int i = 0; i < 2; ++i) { exciterProcessor[i].reset(); eqProcessor[i].reset(); compressorProcessor[i].reset(); highpass[i].reset(); gain[i].reset();
    }
    harmonizerProcessor.reset(); reverbProcessor.reset(); delayProcessor.reset(); dynamicEQProcessor.reset();
    micLevel[0].store(0.0f); micLevel[1].store(0.0f);
    micMute[0] = false; micMute[1] = false; micFxBypass[0] = false;
    micFxBypass[1] = false;
}

void AudioEngine::audioDeviceAboutToStart(AudioIODevice* d) {
    LOG_INFO("AudioEngine: audioDeviceAboutToStart");
    if(d) {
        currentSampleRate = d->getCurrentSampleRate(); 
        currentBlockSize = d->getCurrentBufferSizeSamples();
        if (activeOutputChannels.isZero())
        {
            auto deviceChans = d->getActiveOutputChannels();
            if (!deviceChans.isZero()) {
                activeOutputChannels = deviceChans;
            } else {
                activeOutputChannels.setBit(0);
                activeOutputChannels.setBit(1);
            }
        }
        auto activeIns = d->getActiveInputChannels();
        if (activeIns.isZero())
        {
            auto inputNames = d->getInputChannelNames();
            if (inputNames.size() > 0)
            {
                LOG_WARNING("Device started with 0 active inputs. Attempting to enable " + String(inputNames.size()) + " channels.");
                juce::MessageManager::callAsync([this, count = inputNames.size()]() {
                    if (auto* dev = deviceManager.getCurrentAudioDevice()) {
                        AudioDeviceManager::AudioDeviceSetup setup;
                        deviceManager.getAudioDeviceSetup(setup);
                        setup.useDefaultInputChannels = false;
                        setup.inputChannels.setRange(0, count, true);
                        LOG_INFO("Applying corrected input configuration...");
                        deviceManager.setAudioDeviceSetup(setup, true);
                    }
                });
            }
        }
        else
        {
            LOG_INFO("Active Inputs: " + activeIns.toString(2));
        }
    } 
    prepareToPlay(currentSampleRate,currentBlockSize);
}

void AudioEngine::audioDeviceStopped(){
    LOG_INFO("AudioEngine: audioDeviceStopped");
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData, int numOutputChannels, int numSamples, const AudioIODeviceCallbackContext& context)
{
    ignoreUnused(context);
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (outputChannelData[ch]) FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }

    if (numSamples > backingTrackBuffer.getNumSamples() || numSamples > vocalsMixBuffer.getNumSamples()) return;

    bool isPro = RegistrationManager::getInstance().isProMode();
    auto getPeak = [](const float* data, int num) -> float { float p=0.f; if(data)for(int i=0;i<num;++i){float a=std::abs(data[i]); if(a>p)p=a;} return p; };
    
    for(int ch=0; ch<backingTrackBuffer.getNumChannels(); ++ch) FloatVectorOperations::clear(backingTrackBuffer.getWritePointer(ch), numSamples);
    if (backingTrackInputEnabled[0]) {
        int currentIdx = activePlayerIndex.load();
        int nextIdx = 1 - currentIdx;

        juce::AudioSourceChannelInfo info(&backingTrackBuffer, 0, numSamples); 
        players[currentIdx].getNextAudioBlock(info);
        if (isCrossfadingActive)
        {
            float fadeOutGain = 1.0f - (float)(crossfadeTimeElapsed / crossfadeDuration);
            if (fadeOutGain < 0.0f) fadeOutGain = 0.0f;
            backingTrackBuffer.applyGain(fadeOutGain);
            
            juce::AudioBuffer<float> secondaryBuf(vocalSidechainBuffer.getArrayOfWritePointers(), backingTrackBuffer.getNumChannels(), numSamples);
            secondaryBuf.clear();
            juce::AudioSourceChannelInfo infoSec(&secondaryBuf, 0, numSamples);
            players[nextIdx].getNextAudioBlock(infoSec);
            float fadeInGain = (float)(crossfadeTimeElapsed / crossfadeDuration);
            if (fadeInGain > 1.0f) fadeInGain = 1.0f;
            secondaryBuf.applyGain(fadeInGain);
            for (int ch = 0; ch < backingTrackBuffer.getNumChannels(); ++ch)
                backingTrackBuffer.addFrom(ch, 0, secondaryBuf, ch, 0, numSamples);
            double dt = (double)numSamples / currentSampleRate;
            crossfadeTimeElapsed += dt;

            if (crossfadeTimeElapsed >= crossfadeDuration)
            {
                isCrossfadingActive = false;
                activePlayerIndex.store(nextIdx); 
                shouldStopOldPlayer = true;
            }
        }
        
        float peak = 0.0f;
        if (backingTrackBuffer.getNumChannels() > 0) peak = backingTrackBuffer.getMagnitude(0, numSamples);
        backingTrackLevels[0].store(peak);
    } else { backingTrackLevels[0].store(0.0f);
    }

    for (int btIdx = 1; btIdx <= 8; ++btIdx) {
        float inputPeak = 0.0f;
        if (isPro && backingTrackInputEnabled[btIdx]) {
            int inputChannel = backingTrackInputMapping[btIdx];
            if (inputChannel >= 0 && inputChannel < numInputChannels) {
                const float* inputData = inputChannelData[inputChannel];
                if (inputData) {
                    int pairIdx = (btIdx - 1) / 2;
                    float gain = backingTrackPairGains[pairIdx].load();
                    inputPeak = getPeak(inputData, numSamples);
                    int stereoChannel = (btIdx - 1) % 2;
                    backingTrackBuffer.addFrom(stereoChannel, 0, inputData, numSamples, gain);
                }
            }
        }
        backingTrackLevels[btIdx].store(inputPeak);
    }

    if (latencyCorrectionMs > 0.1f) {
        int delaySamples = static_cast<int>(latencyCorrectionMs * 0.001f * currentSampleRate);
        int bufferSize = btDelayBuffer.getNumSamples();
        if (delaySamples >= bufferSize - numSamples) delaySamples = bufferSize - numSamples - 1;
        for (int ch = 0; ch < 2; ++ch) {
            auto* src = backingTrackBuffer.getReadPointer(ch);
            int samplesToEnd = bufferSize - btDelayWritePos;
            if (numSamples <= samplesToEnd) btDelayBuffer.copyFrom(ch, btDelayWritePos, src, numSamples);
            else { btDelayBuffer.copyFrom(ch, btDelayWritePos, src, samplesToEnd);
            btDelayBuffer.copyFrom(ch, 0, src + samplesToEnd, numSamples - samplesToEnd); }
            auto* dst = backingTrackBuffer.getWritePointer(ch);
            int readPos = (btDelayWritePos - delaySamples + bufferSize) % bufferSize;
            samplesToEnd = bufferSize - readPos;
            if (numSamples <= samplesToEnd) FloatVectorOperations::copy(dst, btDelayBuffer.getReadPointer(ch, readPos), numSamples);
            else { FloatVectorOperations::copy(dst, btDelayBuffer.getReadPointer(ch, readPos), samplesToEnd);
            FloatVectorOperations::copy(dst + samplesToEnd, btDelayBuffer.getReadPointer(ch, 0), numSamples - samplesToEnd); }
        }
        btDelayWritePos = (btDelayWritePos + numSamples) % bufferSize;
    }

    for(int ch=0; ch<vocalsMixBuffer.getNumChannels(); ++ch) FloatVectorOperations::clear(vocalsMixBuffer.getWritePointer(ch), numSamples);
    for(int ch=0; ch<vocalsBypassBuffer.getNumChannels(); ++ch) FloatVectorOperations::clear(vocalsBypassBuffer.getWritePointer(ch), numSamples);
    
    for (int micIdx = 0; micIdx < 2; ++micIdx) {
        if (!isPro && micIdx == 1) {
            micLevel[micIdx].store(0.0f);
            continue;
        }

        float micPeak = 0.0f;
        if (selectedMicInputChannel[micIdx] >= 0 && selectedMicInputChannel[micIdx] < numInputChannels) {
            const float* inputData = inputChannelData[selectedMicInputChannel[micIdx]];
            if (inputData) {
                micPeak = getPeak(inputData, numSamples);
                if (!micMute[micIdx]) {
                    for(int ch=0; ch<micChannelBuffer.getNumChannels(); ++ch) FloatVectorOperations::clear(micChannelBuffer.getWritePointer(ch), numSamples);
                    micChannelBuffer.copyFrom(0, 0, inputData, numSamples); micChannelBuffer.copyFrom(1, 0, inputData, numSamples);
                    float preampGainLinear = Decibels::decibelsToGain(micPreampGain[micIdx]);
                    micChannelBuffer.applyGain(0, 0, numSamples, preampGainLinear); micChannelBuffer.applyGain(1, 0, numSamples, preampGainLinear);
                    if (micFxBypass[micIdx]) {
                        vocalsBypassBuffer.addFrom(0, 0, micChannelBuffer.getReadPointer(0), numSamples);
                        vocalsBypassBuffer.addFrom(1, 0, micChannelBuffer.getReadPointer(1), numSamples);
                    } else {
                        processMicChannelFX(micChannelBuffer, micIdx, numSamples);
                        vocalsMixBuffer.addFrom(0, 0, micChannelBuffer.getReadPointer(0), numSamples); vocalsMixBuffer.addFrom(1, 0, micChannelBuffer.getReadPointer(1), numSamples);
                    }
                }
            }
        }
        micLevel[micIdx].store(micPeak);
    }

    processGlobalVocalFX(vocalsMixBuffer, numSamples);
    if (vocalBoostDb > 0.1f) vocalsMixBuffer.applyGain(Decibels::decibelsToGain(vocalBoostDb));

    for(int ch=0; ch<vocalSidechainBuffer.getNumChannels(); ++ch) FloatVectorOperations::clear(vocalSidechainBuffer.getWritePointer(ch), numSamples);
    vocalSidechainBuffer.addFrom(0, 0, vocalsMixBuffer.getReadPointer(0), numSamples); vocalSidechainBuffer.addFrom(1, 0, vocalsMixBuffer.getReadPointer(1), numSamples);
    vocalSidechainBuffer.addFrom(0, 0, vocalsBypassBuffer.getReadPointer(0), numSamples); vocalSidechainBuffer.addFrom(1, 0, vocalsBypassBuffer.getReadPointer(1), numSamples);
    juce::AudioBuffer<float> btProxy(backingTrackBuffer.getArrayOfWritePointers(), backingTrackBuffer.getNumChannels(), numSamples);
    juce::AudioBuffer<float> scProxy(vocalSidechainBuffer.getArrayOfWritePointers(), vocalSidechainBuffer.getNumChannels(), numSamples);
    dynamicEQProcessor.process(btProxy, scProxy);

    for(int ch=0; ch<masterMixBuffer.getNumChannels(); ++ch) FloatVectorOperations::clear(masterMixBuffer.getWritePointer(ch), numSamples);
    masterMixBuffer.addFrom(0, 0, backingTrackBuffer.getReadPointer(0), numSamples); masterMixBuffer.addFrom(1, 0, backingTrackBuffer.getReadPointer(1), numSamples);
    masterMixBuffer.addFrom(0, 0, vocalsMixBuffer.getReadPointer(0), numSamples); masterMixBuffer.addFrom(1, 0, vocalsMixBuffer.getReadPointer(1), numSamples);
    masterMixBuffer.addFrom(0, 0, vocalsBypassBuffer.getReadPointer(0), numSamples);
    masterMixBuffer.addFrom(1, 0, vocalsBypassBuffer.getReadPointer(1), numSamples);

    float masterGainLinear = Decibels::decibelsToGain(masterVolumeDb);
    masterMixBuffer.applyGain(0, 0, numSamples, masterGainLinear); masterMixBuffer.applyGain(1, 0, numSamples, masterGainLinear);
    if (isRecordingActive && recorderThread) recorderThread->pushAudio(masterMixBuffer, numSamples);

    for (int ch = 0; ch < 2; ++ch) {
        const float* channelData = masterMixBuffer.getReadPointer(ch);
        float peak = getPeak(channelData, numSamples); masterLevel[ch].store(peak);
    }

    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (activeOutputChannels[ch]) {
            int sourceChannel = ch % 2;
            if (outputChannelData[ch]) {
                FloatVectorOperations::copy(outputChannelData[ch], masterMixBuffer.getReadPointer(sourceChannel), numSamples);
            }
        }
    }
}

void AudioEngine::processMicChannelFX(AudioBuffer<float>& stereoBuffer, int channel, int numSamples) {
    if (channel < 0 || channel >= 2) return;
    dsp::AudioBlock<float> block(stereoBuffer.getArrayOfWritePointers(), stereoBuffer.getNumChannels(), (size_t)numSamples);
    dsp::ProcessContextReplacing<float> context(block);
    exciterProcessor[channel].process(context); 
    eqProcessor[channel].process(context); 
    compressorProcessor[channel].process(context);
}

void AudioEngine::processGlobalVocalFX(AudioBuffer<float>& summedBuffer, int numSamples) {
    dsp::AudioBlock<float> block(summedBuffer.getArrayOfWritePointers(), summedBuffer.getNumChannels(), (size_t)numSamples);
    dsp::ProcessContextReplacing<float> context(block);
    harmonizerProcessor.process(context);
    juce::AudioBuffer<float> proxy(summedBuffer.getArrayOfWritePointers(), summedBuffer.getNumChannels(), numSamples);
    reverbProcessor.process(proxy); 
    delayProcessor.process(proxy);
}

void AudioEngine::triggerCrossfade(const juce::String& nextFile, double fadeDurationSec, float nextVol, float nextRate) {
    if (fadeDurationSec <= 0.1) fadeDurationSec = 0.1;
    crossfadeDuration = fadeDurationSec; 
    crossfadeTimeElapsed = 0.0;
    int nextIdx = 1 - activePlayerIndex.load();
    players[nextIdx].loadFile(nextFile); 
    players[nextIdx].setVolume(nextVol); 
    players[nextIdx].setRate(nextRate); 
    players[nextIdx].play();
    isCrossfadingActive = true;
    shouldStopOldPlayer = false; 
}

void AudioEngine::finalizeCrossfade() { 
    int oldIdx = activePlayerIndex.load();
    int newIdx = 1 - oldIdx; 
    activePlayerIndex.store(newIdx); 
    players[oldIdx].stop();
    isCrossfadingActive = false;
}

void AudioEngine::updateCrossfadeState() {
    if (shouldStopOldPlayer.load()) {
        shouldStopOldPlayer.store(false);
        int oldIdx = 1 - activePlayerIndex.load();
        players[oldIdx].stop();
        LOG_INFO("Crossfade finished - Stopped player " + String(oldIdx));
    }
}

VLCMediaPlayer& AudioEngine::getMediaPlayer() { return players[activePlayerIndex.load()]; }

void AudioEngine::stopAllPlayback() { 
    players[0].stop();
    players[1].stop(); 
    players[0].setPosition(0.0f); 
    players[1].setPosition(0.0f);
    isCrossfadingActive = false;
    shouldStopOldPlayer = false;
}

void AudioEngine::cancelCrossfade() { 
    if (isCrossfadingActive) { 
        int secondaryIdx = 1 - activePlayerIndex.load();
        players[secondaryIdx].stop(); 
        isCrossfadingActive = false;
        shouldStopOldPlayer = false;
    } 
}

void AudioEngine::setFxBypass(int channel, bool shouldBypass) { if (channel >= 0 && channel < 2) micFxBypass[channel] = shouldBypass; }
bool AudioEngine::isFxBypassed(int channel) const { if (channel >= 0 && channel < 2) return micFxBypass[channel]; return false; }

StringArray AudioEngine::getAvailableDriverTypes() { StringArray types; for (auto* type : deviceManager.getAvailableDeviceTypes()) types.add(type->getTypeName()); return types; }

StringArray AudioEngine::getSpecificDrivers(const String& driverType) { 
    StringArray drivers;
    if (driverType.isEmpty()) {
        for (auto* type : deviceManager.getAvailableDeviceTypes()) {
             drivers.addArray(type->getDeviceNames(false));
        }
    } else {
        for (auto* type : deviceManager.getAvailableDeviceTypes()) {
            if (type->getTypeName() == driverType) return type->getDeviceNames(false);
        }
    }
    return drivers; 
}

void AudioEngine::setDriverType(const String& driverType) { deviceManager.setCurrentAudioDeviceType(driverType, true); }

void AudioEngine::setSpecificDriver(const String& driverType, const String& specificDriver) { 
    if (specificDriver.isEmpty()) return;
    
    if (specificDriver == "OFF")
    {
        LOG_INFO("Closing audio device (User Selected OFF).");
        activeOutputChannels.clear();
        deviceManager.closeAudioDevice();
        return;
    }

    LOG_INFO("AudioEngine::setSpecificDriver - Switching to: " + specificDriver);
    activeOutputChannels.clear();

    String effectiveType = driverType;
    #if JUCE_LINUX
    if (effectiveType.isEmpty() || effectiveType == "ASIO") effectiveType = "ALSA";
    #endif

    if (effectiveType.isNotEmpty())
        deviceManager.setCurrentAudioDeviceType(effectiveType, true);

    AudioDeviceManager::AudioDeviceSetup setup; 
    deviceManager.getAudioDeviceSetup(setup);
    
    setup.outputDeviceName = specificDriver; 
    setup.inputDeviceName = specificDriver; 
    setup.sampleRate = 0.0;
    setup.bufferSize = 0;
    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = true;
    
    String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isNotEmpty())
    {
        LOG_ERROR("setSpecificDriver failed: " + err);
        deviceManager.closeAudioDevice();
    }
    else
    {
        LOG_INFO("setSpecificDriver successful.");
    }
}

StringArray AudioEngine::getAvailableInputDevices() const { auto* d = deviceManager.getCurrentAudioDevice(); return d ? d->getInputChannelNames() : StringArray(); }
StringArray AudioEngine::getAvailableOutputDevices() const { auto* d = deviceManager.getCurrentAudioDevice(); return d ? d->getOutputChannelNames() : StringArray(); }

void AudioEngine::selectInputDevice(int micIndex, const String& deviceName) { 
    if (micIndex < 0 || micIndex >= 2) return;
    if (deviceName.isEmpty() || deviceName == "OFF") { selectedMicInputChannel[micIndex] = -1; return; } 
    auto* d = deviceManager.getCurrentAudioDevice();
    if (!d) return;
    int idx = d->getInputChannelNames().indexOf(deviceName); 
    if (idx >= 0) { 
        selectedMicInputChannel[micIndex] = idx;
        AudioDeviceManager::AudioDeviceSetup setup; 
        deviceManager.getAudioDeviceSetup(setup); 
        setup.useDefaultInputChannels = false;
        auto inputNames = d->getInputChannelNames();
        setup.inputChannels.setRange(0, inputNames.size(), true); 
        setup.sampleRate = 0.0;
        deviceManager.setAudioDeviceSetup(setup, true);
    } 
    else
    {
        LOG_WARNING("selectInputDevice: '" + deviceName + "' not found in channel list.");
    }
}

void AudioEngine::selectOutputDevices(const StringArray& deviceNames) { 
    activeOutputChannels.clear(); 
    auto* d = deviceManager.getCurrentAudioDevice(); if (!d) return;
    auto all = d->getOutputChannelNames();
    for (const auto& name : deviceNames) { int i = all.indexOf(name);
    if (i >= 0) activeOutputChannels.setBit(i, true); } 
    AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup); 
    setup.outputChannels = activeOutputChannels; 
    setup.sampleRate = 0.0;
    deviceManager.setAudioDeviceSetup(setup, true); 
}

void AudioEngine::setMicInputChannel(int micIndex, int channelIndex) { if (micIndex >= 0 && micIndex < 2) selectedMicInputChannel[micIndex] = channelIndex; }
void AudioEngine::setOutputChannels(const Array<int>&) {}

void AudioEngine::setBackingTrackInputEnabled(int index, bool enabled) { if (index >= 0 && index < 9) backingTrackInputEnabled[index] = enabled; }
bool AudioEngine::isBackingTrackInputEnabled(int index) const { return (index >= 0 && index < 9) ? backingTrackInputEnabled[index] : false; }
void AudioEngine::setBackingTrackInputMapping(int index, int inputChannel) { if (index >= 0 && index < 9) backingTrackInputMapping[index] = inputChannel; }
int AudioEngine::getBackingTrackInputChannel(int index) const { return (index >= 0 && index < 9) ? backingTrackInputMapping[index] : -1; }

String AudioEngine::getCurrentDriverName() const { auto* d = deviceManager.getCurrentAudioDevice(); return d ? d->getName() : "No device"; }
String AudioEngine::getCurrentDriverType() const { auto* t = deviceManager.getCurrentDeviceTypeObject(); return t ? t->getTypeName() : "Unknown"; }
double AudioEngine::getCurrentSampleRate() const { auto* d = deviceManager.getCurrentAudioDevice(); return d ? d->getCurrentSampleRate() : 0.0; }
int AudioEngine::getCurrentBufferSize() const { auto* d = deviceManager.getCurrentAudioDevice(); return d ? d->getCurrentBufferSizeSamples() : 0; }