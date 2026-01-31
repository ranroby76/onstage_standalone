// #D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Layout.cpp
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!
// FIXED: Added RecorderProcessor size (4x width, 3x height)

#include "GraphCanvas.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"

void GraphCanvas::drawPin(juce::Graphics& g, juce::Point<float> pos, juce::Colour color, bool isHovered, bool isHighlighted)
{
    float size = Style::pinSize;
    if (isHovered || isHighlighted) size *= 1.3f;

    // FIX 1: Yellow highlight for valid connection targets during cable drag
    bool isValidTarget = false;
    if (dragCable.active && isHighlighted)
    {
        // Check if this highlighted pin can accept the cable being dragged
        // highlightPin is set in mouseDrag when hovering over pins
        isValidTarget = canConnect(dragCable.sourcePin, highlightPin);
    }
    
    if (isValidTarget)
    {
        // Draw yellow glow for valid connection target
        g.setColour(juce::Colours::yellow.withAlpha(0.6f));
        g.fillEllipse(pos.x - size / 2 - 3, pos.y - size / 2 - 3, size + 6, size + 6);
        
        g.setColour(juce::Colours::yellow);
        g.fillEllipse(pos.x - size / 2, pos.y - size / 2, size, size);
        g.setColour(juce::Colours::white);
        g.drawEllipse(pos.x - size / 2, pos.y - size / 2, size, size, 2.0f);
    }
    else
    {
        g.setColour(color);
        g.fillEllipse(pos.x - size / 2, pos.y - size / 2, size, size);
        g.setColour(juce::Colours::white);
        g.drawEllipse(pos.x - size / 2, pos.y - size / 2, size, size, 1.5f);
    }
}

juce::Rectangle<float> GraphCanvas::getNodeBounds(juce::AudioProcessorGraph::Node* node) const
{
    float x = (float)node->properties.getWithDefault("x", 0.0);
    float y = (float)node->properties.getWithDefault("y", 0.0);

    auto* proc = node->getProcessor();
    int numIn = proc->getTotalNumInputChannels();
    int numOut = proc->getTotalNumOutputChannels();
    if (proc->acceptsMidi())  numIn++;
    if (proc->producesMidi()) numOut++;

    int maxPins = juce::jmax(numIn, numOut);
    float requiredWidth = Style::minNodeWidth;

    if (maxPins > 1)
    {
        float neededWidth = (maxPins + 1) * Style::minPinSpacing;
        requiredWidth = juce::jmax(requiredWidth, neededWidth);
    }

    // FIX: Custom sizes for system tools
    float nodeHeight = Style::nodeHeight;
    float nodeWidth = requiredWidth;
    
    if (dynamic_cast<StereoMeterProcessor*>(proc))
    {
        // Stereo Meter: 4x taller
        nodeHeight = Style::nodeHeight * 4.0f;
    }
    else if (dynamic_cast<MidiMonitorProcessor*>(proc))
    {
        // MIDI Monitor: 6x taller, 2x wider
        nodeHeight = Style::nodeHeight * 6.0f;
        nodeWidth = Style::minNodeWidth * 2.0f;
    }
    else if (dynamic_cast<RecorderProcessor*>(proc))
    {
        // Recorder: 3x taller (180px), 4x wider (480px)
        nodeHeight = Style::nodeHeight * 3.0f;
        nodeWidth = Style::minNodeWidth * 4.0f;
    }

    return { x, y, nodeWidth, nodeHeight };
}

// Pin color: Blue for main audio, Green for sidechain, Red for MIDI
juce::Colour GraphCanvas::getPinColor(const PinID& pinId, juce::AudioProcessorGraph::Node* node)
{
    if (pinId.isMidi) return Style::colPinMidi;

    // I/O nodes are always blue
    auto* cache = getCachedNodeType(node->nodeID);
    bool isIONode = cache ? cache->isIO
                          : (node == processor.audioInputNode.get() ||
                             node == processor.audioOutputNode.get() ||
                             node == processor.midiInputNode.get() ||
                             node == processor.midiOutputNode.get());

    if (isIONode) return Style::colPinAudio;

    // For MeteringProcessor, check if this is a sidechain pin
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
    if (meteringProc && pinId.isInput)
    {
        auto mapping = meteringProc->mapInputChannel(pinId.pinIndex);
        if (mapping.isSidechain)
            return Style::colPinSidechain; // Green
    }

    return Style::colPinAudio; // Blue
}

GraphCanvas::PinID GraphCanvas::findPinAt(juce::Point<float> pos)
{
    if (!processor.mainGraph) return {};

    for (auto* node : processor.mainGraph->getNodes())
    {
        if (!shouldShowNode(node)) continue;

        auto* proc = node->getProcessor();

        // =========================================================================
        // CRITICAL FIX: Use isInstrument() instead of getPluginDescription()
        // getPluginDescription() freezes some plugins when called!
        // Instruments have NO audio inputs - only MIDI input
        // =========================================================================
        int numIn = 0;
        if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
            if (mp->getInnerPlugin() && !mp->isInstrument()) {
                numIn = proc->getTotalNumInputChannels();
            }
        } else {
            numIn = proc->getTotalNumInputChannels();
        }
        
        if (proc->acceptsMidi()) numIn++;

        for (int i = 0; i < numIn; ++i)
        {
            bool isMidi = proc->acceptsMidi() && (i == (numIn - 1));  // MIDI is last pin
            PinID p = { node->nodeID, isMidi ? 0 : i, true, isMidi };
            if (pos.getDistanceFrom(getPinPos(node, p)) <= Style::pinSize) return p;
        }

        int numOut = proc->getTotalNumOutputChannels();
        if (proc->producesMidi()) numOut++;

        for (int i = 0; i < numOut; ++i)
        {
            bool isMidi = proc->producesMidi() && (i == proc->getTotalNumOutputChannels());
            PinID p = { node->nodeID, isMidi ? 0 : i, false, isMidi };
            if (pos.getDistanceFrom(getPinPos(node, p)) <= Style::pinSize) return p;
        }
    }

    return {};
}

juce::AudioProcessorGraph::Node* GraphCanvas::findNodeAt(juce::Point<float> pos)
{
    for (auto* node : processor.mainGraph->getNodes())
    {
        if (shouldShowNode(node) && getNodeBounds(node).contains(pos))
            return node;
    }
    return nullptr;
}
