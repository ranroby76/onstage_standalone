
// ==============================================================================
//  GraphSerializer.cpp
//  OnStage — JSON serialization of the audio graph
// ==============================================================================

#include "GraphSerializer.h"
#include "EffectNodes.h"

// ==============================================================================
//  Save
// ==============================================================================

juce::var GraphSerializer::saveGraph (const OnStageGraph& graph)
{
    auto* obj = new juce::DynamicObject();
    auto& g = graph.getGraph();

    // --- Save permanent node positions ---------------------------------------
    {
        auto* ioObj = new juce::DynamicObject();

        auto saveNodePos = [&] (const char* key,
                                 juce::AudioProcessorGraph::Node::Ptr node)
        {
            if (! node) return;
            auto* n = new juce::DynamicObject();
            n->setProperty ("x", node->properties.getWithDefault ("x", 0.0));
            n->setProperty ("y", node->properties.getWithDefault ("y", 0.0));
            n->setProperty ("bypassed", node->isBypassed());
            ioObj->setProperty (key, juce::var (n));
        };

        saveNodePos ("audioInput",  graph.audioInputNode);
        saveNodePos ("audioOutput", graph.audioOutputNode);
        saveNodePos ("playback",    graph.playbackNode);

        obj->setProperty ("permanentNodes", juce::var (ioObj));
    }

    // --- Save user effect nodes ----------------------------------------------
    {
        juce::Array<juce::var> nodesArray;

        for (auto* node : g.getNodes())
        {
            // Skip permanent nodes
            if (node == graph.audioInputNode.get())  continue;
            if (node == graph.audioOutputNode.get()) continue;
            if (node == graph.playbackNode.get())    continue;

            auto* effectNode = dynamic_cast<EffectProcessorNode*> (node->getProcessor());
            if (! effectNode) continue;

            auto* nObj = new juce::DynamicObject();
            nObj->setProperty ("id",       (int) node->nodeID.uid);
            nObj->setProperty ("type",     effectNode->getEffectType());
            nObj->setProperty ("x",        node->properties.getWithDefault ("x", 0.0));
            nObj->setProperty ("y",        node->properties.getWithDefault ("y", 0.0));
            nObj->setProperty ("bypassed", node->isBypassed());
            nObj->setProperty ("sidechain", effectNode->isSidechainEnabled());

            // Per-effect parameters — stored as a sub-object
            // Each effect's getState() returns a MemoryBlock of parameter data
            juce::MemoryBlock stateData;
            effectNode->getStateInformation (stateData);
            if (stateData.getSize() > 0)
                nObj->setProperty ("state", stateData.toBase64Encoding());

            nodesArray.add (juce::var (nObj));
        }

        obj->setProperty ("nodes", nodesArray);
    }

    // --- Save connections ----------------------------------------------------
    {
        juce::Array<juce::var> connsArray;

        // We need to encode NodeIDs in a way that survives deserialization.
        // Permanent nodes get special string IDs; user nodes use their uid.
        auto nodeIdToVar = [&] (juce::AudioProcessorGraph::NodeID id) -> juce::var
        {
            if (graph.audioInputNode  && id == graph.audioInputNode->nodeID)
                return "audioInput";
            if (graph.audioOutputNode && id == graph.audioOutputNode->nodeID)
                return "audioOutput";
            if (graph.playbackNode    && id == graph.playbackNode->nodeID)
                return "playback";
            return (int) id.uid;
        };

        for (auto& conn : g.getConnections())
        {
            auto* c = new juce::DynamicObject();
            c->setProperty ("srcNode",    nodeIdToVar (conn.source.nodeID));
            c->setProperty ("srcChannel", conn.source.channelIndex);
            c->setProperty ("dstNode",    nodeIdToVar (conn.destination.nodeID));
            c->setProperty ("dstChannel", conn.destination.channelIndex);
            connsArray.add (juce::var (c));
        }

        obj->setProperty ("connections", connsArray);
    }

    // --- Save editor window sizes --------------------------------------------
    {
        auto* wsObj = new juce::DynamicObject();
        for (const auto& [type, size] : graph.editorWindowSizes)
        {
            auto* s = new juce::DynamicObject();
            s->setProperty ("w", size.x);
            s->setProperty ("h", size.y);
            wsObj->setProperty (type, juce::var (s));
        }
        obj->setProperty ("windowSizes", juce::var (wsObj));
    }

    return juce::var (obj);
}

// ==============================================================================
//  Load
// ==============================================================================

bool GraphSerializer::loadGraph (OnStageGraph& graph, const juce::var& data)
{
    if (! data.isObject()) return false;

    auto& g = graph.getGraph();

    // --- Remove all user nodes (keep permanent) ------------------------------
    {
        juce::Array<juce::AudioProcessorGraph::NodeID> toRemove;
        for (auto* node : g.getNodes())
        {
            if (node == graph.audioInputNode.get())  continue;
            if (node == graph.audioOutputNode.get()) continue;
            if (node == graph.playbackNode.get())    continue;
            toRemove.add (node->nodeID);
        }
        for (auto id : toRemove)
        {
            graph.disconnectNode (id);
            g.removeNode (id);
        }
    }

    // --- Restore permanent node positions ------------------------------------
    auto permNodes = data.getProperty ("permanentNodes", {});
    if (permNodes.isObject())
    {
        auto restorePos = [] (juce::AudioProcessorGraph::Node::Ptr node,
                              const juce::var& v)
        {
            if (! node || ! v.isObject()) return;
            node->properties.set ("x", v.getProperty ("x", 0.0));
            node->properties.set ("y", v.getProperty ("y", 0.0));
            node->setBypassed ((bool) v.getProperty ("bypassed", false));
        };

        restorePos (graph.audioInputNode,  permNodes.getProperty ("audioInput", {}));
        restorePos (graph.audioOutputNode, permNodes.getProperty ("audioOutput", {}));
        restorePos (graph.playbackNode,    permNodes.getProperty ("playback", {}));
    }

    // --- Recreate user effect nodes ------------------------------------------
    //  We need to map old IDs → new IDs for connection restoration.
    std::map<int, juce::AudioProcessorGraph::NodeID> idMap;

    auto nodesArr = data.getProperty ("nodes", {});
    if (auto* arr = nodesArr.getArray())
    {
        for (auto& nodeVar : *arr)
        {
            int oldId        = nodeVar.getProperty ("id", 0);
            juce::String type = nodeVar.getProperty ("type", "").toString();
            float x          = (float) (double) nodeVar.getProperty ("x", 300.0);
            float y          = (float) (double) nodeVar.getProperty ("y", 300.0);
            bool bypassed    = (bool) nodeVar.getProperty ("bypassed", false);
            bool scEnabled   = (bool) nodeVar.getProperty ("sidechain", false);

            auto newID = graph.addEffect (type, x, y);
            if (newID.uid == 0) continue;

            idMap[oldId] = newID;

            auto* node = g.getNodeForId (newID);
            if (node) node->setBypassed (bypassed);

            // Restore sidechain state
            auto* effectNode = dynamic_cast<EffectProcessorNode*> (
                node ? node->getProcessor() : nullptr);
            if (effectNode && scEnabled)
                effectNode->enableSidechain();

            // Restore per-effect state
            juce::String stateStr = nodeVar.getProperty ("state", "").toString();
            if (stateStr.isNotEmpty() && effectNode)
            {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (stateStr))
                    effectNode->setStateInformation (mb.getData(), (int) mb.getSize());
            }
        }
    }

    // --- Restore connections --------------------------------------------------
    auto connsArr = data.getProperty ("connections", {});
    if (auto* arr = connsArr.getArray())
    {
        auto resolveNodeID = [&] (const juce::var& v) -> juce::AudioProcessorGraph::NodeID
        {
            if (v.isString())
            {
                juce::String s = v.toString();
                if (s == "audioInput"  && graph.audioInputNode)  return graph.audioInputNode->nodeID;
                if (s == "audioOutput" && graph.audioOutputNode) return graph.audioOutputNode->nodeID;
                if (s == "playback"    && graph.playbackNode)    return graph.playbackNode->nodeID;
                return {};
            }
            int oldId = (int) v;
            auto it = idMap.find (oldId);
            return (it != idMap.end()) ? it->second : juce::AudioProcessorGraph::NodeID {};
        };

        for (auto& connVar : *arr)
        {
            auto srcID = resolveNodeID (connVar.getProperty ("srcNode", {}));
            auto dstID = resolveNodeID (connVar.getProperty ("dstNode", {}));
            int  srcCh = connVar.getProperty ("srcChannel", 0);
            int  dstCh = connVar.getProperty ("dstChannel", 0);

            if (srcID.uid != 0 && dstID.uid != 0)
            {
                g.addConnection ({
                    { srcID, srcCh },
                    { dstID, dstCh }
                });
            }
        }
    }

    // --- Restore editor window sizes -----------------------------------------
    auto winSizes = data.getProperty ("windowSizes", {});
    if (winSizes.isObject())
    {
        if (auto* dynObj = winSizes.getDynamicObject())
        {
            graph.editorWindowSizes.clear();
            for (const auto& prop : dynObj->getProperties())
            {
                auto val = prop.value;
                if (val.isObject())
                {
                    int w = (int) val.getProperty ("w", 0);
                    int h = (int) val.getProperty ("h", 0);
                    if (w > 0 && h > 0)
                        graph.editorWindowSizes[prop.name.toString()] = { w, h };
                }
            }
        }
    }

    return true;
}

// ==============================================================================
//  File helpers
// ==============================================================================

bool GraphSerializer::saveToFile (const OnStageGraph& graph, const juce::File& file)
{
    auto json = juce::JSON::toString (saveGraph (graph));
    return file.replaceWithText (json);
}

bool GraphSerializer::loadFromFile (OnStageGraph& graph, const juce::File& file)
{
    auto json = file.loadFileAsString();
    if (json.isEmpty()) return false;

    auto parsed = juce::JSON::parse (json);
    if (parsed.isVoid()) return false;

    return loadGraph (graph, parsed);
}
