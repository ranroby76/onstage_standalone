// ==============================================================================
//  OnStageGraph.h
//  OnStage — Owns the juce::AudioProcessorGraph and manages I/O + effect nodes
//
//  The graph replaces the old hardcoded mic-chain architecture.  Every audio
//  route is a visible wire on the WiringCanvas.
//
//  Permanent nodes:
//    • Audio Input   — hardware mic/line inputs
//    • Audio Output  — hardware speaker/monitor outputs
//    • Playback      — media player (karaoke backing track)
//
//  User nodes:
//    • Any EffectProcessorNode added via the right-click menu
//
//  I/O node lifecycle:
//    When the ASIO device changes (driver switch, SR/buffer change), only the
//    I/O nodes are torn down and rebuilt with the new channel counts.  User
//    effect nodes and their inter-effect connections are preserved.
//    Connections *to/from* I/O nodes are saved and restored by channel index.
//
//  Zombie buffer defense:
//    After any device restart the graph's internal buffers are flushed with
//    silence to prevent stale audio leaking through.
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "EffectNodes.h"
#include "PlaybackNode.h"

class OnStageGraph
{
public:
    OnStageGraph();
    ~OnStageGraph();

    // --- Lifecycle -----------------------------------------------------------

    //  Call once after the ASIO device is opened so we know channel counts.
    //  Safe to call repeatedly — rebuilds only the I/O nodes, keeping
    //  user-added effects and their inter-effect wires intact.
    void prepare (double sampleRate, int blockSize,
                  int numHardwareInputs, int numHardwareOutputs,
                  MediaPlayerType& mediaPlayer);

    //  Full teardown — destroys everything (called on app shutdown).
    void releaseResources();

    //  Device stopped — marks offline but preserves topology.
    void suspend();

    //  Flush all graph buffers with silence (zombie defense).
    void flushBuffers();

    // --- Process (called from AudioEngine callback) --------------------------
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    // --- Node management -----------------------------------------------------
    using NodeID = juce::AudioProcessorGraph::NodeID;
    using NodePtr = juce::AudioProcessorGraph::Node::Ptr;

    //  Add an effect by type string.  Returns the new NodeID.
    NodeID addEffect (const juce::String& effectType,
                      float posX = 300.0f, float posY = 300.0f);

    //  Remove a user node (cannot remove permanent I/O nodes).
    void removeNode (NodeID id);

    //  Disconnect all wires from a node.
    void disconnectNode (NodeID id);

    //  Wire / unwire
    bool addConnection    (const juce::AudioProcessorGraph::Connection& c);
    bool removeConnection (const juce::AudioProcessorGraph::Connection& c);

    // --- Accessors -----------------------------------------------------------
    juce::AudioProcessorGraph&       getGraph()       { return *graph; }
    const juce::AudioProcessorGraph& getGraph() const { return *graph; }

    bool isPrepared() const { return prepared; }

    // Permanent nodes (exposed for canvas rendering)
    NodePtr audioInputNode;
    NodePtr audioOutputNode;
    NodePtr playbackNode;

    // Metering helpers (atomic, safe to read from UI)
    std::atomic<float> inputRms[32]  {};
    std::atomic<float> outputRms[32] {};

    // Editor window sizes (persisted in project patch)
    // Key = effect type string, Value = {width, height}
    std::map<juce::String, juce::Point<int>> editorWindowSizes;

    // Hardware channel names (populated during prepare from AudioIODevice)
    juce::StringArray inputChannelNames;
    juce::StringArray outputChannelNames;

private:
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    bool prepared       = false;
    bool graphCreated   = false;   // true once the graph object exists & is usable
    int  zombieFlushCountdown = 0; // blocks remaining to flush

    // Gain smoothing for I/O bypass (prevents clicks when toggling ON/OFF)
    float inputGainCurrent  = 1.0f;
    float outputGainCurrent = 1.0f;
    static constexpr float kGainSmoothingCoeff = 0.05f;  // ~20 samples to fade

    // Saved I/O wire list (for reconnection after I/O node rebuild)
    struct SavedIOConnection
    {
        bool fromIONode;           // true = source is an I/O node, false = dest is
        NodeID otherNode;
        int    ioChannel;          // channel index on the I/O node
        int    otherChannel;       // channel index on the other node
        bool   isInputNode;       // true = audioInputNode, false = audioOutputNode
    };

    std::vector<SavedIOConnection> saveIOConnections() const;
    void restoreIOConnections (const std::vector<SavedIOConnection>& saved);

    // Rebuild only the I/O processor nodes (preserving user effects)
    void rebuildIONodes (int numInputs, int numOutputs, MediaPlayerType& mediaPlayer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnStageGraph)
};
