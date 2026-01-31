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

    PinID source = start.isInput ? end : start;
    PinID dest   = start.isInput ? start : end;

    // Check if connecting to a sidechain pin and auto-enable sidechain
    auto* destNode = processor.mainGraph->getNodeForId(dest.nodeID);

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

    // Create the connection directly - JUCE handles fan-out natively
    if (source.isMidi)
    {
        processor.mainGraph->addConnection({
            { source.nodeID, juce::AudioProcessorGraph::midiChannelIndex },
            { dest.nodeID,   juce::AudioProcessorGraph::midiChannelIndex }
        });
    }
    else
    {
        processor.mainGraph->addConnection({
            { source.nodeID, source.pinIndex },
            { dest.nodeID,   dest.pinIndex }
        });
    }

    markDirty();
}

juce::AudioProcessorGraph::Connection GraphCanvas::getConnectionAt(juce::Point<float> pos)
{
    const float hitTolerance = 5.0f;

    auto connections = processor.mainGraph->getConnections();
    
    // FIX: Find the CLOSEST connection to the click point, not just the first match
    // This prevents wrong wire deletion when paths overlap (e.g., output wires vs sidechain input wires)
    juce::AudioProcessorGraph::Connection closestConnection = { 
        { juce::AudioProcessorGraph::NodeID(), 0 }, 
        { juce::AudioProcessorGraph::NodeID(), 0 } 
    };
    float closestDistance = std::numeric_limits<float>::max();
    
    for (const auto& connection : connections)
    {
        auto* src = processor.mainGraph->getNodeForId(connection.source.nodeID);
        auto* dst = processor.mainGraph->getNodeForId(connection.destination.nodeID);

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

            if (p.getBounds().expanded(hitTolerance).contains(pos))
            {
                // Calculate distance from click point to the wire path
                // Use the midpoint of the wire as approximation
                juce::Point<float> wireMidpoint((start.x + end.x) / 2.0f, (start.y + end.y) / 2.0f);
                float distance = pos.getDistanceFrom(wireMidpoint);
                
                if (distance < closestDistance)
                {
                    closestDistance = distance;
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
        // FIX 2: Before removing connection, check if it's a sidechain connection
        // If removing the last sidechain connection, disable sidechain processing
        auto* destNode = processor.mainGraph->getNodeForId(conn.destination.nodeID);
        
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
                    processor.mainGraph->removeConnection(conn);
                    
                    // Check if there are any other sidechain connections remaining
                    bool hasOtherSidechainConnections = false;
                    for (auto& otherConn : processor.mainGraph->getConnections())
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
        processor.mainGraph->removeConnection(conn);
        markDirty();
    }
}
