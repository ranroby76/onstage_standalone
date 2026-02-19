// ==============================================================================
//  WorkspaceManager.h
//  OnStage — 16-slot workspace save/switch system
//
//  Each workspace stores a complete graph state (nodes + connections).
//  Switching workspaces saves the current graph, clears it, then restores
//  the target workspace's saved state.
//
//  Uses GraphSerializer for save/restore (JSON-based, same as presets).
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include "../graph/OnStageGraph.h"
#include "../graph/GraphSerializer.h"
#include "../PresetManager.h"

class WorkspaceManager
{
public:
    static constexpr int maxWorkspaces = 16;

    WorkspaceManager (OnStageGraph& graph, PresetManager& presets)
        : stageGraph (graph), presetManager (presets)
    {
        for (int i = 0; i < maxWorkspaces; ++i)
        {
            names[i]    = juce::String (i + 1);
            enabled[i]  = (i == 0);
            occupied[i] = false;
        }
    }

    // --- Queries -------------------------------------------------------------
    int  getActiveWorkspace() const           { return activeWorkspace; }
    bool isEnabled  (int i) const             { return inRange (i) && enabled[i]; }
    bool isOccupied (int i) const             { return inRange (i) && occupied[i]; }
    juce::String getName (int i) const        { return inRange (i) ? names[i] : juce::String(); }

    void setName    (int i, const juce::String& n) { if (inRange (i)) names[i] = n; }
    void setEnabled (int i, bool e)                { if (inRange (i)) enabled[i] = e; }

    // --- Switch --------------------------------------------------------------
    void switchWorkspace (int target)
    {
        if (! inRange (target) || target == activeWorkspace || ! enabled[target])
            return;

        // Save current
        data[activeWorkspace] = GraphSerializer::saveGraph (stageGraph);
        occupied[activeWorkspace] = true;

        // Clear live graph (remove all user effect nodes, keep I/O + media)
        clearUserNodes();

        // Restore target
        if (occupied[target])
            GraphSerializer::loadGraph (stageGraph, data[target]);

        activeWorkspace = target;
    }

    // --- Clear one workspace -------------------------------------------------
    void clearWorkspace (int index)
    {
        if (! inRange (index)) return;

        if (index == activeWorkspace)
            clearUserNodes();

        data[index]     = juce::var();
        occupied[index] = false;
    }

    // --- Duplicate -----------------------------------------------------------
    void duplicateWorkspace (int src, int dst)
    {
        if (! inRange (src) || ! inRange (dst) || src == dst) return;

        juce::var srcData;
        if (src == activeWorkspace)
            srcData = GraphSerializer::saveGraph (stageGraph);
        else
            srcData = data[src];

        if (dst == activeWorkspace)
        {
            clearUserNodes();
            if (! srcData.isVoid())
                GraphSerializer::loadGraph (stageGraph, srcData);
        }

        data[dst]     = srcData;
        occupied[dst] = ! srcData.isVoid();
        enabled[dst]  = true;
    }

    // --- Reset all -----------------------------------------------------------
    void resetAll()
    {
        clearUserNodes();

        for (int i = 0; i < maxWorkspaces; ++i)
        {
            data[i]     = juce::var();
            occupied[i] = false;
            enabled[i]  = (i == 0);
            names[i]    = juce::String (i + 1);
        }
        activeWorkspace = 0;
    }

    // --- State persistence (save/load with settings) -------------------------
    juce::var getState() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("active", activeWorkspace);

        juce::Array<juce::var> slots;
        for (int i = 0; i < maxWorkspaces; ++i)
        {
            auto slot = std::make_unique<juce::DynamicObject>();
            slot->setProperty ("name",     names[i]);
            slot->setProperty ("enabled",  enabled[i]);
            slot->setProperty ("occupied", occupied[i]);
            if (i == activeWorkspace)
                slot->setProperty ("data", GraphSerializer::saveGraph (stageGraph));
            else if (occupied[i])
                slot->setProperty ("data", data[i]);
            slots.add (juce::var (slot.release()));
        }
        obj->setProperty ("slots", slots);
        return juce::var (obj.release());
    }

    void restoreState (const juce::var& state)
    {
        if (state.isVoid()) return;

        activeWorkspace = (int) state.getProperty ("active", 0);
        auto* slots = state.getProperty ("slots", {}).getArray();
        if (! slots) return;

        for (int i = 0; i < juce::jmin ((int) slots->size(), maxWorkspaces); ++i)
        {
            auto slot = (*slots)[i];
            names[i]    = slot.getProperty ("name", juce::String (i + 1)).toString();
            enabled[i]  = (bool) slot.getProperty ("enabled", i == 0);
            occupied[i] = (bool) slot.getProperty ("occupied", false);
            data[i]     = slot.getProperty ("data", {});
        }

        // Load active workspace into live graph
        if (occupied[activeWorkspace])
        {
            clearUserNodes();
            GraphSerializer::loadGraph (stageGraph, data[activeWorkspace]);
        }
    }

private:
    OnStageGraph&  stageGraph;
    PresetManager& presetManager;

    int          activeWorkspace = 0;
    juce::String names[maxWorkspaces];
    bool         enabled[maxWorkspaces]  {};
    bool         occupied[maxWorkspaces] {};
    juce::var    data[maxWorkspaces];

    static bool inRange (int i) { return i >= 0 && i < maxWorkspaces; }

    void clearUserNodes()
    {
        // Remove all nodes except permanent I/O + media nodes
        auto& g = stageGraph.getGraph();
        juce::Array<juce::AudioProcessorGraph::NodeID> toRemove;

        for (auto* node : g.getNodes())
        {
            if (node == stageGraph.audioInputNode.get())  continue;
            if (node == stageGraph.audioOutputNode.get()) continue;
            if (node == stageGraph.playbackNode.get())       continue;
            toRemove.add (node->nodeID);
        }

        for (auto id : toRemove)
            stageGraph.removeNode (id);
    }
};
