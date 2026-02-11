// ==============================================================================
//  GraphSerializer.h
//  OnStage — Save / load the entire wiring graph to/from JSON
//
//  Stores:
//    • Which effect nodes exist and their type strings
//    • Node positions (x, y) on the canvas
//    • Bypass state per node
//    • All connections (source nodeID:channel → dest nodeID:channel)
//    • Per-effect DSP parameters (delegated to each panel's getState/setState)
//
//  Permanent nodes (Audio Input, Audio Output, Playback) are saved by
//  special tags so they survive serialization round-trips.
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include "../graph/OnStageGraph.h"

class GraphSerializer
{
public:
    // --- Save graph state to a JSON-compatible ValueTree ---------------------
    static juce::var saveGraph (const OnStageGraph& graph);

    // --- Restore graph state from JSON ---------------------------------------
    //  Clears current user nodes (keeps permanent I/O) and rebuilds from data.
    //  Returns true on success.
    static bool loadGraph (OnStageGraph& graph, const juce::var& data);

    // --- File helpers --------------------------------------------------------
    static bool saveToFile (const OnStageGraph& graph, const juce::File& file);
    static bool loadFromFile (OnStageGraph& graph, const juce::File& file);
};
