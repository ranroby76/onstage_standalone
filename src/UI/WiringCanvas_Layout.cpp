
// ==============================================================================
//  WiringCanvas_Layout.cpp
//  OnStage — Node geometry, pin positions, hit detection
//
//  Audio-only pins.  Sidechain nodes get extra input pins (channels 2-3).
//  Custom node heights supported (e.g., PreAmp node is taller).
//  Recorder nodes get special large dimensions (360×160).
//
//  FIX: Sidechain nodes (DynamicEQ, Compressor) always show 4 input pins
// ==============================================================================

#include "WiringCanvas.h"

// ==============================================================================
//  Helper: Get effective number of input channels (includes sidechain)
// ==============================================================================

static int getEffectiveInputChannels(juce::AudioProcessor* proc, 
                                      const WiringCanvas::NodeTypeCache* cache)
{
    int numIn = proc->getTotalNumInputChannels();
    
    // FIX: For nodes with sidechain, always report 4 input channels
    // This ensures green sidechain pins are visible and clickable
    if (cache && cache->hasSidechain && numIn < 4)
        numIn = 4;
    
    return numIn;
}

// ==============================================================================
//  Node bounds
// ==============================================================================

juce::Rectangle<float> WiringCanvas::getNodeBounds (
    juce::AudioProcessorGraph::Node* node) const
{
    float x = (float) node->properties.getWithDefault ("x", 0.0);
    float y = (float) node->properties.getWithDefault ("y", 0.0);

    auto* proc = node->getProcessor();
    auto* cache = const_cast<WiringCanvas*>(this)->getCached (node->nodeID);

    // --- Recorder nodes get special large dimensions -------------------------
    if (cache && cache->isRecorder)
        return { x, y, WiringStyle::recorderNodeWidth, WiringStyle::recorderNodeHeight };

    // FIX: Use effective input channels for sidechain nodes
    int numIn  = getEffectiveInputChannels(proc, cache);
    int numOut = proc->getTotalNumOutputChannels();
    int maxPins = juce::jmax (numIn, numOut);

    float requiredWidth = WiringStyle::minNodeWidth;
    if (maxPins > 1)
    {
        float neededWidth = (maxPins + 1) * WiringStyle::minPinSpacing;
        requiredWidth = juce::jmax (requiredWidth, neededWidth);
    }

    // I/O nodes are a bit wider
    if (cache && (cache->isAudioInput || cache->isAudioOutput))
        requiredWidth = juce::jmax (requiredWidth, WiringStyle::ioNodeMinWidth);

    // Check for custom height from effect nodes (e.g., PreAmp)
    float nodeHeight = WiringStyle::nodeHeight;
    if (cache && cache->effectNode)
    {
        float customHeight = cache->effectNode->getCustomNodeHeight();
        if (customHeight > 0.0f)
            nodeHeight = customHeight;
    }

    return { x, y, requiredWidth, nodeHeight };
}

// ==============================================================================
//  Pin positions  (inputs on top, outputs on bottom — same as Colosseum)
// ==============================================================================

juce::Point<float> WiringCanvas::getPinPos (
    juce::AudioProcessorGraph::Node* node, const PinID& pin) const
{
    auto nodeBounds = getNodeBounds (node);
    auto* proc = node->getProcessor();
    auto* cache = const_cast<WiringCanvas*>(this)->getCached (node->nodeID);

    // FIX: Use effective input channels for sidechain nodes
    int totalPins = pin.isInput ? getEffectiveInputChannels(proc, cache)
                                : proc->getTotalNumOutputChannels();
    if (totalPins == 0) return nodeBounds.getCentre();

    float spacing = WiringStyle::minPinSpacing;
    float totalWidth = spacing * (totalPins + 1);
    float startX = nodeBounds.getCentreX() - totalWidth / 2.0f;
    float x = startX + spacing * (float)(pin.pinIndex + 1);

    // hookLength offset above/below the node body
    constexpr float hookLength = 10.0f;
    float y = pin.isInput ? (nodeBounds.getY() - hookLength)
                          : (nodeBounds.getBottom() + hookLength);

    return { x, y };
}

juce::Point<float> WiringCanvas::getPinCenter (const PinID& pin) const
{
    auto* node = stageGraph.getGraph().getNodeForId (pin.nodeID);
    if (! node) return {};
    return getPinPos (node, pin);
}

// ==============================================================================
//  Pin color  (blue for audio, green for sidechain inputs ≥ ch 2)
// ==============================================================================

juce::Colour WiringCanvas::getPinColor (const PinID& pin,
                                         juce::AudioProcessorGraph::Node* node) const
{
    auto* cache = const_cast<WiringCanvas*>(this)->getCached (node->nodeID);

    // I/O nodes always blue
    if (cache && (cache->isAudioInput || cache->isAudioOutput || cache->isPlayback))
        return WiringStyle::colPinAudio;

    // Sidechain inputs are green (channels 2+)
    if (cache && cache->effectNode && cache->hasSidechain && pin.isInput && pin.pinIndex >= 2)
        return WiringStyle::colPinSidechain;

    return WiringStyle::colPinAudio;
}

// ==============================================================================
//  Hit-testing:  find pin at mouse position
//  FIX: Properly detects sidechain pins (channels 2-3) for nodes with sidechain
// ==============================================================================

WiringCanvas::PinID WiringCanvas::findPinAt (juce::Point<float> pos)
{
    auto& g = stageGraph.getGraph();

    for (auto* node : g.getNodes())
    {
        if (! shouldShowNode (node)) continue;
        auto* proc = node->getProcessor();
        auto* cache = getCached (node->nodeID);

        // FIX: Use effective input channels for sidechain nodes
        int numIn = getEffectiveInputChannels(proc, cache);
        
        // Input pins
        for (int i = 0; i < numIn; ++i)
        {
            PinID p { node->nodeID, i, true };
            if (pos.getDistanceFrom (getPinPos (node, p)) <= WiringStyle::pinSize)
                return p;
        }

        // Output pins
        int numOut = proc->getTotalNumOutputChannels();
        for (int i = 0; i < numOut; ++i)
        {
            PinID p { node->nodeID, i, false };
            if (pos.getDistanceFrom (getPinPos (node, p)) <= WiringStyle::pinSize)
                return p;
        }
    }

    return {};
}

// ==============================================================================
//  Hit-testing:  find node at mouse position
// ==============================================================================

juce::AudioProcessorGraph::Node* WiringCanvas::findNodeAt (juce::Point<float> pos)
{
    for (auto* node : stageGraph.getGraph().getNodes())
    {
        if (shouldShowNode (node) && getNodeBounds (node).contains (pos))
            return node;
    }
    return nullptr;
}
