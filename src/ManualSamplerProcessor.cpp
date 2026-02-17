
// #D:\Workspace\Subterraneum_plugins_daw\src\ManualSamplerProcessor.cpp
// MANUAL SAMPLER - Implementation
// Armed mode: waits for MIDI note-on, records audio, stops on silence

#include "ManualSamplerProcessor.h"

// =============================================================================
// Static Variables
// =============================================================================
juce::File ManualSamplerProcessor::globalDefaultFolder;
juce::SpinLock ManualSamplerProcessor::folderLock;

// =============================================================================
// Note Name Helper
// =============================================================================
juce::String ManualSamplerProcessor::midiNoteToName(int noteNumber)
{
    static const char* noteNames[] = { "C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B" };
    int octave = (noteNumber / 12) - 1;
    int note = noteNumber % 12;
    return juce::String(noteNames[note]) + juce::String(octave);
}

// =============================================================================
// Global Folder Management
// =============================================================================
void ManualSamplerProcessor::setGlobalDefaultFolder(const juce::File& folder) {
    juce::SpinLock::ScopedLockType lock(folderLock);
    globalDefaultFolder = folder;
}

juce::File ManualSamplerProcessor::getGlobalDefaultFolder() {
    juce::SpinLock::ScopedLockType lock(folderLock);
    return globalDefaultFolder;
}

juce::File ManualSamplerProcessor::getEffectiveDefaultFolder() {
    juce::SpinLock::ScopedLockType lock(folderLock);
    
    if (globalDefaultFolder.exists())
        return globalDefaultFolder;
    
    auto defaultFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("Colosseum")
                             .getChildFile("samples");
    
    if (!defaultFolder.exists())
        defaultFolder.createDirectory();
    
    return defaultFolder;
}

void ManualSamplerProcessor::openRecordingFolder() const {
    juce::File folderToOpen = recordingFolder;
    if (!folderToOpen.exists())
        folderToOpen = getEffectiveDefaultFolder();
    if (!folderToOpen.exists())
        folderToOpen.createDirectory();
    folderToOpen.revealToUser();
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
ManualSamplerProcessor::ManualSamplerProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true))
{
    recordingFolder = getEffectiveDefaultFolder();
    
    for (auto& sample : waveformBuffer)
        sample = { 0.0f, 0.0f, 0.0f, 0.0f };
}

ManualSamplerProcessor::~ManualSamplerProcessor() {
    stopAndSaveRecording();
    backgroundWriter.reset();
    writerThread.stopThread(5000);
}

// =============================================================================
// Audio Processing
// =============================================================================
void ManualSamplerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
}

void ManualSamplerProcessor::releaseResources() {
    stopAndSaveRecording();
}

void ManualSamplerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels < 2 || numSamples == 0) return;
    
    const float* leftChannel = buffer.getReadPointer(0);
    const float* rightChannel = buffer.getReadPointer(1);
    
    // =========================================================================
    // Calculate levels for metering + silence detection
    // =========================================================================
    float maxL = 0.0f, maxR = 0.0f;
    float blockRms = 0.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        float l = leftChannel[i];
        float r = rightChannel[i];
        float absL = std::abs(l);
        float absR = std::abs(r);
        
        maxL = std::max(maxL, absL);
        maxR = std::max(maxR, absR);
        blockRms += l * l + r * r;
        
        // Waveform downsampling
        currentMinL = std::min(currentMinL, l);
        currentMaxL = std::max(currentMaxL, l);
        currentMinR = std::min(currentMinR, r);
        currentMaxR = std::max(currentMaxR, r);
        
        waveformDownsampleCounter++;
        if (waveformDownsampleCounter >= waveformDownsampleFactor) {
            int writePos = waveformWritePos.load();
            waveformBuffer[writePos] = { currentMinL, currentMaxL, currentMinR, currentMaxR };
            waveformWritePos.store((writePos + 1) % waveformBufferSize);
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
    
    blockRms = std::sqrt(blockRms / (float)(numSamples * 2));
    float blockRmsDb = (blockRms > 0.0f) ? juce::Decibels::gainToDecibels(blockRms) : -100.0f;
    
    // =========================================================================
    // State Machine
    // =========================================================================
    State currentState = state.load();
    float threshDb = silenceThresholdDb.load();
    int silenceDurationSamples = (int)(silenceDurationMs.load() * currentSampleRate / 1000.0);
    
    if (currentState == IDLE && isArmed.load())
    {
        // Check incoming MIDI for note-on
        for (const auto metadata : midiMessages) {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn() && msg.getVelocity() > 0) {
                currentNote = msg.getNoteNumber();
                currentVelocity = msg.getVelocity();
                hasReceivedAudio = false;
                silenceSampleCount = 0;
                
                if (startNewRecording(currentNote, currentVelocity)) {
                    state.store(RECORDING);
                }
                break;
            }
        }
    }
    
    if (currentState == RECORDING || currentState == WAITING_SILENCE)
    {
        // Write audio to disk
        if (backgroundWriter != nullptr) {
            backgroundWriter->write(buffer.getArrayOfReadPointers(), numSamples);
            samplesRecorded.fetch_add(numSamples);
        }
        
        // Track if we've heard any audio above threshold
        if (blockRmsDb > threshDb)
            hasReceivedAudio = true;
        
        // Silence detection (only after audio has been received)
        if (hasReceivedAudio) {
            if (blockRmsDb < threshDb) {
                silenceSampleCount += numSamples;
                if (silenceSampleCount >= silenceDurationSamples) {
                    stopAndSaveRecording();
                    state.store(IDLE);
                }
            } else {
                silenceSampleCount = 0;
            }
        }
    }
    
    // Clear the buffer - termination point
    buffer.clear();
}

bool ManualSamplerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
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
juce::File ManualSamplerProcessor::buildFilePath(int note, int velocity) const
{
    juce::File targetFolder = recordingFolder;
    if (!targetFolder.exists())
        targetFolder = getEffectiveDefaultFolder();
    if (!targetFolder.exists())
        targetFolder.createDirectory();
    
    juce::String safeName = familyName.replaceCharacters(" :/\\*?\"<>|", "___________");
    juce::String noteName = midiNoteToName(note);
    juce::String filename = safeName + "_" + noteName + "_V" + juce::String(velocity) + ".wav";
    
    return targetFolder.getChildFile(filename);
}

bool ManualSamplerProcessor::startNewRecording(int note, int velocity)
{
    stopAndSaveRecording();
    
    if (!writerThread.isThreadRunning())
        writerThread.startThread();
    
    double sampleRate = currentSampleRate;
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    juce::File targetFile = buildFilePath(note, velocity);
    
    // Overwrite existing file (last take wins)
    if (targetFile.exists())
        targetFile.deleteFile();
    
    auto wavFormat = std::make_unique<juce::WavAudioFormat>();
    auto outputStream = std::make_unique<juce::FileOutputStream>(targetFile);
    
    if (outputStream->failedToOpen())
        return false;
    
    auto* writer = wavFormat->createWriterFor(
        outputStream.release(), sampleRate, juce::AudioChannelSet::stereo(), 24, {}, 0);
    
    if (writer == nullptr)
        return false;
    
    backgroundWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, writerThread, 32768);
    
    samplesRecorded.store(0);
    return true;
}

void ManualSamplerProcessor::stopAndSaveRecording()
{
    if (backgroundWriter != nullptr) {
        backgroundWriter.reset();
        lastRecordedNote.store(currentNote);
        lastRecordedVelocity.store(currentVelocity);
        totalFilesRecorded.fetch_add(1);
    }
    state.store(IDLE);
}

double ManualSamplerProcessor::getRecordingLengthSeconds() const {
    if (currentSampleRate <= 0) return 0.0;
    return (double)samplesRecorded.load() / currentSampleRate;
}

std::vector<ManualSamplerProcessor::WaveformSample> ManualSamplerProcessor::getWaveformData(int numSamples) const {
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

// =============================================================================
// State Persistence
// =============================================================================
void ManualSamplerProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree vt("ManualSamplerState");
    vt.setProperty("familyName", familyName, nullptr);
    vt.setProperty("armed", isArmed.load(), nullptr);
    vt.setProperty("folder", recordingFolder.getFullPathName(), nullptr);
    vt.setProperty("silenceThresholdDb", (double)silenceThresholdDb.load(), nullptr);
    vt.setProperty("silenceDurationMs", (double)silenceDurationMs.load(), nullptr);
    
    juce::MemoryOutputStream stream(destData, false);
    vt.writeToStream(stream);
}

void ManualSamplerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto vt = juce::ValueTree::readFromData(data, sizeInBytes);
    if (vt.isValid()) {
        familyName = vt.getProperty("familyName", "Sample").toString();
        isArmed.store((bool)vt.getProperty("armed", false));
        
        juce::String folderPath = vt.getProperty("folder", "").toString();
        if (folderPath.isNotEmpty())
            recordingFolder = juce::File(folderPath);
        
        silenceThresholdDb.store((float)(double)vt.getProperty("silenceThresholdDb", -60.0));
        silenceDurationMs.store((float)(double)vt.getProperty("silenceDurationMs", 500.0));
    }
}





