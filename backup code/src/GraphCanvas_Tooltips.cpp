#include "GraphCanvas.h"

void GraphCanvas::showPinInfo(const PinID& pin, const juce::Point<float>& componentPos)
{
    auto* node = processor.mainGraph->getNodeForId(pin.nodeID);
    if (!node) return;

    bool isActive = !node->isBypassed();
    juce::String text;

    auto* cache = getCachedNodeType(pin.nodeID);
    bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
    bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());
    bool isMidiInput   = cache ? cache->isMidiInput   : (node == processor.midiInputNode.get());
    bool isMidiOutput  = cache ? cache->isMidiOutput  : (node == processor.midiOutputNode.get());

    if (pin.isMidi)
    {
        if (isMidiInput) text = "MIDI Input";
        else if (isMidiOutput) text = "MIDI Output";
        else text = pin.isInput ? "MIDI In" : "MIDI Out";
    }
    else if (isAudioInput && !pin.isInput)
    {
        text = processor.getDeviceInputChannelName(pin.pinIndex);
    }
    else if (isAudioOutput && pin.isInput)
    {
        text = processor.getDeviceOutputChannelName(pin.pinIndex);
    }
    else
    {
        MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
        if (meteringProc && pin.isInput)
        {
            auto mapping = meteringProc->mapInputChannel(pin.pinIndex);
            if (mapping.isSidechain)
                text = "Sidechain Ch " + juce::String(mapping.channelInBus + 1);
            else
                text = "Main Input Ch " + juce::String(mapping.channelInBus + 1);
        }
        else
        {
            juce::String side = pin.isInput ? "Input" : "Output";
            text = side + " Ch " + juce::String(pin.pinIndex + 1);
        }
    }

    // FIX 1: Convert component coordinates to screen coordinates properly
    // componentPos is relative to this GraphCanvas component
    auto screenBounds = getScreenBounds();
    juce::Point<int> tooltipScreenPos(
        screenBounds.getX() + (int)componentPos.x + 10, 
        screenBounds.getY() + (int)componentPos.y + 10
    );

    auto* content = new StatusToolTip(text, isActive);
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(content),
        juce::Rectangle<int>(tooltipScreenPos.x, tooltipScreenPos.y, 1, 1),
        nullptr);
}

void GraphCanvas::showWireMenu(const juce::AudioProcessorGraph::Connection& conn, const juce::Point<float>& componentPos)
{
    // FIX: Use getActiveGraph() instead of processor.mainGraph so this works in containers
    auto* ag = getActiveGraph();
    if (!ag) return;
    
    juce::String text = conn.source.isMIDI() ? "MIDI Connection" : "Audio Connection";

    // Check if sidechain connection
    auto* dstNode = ag->getNodeForId(conn.destination.nodeID);
    if (dstNode && !conn.source.isMIDI())
    {
        auto* cache = getCachedNodeType(conn.destination.nodeID);
        MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(dstNode->getProcessor());
        if (meteringProc)
        {
            auto mapping = meteringProc->mapInputChannel(conn.destination.channelIndex);
            if (mapping.isSidechain)
                text = "Sidechain Connection";
        }
    }

    bool isActive = false;
    auto* src = ag->getNodeForId(conn.source.nodeID);
    auto* dst = ag->getNodeForId(conn.destination.nodeID);
    if (src && dst)
        isActive = (!src->isBypassed() && !dst->isBypassed());

    auto* content = new StatusToolTip(text, isActive, [this, conn]()
    {
        // FIX: Use getActiveGraph() for deletion too
        if (auto* graph = getActiveGraph())
        {
            graph->removeConnection(conn);
            refreshCache();  // FIX: Rebuild cache so deletion is visible immediately
            markDirty();
        }
    });

    // FIX 1: Convert component coordinates to screen coordinates properly
    auto screenBounds = getScreenBounds();
    juce::Point<int> tooltipScreenPos(
        screenBounds.getX() + (int)componentPos.x + 10, 
        screenBounds.getY() + (int)componentPos.y + 10
    );

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(content),
        juce::Rectangle<int>(tooltipScreenPos.x, tooltipScreenPos.y, 1, 1),
        nullptr);
}