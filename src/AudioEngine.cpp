// ==============================================================================
//  AudioEngine.cpp
//  OnStage — Audio engine implementation with graph-based routing
//
//  Device lifecycle:
//    audioDeviceAboutToStart  → graph->prepare()   (rebuilds I/O nodes)
//    audioDeviceStopped       → graph->suspend()   (preserves topology)
//    shutdown                 → graph->releaseResources() (full teardown)
// ==============================================================================

#include "AudioEngine.h"
#include "graph/OnStageGraph.h"
#include "graph/GraphSerializer.h"
#include "PresetManager.h"

// ==============================================================================
//  Construction / Destruction
// ==============================================================================

AudioEngine::AudioEngine()
{
    graph = std::make_unique<OnStageGraph>();
    formatManager.registerBasicFormats();
    writerThread.startThread (juce::Thread::Priority::normal);
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

// ==============================================================================
//  Lifecycle
// ==============================================================================

void AudioEngine::initialise()
{
    LOG_INFO ("AudioEngine::initialise");

    ioSettings.loadSettings();

    // Don't auto-open any device — the IOPage drives ASIO selection.
    // initialise with 0 channels so no device opens.
    deviceManager.initialise (0, 0, nullptr, false);

    deviceManager.addChangeListener (this);
    deviceManager.addAudioCallback (this);
}

void AudioEngine::shutdown()
{
    LOG_INFO ("AudioEngine::shutdown starting");

    // 1. Stop recording first (releases writer, but thread keeps running)
    stopRecording();
    
    // 2. Stop the recording thread (wait up to 2 seconds)
    if (writerThread.isThreadRunning())
    {
        LOG_INFO ("AudioEngine::shutdown - stopping writer thread");
        writerThread.stopThread (2000);
    }
    
    // 3. Stop media player (VLC threads)
    LOG_INFO ("AudioEngine::shutdown - stopping media player");
    mediaPlayer.stop();
    mediaPlayer.releaseResources();
    
    // 4. Remove audio callbacks BEFORE closing device
    //    (prevents callbacks during teardown)
    deviceManager.removeAudioCallback (this);
    deviceManager.removeChangeListener (this);
    
    // 5. Explicitly close the audio device (ASIO driver cleanup)
    LOG_INFO ("AudioEngine::shutdown - closing audio device");
    deviceManager.closeAudioDevice();
    
    // 6. Release graph resources last
    LOG_INFO ("AudioEngine::shutdown - releasing graph");
    if (graph)
        graph->releaseResources();
    
    LOG_INFO ("AudioEngine::shutdown complete");
}

// ==============================================================================
//  AudioIODeviceCallback
// ==============================================================================

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr) return;

    currentSampleRate = device->getCurrentSampleRate();
    currentBlockSize  = device->getCurrentBufferSizeSamples();

    auto activeIns  = device->getActiveInputChannels();
    auto activeOuts = device->getActiveOutputChannels();
    currentNumInputs  = activeIns.countNumberOfSetBits();
    currentNumOutputs = activeOuts.countNumberOfSetBits();

    LOG_INFO ("AudioEngine::audioDeviceAboutToStart  SR="
              + juce::String (currentSampleRate)
              + "  BS=" + juce::String (currentBlockSize)
              + "  ins=" + juce::String (currentNumInputs)
              + "  outs=" + juce::String (currentNumOutputs));

    // Prepare the graph — rebuilds only I/O nodes, keeps user effects alive
    graph->prepare (currentSampleRate, currentBlockSize,
                    currentNumInputs, currentNumOutputs,
                    mediaPlayer);

    // Store hardware channel names for pin tooltips
    graph->inputChannelNames  = device->getInputChannelNames();
    graph->outputChannelNames = device->getOutputChannelNames();
}

void AudioEngine::audioDeviceStopped()
{
    LOG_INFO ("AudioEngine::audioDeviceStopped");

    // Suspend — NOT releaseResources.
    // This preserves the user's effect nodes and wiring.
    // Only the I/O nodes will be rebuilt on next audioDeviceAboutToStart.
    graph->suspend();
}

void AudioEngine::audioDeviceIOCallbackWithContext (
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    processAudio (inputChannelData, numInputChannels,
                  outputChannelData, numOutputChannels,
                  numSamples);
}

// ==============================================================================
//  Core audio processing
// ==============================================================================

void AudioEngine::processAudio (const float* const* inputs,  int numIns,
                                 float* const* outputs, int numOuts,
                                 int numSamples)
{
    // --- Safety: if graph is not prepared, output silence ---------------------
    if (! graph || ! graph->isPrepared())
    {
        for (int ch = 0; ch < numOuts; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);
        return;
    }

    const int totalChannels = juce::jmax (numIns, numOuts);
    juce::AudioBuffer<float> buffer (totalChannels, numSamples);
    buffer.clear();

    // --- 1. Copy hardware inputs into the buffer -----------------------------
    for (int ch = 0; ch < numIns; ++ch)
    {
        if (inputs[ch] != nullptr)
            buffer.copyFrom (ch, 0, inputs[ch], numSamples);

        // Update input metering
        float peak = 0.0f;
        const float* src = inputs[ch];
        if (src != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs (src[i]));
        }
        if (ch < kMaxChannels)
            inputLevels[ch].store (peak, std::memory_order_relaxed);
    }

    // --- 2. Run the graph (zombie flush handled internally) ------------------
    juce::MidiBuffer midi;
    graph->processBlock (buffer, midi);

    // --- 3. Apply master volume ----------------------------------------------
    const float vol = masterVolume.load (std::memory_order_relaxed);
    if (vol != 1.0f)
        buffer.applyGain (vol);

    // --- 4. Copy processed audio to hardware outputs -------------------------
    for (int ch = 0; ch < numOuts; ++ch)
    {
        if (outputs[ch] != nullptr)
        {
            if (ch < buffer.getNumChannels())
                juce::FloatVectorOperations::copy (outputs[ch],
                                                    buffer.getReadPointer (ch),
                                                    numSamples);
            else
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);
        }

        // Update output metering
        float peak = 0.0f;
        if (ch < buffer.getNumChannels())
            peak = buffer.getMagnitude (ch, 0, numSamples);
        if (ch < kMaxChannels)
            outputLevels[ch].store (peak, std::memory_order_relaxed);
    }

    // --- 5. Feed recording writer (if active) --------------------------------
    {
        const juce::ScopedLock sl (writerLock);
        if (threadedWriter != nullptr)
        {
            threadedWriter->write (buffer.getArrayOfReadPointers(),
                                   numSamples);
        }
    }
}

// ==============================================================================
//  Change listener (device reconfiguration)
// ==============================================================================

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        LOG_INFO ("AudioEngine: device configuration changed");
    }
}

// ==============================================================================
//  Master volume
// ==============================================================================

void AudioEngine::setMasterVolume (float linearGain)
{
    masterVolume.store (juce::jlimit (0.0f, 2.0f, linearGain),
                        std::memory_order_relaxed);
}

// ==============================================================================
//  Metering
// ==============================================================================

float AudioEngine::getInputLevel (int channel) const
{
    if (channel < 0 || channel >= kMaxChannels) return 0.0f;
    return inputLevels[channel].load (std::memory_order_relaxed);
}

float AudioEngine::getOutputLevel (int channel) const
{
    if (channel < 0 || channel >= kMaxChannels) return 0.0f;
    return outputLevels[channel].load (std::memory_order_relaxed);
}

// ==============================================================================
//  Recording
// ==============================================================================

void AudioEngine::startRecording (const juce::File& outputFile)
{
    stopRecording();

    if (outputFile == juce::File())
    {
        LOG_ERROR ("AudioEngine::startRecording — no file specified");
        return;
    }

    outputFile.deleteFile();

    std::unique_ptr<juce::AudioFormatWriter> writer;

    if (auto* wavFormat = formatManager.findFormatForFileExtension ("wav"))
    {
        auto stream = outputFile.createOutputStream();
        if (stream != nullptr)
        {
            const int numChToRecord = juce::jmin (currentNumOutputs, 2);
            writer.reset (wavFormat->createWriterFor (
                stream.release(), currentSampleRate, (unsigned int) numChToRecord,
                24, {}, 0));
        }
    }

    if (writer != nullptr)
    {
        const juce::ScopedLock sl (writerLock);
        threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (
            writer.release(), writerThread, 65536);
        recording.store (true, std::memory_order_relaxed);
        LOG_INFO ("Recording started → " + outputFile.getFullPathName());
    }
    else
    {
        LOG_ERROR ("AudioEngine::startRecording — failed to create writer");
    }
}

void AudioEngine::stopRecording()
{
    {
        const juce::ScopedLock sl (writerLock);
        threadedWriter.reset();
    }
    recording.store (false, std::memory_order_relaxed);
}

// ==============================================================================
//  Graph state persistence
// ==============================================================================

void AudioEngine::saveGraphState (const juce::File& file)
{
    LOG_INFO ("AudioEngine::saveGraphState → " + file.getFullPathName());
    GraphSerializer::saveToFile (getGraph(), file);
}

void AudioEngine::loadGraphState (const juce::File& file, PresetManager& presetMgr)
{
    LOG_INFO ("AudioEngine::loadGraphState ← " + file.getFullPathName());
    GraphSerializer::loadFromFile (getGraph(), file);
}