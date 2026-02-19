
// ==============================================================================
//  WiringCanvas_Connections.cpp
//  OnStage — Connection logic, sidechain auto-enable/disable
// ==============================================================================

#include "WiringCanvas.h"

// ==============================================================================
//  Can two pins connect?   (audio-only, no self-loops, input↔output only)
// ==============================================================================

bool WiringCanvas::canConnect (PinID a, PinID b)
{
    if (a.nodeID == b.nodeID)   return false;   // no self-loops
    if (a.isInput == b.isInput) return false;   // must be input↔output
    return true;
}

// ==============================================================================
//  Create a connection (with sidechain auto-enable)
// ==============================================================================

void WiringCanvas::createConnection (PinID a, PinID b)
{
    if (! canConnect (a, b)) return;

    PinID source = a.isInput ? b : a;
    PinID dest   = a.isInput ? a : b;

    auto& g = stageGraph.getGraph();

    // --- Sidechain auto-enable -----------------------------------------------
    auto* destNode = g.getNodeForId (dest.nodeID);
    if (destNode)
    {
        auto* cache = getCached (dest.nodeID);
        EffectProcessorNode* effectNode = cache ? cache->effectNode
                                                : dynamic_cast<EffectProcessorNode*> (destNode->getProcessor());

        if (effectNode && effectNode->hasSidechain())
        {
            auto mapping = effectNode->mapInputChannel (dest.pinIndex);
            if (mapping.isSidechain)
            {
                // FIX: Force-enable the sidechain bus on the AudioProcessor level
                // before attempting the connection.  This ensures
                // getTotalNumInputChannels() returns 4 so that
                // AudioProcessorGraph::addConnection() won't reject the wire.
                if (auto* scBus = effectNode->getBus (true, 1))
                    scBus->enable (true);

                effectNode->enableSidechain();
            }
        }
    }

    // --- Create the audio connection -----------------------------------------
    g.addConnection ({
        { source.nodeID, source.pinIndex },
        { dest.nodeID,   dest.pinIndex }
    });

    markDirty();
}

// ==============================================================================
//  Get the closest connection at a screen position (for wire hover / delete)
// ==============================================================================

juce::AudioProcessorGraph::Connection WiringCanvas::getConnectionAt (juce::Point<float> pos)
{
    constexpr float hitTolerance = 3.0f;
    auto& g = stageGraph.getGraph();

    juce::AudioProcessorGraph::Connection closest {
        { juce::AudioProcessorGraph::NodeID(), 0 },
        { juce::AudioProcessorGraph::NodeID(), 0 }
    };
    float closestDist = std::numeric_limits<float>::max();

    for (const auto& conn : g.getConnections())
    {
        auto* src = g.getNodeForId (conn.source.nodeID);
        auto* dst = g.getNodeForId (conn.destination.nodeID);
        if (! src || ! dst || ! shouldShowNode (src) || ! shouldShowNode (dst))
            continue;

        PinID srcPin { src->nodeID, conn.source.channelIndex,      false };
        PinID dstPin { dst->nodeID, conn.destination.channelIndex, true  };

        auto start = getPinPos (src, srcPin);
        auto end   = getPinPos (dst, dstPin);

        // Build a bezier path matching drawWire()
        juce::Path p;
        p.startNewSubPath (start);
        p.cubicTo (start.x, start.y + 50, end.x, end.y - 50, end.x, end.y);

        // Stroke for hit-test
        juce::Path stroked;
        juce::PathStrokeType stroke (hitTolerance * 2.0f);
        stroke.createStrokedPath (stroked, p);

        if (stroked.contains (pos))
        {
            // Sample 20 points to find closest
            float minDist = std::numeric_limits<float>::max();
            for (int i = 0; i <= 20; ++i)
            {
                float t  = (float) i / 20.0f;
                float u  = 1.0f - t;
                float u2 = u * u, u3 = u2 * u;
                float t2 = t * t, t3 = t2 * t;

                juce::Point<float> cp1 (start.x, start.y + 50);
                juce::Point<float> cp2 (end.x,   end.y   - 50);

                float px = u3 * start.x + 3*u2*t * cp1.x + 3*u*t2 * cp2.x + t3 * end.x;
                float py = u3 * start.y + 3*u2*t * cp1.y + 3*u*t2 * cp2.y + t3 * end.y;

                float d = pos.getDistanceFrom ({ px, py });
                if (d < minDist) minDist = d;
            }

            if (minDist < closestDist)
            {
                closestDist = minDist;
                closest = conn;
            }
        }
    }

    return closest;
}

// ==============================================================================
//  Delete connection at position (with sidechain auto-disable)
// ==============================================================================

void WiringCanvas::deleteConnectionAt (juce::Point<float> pos)
{
    auto conn = getConnectionAt (pos);
    if (conn.source.nodeID.uid == 0) return;

    auto& g = stageGraph.getGraph();

    // --- Sidechain auto-disable ----------------------------------------------
    auto* destNode = g.getNodeForId (conn.destination.nodeID);
    if (destNode)
    {
        auto* cache = getCached (conn.destination.nodeID);
        EffectProcessorNode* effectNode = cache ? cache->effectNode
                                                : dynamic_cast<EffectProcessorNode*> (destNode->getProcessor());

        if (effectNode && effectNode->hasSidechain())
        {
            auto mapping = effectNode->mapInputChannel (conn.destination.channelIndex);
            if (mapping.isSidechain)
            {
                // Remove the wire first
                g.removeConnection (conn);

                // Check if any sidechain connections remain
                bool anySC = false;
                for (auto& other : g.getConnections())
                {
                    if (other.destination.nodeID == destNode->nodeID &&
                        other.destination.channelIndex >= 2)
                    {
                        anySC = true;
                        break;
                    }
                }
                if (! anySC)
                    effectNode->disableSidechain();

                markDirty();
                return;
            }
        }
    }

    g.removeConnection (conn);
    markDirty();
}
