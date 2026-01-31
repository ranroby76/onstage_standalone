// FIXED: Plugin drag-and-drop matching - use fileOrIdentifier instead of createIdentifierString()
// FIXED: Signature must match header exactly (SourceDetails, not juce::DragAndDropTarget::SourceDetails)

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"

bool GraphCanvas::isInterestedInDragSource(const SourceDetails& dragSourceDetails) {
    juce::String dragId = dragSourceDetails.description.toString();
    
    // Accept system tools
    if (dragId.startsWith("TOOL:")) return true;
    
    // Accept plugins from browser panel
    auto types = processor.knownPluginList.getTypes();
    for (const auto& desc : types) {
        // FIX: Match by fileOrIdentifier (what getDragSourceDescription returns)
        if (desc.fileOrIdentifier == dragId) return true;
        // Also try createIdentifierString for backward compatibility
        if (desc.createIdentifierString() == dragId) return true;
    }
    
    return false;
}

void GraphCanvas::itemDragEnter(const SourceDetails& /*dragSourceDetails*/) {
    // Visual feedback when drag enters
    isDragHovering = true;
    needsRepaint = true;
}

void GraphCanvas::itemDragMove(const SourceDetails& dragSourceDetails) {
    // Track position for visual feedback
    dragHoverPosition = dragSourceDetails.localPosition.toInt();
}

void GraphCanvas::itemDragExit(const SourceDetails& /*dragSourceDetails*/) {
    // Clear visual feedback when drag exits
    isDragHovering = false;
    needsRepaint = true;
}

void GraphCanvas::itemDropped(const SourceDetails& dragSourceDetails) {
    isDragHovering = false;
    juce::String dragId = dragSourceDetails.description.toString();
    juce::Point<int> dropPos = dragSourceDetails.localPosition.toInt();
    
    // =========================================================================
    // Handle System Tools
    // =========================================================================
    if (dragId.startsWith("TOOL:")) {
        juce::String toolName = dragId.substring(5);
        juce::AudioProcessorGraph::Node::Ptr nodePtr;
        
        if (toolName == "Connector") {
            nodePtr = processor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new SimpleConnectorProcessor()));
        } else if (toolName == "StereoMeter") {
            nodePtr = processor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new StereoMeterProcessor()));
        } else if (toolName == "MidiMonitor") {
            nodePtr = processor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new MidiMonitorProcessor()));
        }
        
        if (nodePtr) {
            nodePtr->properties.set("x", (double)dropPos.x);
            nodePtr->properties.set("y", (double)dropPos.y);
            markDirty();
        }
        
        repaint();
        return;
    }
    
    // =========================================================================
    // Handle Plugins - FIX: Match by fileOrIdentifier (primary) or createIdentifierString (fallback)
    // =========================================================================
    auto types = processor.knownPluginList.getTypes();
    for (const auto& desc : types) {
        // PRIMARY: Match by fileOrIdentifier (this is what getDragSourceDescription returns)
        if (desc.fileOrIdentifier == dragId) {
            addPluginAtPosition(desc, dropPos);
            repaint();
            return;
        }
        // FALLBACK: Match by createIdentifierString for backward compatibility
        if (desc.createIdentifierString() == dragId) {
            addPluginAtPosition(desc, dropPos);
            repaint();
            return;
        }
    }
    
    // If no match found, show error
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "Plugin Not Found",
        "Could not find plugin in the known plugins list.\n\nTry rescanning your plugins.",
        "OK");
    
    repaint();
}

void GraphCanvas::addPluginAtPosition(const juce::PluginDescription& description, juce::Point<int> position) {
    juce::String error;
    
    auto instance = processor.formatManager.createPluginInstance(
        description,
        processor.getSampleRate(),
        processor.getBlockSize(),
        error);
    
    if (instance) {
        auto meteringProc = std::make_unique<MeteringProcessor>(std::move(instance));
        auto nodePtr = processor.mainGraph->addNode(std::move(meteringProc));
        
        if (nodePtr) {
            nodePtr->properties.set("x", (double)position.x);
            nodePtr->properties.set("y", (double)position.y);
            updateParentSelector();
            markDirty();
        }
    } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Plugin Load Failed",
            "Could not load: " + description.name + "\n\n" + 
            (error.isEmpty() ? "Unknown error" : error),
            "OK");
    }
}