// D:\Workspace\ONSTAGE_WIRED\src\dsp\RecorderProcessor.cpp
// RECORDER SYSTEM TOOL - Implementation
// Streams audio directly to disk with ThreadedWriter for glitch-free recording
// OnStage Version: 24-bit at device sample rate
//
// FIXES APPLIED:
//   1. Metering/waveform skipped when not recording AND no UI watching
//   2. backgroundWriter protected by CriticalSection (no race between audio & message thread)
//   3. writerThread started once in ctor, stopped once in dtor
//   4. Waveform ring buffer reads use waveformLock (no torn reads)
//   5. Level metering decays to true zero with floor
//   6. Static registry has shutdown guard to prevent crash on app exit

#include "RecorderProcessor.h"

// =============================================================================
// Static Variables
// =============================================================================
std::atomic<bool>                   RecorderProcessor::registryShutdown { false };
juce::Array<RecorderProcessor*>     RecorderProcessor::syncedRecorders;
juce::SpinLock                      RecorderProcessor::registryLock;
juce::File                          RecorderProcessor::globalDefaultFolder;
juce::SpinLock                      RecorderProcessor::folderLock;

// =============================================================================
// Static Global Folder Management
// =============================================================================
void RecorderProcessor::setGlobalDefaultFolder (const juce::File& folder)
{
    juce::SpinLock::ScopedLockType lock (folderLock);
    globalDefaultFolder = folder;
}

juce::File RecorderProcessor::getGlobalDefaultFolder()
{
    juce::SpinLock::ScopedLockType lock (folderLock);
    return globalDefaultFolder;
}

juce::File RecorderProcessor::getEffectiveDefaultFolder()
{
    juce::SpinLock::ScopedLockType lock (folderLock);

    if (globalDefaultFolder.exists())
        return globalDefaultFolder;

    auto defaultFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("OnStage")
                             .getChildFile ("recordings");

    if (! defaultFolder.exists())
        defaultFolder.createDirectory();

    return defaultFolder;
}

void RecorderProcessor::openRecordingFolder() const
{
    juce::File folderToOpen = recordingFolder;

    if (! folderToOpen.exists())
        folderToOpen = getEffectiveDefaultFolder();

    if (! folderToOpen.exists())
        folderToOpen.createDirectory();

    folderToOpen.revealToUser();
}

// =============================================================================
// Static Sync Registry  (FIX #6: shutdown guard)
// =============================================================================
void RecorderProcessor::registerRecorder (RecorderProcessor* recorder)
{
    if (registryShutdown.load (std::memory_order_relaxed)) return;

    juce::SpinLock::ScopedLockType lock (registryLock);
    if (! syncedRecorders.contains (recorder))
        syncedRecorders.add (recorder);
}

void RecorderProcessor::unregisterRecorder (RecorderProcessor* recorder)
{
    if (registryShutdown.load (std::memory_order_relaxed)) return;

    juce::SpinLock::ScopedLockType lock (registryLock);
    syncedRecorders.removeFirstMatchingValue (recorder);
}

void RecorderProcessor::startAllSyncedRecorders()
{
    juce::SpinLock::ScopedLockType lock (registryLock);
    for (auto* recorder : syncedRecorders)
    {
        if (recorder->isSyncMode() && ! recorder->isCurrentlyRecording())
            recorder->triggerSyncedRecording();
    }
}

void RecorderProcessor::stopAllSyncedRecorders()
{
    juce::SpinLock::ScopedLockType lock (registryLock);
    for (auto* recorder : syncedRecorders)
    {
        if (recorder->isSyncMode() && recorder->isCurrentlyRecording())
            recorder->triggerSyncedStop();
    }
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
RecorderProcessor::RecorderProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true))
{
    recordingFolder = getEffectiveDefaultFolder();

    for (auto& sample : waveformBuffer)
        sample = { 0.0f, 0.0f, 0.0f, 0.0f };

    registerRecorder (this);

    // FIX #3: Start the writer thread once — it idles when no recording is active.
    //         TimeSliceThread only wakes when a TimeSliceClient is registered,
    //         so idle cost is essentially zero.
    writerThread.startThread (juce::Thread::Priority::normal);
}

RecorderProcessor::~RecorderProcessor()
{
    // Unregister FIRST to prevent sync callbacks during teardown
    unregisterRecorder (this);

    // Stop any active recording
    {
        const juce::ScopedLock sl (writerLock);
        isRecording.store (false, std::memory_order_relaxed);
        backgroundWriter.reset();  // flushes remaining samples
    }

    // FIX #3: Stop the thread once, here.
    writerThread.stopThread (2000);
}

// =============================================================================
// Audio Processing
// =============================================================================
void RecorderProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
}

void RecorderProcessor::releaseResources()
{
    // Full cleanup on graph teardown
    {
        const juce::ScopedLock sl (writerLock);
        isRecording.store (false, std::memory_order_relaxed);
        backgroundWriter.reset();
    }
    // NOTE: writerThread stays alive — it will be stopped in the destructor.
}

void RecorderProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& /*midiMessages*/)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels < 2 || numSamples == 0)
        return;

    const bool recording  = isRecording.load (std::memory_order_relaxed);
    const bool uiWatching = waveformActive.load (std::memory_order_relaxed);

    // FIX #1: Only do metering/waveform work when someone cares
    if (recording || uiWatching)
    {
        const float* leftChannel  = buffer.getReadPointer (0);
        const float* rightChannel = buffer.getReadPointer (1);

        float maxL = 0.0f, maxR = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float l = std::abs (leftChannel[i]);
            float r = std::abs (rightChannel[i]);

            maxL = std::max (maxL, l);
            maxR = std::max (maxR, r);

            // Waveform downsampling
            currentMinL = std::min (currentMinL, leftChannel[i]);
            currentMaxL = std::max (currentMaxL, leftChannel[i]);
            currentMinR = std::min (currentMinR, rightChannel[i]);
            currentMaxR = std::max (currentMaxR, rightChannel[i]);

            ++waveformDownsampleCounter;
            if (waveformDownsampleCounter >= waveformDownsampleFactor)
            {
                // FIX #4: Lock the waveform buffer for write
                {
                    juce::SpinLock::ScopedLockType lock (waveformLock);
                    int writePos = waveformWritePos.load (std::memory_order_relaxed);
                    waveformBuffer[writePos] = { currentMinL, currentMaxL,
                                                  currentMinR, currentMaxR };
                    waveformWritePos.store ((writePos + 1) % waveformBufferSize,
                                            std::memory_order_relaxed);
                }

                waveformDownsampleCounter = 0;
                currentMinL = currentMaxL = leftChannel[i];
                currentMinR = currentMaxR = rightChannel[i];
            }
        }

        // FIX #5: Exponential decay with floor — reaches true zero
        constexpr float alpha = 0.3f;
        constexpr float floor = 1e-7f;  // ~-140 dB

        float newL = alpha * maxL + (1.0f - alpha) * leftLevel.load (std::memory_order_relaxed);
        float newR = alpha * maxR + (1.0f - alpha) * rightLevel.load (std::memory_order_relaxed);

        leftLevel.store  (newL < floor ? 0.0f : newL, std::memory_order_relaxed);
        rightLevel.store (newR < floor ? 0.0f : newR, std::memory_order_relaxed);
    }
    else
    {
        // Not recording, no UI watching — zero the meters so they don't stick
        leftLevel.store  (0.0f, std::memory_order_relaxed);
        rightLevel.store (0.0f, std::memory_order_relaxed);
    }

    // FIX #2: Write to disk under lock so stopRecording() can't yank the writer
    if (recording)
    {
        const juce::ScopedLock sl (writerLock);
        if (backgroundWriter != nullptr)
        {
            backgroundWriter->write (buffer.getArrayOfReadPointers(), numSamples);
            samplesRecorded.fetch_add (numSamples, std::memory_order_relaxed);
        }
    }
}

bool RecorderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet().isDisabled();
}

// =============================================================================
// Recording Control
// =============================================================================
bool RecorderProcessor::startRecording()
{
    if (isRecording.load (std::memory_order_relaxed))
        return false;

    // Ensure recording folder exists
    if (! recordingFolder.exists())
        recordingFolder = getEffectiveDefaultFolder();

    if (! recordingFolder.exists())
        recordingFolder.createDirectory();

    // Create unique filename with timestamp
    auto timestamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
    auto filename  = recorderName.isEmpty() ? "Recording" : recorderName;
    lastRecordingFile = recordingFolder.getChildFile (filename + "_" + timestamp + ".wav")
                                       .getNonexistentSibling();

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> fileStream (lastRecordingFile.createOutputStream());

    if (fileStream == nullptr)
        return false;

    double sampleRate = currentSampleRate;
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (
            fileStream.release(),
            sampleRate,
            2,       // stereo
            24,      // 24-bit
            {},      // metadata
            0));

    if (writer == nullptr)
        return false;

    // FIX #2: Create the threaded writer under lock, then set the flag
    {
        const juce::ScopedLock sl (writerLock);
        backgroundWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (
            writer.release(), writerThread, 32768));
    }

    samplesRecorded.store (0, std::memory_order_relaxed);
    isRecording.store (true, std::memory_order_release);

    return true;
}

void RecorderProcessor::stopRecording()
{
    if (! isRecording.load (std::memory_order_relaxed))
        return;

    // FIX #2: Set flag FIRST, then destroy writer under lock.
    //         processBlock checks isRecording before taking the lock,
    //         so once this store is visible, no new writes will start.
    isRecording.store (false, std::memory_order_release);

    {
        const juce::ScopedLock sl (writerLock);
        backgroundWriter.reset();  // flushes remaining samples to disk
    }

    // FIX #3: Thread stays alive — no per-session stop/start.
}

void RecorderProcessor::triggerSyncedRecording()
{
    startRecording();
}

void RecorderProcessor::triggerSyncedStop()
{
    stopRecording();
}

double RecorderProcessor::getRecordingLengthSeconds() const
{
    if (currentSampleRate <= 0.0) return 0.0;
    return (double) samplesRecorded.load (std::memory_order_relaxed) / currentSampleRate;
}

// FIX #4: Waveform reads now use the lock
std::vector<RecorderProcessor::WaveformSample>
RecorderProcessor::getWaveformData (int numSamples) const
{
    std::vector<WaveformSample> result;
    result.reserve (numSamples);

    juce::SpinLock::ScopedLockType lock (waveformLock);

    int readPos  = waveformWritePos.load (std::memory_order_relaxed);
    int startPos = (readPos - numSamples + waveformBufferSize) % waveformBufferSize;

    for (int i = 0; i < numSamples; ++i)
    {
        int idx = (startPos + i) % waveformBufferSize;
        result.push_back (waveformBuffer[idx]);
    }

    return result;
}

// =============================================================================
// State
// =============================================================================
void RecorderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("RecorderState");
    state.setProperty ("name",     recorderName, nullptr);
    state.setProperty ("syncMode", syncMode.load (std::memory_order_relaxed), nullptr);
    state.setProperty ("folder",   recordingFolder.getFullPathName(), nullptr);

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void RecorderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, sizeInBytes);

    if (state.isValid())
    {
        recorderName = state.getProperty ("name", "Untitled").toString();
        syncMode.store ((bool) state.getProperty ("syncMode", true), std::memory_order_relaxed);

        juce::String folderPath = state.getProperty ("folder", "").toString();
        if (folderPath.isNotEmpty())
            recordingFolder = juce::File (folderPath);
    }
}
