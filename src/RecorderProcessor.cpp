
// #D:\Workspace\Subterraneum_plugins_daw\src\RecorderProcessor.cpp
// RECORDER SYSTEM TOOL - Implementation
// Streams audio directly to disk with ThreadedWriter for glitch-free recording
// FIX: Added global default folder support, default to My Documents\Colosseum\recordings

#include "RecorderProcessor.h"

// =============================================================================
// Static Variables
// =============================================================================
juce::Array<RecorderProcessor*> RecorderProcessor::syncedRecorders;
juce::SpinLock RecorderProcessor::registryLock;
juce::File RecorderProcessor::globalDefaultFolder;
juce::SpinLock RecorderProcessor::folderLock;

// =============================================================================
// Static Global Folder Management
// =============================================================================
void RecorderProcessor::setGlobalDefaultFolder(const juce::File& folder) {
    juce::SpinLock::ScopedLockType lock(folderLock);
    globalDefaultFolder = folder;
}

juce::File RecorderProcessor::getGlobalDefaultFolder() {
    juce::SpinLock::ScopedLockType lock(folderLock);
    return globalDefaultFolder;
}

juce::File RecorderProcessor::getEffectiveDefaultFolder() {
    juce::SpinLock::ScopedLockType lock(folderLock);
    
    // If user has set a custom folder, use it
    if (globalDefaultFolder.exists()) {
        return globalDefaultFolder;
    }
    
    // Otherwise, use the app default: My Documents\Colosseum\recordings
    auto defaultFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("Colosseum")
                             .getChildFile("recordings");
    
    if (!defaultFolder.exists()) {
        defaultFolder.createDirectory();
    }
    
    return defaultFolder;
}

void RecorderProcessor::openRecordingFolder() const {
    juce::File folderToOpen = recordingFolder;
    
    // If no per-instance folder set, use effective default
    if (!folderToOpen.exists()) {
        folderToOpen = getEffectiveDefaultFolder();
    }
    
    // Ensure folder exists
    if (!folderToOpen.exists()) {
        folderToOpen.createDirectory();
    }
    
    // Open in system file explorer
    folderToOpen.revealToUser();
}

// =============================================================================
// Static Sync Registry
// =============================================================================
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
    // Set recording folder to effective default
    recordingFolder = getEffectiveDefaultFolder();
    
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
    backgroundWriter.reset();
    writerThread.stopThread(5000);
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
    
    // Use per-instance folder if set, otherwise use effective default
    juce::File targetFolder = recordingFolder;
    if (!targetFolder.exists()) {
        targetFolder = getEffectiveDefaultFolder();
        recordingFolder = targetFolder;  // Update instance folder
    }
    
    // Ensure recording folder exists
    if (!targetFolder.exists()) {
        targetFolder.createDirectory();
    }
    
    // Generate filename with timestamp and recorder name
    juce::String safeName = recorderName.replaceCharacters(" :/\\*?\"<>|", "___________");
    juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    juce::String filename = safeName + "_" + timestamp;
    
    lastRecordingFile = targetFolder.getNonexistentChildFile(filename, ".wav");
    
    auto wavFormat = std::make_unique<juce::WavAudioFormat>();
    auto outputStream = std::make_unique<juce::FileOutputStream>(lastRecordingFile);
    
    if (outputStream->failedToOpen()) {
        return false;
    }
    
    // Create writer: stereo, 24-bit WAV
    auto* writer = wavFormat->createWriterFor(
        outputStream.release(),
        sampleRate,
        juce::AudioChannelSet::stereo(),
        24,     // 24-bit
        {},     // no metadata
        0);
    
    if (writer == nullptr) {
        return false;
    }
    
    backgroundWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, writerThread, 32768);
    
    samplesRecorded.store(0);
    isRecording.store(true);
    
    // If sync mode, trigger all synced recorders
    if (syncMode.load()) {
        startAllSyncedRecorders();
    }
    
    return true;
}

void RecorderProcessor::stopRecording() {
    isRecording.store(false);
    
    if (backgroundWriter != nullptr) {
        backgroundWriter.reset();
    }
}

void RecorderProcessor::triggerSyncedRecording() {
    if (isRecording.load()) return;
    
    // Direct start without triggering sync cascade
    if (!writerThread.isThreadRunning()) {
        writerThread.startThread();
    }
    
    double sampleRate = currentSampleRate;
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    juce::File targetFolder = recordingFolder;
    if (!targetFolder.exists()) {
        targetFolder = getEffectiveDefaultFolder();
        recordingFolder = targetFolder;
    }
    
    if (!targetFolder.exists()) {
        targetFolder.createDirectory();
    }
    
    juce::String safeName = recorderName.replaceCharacters(" :/\\*?\"<>|", "___________");
    juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    juce::String filename = safeName + "_" + timestamp;
    
    lastRecordingFile = targetFolder.getNonexistentChildFile(filename, ".wav");
    
    auto wavFormat = std::make_unique<juce::WavAudioFormat>();
    auto outputStream = std::make_unique<juce::FileOutputStream>(lastRecordingFile);
    
    if (outputStream->failedToOpen()) return;
    
    auto* writer = wavFormat->createWriterFor(
        outputStream.release(), sampleRate, juce::AudioChannelSet::stereo(), 24, {}, 0);
    
    if (writer == nullptr) return;
    
    backgroundWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, writerThread, 32768);
    
    samplesRecorded.store(0);
    isRecording.store(true);
}

void RecorderProcessor::triggerSyncedStop() {
    stopRecording();
}

double RecorderProcessor::getRecordingLengthSeconds() const {
    if (currentSampleRate <= 0) return 0.0;
    return (double)samplesRecorded.load() / currentSampleRate;
}

std::vector<RecorderProcessor::WaveformSample> RecorderProcessor::getWaveformData(int numSamples) const {
    std::vector<WaveformSample> result;
    result.reserve(numSamples);
    
    int readPos = waveformWritePos.load();
    int startPos = (readPos - numSamples + waveformBufferSize) % waveformBufferSize;
    
    for (int i = 0; i < numSamples; ++i) {
        int idx = (startPos + i) % waveformBufferSize;
        result.push_back(waveformBuffer[idx]);
    }
    
    return result;
}

void RecorderProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state("RecorderState");
    state.setProperty("name", recorderName, nullptr);
    state.setProperty("syncMode", syncMode.load(), nullptr);
    state.setProperty("folder", recordingFolder.getFullPathName(), nullptr);
    
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void RecorderProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto state = juce::ValueTree::readFromData(data, sizeInBytes);
    
    if (state.isValid()) {
        recorderName = state.getProperty("name", "Untitled").toString();
        syncMode.store((bool)state.getProperty("syncMode", true));
        
        juce::String folderPath = state.getProperty("folder", "").toString();
        if (folderPath.isNotEmpty()) {
            recordingFolder = juce::File(folderPath);
        }
    }
}





