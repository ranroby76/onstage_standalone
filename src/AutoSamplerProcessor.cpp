// #D:\Workspace\Subterraneum_plugins_daw\src\AutoSamplerProcessor.cpp
// AUTO SAMPLING - Implementation
// Architecture: MIDI-only node. Taps audio from end of connected plugin chain.
// State machine: IDLE -> STARTING_NOTE -> RECORDING_NOTE -> WAITING_SILENCE -> loop/DONE
// Chain walking: follows MIDI output -> VSTi -> effects -> last MeteringProcessor

#include "AutoSamplerProcessor.h"
#include "PluginProcessor.h"
#include "SimpleConnectorProcessor.h"

// =============================================================================
// Static Variables
// =============================================================================
juce::File AutoSamplerProcessor::globalDefaultFolder;
juce::SpinLock AutoSamplerProcessor::folderLock;

// =============================================================================
// Note Name Helper
// =============================================================================
juce::String AutoSamplerProcessor::midiNoteToName(int noteNumber)
{
    static const char* noteNames[] = { "C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B" };
    int octave = (noteNumber / 12) - 1;
    int note = noteNumber % 12;
    return juce::String(noteNames[note]) + juce::String(octave);
}

// =============================================================================
// Global Folder Management
// =============================================================================
void AutoSamplerProcessor::setGlobalDefaultFolder(const juce::File& folder) {
    juce::SpinLock::ScopedLockType lock(folderLock);
    globalDefaultFolder = folder;
}

juce::File AutoSamplerProcessor::getGlobalDefaultFolder() {
    juce::SpinLock::ScopedLockType lock(folderLock);
    return globalDefaultFolder;
}

juce::File AutoSamplerProcessor::getEffectiveDefaultFolder() {
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

void AutoSamplerProcessor::openRecordingFolder() const {
    juce::File folderToOpen = recordingFolder;
    if (!folderToOpen.exists())
        folderToOpen = getEffectiveDefaultFolder();
    if (!folderToOpen.exists())
        folderToOpen.createDirectory();
    folderToOpen.revealToUser();
}

// =============================================================================
// Text Syntax Parser
// =============================================================================
std::vector<int> AutoSamplerProcessor::parseGroup(const juce::String& group)
{
    std::vector<int> result;
    auto tokens = juce::StringArray::fromTokens(group.trim(), ",", "");
    
    for (auto& token : tokens) {
        auto t = token.trim();
        if (t.isEmpty()) continue;
        
        if (t.contains("-")) {
            int dashPos = t.indexOf("-");
            int start = t.substring(0, dashPos).trim().getIntValue();
            int end = t.substring(dashPos + 1).trim().getIntValue();
            
            start = juce::jlimit(0, 127, start);
            end = juce::jlimit(0, 127, end);
            
            if (start <= end) {
                for (int i = start; i <= end; i++)
                    result.push_back(i);
            } else {
                for (int i = start; i >= end; i--)
                    result.push_back(i);
            }
        } else {
            int val = juce::jlimit(0, 127, t.getIntValue());
            result.push_back(val);
        }
    }
    
    return result;
}

std::vector<float> AutoSamplerProcessor::parseFloatGroup(const juce::String& group)
{
    std::vector<float> result;
    auto tokens = juce::StringArray::fromTokens(group.trim(), ",", "");
    
    for (auto& token : tokens) {
        auto t = token.trim();
        if (t.isEmpty()) continue;
        
        float val = t.getFloatValue();
        if (val > 0.0f)
            result.push_back(val);
    }
    
    return result;
}

std::vector<AutoSamplerProcessor::NoteVelocityPair> AutoSamplerProcessor::parseNoteList(const juce::String& text)
{
    std::vector<NoteVelocityPair> result;
    
    auto lines = juce::StringArray::fromLines(text);
    
    for (auto& line : lines) {
        auto trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWith("//") || trimmed.startsWith("#"))
            continue;
        
        // Find first and second [...] groups (notes and velocities - required)
        int firstOpen = trimmed.indexOf("[");
        int firstClose = trimmed.indexOf("]");
        if (firstOpen < 0 || firstClose < 0 || firstClose <= firstOpen)
            continue;
        
        int secondOpen = trimmed.indexOf(firstClose + 1, "[");
        int secondClose = (secondOpen >= 0) ? trimmed.indexOf(secondOpen + 1, "]") : -1;
        if (secondOpen < 0 || secondClose < 0)
            continue;
        
        // Find optional third [...] group (durations in seconds)
        int thirdOpen = trimmed.indexOf(secondClose + 1, "[");
        int thirdClose = (thirdOpen >= 0) ? trimmed.indexOf(thirdOpen + 1, "]") : -1;
        
        auto notesStr = trimmed.substring(firstOpen + 1, firstClose);
        auto velsStr = trimmed.substring(secondOpen + 1, secondClose);
        
        std::vector<float> durations;
        if (thirdOpen >= 0 && thirdClose > thirdOpen)
        {
            auto durStr = trimmed.substring(thirdOpen + 1, thirdClose);
            durations = parseFloatGroup(durStr);
        }
        
        bool notesHasRange = notesStr.contains("-");
        bool velsHasRange = velsStr.contains("-");
        
        auto notes = parseGroup(notesStr);
        auto vels = parseGroup(velsStr);
        
        if (notes.empty() || vels.empty())
            continue;
        
        // Build note/velocity pairs
        std::vector<NoteVelocityPair> linePairs;
        
        if (!notesHasRange && !velsHasRange && notes.size() == vels.size()) {
            for (size_t i = 0; i < notes.size(); i++)
                linePairs.push_back({ notes[i], juce::jlimit(1, 127, vels[i]), 0.0f });
        } else {
            for (int n : notes)
                for (int v : vels)
                    linePairs.push_back({ n, juce::jlimit(1, 127, v), 0.0f });
        }
        
        // Apply durations if provided
        if (!durations.empty())
        {
            if (durations.size() == 1)
            {
                for (auto& p : linePairs)
                    p.durationSeconds = durations[0];
            }
            else if (durations.size() == linePairs.size())
            {
                for (size_t i = 0; i < linePairs.size(); i++)
                    linePairs[i].durationSeconds = durations[i];
            }
        }
        
        for (auto& p : linePairs)
            result.push_back(p);
    }
    
    return result;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
AutoSamplerProcessor::AutoSamplerProcessor(juce::AudioProcessorGraph* graph, 
                                           SubterraneumAudioProcessor* mainProc)
    : AudioProcessor(BusesProperties())  // No audio buses - MIDI output only
    , graphRef(graph)
    , mainProcessorRef(mainProc)
{
    recordingFolder = getEffectiveDefaultFolder();
    
    for (auto& sample : waveformBuffer)
        sample = { 0.0f, 0.0f, 0.0f, 0.0f };
}

AutoSamplerProcessor::~AutoSamplerProcessor() {
    // CRITICAL: Invalidate alive flag FIRST to prevent async callbacks from firing
    aliveFlag->store(false);
    
    stopAutoSampling();
    backgroundWriter.reset();
    writerThread.stopThread(5000);
}

// =============================================================================
// Chain Walking - Find the last MeteringProcessor in the audio chain
// Starting from our MIDI output target, follows audio connections downstream
// =============================================================================
MeteringProcessor* AutoSamplerProcessor::findChainEnd()
{
    if (!graphRef || !mainProcessorRef) return nullptr;
    
    // Find our own node in the graph
    juce::AudioProcessorGraph::Node* myNode = nullptr;
    for (auto* node : graphRef->getNodes()) {
        if (node->getProcessor() == this) {
            myNode = node;
            break;
        }
    }
    if (!myNode) return nullptr;
    
    // Find where our MIDI output connects to
    juce::AudioProcessorGraph::NodeID midiTargetID;
    for (auto& conn : graphRef->getConnections()) {
        if (conn.source.nodeID == myNode->nodeID &&
            conn.source.channelIndex == juce::AudioProcessorGraph::midiChannelIndex) {
            midiTargetID = conn.destination.nodeID;
            break;
        }
    }
    
    if (midiTargetID.uid == 0) return nullptr;
    
    // Follow the audio chain downstream from the MIDI target
    // Track the last MeteringProcessor we encounter (that's our tap point)
    MeteringProcessor* lastMetering = nullptr;
    juce::AudioProcessorGraph::NodeID currentID = midiTargetID;
    std::set<uint32> visited;
    
    while (currentID.uid != 0)
    {
        if (visited.count(currentID.uid)) break;  // Cycle protection
        visited.insert(currentID.uid);
        
        auto* node = graphRef->getNodeForId(currentID);
        if (!node) break;
        
        // Stop at Audio Output node
        if (node == mainProcessorRef->audioOutputNode.get())
            break;
        
        // Check if this is a plugin wrapper (our tap candidate)
        if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor()))
            lastMetering = mp;
        
        // Follow first audio output connection (channel 0 = left audio)
        juce::AudioProcessorGraph::NodeID nextID;
        for (auto& conn : graphRef->getConnections()) {
            if (conn.source.nodeID == currentID &&
                conn.source.channelIndex == 0) {  // Audio channel 0
                if (visited.count(conn.destination.nodeID.uid) == 0) {
                    nextID = conn.destination.nodeID;
                    break;
                }
            }
        }
        
        currentID = nextID;
    }
    
    return lastMetering;
}

// =============================================================================
// Audio Processing
// =============================================================================
void AutoSamplerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
}

void AutoSamplerProcessor::releaseResources() {
    stopAutoSampling();
}

void AutoSamplerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear outgoing MIDI (we add our own events via state machine)
    midiMessages.clear();
    
    // No audio output (we have no audio buses, buffer may be 0-channel)
    if (buffer.getNumChannels() > 0)
        buffer.clear();
    
    if (!autoRunning.load()) {
        // Decay levels when idle
        leftLevel.store(leftLevel.load() * 0.95f);
        rightLevel.store(rightLevel.load() * 0.95f);
        return;
    }
    
    // =========================================================================
    // Read audio from tap source for metering, recording, and silence detection
    // =========================================================================
    float blockRmsDb = -100.0f;
    int tapSamples = 0;
    
    if (tapSource != nullptr && tapSource->isAudioTapEnabled())
    {
        tapSamples = tapSource->getTapSamplesReady();
        
        if (tapSamples > 0)
        {
            const auto& tapBuf = tapSource->getTapBuffer();
            int samples = juce::jmin(tapSamples, tapBuf.getNumSamples());
            
            // Calculate levels and RMS from tap audio
            float maxL = 0.0f, maxR = 0.0f;
            float blockRms = 0.0f;
            
            for (int i = 0; i < samples; ++i) {
                float l = (tapBuf.getNumChannels() > 0) ? tapBuf.getSample(0, i) : 0.0f;
                float r = (tapBuf.getNumChannels() > 1) ? tapBuf.getSample(1, i) : l;
                
                maxL = std::max(maxL, std::abs(l));
                maxR = std::max(maxR, std::abs(r));
                blockRms += l * l + r * r;
                
                // Waveform ring buffer
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
            
            leftLevel.store(std::max(maxL, leftLevel.load() * 0.95f));
            rightLevel.store(std::max(maxR, rightLevel.load() * 0.95f));
            
            blockRms = std::sqrt(blockRms / (float)(samples * 2));
            blockRmsDb = (blockRms > 0.0f) ? juce::Decibels::gainToDecibels(blockRms) : -100.0f;
            
            tapSource->consumeTapSamples();
        }
    }
    
    // =========================================================================
    // Auto-Sampling State Machine
    // =========================================================================
    float threshDb = silenceThresholdDb.load();
    int silenceDurationSamples = (int)(silenceDurationMs.load() * currentSampleRate / 1000.0);
    
    // Use tap sample count for timing, fall back to a nominal block size
    int blockSamples = (tapSamples > 0) ? tapSamples : 512;
    
    State currentState = state.load();
    int idx = currentIndex.load();
    
    switch (currentState)
    {
    case STARTING_NOTE:
    {
        if (idx >= (int)noteList.size()) {
            state.store(DONE);
            autoRunning.store(false);
            break;
        }
        
        auto& pair = noteList[idx];
        activeNote.store(pair.note);
        activeVelocity.store(pair.velocity);
        
        // Calculate hold time: per-note duration overrides global noteHoldMs
        if (pair.durationSeconds > 0.0f)
            currentNoteHoldSamples = (int)(pair.durationSeconds * currentSampleRate);
        else
            currentNoteHoldSamples = (int)(noteHoldMs.load() * currentSampleRate / 1000.0);
        
        // Send note-on via MIDI output
        midiMessages.addEvent(
            juce::MidiMessage::noteOn(1, pair.note, (juce::uint8)pair.velocity), 0);
        
        noteOnSent = true;
        noteOffSent = false;
        noteHoldSampleCount = 0;
        silenceSampleCount = 0;
        hasReceivedAudio = false;
        
        // Start recording this note
        startNewRecording(pair.note, pair.velocity);
        
        state.store(RECORDING_NOTE);
        break;
    }
    
    case RECORDING_NOTE:
    {
        // Write tap audio to disk
        if (tapSamples > 0 && tapSource != nullptr && backgroundWriter != nullptr) {
            const auto& tapBuf = tapSource->getTapBuffer();
            int samples = juce::jmin(tapSamples, tapBuf.getNumSamples());
            backgroundWriter->write(tapBuf.getArrayOfReadPointers(), samples);
            samplesRecorded.fetch_add(samples);
        }
        
        if (blockRmsDb > threshDb)
            hasReceivedAudio = true;
        
        noteHoldSampleCount += blockSamples;
        
        // After hold time elapsed, send note-off
        if (noteHoldSampleCount >= currentNoteHoldSamples && !noteOffSent) {
            int note = activeNote.load();
            midiMessages.addEvent(
                juce::MidiMessage::noteOff(1, note), 0);
            noteOffSent = true;
            state.store(WAITING_SILENCE);
        }
        break;
    }
    
    case WAITING_SILENCE:
    {
        // Continue recording (captures release tail / reverb / delay)
        if (tapSamples > 0 && tapSource != nullptr && backgroundWriter != nullptr) {
            const auto& tapBuf = tapSource->getTapBuffer();
            int samples = juce::jmin(tapSamples, tapBuf.getNumSamples());
            backgroundWriter->write(tapBuf.getArrayOfReadPointers(), samples);
            samplesRecorded.fetch_add(samples);
        }
        
        if (blockRmsDb > threshDb)
            hasReceivedAudio = true;
        
        // Silence detection (only after audio was received)
        if (hasReceivedAudio) {
            if (blockRmsDb < threshDb) {
                silenceSampleCount += blockSamples;
                if (silenceSampleCount >= silenceDurationSamples) {
                    stopAndSaveCurrentRecording();
                    
                    int nextIdx = idx + 1;
                    currentIndex.store(nextIdx);
                    
                    if (nextIdx < (int)noteList.size()) {
                        state.store(STARTING_NOTE);
                    } else {
                        state.store(DONE);
                        autoRunning.store(false);
                        // Disable tap when done
                        if (tapSource) tapSource->disableAudioTap();
                        tapSource = nullptr;
                        
                        // Generate SFZ file on message thread if enabled
                        // Use shared alive flag to prevent use-after-free
                        if (createSfz.load())
                        {
                            auto aliveFlag = this->aliveFlag;
                            auto* self = this;
                            juce::MessageManager::callAsync([aliveFlag, self]() {
                                if (aliveFlag && aliveFlag->load())
                                    self->generateSfzFile();
                            });
                        }
                    }
                }
            } else {
                silenceSampleCount = 0;
            }
        }
        break;
    }
    
    case DONE:
    case IDLE:
    default:
        autoRunning.store(false);
        if (tapSource) {
            tapSource->disableAudioTap();
            tapSource = nullptr;
        }
        break;
    }
}

// =============================================================================
// Auto-Sampling Control
// =============================================================================
bool AutoSamplerProcessor::startAutoSampling()
{
    stopAutoSampling();
    
    // Parse the editor text into note/velocity/duration triples
    noteList = parseNoteList(editorText);
    
    if (noteList.empty()) {
        lastError = "No notes configured. Use the editor (E) to add notes.";
        return false;
    }
    
    // Walk the chain to find our audio tap source
    tapSource = findChainEnd();
    
    if (tapSource == nullptr) {
        lastError = "Connect MIDI output to a VSTi/plugin chain first.";
        return false;
    }
    
    // Enable the audio tap on the source
    tapSource->enableAudioTap();
    
    totalNotes.store((int)noteList.size());
    currentIndex.store(0);
    activeNote.store(-1);
    activeVelocity.store(-1);
    lastError = "";
    
    state.store(STARTING_NOTE);
    autoRunning.store(true);
    
    return true;
}

void AutoSamplerProcessor::stopAutoSampling()
{
    autoRunning.store(false);
    
    // If we were mid-note, the note-off should be sent
    int note = activeNote.load();
    if (note >= 0 && noteOnSent && !noteOffSent) {
        noteOffSent = true;
    }
    
    stopAndSaveCurrentRecording();
    
    // Stop the writer thread when session ends (no longer needed until next session)
    if (writerThread.isThreadRunning())
        writerThread.stopThread(5000);
    
    // Disable audio tap
    if (tapSource) {
        tapSource->disableAudioTap();
        tapSource = nullptr;
    }
    
    state.store(IDLE);
    activeNote.store(-1);
    activeVelocity.store(-1);
}

// =============================================================================
// Recording Control
// =============================================================================
juce::File AutoSamplerProcessor::buildFilePath(int note, int velocity) const
{
    juce::File targetFolder = recordingFolder;
    if (!targetFolder.exists())
        targetFolder = getEffectiveDefaultFolder();
    if (!targetFolder.exists())
        targetFolder.createDirectory();
    
    juce::String safeName = familyName.replaceCharacters(" :/\\*?\"<>|", "___________");
    
    // When SFZ is enabled, use FamilyName/samples/ subfolder
    if (createSfz.load())
    {
        targetFolder = targetFolder.getChildFile(safeName).getChildFile("samples");
        if (!targetFolder.exists())
            targetFolder.createDirectory();
    }
    
    juce::String noteName = midiNoteToName(note);
    juce::String filename = safeName + "_" + noteName + "_V" + juce::String(velocity) + ".wav";
    
    return targetFolder.getChildFile(filename);
}

bool AutoSamplerProcessor::startNewRecording(int note, int velocity)
{
    stopAndSaveCurrentRecording();
    
    if (!writerThread.isThreadRunning())
        writerThread.startThread();
    
    double sampleRate = currentSampleRate;
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    juce::File targetFile = buildFilePath(note, velocity);
    
    if (targetFile.exists())
        targetFile.deleteFile();
    
    auto wavFormat = std::make_unique<juce::WavAudioFormat>();
    auto outputStream = std::make_unique<juce::FileOutputStream>(targetFile);
    
    if (outputStream->failedToOpen())
        return false;
    
    auto* writer = wavFormat->createWriterFor(
        outputStream.release(), sampleRate, 2, 24, {}, 0);
    
    if (writer == nullptr)
        return false;
    
    backgroundWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, writerThread, 32768);
    
    samplesRecorded.store(0);
    return true;
}

void AutoSamplerProcessor::stopAndSaveCurrentRecording()
{
    if (backgroundWriter != nullptr)
        backgroundWriter.reset();
}

double AutoSamplerProcessor::getRecordingLengthSeconds() const {
    if (currentSampleRate <= 0) return 0.0;
    return (double)samplesRecorded.load() / currentSampleRate;
}

std::vector<AutoSamplerProcessor::WaveformSample> AutoSamplerProcessor::getWaveformData(int numSamples) const {
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
// SFZ File Generation
// =============================================================================
void AutoSamplerProcessor::generateSfzFile()
{
    if (noteList.empty()) return;
    
    juce::File targetFolder = recordingFolder;
    if (!targetFolder.exists())
        targetFolder = getEffectiveDefaultFolder();
    if (!targetFolder.exists()) return;
    
    juce::String safeName = familyName.replaceCharacters(" :/\\*?\"<>|", "___________");
    
    // SFZ goes in the recording root folder, samples are in FamilyName/samples/
    juce::File sfzFile = targetFolder.getChildFile(safeName + ".sfz");
    
    // Collect unique sorted notes and build velocity layers per note
    struct NoteRegion {
        int note;
        std::vector<int> velocities;
    };
    
    std::map<int, std::vector<int>> noteVelMap;
    for (auto& pair : noteList)
    {
        // Only include if the wav file actually exists
        juce::File wavFile = buildFilePath(pair.note, pair.velocity);
        if (wavFile.existsAsFile())
            noteVelMap[pair.note].push_back(pair.velocity);
    }
    
    if (noteVelMap.empty()) return;
    
    // Build sorted list of unique notes
    std::vector<int> sortedNotes;
    for (auto& [note, vels] : noteVelMap)
    {
        sortedNotes.push_back(note);
        std::sort(vels.begin(), vels.end());
    }
    std::sort(sortedNotes.begin(), sortedNotes.end());
    
    bool doFillGap = fillGap.load();
    
    // Calculate key ranges per note
    struct KeyRange { int lokey; int hikey; };
    std::map<int, KeyRange> keyRanges;
    
    if (doFillGap && sortedNotes.size() >= 1)
    {
        for (size_t i = 0; i < sortedNotes.size(); ++i)
        {
            int lo, hi;
            
            if (i == 0)
                lo = 0;
            else
                lo = (sortedNotes[i - 1] + sortedNotes[i]) / 2 + 1;
            
            if (i == sortedNotes.size() - 1)
                hi = 127;
            else
                hi = (sortedNotes[i] + sortedNotes[i + 1]) / 2;
            
            keyRanges[sortedNotes[i]] = { lo, hi };
        }
    }
    else
    {
        // No fill-gap: each note maps to itself only
        for (int note : sortedNotes)
            keyRanges[note] = { note, note };
    }
    
    // Generate SFZ content
    juce::String sfz;
    sfz << "// Auto-generated by Colosseum Auto Sampler\n";
    sfz << "// Family: " << familyName << "\n";
    sfz << "// Notes: " << (int)sortedNotes.size() << "\n";
    sfz << "// Fill-Gap: " << (doFillGap ? "ON" : "OFF") << "\n\n";
    
    for (int note : sortedNotes)
    {
        auto& vels = noteVelMap[note];
        auto& range = keyRanges[note];
        
        if (vels.size() == 1)
        {
            // Single velocity layer
            juce::File wavFile = buildFilePath(note, vels[0]);
            sfz << "<region>\n";
            sfz << "sample=" << safeName << "/samples/" << wavFile.getFileName() << "\n";
            sfz << "lokey=" << range.lokey << " hikey=" << range.hikey 
                << " pitch_keycenter=" << note << "\n";
            sfz << "lovel=0 hivel=127\n\n";
        }
        else
        {
            // Multiple velocity layers — split evenly
            for (size_t v = 0; v < vels.size(); ++v)
            {
                int lovel, hivel;
                if (v == 0)
                    lovel = 0;
                else
                    lovel = (vels[v - 1] + vels[v]) / 2 + 1;
                
                if (v == vels.size() - 1)
                    hivel = 127;
                else
                    hivel = (vels[v] + vels[v + 1]) / 2;
                
                juce::File wavFile = buildFilePath(note, vels[v]);
                sfz << "<region>\n";
                sfz << "sample=" << safeName << "/samples/" << wavFile.getFileName() << "\n";
                sfz << "lokey=" << range.lokey << " hikey=" << range.hikey
                    << " pitch_keycenter=" << note << "\n";
                sfz << "lovel=" << lovel << " hivel=" << hivel << "\n\n";
            }
        }
    }
    
    sfzFile.replaceWithText(sfz);
}

// =============================================================================
// State Persistence
// =============================================================================
void AutoSamplerProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree vt("AutoSamplerState");
    vt.setProperty("familyName", familyName, nullptr);
    vt.setProperty("editorText", editorText, nullptr);
    vt.setProperty("folder", recordingFolder.getFullPathName(), nullptr);
    vt.setProperty("silenceThresholdDb", (double)silenceThresholdDb.load(), nullptr);
    vt.setProperty("silenceDurationMs", (double)silenceDurationMs.load(), nullptr);
    vt.setProperty("noteHoldMs", (double)noteHoldMs.load(), nullptr);
    vt.setProperty("createSfz", createSfz.load(), nullptr);
    vt.setProperty("fillGap", fillGap.load(), nullptr);
    
    juce::MemoryOutputStream stream(destData, false);
    vt.writeToStream(stream);
}

void AutoSamplerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto vt = juce::ValueTree::readFromData(data, sizeInBytes);
    if (vt.isValid()) {
        familyName = vt.getProperty("familyName", "Sample").toString();
        editorText = vt.getProperty("editorText", "").toString();
        
        juce::String folderPath = vt.getProperty("folder", "").toString();
        if (folderPath.isNotEmpty())
            recordingFolder = juce::File(folderPath);
        
        silenceThresholdDb.store((float)(double)vt.getProperty("silenceThresholdDb", -60.0));
        silenceDurationMs.store((float)(double)vt.getProperty("silenceDurationMs", 500.0));
        noteHoldMs.store((float)(double)vt.getProperty("noteHoldMs", 2000.0));
        createSfz.store((bool)vt.getProperty("createSfz", false));
        fillGap.store((bool)vt.getProperty("fillGap", false));
    }
}


