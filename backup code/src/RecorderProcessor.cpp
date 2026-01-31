// #D:\Workspace\Subterraneum_plugins_daw\src\RecorderProcessor.cpp
// RECORDER SYSTEM TOOL - Implementation
// Streams audio directly to disk with ThreadedWriter for glitch-free recording

#include "RecorderProcessor.h"

// =============================================================================
// Static Sync Registry
// =============================================================================
juce::Array<RecorderProcessor*> RecorderProcessor::syncedRecorders;
juce::SpinLock RecorderProcessor::registryLock;

void RecorderProcessor::registerRecorder(RecorderProcessor* recorder) {
    juce::SpinLock::ScopedLockType lock(registryLock);
    if (!syncedRecorders.contains(recorder)) {
        syncedRecorders.add(recorder);
    }
}

void RecorderProcessor::unregisterRecorder(RecorderProcessor* recorder) {
    juce::SpinLock::ScopedLockType lock(registryLock);
    syncedRecorders.removeFirstMatchingValue(recorder);
}

void RecorderProcessor::startAllSyncedRecorders() {
    juce::SpinLock::ScopedLockType lock(registryLock);
    for (auto* recorder : syncedRecorders) {
        if (recorder->isSyncMode() && !recorder->isCurrentlyRecording()) {
            recorder->triggerSyncedRecording();
        }
    }
}

void RecorderProcessor::stopAllSyncedRecorders() {
    juce::SpinLock::ScopedLockType lock(registryLock);
    for (auto* recorder : syncedRecorders) {
        if (recorder->isSyncMode() && recorder->isCurrentlyRecording()) {
            recorder->triggerSyncedStop();
        }
    }
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
RecorderProcessor::RecorderProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true))
        // No output - this is a termination point
{
    // Set default recording folder
    recordingFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                          .getChildFile("Colosseum Recordings");
    if (!recordingFolder.exists()) {
        recordingFolder.createDirectory();
    }
    
    // Initialize waveform buffer
    for (auto& sample : waveformBuffer) {
        sample = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    
    // Register with sync manager
    registerRecorder(this);
}

RecorderProcessor::~RecorderProcessor() {
    unregisterRecorder(this);
    stopRecording();
    writerThread.stopThread(1000);
}

// =============================================================================
// Audio Processing
// =============================================================================
void RecorderProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
}

void RecorderProcessor::releaseResources() {
    stopRecording();
}

void RecorderProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ignoreUnused(midiMessages);
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels < 2 || numSamples == 0) return;
    
    const float* leftChannel = buffer.getReadPointer(0);
    const float* rightChannel = buffer.getReadPointer(1);
    
    // Calculate levels for metering
    float maxL = 0.0f, maxR = 0.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        float l = std::abs(leftChannel[i]);
        float r = std::abs(rightChannel[i]);
        
        maxL = std::max(maxL, l);
        maxR = std::max(maxR, r);
        
        // Update waveform min/max for visualization
        currentMinL = std::min(currentMinL, leftChannel[i]);
        currentMaxL = std::max(currentMaxL, leftChannel[i]);
        currentMinR = std::min(currentMinR, rightChannel[i]);
        currentMaxR = std::max(currentMaxR, rightChannel[i]);
        
        waveformDownsampleCounter++;
        if (waveformDownsampleCounter >= waveformDownsampleFactor) {
            // Store waveform sample
            int writePos = waveformWritePos.load();
            waveformBuffer[writePos] = { currentMinL, currentMaxL, currentMinR, currentMaxR };
            waveformWritePos.store((writePos + 1) % waveformBufferSize);
            
            // Reset for next chunk
            currentMinL = 0.0f; currentMaxL = 0.0f;
            currentMinR = 0.0f; currentMaxR = 0.0f;
            waveformDownsampleCounter = 0;
        }
    }
    
    // Smooth level decay
    float currentL = leftLevel.load();
    float currentR = rightLevel.load();
    leftLevel.store(std::max(maxL, currentL * 0.95f));
    rightLevel.store(std::max(maxR, currentR * 0.95f));
    
    // Write to disk if recording
    if (isRecording.load() && backgroundWriter != nullptr) {
        backgroundWriter->write(buffer.getArrayOfReadPointers(), numSamples);
        samplesRecorded.fetch_add(numSamples);
    }
    
    // Clear the buffer - this is a termination point, no audio passes through
    buffer.clear();
}

bool RecorderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // We need stereo input, no output
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::disabled() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    return true;
}

// =============================================================================
// Recording Control
// =============================================================================
bool RecorderProcessor::startRecording() {
    stopRecording();
    
    if (!writerThread.isThreadRunning()) {
        writerThread.startThread();
    }
    
    double sampleRate = currentSampleRate;
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    // Ensure recording folder exists
    if (!recordingFolder.exists()) {
        recordingFolder.createDirectory();
    }
    
    // Generate filename with timestamp and recorder name
    juce::String safeName = recorderName.replaceCharacters(" :/\\*?\"<>|", "___________");
    juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    juce::String filename = safeName + "_" + timestamp;
    
    lastRecordingFile = recordingFolder.getNonexistentChildFile(filename, ".wav");
    
    auto wavFormat = std::make_unique<juce::WavAudioFormat>();
    auto outputStream = std::make_unique<juce::FileOutputStream>(lastRecordingFile);
    
    if (outputStream->failedToOpen()) {
        return false;
    }
    
    // Create writer: stereo, 24-bit WAV
    auto* writer = wavFormat->createWriterFor(
        outputStream.release(),  // Writer takes ownership
        sampleRate,
        2,      // Stereo
        24,     // 24-bit
        {},     // No metadata
        0       // Quality (not used for WAV)
    );
    
    if (writer != nullptr) {
        // 32768 sample buffer for smooth disk streaming
        backgroundWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            writer, writerThread, 32768
        );
        samplesRecorded.store(0);
        isRecording.store(true);
        
        // If in sync mode, start all other synced recorders
        if (syncMode.load()) {
            startAllSyncedRecorders();
        }
        
        return true;
    }
    
    return false;
}

void RecorderProcessor::stopRecording() {
    bool wasRecording = isRecording.exchange(false);
    backgroundWriter.reset();
    
    // If was recording in sync mode, stop all others too
    if (wasRecording && syncMode.load()) {
        stopAllSyncedRecorders();
    }
}

void RecorderProcessor::triggerSyncedRecording() {
    // Called by sync manager - start without triggering others (avoid infinite loop)
    if (isRecording.load()) return;
    
    if (!writerThread.isThreadRunning()) {
        writerThread.startThread();
    }
    
    double sampleRate = currentSampleRate;
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    if (!recordingFolder.exists()) {
        recordingFolder.createDirectory();
    }
    
    juce::String safeName = recorderName.replaceCharacters(" :/\\*?\"<>|", "___________");
    juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    juce::String filename = safeName + "_" + timestamp;
    
    lastRecordingFile = recordingFolder.getNonexistentChildFile(filename, ".wav");
    
    auto wavFormat = std::make_unique<juce::WavAudioFormat>();
    auto outputStream = std::make_unique<juce::FileOutputStream>(lastRecordingFile);
    
    if (outputStream->failedToOpen()) return;
    
    auto* writer = wavFormat->createWriterFor(
        outputStream.release(), sampleRate, 2, 24, {}, 0
    );
    
    if (writer != nullptr) {
        backgroundWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            writer, writerThread, 32768
        );
        samplesRecorded.store(0);
        isRecording.store(true);
    }
}

void RecorderProcessor::triggerSyncedStop() {
    isRecording.store(false);
    backgroundWriter.reset();
}

// =============================================================================
// Recording Info
// =============================================================================
double RecorderProcessor::getRecordingLengthSeconds() const {
    if (currentSampleRate <= 0) return 0.0;
    return static_cast<double>(samplesRecorded.load()) / currentSampleRate;
}

// =============================================================================
// Waveform Data
// =============================================================================
std::vector<RecorderProcessor::WaveformSample> RecorderProcessor::getWaveformData(int numSamples) const {
    std::vector<WaveformSample> result(numSamples);
    
    int readPos = waveformWritePos.load();
    int startPos = (readPos - numSamples + waveformBufferSize) % waveformBufferSize;
    
    for (int i = 0; i < numSamples; ++i) {
        int idx = (startPos + i) % waveformBufferSize;
        result[i] = waveformBuffer[idx];
    }
    
    return result;
}

// =============================================================================
// State Save/Load
// =============================================================================
void RecorderProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::XmlElement xml("RecorderState");
    xml.setAttribute("name", recorderName);
    xml.setAttribute("syncMode", syncMode.load());
    xml.setAttribute("recordingFolder", recordingFolder.getFullPathName());
    copyXmlToBinary(xml, destData);
}

void RecorderProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName("RecorderState")) {
        recorderName = xml->getStringAttribute("name", "Untitled");
        syncMode.store(xml->getBoolAttribute("syncMode", true));
        juce::String folderPath = xml->getStringAttribute("recordingFolder", "");
        if (folderPath.isNotEmpty()) {
            recordingFolder = juce::File(folderPath);
        }
    }
}
