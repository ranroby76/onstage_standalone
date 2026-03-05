#include "GraphCanvas.h"

bool GraphCanvas::canConnect(PinID start, PinID end)
{
    if (start.nodeID == end.nodeID) return false;
    if (start.isInput == end.isInput) return false;
    if (start.isMidi != end.isMidi) return false;
    return true;
}

// FIXED: Removed faulty SimpleConnector tap logic
// JUCE AudioProcessorGraph natively supports fan-out (one output -> multiple inputs)
void GraphCanvas::createConnection(PinID start, PinID end)
{
    if (!canConnect(start, end)) return;

    auto* ag = getActiveGraph();
    if (!ag) return;

    PinID source = start.isInput ? end : start;
    PinID dest   = start.isInput ? start : end;

    // Check if connecting to a sidechain pin and auto-enable sidechain
    auto* destNode = ag->getNodeForId(dest.nodeID);

    if (destNode && !dest.isMidi)
    {
        auto* cache = getCachedNodeType(dest.nodeID);
        MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(destNode->getProcessor());

        if (meteringProc && meteringProc->hasSidechain())
        {
            auto mapping = meteringProc->mapInputChannel(dest.pinIndex);
            if (mapping.isSidechain)
            {
                // Auto-enable sidechain when connecting to sidechain pin
                meteringProc->enableSidechain();
            }
        }
    }

    // Create the connection in the active graph - JUCE handles fan-out natively
    if (source.isMidi)
    {
        ag->addConnection({
            { source.nodeID, juce::AudioProcessorGraph::midiChannelIndex },
            { dest.nodeID,   juce::AudioProcessorGraph::midiChannelIndex }
        });
    }
    else
    {
        ag->addConnection({
            { source.nodeID, source.pinIndex },
            { dest.nodeID,   dest.pinIndex }
        });
    }

    markDirty();
}

juce::AudioProcessorGraph::Connection GraphCanvas::getConnectionAt(juce::Point<float> pos)
{
    // FIX: Actual hit tolerance for distance from wire (in pixels)
    // This is the maximum distance from the actual wire line, not bounding box
    const float hitTolerance = 3.0f;

    auto* ag = getActiveGraph();
    if (!ag) return { { juce::AudioProcessorGraph::NodeID(), 0 }, { juce::AudioProcessorGraph::NodeID(), 0 } };

    auto connections = ag->getConnections();
    
    // FIX: Find the CLOSEST connection to the click point, not just the first match
    // This prevents wrong wire deletion when paths overlap (e.g., output wires vs sidechain input wires)
    juce::AudioProcessorGraph::Connection closestConnection = { 
        { juce::AudioProcessorGraph::NodeID(), 0 }, 
        { juce::AudioProcessorGraph::NodeID(), 0 } 
    };
    float closestDistance = std::numeric_limits<float>::max();
    
    for (const auto& connection : connections)
    {
        auto* src = ag->getNodeForId(connection.source.nodeID);
        auto* dst = ag->getNodeForId(connection.destination.nodeID);

        if (src && dst && shouldShowNode(src) && shouldShowNode(dst))
        {
            bool isMidi = connection.source.isMIDI();
            PinID p1 = { src->nodeID, isMidi ? 0 : connection.source.channelIndex, false, isMidi };
            PinID p2 = { dst->nodeID, isMidi ? 0 : connection.destination.channelIndex, true,  isMidi };

            auto start = getPinPos(src, p1);
            auto end   = getPinPos(dst, p2);

            juce::Path p;
            p.startNewSubPath(start);
            p.cubicTo(start.x, start.y + 50, end.x, end.y - 50, end.x, end.y);

            // FIX: Use stroked path for accurate hit detection
            // This checks actual distance to the curve, not just bounding box
            juce::Path strokedPath;
            juce::PathStrokeType stroke(hitTolerance * 2.0f);
            stroke.createStrokedPath(strokedPath, p);
            
            if (strokedPath.contains(pos))
            {
                // Calculate minimum distance to the actual curve by sampling points
                float minDistance = std::numeric_limits<float>::max();
                const int numSamples = 20;
                for (int i = 0; i <= numSamples; ++i)
                {
                    float t = (float)i / (float)numSamples;
                    // Cubic bezier formula
                    float oneMinusT = 1.0f - t;
                    float oneMinusT2 = oneMinusT * oneMinusT;
                    float oneMinusT3 = oneMinusT2 * oneMinusT;
                    float t2 = t * t;
                    float t3 = t2 * t;
                    
                    // Control points for the bezier
                    juce::Point<float> cp1(start.x, start.y + 50);
                    juce::Point<float> cp2(end.x, end.y - 50);
                    
                    // Calculate point on curve
                    float px = oneMinusT3 * start.x + 3.0f * oneMinusT2 * t * cp1.x + 
                               3.0f * oneMinusT * t2 * cp2.x + t3 * end.x;
                    float py = oneMinusT3 * start.y + 3.0f * oneMinusT2 * t * cp1.y + 
                               3.0f * oneMinusT * t2 * cp2.y + t3 * end.y;
                    
                    float dist = pos.getDistanceFrom(juce::Point<float>(px, py));
                    if (dist < minDistance)
                        minDistance = dist;
                }
                
                if (minDistance < closestDistance)
                {
                    closestDistance = minDistance;
                    closestConnection = connection;
                }
            }
        }
    }

    return closestConnection;
}

void GraphCanvas::deleteConnectionAt(juce::Point<float> pos)
{
    auto conn = getConnectionAt(pos);
    if (conn.source.nodeID.uid != 0)
    {
        auto* ag = getActiveGraph();
        if (!ag) return;

        // FIX 2: Before removing connection, check if it's a sidechain connection
        // If removing the last sidechain connection, disable sidechain processing
        auto* destNode = ag->getNodeForId(conn.destination.nodeID);
        
        if (destNode && !conn.destination.isMIDI())
        {
            auto* cache = getCachedNodeType(conn.destination.nodeID);
            MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(destNode->getProcessor());
            
            if (meteringProc && meteringProc->hasSidechain())
            {
                auto mapping = meteringProc->mapInputChannel(conn.destination.channelIndex);
                
                // If this is a sidechain connection being removed
                if (mapping.isSidechain)
                {
                    // Remove the connection first
                    ag->removeConnection(conn);
                    
                    // Check if there are any other sidechain connections remaining
                    bool hasOtherSidechainConnections = false;
                    for (auto& otherConn : ag->getConnections())
                    {
                        if (otherConn.destination.nodeID == destNode->nodeID &&
                            !otherConn.destination.isMIDI() &&
                            otherConn.destination.channelIndex >= 2)  // Sidechain channels are 2-3
                        {
                            hasOtherSidechainConnections = true;
                            break;
                        }
                    }
                    
                    // If no more sidechain connections, disable sidechain
                    if (!hasOtherSidechainConnections)
                    {
                        meteringProc->disableSidechain();
                    }
                    
                    markDirty();
                    return;
                }
            }
        }
        
        // Normal connection removal (not sidechain)
        ag->removeConnection(conn);
        markDirty();
    }
}