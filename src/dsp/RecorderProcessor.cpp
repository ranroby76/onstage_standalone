// D:\Workspace\ONSTAGE_WIRED\src\dsp\RecorderProcessor.cpp
// RECORDER SYSTEM TOOL - Implementation
// Streams audio directly to disk with ThreadedWriter for glitch-free recording
// OnStage Version: 24-bit/44100Hz, default folder: My Documents\OnStage\recordings
//
// FIX: writerThread is now properly stopped in stopRecording() and destructor
//      to prevent zombie OS threads on app exit or mid-session hangs.

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
    
    // Otherwise, use the app default: My Documents\OnStage\recordings
    auto defaultFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("OnStage")
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
    // Unregister FIRST to prevent sync callbacks during teardown
    unregisterRecorder(this);
    
    // Stop recording (resets backgroundWriter)
    isRecording.store(false);
    backgroundWriter.reset();
    
    // FIX: Always stop the writer thread and wait for it to finish.
    // This is the primary cause of zombie processes — the TimeSliceThread
    // was left running as an OS thread after the processor was destroyed.
    if (writerThread.isThreadRunning())
        writerThread.stopThread(2000);
}

// =============================================================================
// Audio Processing
// =============================================================================
void RecorderProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
}

void RecorderProcessor::releaseResources() {
    // FIX: Full cleanup on releaseResources (called during graph teardown)
    isRecording.store(false);
    backgroundWriter.reset();
    
    if (writerThread.isThreadRunning())
        writerThread.stopThread(1000);
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
            
            // Reset for next window
            waveformDownsampleCounter = 0;
            currentMinL = currentMaxL = leftChannel[i];
            currentMinR = currentMaxR = rightChannel[i];
        }
    }
    
    // Exponential smoothing for meter display
    constexpr float alpha = 0.3f;
    leftLevel.store(alpha * maxL + (1.0f - alpha) * leftLevel.load());
    rightLevel.store(alpha * maxR + (1.0f - alpha) * rightLevel.load());
    
    // Write to disk if recording
    if (isRecording.load() && backgroundWriter != nullptr) {
        backgroundWriter->write(buffer.getArrayOfReadPointers(), numSamples);
        samplesRecorded.fetch_add(numSamples);
    }
}

bool RecorderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Only stereo input, no output
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet().isDisabled();
}

// =============================================================================
// Recording Control
// =============================================================================
bool RecorderProcessor::startRecording() {
    if (isRecording.load()) return false;
    
    // Ensure recording folder exists
    if (!recordingFolder.exists()) {
        recordingFolder = getEffectiveDefaultFolder();
    }
    
    if (!recordingFolder.exists()) {
        recordingFolder.createDirectory();
    }
    
    // Create unique filename with timestamp
    auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    auto filename = recorderName.isEmpty() ? "Recording" : recorderName;
    lastRecordingFile = recordingFolder.getChildFile(filename + "_" + timestamp + ".wav")
                                       .getNonexistentSibling();
    
    // Create audio format
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> fileStream(lastRecordingFile.createOutputStream());
    
    if (fileStream == nullptr) return false;
    
    // FIX: Use currentSampleRate instead of hardcoded 44100.
    // The ASIO device may be running at 48000, 96000, etc.
    double sampleRate = currentSampleRate;
    if (sampleRate <= 0.0) sampleRate = 44100.0;
    
    // Create writer with 24-bit depth
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            fileStream.release(),
            sampleRate,
            2,                 // Channels: Stereo
            24,                // Bit depth: 24-bit
            {},                // Metadata
            0                  // Quality option
        )
    );
    
    if (writer == nullptr) return false;
    
    // FIX: Only start the thread if it's not already running.
    // Starting an already-running TimeSliceThread is harmless but wasteful.
    if (!writerThread.isThreadRunning())
        writerThread.startThread(juce::Thread::Priority::normal);
    
    // Wrap in ThreadedWriter for background I/O
    backgroundWriter.reset(new juce::AudioFormatWriter::ThreadedWriter(
        writer.release(), writerThread, 32768));
    
    samplesRecorded.store(0);
    isRecording.store(true);
    
    return true;
}

void RecorderProcessor::stopRecording() {
    if (!isRecording.load()) return;
    
    // FIX: Set flag FIRST so processBlock stops feeding the writer immediately
    isRecording.store(false);
    
    // Reset the threaded writer (flushes remaining samples to disk)
    backgroundWriter.reset();
    
    // FIX: Stop the writer thread after each recording session.
    // This prevents orphaned OS threads accumulating during the session
    // and ensures clean shutdown. The thread will be restarted on next
    // startRecording() call.
    if (writerThread.isThreadRunning())
        writerThread.stopThread(2000);
}

void RecorderProcessor::triggerSyncedRecording() {
    startRecording();
}

void RecorderProcessor::triggerSyncedStop() {
    stopRecording();
}

double RecorderProcessor::getRecordingLengthSeconds() const {
    if (currentSampleRate <= 0.0) return 0.0;
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