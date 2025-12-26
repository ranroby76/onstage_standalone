/*
  ==============================================================================

    AudioEngine.cpp
    OnStage

  ==============================================================================
*/

#include "AudioEngine.h"
#include "AppLogger.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

using namespace juce;

// ==============================================================================
// AudioEngine Implementation
// ==============================================================================

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    
    // Start recorder thread
    writerThread.startThread();
    
    // Start with 0/0 to let UI drive the config, allow auto-open of default if available
    deviceManager.initialise(0, 0, nullptr, true); 
    deviceManager.addAudioCallback(this);

    // Initialize Internal Player
    mediaPlayer = std::make_unique<MediaPlayerType>();
    
    startTimer(200); 
    
    // Default Routing
    outputRoutingMasks.resize(32, 0); 
    inputRoutingMasks.resize(32, 0);
    inputGains.resize(32, 1.0f);
    
    // Clear meters
    for (int i=0; i<32; ++i) {
        inputLevelMeters[i].store(0.0f);
        outputLevels[i].store(0.0f);
    }
    for (int i=0; i<9; ++i) backingLevels[i].store(0.0f);
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    stopAllPlayback();
    deviceManager.removeAudioCallback(this);
    
    if (backgroundWriter) {
        backgroundWriter = nullptr;
        writerThread.stopThread(1000);
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    double sampleRate = device->getCurrentSampleRate();
    int samplesPerBlock = device->getCurrentBufferSizeSamples();

    dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock), 2 };

    // Prep DSP
    for (int i=0; i<2; ++i) {
        micChains[i].exciter.prepare(spec);  // 1. AIR
        micChains[i].sculpt.prepare(spec);   // 2. SCULPT
        micChains[i].eq.prepare(spec);       // 3. EQ
        micChains[i].comp.prepare(spec);     // 4. COMP
    }
    harmonizer.prepare(spec);
    reverb.prepare(spec);
    delay.prepare(sampleRate, samplesPerBlock, 2);
    dynamicEQ.prepare(spec);
    
    if (mediaPlayer) mediaPlayer->prepareToPlay(samplesPerBlock, sampleRate);
    
    // Resize Routing vectors to match hardware
    int numIns = device->getActiveInputChannels().countNumberOfSetBits();
    int numOuts = device->getActiveOutputChannels().countNumberOfSetBits();
    
    if (inputRoutingMasks.size() < numIns) {
        inputRoutingMasks.resize(numIns, 0);
        inputGains.resize(numIns, 1.0f);
    }
    if (outputRoutingMasks.size() < numOuts) outputRoutingMasks.resize(numOuts, 0);
}

void AudioEngine::audioDeviceStopped() {}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);
    processAudio(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
}

// -------------------------------------------------------------------------
// Processing Logic
// -------------------------------------------------------------------------
void AudioEngine::processAudio(const float* const* inputChannelData, int numInputChannels,
                               float* const* outputChannelData, int numOutputChannels,
                               int numSamples)
{
    // Clear outputs
    for (int i=0; i<numOutputChannels; ++i)
        if (outputChannelData[i]) FloatVectorOperations::clear(outputChannelData[i], numSamples);

    // Mic buffers are STEREO (2 channels) for proper stereo effects processing
    AudioBuffer<float> mic1Sum(2, numSamples); mic1Sum.clear();
    AudioBuffer<float> mic2Sum(2, numSamples); mic2Sum.clear();
    AudioBuffer<float> musicBus(2, numSamples); musicBus.clear();

    // 1. INPUT MATRIX SUMMING
    for (int i = 0; i < numInputChannels; ++i)
    {
        if (inputChannelData[i] == nullptr) continue;
        
        // Input Metering
        float level = AudioBuffer<float>(const_cast<float**>(inputChannelData) + i, 1, numSamples)
                      .getMagnitude(0, numSamples);
        if (i < 32) inputLevelMeters[i].store(level);

        int mask = (i < (int)inputRoutingMasks.size()) ? inputRoutingMasks[i] : 0;
        float gain = (i < (int)inputGains.size()) ? inputGains[i] : 1.0f;

        if (mask == 0) continue; 

        // Mic inputs: Convert mono to stereo (duplicate to both L and R channels)
        if (mask & 1) { 
            mic1Sum.addFrom(0, 0, inputChannelData[i], numSamples, gain);  // Left
            mic1Sum.addFrom(1, 0, inputChannelData[i], numSamples, gain);  // Right (same signal)
        }
        if (mask & 2) {
            mic2Sum.addFrom(0, 0, inputChannelData[i], numSamples, gain);  // Left
            mic2Sum.addFrom(1, 0, inputChannelData[i], numSamples, gain);  // Right (same signal)
        }
        
        // Music bus routing (already handles stereo)
        if (mask & 4) musicBus.addFrom(0, 0, inputChannelData[i], numSamples, gain);
        if (mask & 8) musicBus.addFrom(1, 0, inputChannelData[i], numSamples, gain);
    }

    // 2. VOCAL PROCESSING
    AudioBuffer<float> vocalBus(2, numSamples);
    vocalBus.clear();

    // -- MIC 1 (Already Stereo) --
    if (!micChains[0].muted) {
        float chainGain = Decibels::decibelsToGain(micChains[0].preampGainDb);
        mic1Sum.applyGain(chainGain);

        if (!micChains[0].fxBypassed) {
            dsp::AudioBlock<float> block(mic1Sum);  // 2-channel stereo block
            dsp::ProcessContextReplacing<float> ctx(block);
            micChains[0].exciter.process(ctx);  // 1. AIR (stereo)
            micChains[0].sculpt.process(ctx);   // 2. SCULPT (stereo)
            micChains[0].eq.process(ctx);       // 3. EQ (stereo)
            micChains[0].comp.process(ctx);     // 4. COMP (stereo)
        }
        // Add stereo signal to vocal bus
        vocalBus.addFrom(0, 0, mic1Sum, 0, 0, numSamples);  // L
        vocalBus.addFrom(1, 0, mic1Sum, 1, 0, numSamples);  // R
    }

    // -- MIC 2 (Already Stereo) --
    if (!micChains[1].muted) {
        float chainGain = Decibels::decibelsToGain(micChains[1].preampGainDb);
        mic2Sum.applyGain(chainGain);

        if (!micChains[1].fxBypassed) {
            dsp::AudioBlock<float> block(mic2Sum);  // 2-channel stereo block
            dsp::ProcessContextReplacing<float> ctx(block);
            micChains[1].exciter.process(ctx);  // 1. AIR (stereo)
            micChains[1].sculpt.process(ctx);   // 2. SCULPT (stereo)
            micChains[1].eq.process(ctx);       // 3. EQ (stereo)
            micChains[1].comp.process(ctx);     // 4. COMP (stereo)
        }
        // Add stereo signal to vocal bus
        vocalBus.addFrom(0, 0, mic2Sum, 0, 0, numSamples);  // L
        vocalBus.addFrom(1, 0, mic2Sum, 1, 0, numSamples);  // R
    }

    // FIX: 3. GLOBAL VOCAL FX (Harmonizer, Reverb, Delay) - Bypass if EITHER mic has FX bypassed
    bool globalFxBypassed = micChains[0].fxBypassed || micChains[1].fxBypassed;
    
    if (!globalFxBypassed)
    {
        {
            dsp::AudioBlock<float> block(vocalBus);
            dsp::ProcessContextReplacing<float> ctx(block);
            harmonizer.process(ctx);
        }
        {
            dsp::AudioBlock<float> block(vocalBus);
            dsp::ProcessContextReplacing<float> ctx(block);
            reverb.process(ctx);
        }
        delay.process(vocalBus.getArrayOfWritePointers(), 2, numSamples);
        {
            dsp::AudioBlock<float> block(vocalBus);
            dsp::ProcessContextReplacing<float> ctx(block);
            dynamicEQ.process(ctx);
        }
    }

    vocalBus.applyGain(vocalBoostLinear);

    // 3. INTERNAL PLAYER AUDIO
    AudioBuffer<float> playerBuffer(2, numSamples); playerBuffer.clear();
    if (mediaPlayer) {
        AudioSourceChannelInfo info;
        info.buffer = &playerBuffer;
        info.startSample = 0;
        info.numSamples = numSamples;
        mediaPlayer->getNextAudioBlock(info);
    }
    
    internalPlayerLevel.store(playerBuffer.getMagnitude(0, numSamples));

    // 4. MASTER BUS (vocals + music + player)
    AudioBuffer<float> master(2, numSamples);
    master.makeCopyOf(vocalBus);
    master.addFrom(0, 0, musicBus, 0, 0, numSamples);
    master.addFrom(1, 0, musicBus, 1, 0, numSamples);
    master.addFrom(0, 0, playerBuffer, 0, 0, numSamples);
    master.addFrom(1, 0, playerBuffer, 1, 0, numSamples);
    master.applyGain(masterGain);

    // 5. OUTPUT ROUTING MATRIX
    for (int i = 0; i < numOutputChannels; ++i)
    {
        if (outputChannelData[i] == nullptr) continue;
        
        int mask = (i < (int)outputRoutingMasks.size()) ? outputRoutingMasks[i] : 0;
        if (mask & 1) FloatVectorOperations::add(outputChannelData[i], master.getReadPointer(0), numSamples);
        if (mask & 2) FloatVectorOperations::add(outputChannelData[i], master.getReadPointer(1), numSamples);
        
        // Meter each physical output channel
        if (i < 32 && outputChannelData[i]) {
            float level = AudioBuffer<float>(const_cast<float**>(&outputChannelData[i]), 1, numSamples)
                         .getMagnitude(0, numSamples);
            outputLevels[i].store(level);
        }
    }
    
    if (isRecording && backgroundWriter) {
        backgroundWriter->write(master.getArrayOfReadPointers(), numSamples);
    }
}

// -------------------------------------------------------------------------
// Control & Logic
// -------------------------------------------------------------------------
void AudioEngine::launchEngine() {} // Internal only
void AudioEngine::terminateEngine() {}

void AudioEngine::timerCallback() {
    // No IPC sync needed
}

// FIX: stopAllPlayback implementation
void AudioEngine::stopAllPlayback() {
    if (mediaPlayer) mediaPlayer->stop();
}

// Backing track pitch control (removed - no longer used)
void AudioEngine::setBackingTrackPitch(float semitones) {
    // Pitch shifting removed to preserve audio quality
    juce::ignoreUnused(semitones);
}

// Meters
float AudioEngine::getInputLevel(int channel) const {
    if (channel >= 0 && channel < 32) return inputLevelMeters[channel].load();
    return 0.0f;
}
float AudioEngine::getOutputLevel(int channel) const { 
    if (channel >= 0 && channel < 32) return outputLevels[channel].load();
    return 0.0f;
}
float AudioEngine::getBackingTrackLevel(int channel) const { 
    if (channel == 0) return internalPlayerLevel.load();
    return 0.0f; 
}

// Routing
juce::StringArray AudioEngine::getSpecificDrivers(const juce::String& type) {
    juce::StringArray drivers;
    if (type.equalsIgnoreCase("ASIO")) {
        for (auto* deviceType : deviceManager.getAvailableDeviceTypes()) {
            if (deviceType->getTypeName() == "ASIO") {
                deviceType->scanForDevices();
                drivers = deviceType->getDeviceNames();
                break;
            }
        }
    }
    return drivers;
}

void AudioEngine::setSpecificDriver(const juce::String& type, const juce::String& name) {
    if (name == "OFF") { deviceManager.closeAudioDevice(); return; }
    
    deviceManager.setCurrentAudioDeviceType("ASIO", true);
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    
    setup.inputDeviceName = name; 
    setup.outputDeviceName = name;
    setup.useDefaultInputChannels = true; 
    setup.useDefaultOutputChannels = true;
    
    if (deviceManager.setAudioDeviceSetup(setup, true).isNotEmpty()) {
        LOG_ERROR("ASIO Load Error"); return; 
    }

    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        int ins = device->getInputChannelNames().size();
        int outs = device->getOutputChannelNames().size();
        
        inputRoutingMasks.resize(ins, 0);
        inputGains.resize(ins, 1.0f);
        outputRoutingMasks.resize(outs, 0);

        juce::BigInteger inputBits; inputBits.setRange(0, ins, true);
        juce::BigInteger outputBits; outputBits.setRange(0, outs, true);
        
        if (setup.inputChannels != inputBits || setup.outputChannels != outputBits) {
            setup.useDefaultInputChannels = false; setup.useDefaultOutputChannels = false;
            setup.inputChannels = inputBits; setup.outputChannels = outputBits;
            deviceManager.setAudioDeviceSetup(setup, true);
        }
    }
}

juce::StringArray AudioEngine::getAvailableInputDevices() {
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device) return device->getInputChannelNames();
    return {};
}
juce::StringArray AudioEngine::getAvailableOutputDevices() {
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device) return device->getOutputChannelNames();
    return {};
}
juce::StringArray AudioEngine::getAvailableMidiInputs() {
    juce::StringArray devices;
    for (auto& device : juce::MidiInput::getAvailableDevices()) devices.add(device.name);
    return devices;
}
void AudioEngine::openDriverControlPanel() {
    auto* device = deviceManager.getCurrentAudioDevice();
    if (!device || device->getTypeName() != "ASIO") return;
    
    device->showControlPanel();
}
void AudioEngine::setMidiInput(const juce::String& deviceName) { deviceManager.setMidiInputDeviceEnabled(deviceName, true); }

void AudioEngine::setMicMute(int i, bool m) { if(i<2) micChains[i].muted=m; }
bool AudioEngine::isMicMuted(int i) const { return (i<2) ? micChains[i].muted : false; }
void AudioEngine::setFxBypass(int i, bool b) { if(i<2) micChains[i].fxBypassed=b; }
bool AudioEngine::isFxBypassed(int i) const { return (i<2) ? micChains[i].fxBypassed : false; }
void AudioEngine::setMicPreampGain(int i, float g) { if(i<2) micChains[i].preampGainDb=g; }
float AudioEngine::getMicPreampGain(int i) const { return (i<2) ? micChains[i].preampGainDb : 0.0f; }

void AudioEngine::setMasterVolume(float g) { masterGain = Decibels::decibelsToGain(g); }
void AudioEngine::setOutputRoute(int i, int m) {
    if (i>=0 && i<32) {
        if(i >= (int)outputRoutingMasks.size()) outputRoutingMasks.resize(i+1, 0);
        outputRoutingMasks[i] = m;
    }
}
int AudioEngine::getOutputRoute(int i) const {
    if (i>=0 && i<(int)outputRoutingMasks.size()) return outputRoutingMasks[i];
    return 0;
}

void AudioEngine::setInputRoute(int i, int m) {
    if (i >= 0 && i < (int)inputRoutingMasks.size()) inputRoutingMasks[i] = m;
}
int AudioEngine::getInputRoute(int i) const {
    if (i >= 0 && i < (int)inputRoutingMasks.size()) return inputRoutingMasks[i];
    return 0;
}
void AudioEngine::setInputGain(int i, float g) {
    if (i >= 0 && i < (int)inputGains.size()) inputGains[i] = g;
}
float AudioEngine::getInputGain(int i) const {
    if (i >= 0 && i < (int)inputGains.size()) return inputGains[i];
    return 1.0f;
}

void AudioEngine::setLatencyCorrectionMs(float ms) { latencySamples = (int)(44100.0 * ms * 0.001); }
void AudioEngine::setVocalBoostDb(float db) { vocalBoostLinear = Decibels::decibelsToGain(db); }

// Legacy stubs
void AudioEngine::setBackingTrackInputMapping(int, int) {}
void AudioEngine::setBackingTrackInputEnabled(int, bool) {}
void AudioEngine::setBackingTrackPairGain(int, float) {}
float AudioEngine::getBackingTrackPairGain(int) const { return 1.0f; }
int AudioEngine::getBackingTrackInputChannel(int) const { return -1; }

void AudioEngine::triggerCrossfade(const juce::String& nextPath, double duration, float nextVol, float nextSpeed) {
    // Crossfade Logic for internal player not implemented in this phase
}
void AudioEngine::updateCrossfadeState() {}
void AudioEngine::showVideoWindow() {}

bool AudioEngine::startRecording() {
    stopRecording();
    
    // Get actual sample rate from device
    auto* device = deviceManager.getCurrentAudioDevice();
    double sampleRate = device ? device->getCurrentSampleRate() : 44100.0;
    
    lastRecordingFile = File::getSpecialLocation(File::userMusicDirectory).getNonexistentChildFile("OnStage_Recording", ".wav");
    auto* wavFormat = new WavAudioFormat();
    auto* outputStream = new FileOutputStream(lastRecordingFile);
    if (outputStream->failedToOpen()) { delete outputStream; delete wavFormat; return false; }
    
    auto* writer = wavFormat->createWriterFor(outputStream, sampleRate, 2, 24, {}, 0);
    if (writer != nullptr) {
        backgroundWriter.reset(new AudioFormatWriter::ThreadedWriter(writer, writerThread, 32768));
        isRecording = true; delete wavFormat; return true;
    }
    delete writer; delete wavFormat; return false;
}
void AudioEngine::stopRecording() { isRecording = false; backgroundWriter = nullptr; }

EQProcessor& AudioEngine::getEQProcessor(int i) { return micChains[i].eq; }
CompressorProcessor& AudioEngine::getCompressorProcessor(int i) { return micChains[i].comp; }
ExciterProcessor& AudioEngine::getExciterProcessor(int i) { return micChains[i].exciter; }
SculptProcessor& AudioEngine::getSculptProcessor(int i) { return micChains[i].sculpt; }