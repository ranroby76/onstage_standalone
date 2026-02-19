
// ==============================================================================
//  OnStageGraph.cpp
//  OnStage — AudioProcessorGraph owner / manager
//
//  Key design decisions:
//    • I/O nodes are rebuilt when hardware changes; user nodes survive.
//    • Connections to/from I/O nodes are saved by channel index and restored.
//    • Zombie buffer defense: after any device restart, several blocks of
//      silence are forced through the graph to flush stale samples trapped
//      in delay lines, reverb tails, and internal JUCE graph buffers.
// ==============================================================================

#include "OnStageGraph.h"
#include "../AppLogger.h"

// Number of silent blocks pushed through the graph after a restart
static constexpr int kZombieFlushBlocks = 4;

// ==============================================================================
//  Construction / Destruction
// ==============================================================================

OnStageGraph::OnStageGraph()
{
    graph = std::make_unique<juce::AudioProcessorGraph>();
    graphCreated = true;
}

OnStageGraph::~OnStageGraph()
{
    releaseResources();
}

// ==============================================================================
//  prepare  —  (re)build I/O nodes, preserve user topology
// ==============================================================================

void OnStageGraph::prepare (double sampleRate, int blockSize,
                             int numHardwareInputs, int numHardwareOutputs,
                             MediaPlayerType& mediaPlayer)
{
    LOG_INFO ("OnStageGraph::prepare  SR=" + juce::String (sampleRate)
              + "  BS=" + juce::String (blockSize)
              + "  ins=" + juce::String (numHardwareInputs)
              + "  outs=" + juce::String (numHardwareOutputs));

    // --- 1. Configure graph bus layout to match hardware ---------------------
    graph->setPlayConfigDetails (numHardwareInputs, numHardwareOutputs,
                                 sampleRate, blockSize);

    // --- 2. Rebuild I/O nodes (saves + restores connections) -----------------
    rebuildIONodes (numHardwareInputs, numHardwareOutputs, mediaPlayer);

    // --- 3. Prepare the whole graph (prepares all nodes including effects) ---
    graph->prepareToPlay (sampleRate, blockSize);

    // --- 4. Arm zombie flush -------------------------------------------------
    zombieFlushCountdown = kZombieFlushBlocks;

    prepared = true;

    LOG_INFO ("OnStageGraph::prepare complete — zombie flush armed for "
              + juce::String (kZombieFlushBlocks) + " blocks");
}

// ==============================================================================
//  rebuildIONodes  —  tear down only I/O nodes, keep user effects
//
//  Default positions (grid-aligned, ~45px per grid square):
//    Audio Input:  grid (2, 2)  → (90, 90)   — top-left
//    Audio Output: grid (2, 12) → (90, 540)  — bottom-left
//    Playback:     grid (15, 2) → (675, 90)  — top-right
// ==============================================================================

void OnStageGraph::rebuildIONodes (int numInputs, int numOutputs,
                                    MediaPlayerType& mediaPlayer)
{
    // --- Save existing I/O connections before removing nodes ------------------
    auto savedWires = saveIOConnections();

    // --- Save positions of existing I/O nodes --------------------------------
    double inputX  = 90.0,   inputY  = 90.0;
    double outputX = 90.0,   outputY = 540.0;
    double playX   = 675.0,  playY   = 90.0;

    if (audioInputNode)
    {
        inputX = audioInputNode->properties.getWithDefault ("x", inputX);
        inputY = audioInputNode->properties.getWithDefault ("y", inputY);
    }
    if (audioOutputNode)
    {
        outputX = audioOutputNode->properties.getWithDefault ("x", outputX);
        outputY = audioOutputNode->properties.getWithDefault ("y", outputY);
    }
    if (playbackNode)
    {
        playX = playbackNode->properties.getWithDefault ("x", playX);
        playY = playbackNode->properties.getWithDefault ("y", playY);
    }

    // --- Remove old I/O nodes ------------------------------------------------
    if (audioInputNode)
    {
        // Disconnect all wires first to avoid dangling refs
        auto connections = graph->getConnections();
        for (auto& c : connections)
            if (c.source.nodeID == audioInputNode->nodeID ||
                c.destination.nodeID == audioInputNode->nodeID)
                graph->removeConnection (c);

        graph->removeNode (audioInputNode->nodeID);
        audioInputNode = nullptr;
    }

    if (audioOutputNode)
    {
        auto connections = graph->getConnections();
        for (auto& c : connections)
            if (c.source.nodeID == audioOutputNode->nodeID ||
                c.destination.nodeID == audioOutputNode->nodeID)
                graph->removeConnection (c);

        graph->removeNode (audioOutputNode->nodeID);
        audioOutputNode = nullptr;
    }

    if (playbackNode)
    {
        auto connections = graph->getConnections();
        for (auto& c : connections)
            if (c.source.nodeID == playbackNode->nodeID ||
                c.destination.nodeID == playbackNode->nodeID)
                graph->removeConnection (c);

        graph->removeNode (playbackNode->nodeID);
        playbackNode = nullptr;
    }

    // --- Create new I/O nodes with correct channel counts --------------------

    // Audio Input (hardware → graph)
    auto* inputProc = new juce::AudioProcessorGraph::AudioGraphIOProcessor (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
    audioInputNode = graph->addNode (std::unique_ptr<juce::AudioProcessor> (inputProc));
    audioInputNode->properties.set ("x", inputX);
    audioInputNode->properties.set ("y", inputY);

    // Audio Output (graph → hardware)
    auto* outputProc = new juce::AudioProcessorGraph::AudioGraphIOProcessor (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
    audioOutputNode = graph->addNode (std::unique_ptr<juce::AudioProcessor> (outputProc));
    audioOutputNode->properties.set ("x", outputX);
    audioOutputNode->properties.set ("y", outputY);

    // Playback (media player source — always stereo out)
    auto playbackProc = std::make_unique<PlaybackNode> (mediaPlayer);
    playbackNode = graph->addNode (std::move (playbackProc));
    playbackNode->properties.set ("x", playX);
    playbackNode->properties.set ("y", playY);

    // --- Restore I/O connections (best-effort — channel may not exist) --------
    restoreIOConnections (savedWires);

    LOG_INFO ("OnStageGraph::rebuildIONodes — rebuilt I/O ("
              + juce::String (numInputs) + " in, "
              + juce::String (numOutputs) + " out), restored "
              + juce::String ((int) savedWires.size()) + " wires");
}

// ==============================================================================
//  saveIOConnections  —  capture all wires touching I/O or playback nodes
// ==============================================================================

std::vector<OnStageGraph::SavedIOConnection> OnStageGraph::saveIOConnections() const
{
    std::vector<SavedIOConnection> saved;

    if (! graphCreated) return saved;

    auto connections = graph->getConnections();

    for (auto& c : connections)
    {
        // --- Connections FROM input node (source = audioInputNode) ------------
        if (audioInputNode && c.source.nodeID == audioInputNode->nodeID)
        {
            saved.push_back ({
                true,                           // fromIONode
                c.destination.nodeID,           // otherNode
                c.source.channelIndex,          // ioChannel
                c.destination.channelIndex,     // otherChannel
                true                            // isInputNode
            });
        }
        // --- Connections TO output node (dest = audioOutputNode) --------------
        else if (audioOutputNode && c.destination.nodeID == audioOutputNode->nodeID)
        {
            saved.push_back ({
                false,                          // fromIONode (dest is IO)
                c.source.nodeID,                // otherNode
                c.destination.channelIndex,     // ioChannel
                c.source.channelIndex,          // otherChannel
                false                           // isInputNode
            });
        }
        // --- Connections FROM playback node -----------------------------------
        else if (playbackNode && c.source.nodeID == playbackNode->nodeID)
        {
            // We don't save playback wires specially — they reconnect automatically
        }
    }

    return saved;
}

// ==============================================================================
//  restoreIOConnections  —  reconnect saved wires to new I/O nodes
// ==============================================================================

void OnStageGraph::restoreIOConnections (const std::vector<SavedIOConnection>& saved)
{
    for (const auto& s : saved)
    {
        // Skip if the "other" node no longer exists
        if (! graph->getNodeForId (s.otherNode)) continue;

        juce::AudioProcessorGraph::Connection c;

        if (s.isInputNode && audioInputNode)
        {
            // Validate channel index against new node
            if (s.ioChannel >= audioInputNode->getProcessor()->getTotalNumOutputChannels())
                continue;

            c.source      = { audioInputNode->nodeID, s.ioChannel };
            c.destination = { s.otherNode, s.otherChannel };
        }
        else if (! s.isInputNode && audioOutputNode)
        {
            if (s.ioChannel >= audioOutputNode->getProcessor()->getTotalNumInputChannels())
                continue;

            c.source      = { s.otherNode, s.otherChannel };
            c.destination = { audioOutputNode->nodeID, s.ioChannel };
        }
        else
        {
            continue;
        }

        graph->addConnection (c);
    }
}

// ==============================================================================
//  suspend  —  device stopped, preserve state
// ==============================================================================

void OnStageGraph::suspend()
{
    if (prepared)
    {
        LOG_INFO ("OnStageGraph::suspend — device offline");
        prepared = false;
    }
}

// ==============================================================================
//  releaseResources  —  full teardown (app shutdown)
// ==============================================================================

void OnStageGraph::releaseResources()
{
    if (! graphCreated) return;

    LOG_INFO ("OnStageGraph::releaseResources — full teardown");

    graph->releaseResources();
    graph->clear();

    audioInputNode  = nullptr;
    audioOutputNode = nullptr;
    playbackNode    = nullptr;

    prepared     = false;
    graphCreated = false;
}

// ==============================================================================
//  flushBuffers  —  push silence through entire graph (zombie defense)
// ==============================================================================

void OnStageGraph::flushBuffers()
{
    if (! prepared) return;

    int numCh   = graph->getMainBusNumOutputChannels();
    int blockSz = graph->getBlockSize();

    if (numCh <= 0 || blockSz <= 0) return;

    juce::AudioBuffer<float> silence (numCh, blockSz);
    silence.clear();

    juce::MidiBuffer emptyMidi;

    // Push several blocks of silence
    for (int i = 0; i < kZombieFlushBlocks; ++i)
        graph->processBlock (silence, emptyMidi);

    LOG_INFO ("OnStageGraph::flushBuffers — flushed "
              + juce::String (kZombieFlushBlocks) + " silent blocks");
}

// ==============================================================================
//  processBlock
// ==============================================================================

void OnStageGraph::processBlock (juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midi)
{
    if (! prepared) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // --- Zombie flush: replace input with silence for the first N blocks -----
    if (zombieFlushCountdown > 0)
    {
        buffer.clear();
        --zombieFlushCountdown;
    }

    // --- Update INPUT metering (BEFORE graph processing) ---------------------
    for (int ch = 0; ch < numChannels && ch < 32; ++ch)
    {
        float peak = buffer.getMagnitude (ch, 0, numSamples);
        inputRms[ch].store (peak, std::memory_order_relaxed);
    }

    // --- Apply input gain smoothing (for bypass fade) ------------------------
    float inputGainTarget = (audioInputNode && audioInputNode->isBypassed()) ? 0.0f : 1.0f;

    if (std::abs (inputGainCurrent - inputGainTarget) > 0.0001f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            inputGainCurrent += kGainSmoothingCoeff * (inputGainTarget - inputGainCurrent);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.getWritePointer (ch)[i] *= inputGainCurrent;
            }
        }
    }
    else if (inputGainTarget == 0.0f)
    {
        buffer.clear();
        inputGainCurrent = 0.0f;
    }

    // --- Run the graph -------------------------------------------------------
    graph->processBlock (buffer, midi);

    // --- Apply output gain smoothing (for bypass fade) -----------------------
    float outputGainTarget = (audioOutputNode && audioOutputNode->isBypassed()) ? 0.0f : 1.0f;

    if (std::abs (outputGainCurrent - outputGainTarget) > 0.0001f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            outputGainCurrent += kGainSmoothingCoeff * (outputGainTarget - outputGainCurrent);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.getWritePointer (ch)[i] *= outputGainCurrent;
            }
        }
    }
    else if (outputGainTarget == 0.0f)
    {
        buffer.clear();
        outputGainCurrent = 0.0f;
    }

    // --- Update OUTPUT metering (AFTER graph processing) ---------------------
    for (int ch = 0; ch < numChannels && ch < 32; ++ch)
    {
        float peak = buffer.getMagnitude (ch, 0, numSamples);
        outputRms[ch].store (peak, std::memory_order_relaxed);
    }
}

// ==============================================================================
//  Node management
// ==============================================================================

OnStageGraph::NodeID OnStageGraph::addEffect (const juce::String& effectType,
                                               float posX, float posY)
{
    auto node = createEffectNode (effectType);
    if (! node)
    {
        LOG_ERROR ("OnStageGraph::addEffect — unknown type: " + effectType);
        return {};
    }

    // Prepare the new processor to match the graph's current config
    if (prepared)
    {
        node->setPlayConfigDetails (
            node->getTotalNumInputChannels(),
            node->getTotalNumOutputChannels(),
            graph->getSampleRate(),
            graph->getBlockSize());
        node->prepareToPlay (graph->getSampleRate(), graph->getBlockSize());
    }

    auto added = graph->addNode (std::move (node));
    added->properties.set ("x", (double) posX);
    added->properties.set ("y", (double) posY);

    LOG_INFO ("OnStageGraph::addEffect — added " + effectType
              + " as node " + juce::String (added->nodeID.uid));

    return added->nodeID;
}

void OnStageGraph::removeNode (NodeID id)
{
    // Guard: never remove permanent nodes
    if (audioInputNode  && id == audioInputNode->nodeID)  return;
    if (audioOutputNode && id == audioOutputNode->nodeID) return;
    if (playbackNode    && id == playbackNode->nodeID)    return;

    disconnectNode (id);
    graph->removeNode (id);
}

void OnStageGraph::disconnectNode (NodeID id)
{
    auto connections = graph->getConnections();
    for (auto& c : connections)
    {
        if (c.source.nodeID == id || c.destination.nodeID == id)
            graph->removeConnection (c);
    }
}

bool OnStageGraph::addConnection (const juce::AudioProcessorGraph::Connection& c)
{
    return graph->addConnection (c);
}

bool OnStageGraph::removeConnection (const juce::AudioProcessorGraph::Connection& c)
{
    return graph->removeConnection (c);
}
