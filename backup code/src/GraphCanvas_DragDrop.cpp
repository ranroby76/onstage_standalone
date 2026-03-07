// #D:\Workspace\onstage_colosseum_upgrade\src\GraphCanvas_DragDrop.cpp
// FIXED: Plugin drag-and-drop matching - use fileOrIdentifier instead of createIdentifierString()
// FIXED: Signature must match header exactly (SourceDetails, not juce::DragAndDropTarget::SourceDetails)
// FIXED: Added Recorder system tool support
// NEW: Added Container system tool support
// CLEANED: Removed obsolete MIDI tools for OnStage (effects-only mode)

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "RecorderProcessor.h"
#include "TransientSplitterProcessor.h"
#include "ContainerProcessor.h"

bool GraphCanvas::isInterestedInDragSource(const SourceDetails& dragSourceDetails) {
    juce::String dragId = dragSourceDetails.description.toString();

    // Accept system tools
    if (dragId.startsWith("TOOL:")) return true;

    // Accept container preset files
    if (dragId.startsWith("CONTAINERPRESET:")) return true;

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
    juce::Point<int> dropPos = toVirtual(dragSourceDetails.localPosition.toFloat()).toInt();

    // =========================================================================
    // Handle System Tools
    // =========================================================================
    if (dragId.startsWith("TOOL:")) {
        juce::String toolName = dragId.substring(5);
        juce::AudioProcessorGraph::Node::Ptr nodePtr;
        auto* activeGraph = getActiveGraph();
        if (!activeGraph) return;

        if (toolName == "Connector") {
            nodePtr = activeGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new SimpleConnectorProcessor()));
        } else if (toolName == "StereoMeter") {
            nodePtr = activeGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new StereoMeterProcessor()));
        } else if (toolName == "Recorder") {
            nodePtr = activeGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new RecorderProcessor()));
        } else if (toolName == "TransientSplitter") {
            nodePtr = activeGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new TransientSplitterProcessor()));
        } else if (toolName == "Container") {
            // Block nested containers
            if (isInsideContainer())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Not Supported",
                    "Containers cannot be nested inside other containers.\n\nDive out to the main rack first.",
                    "OK");
                repaint();
                return;
            }
            auto containerProc = std::make_unique<ContainerProcessor>();
            containerProc->setContainerName("Container " + juce::String(++processor.containerCounter));
            containerProc->setParentProcessor(&processor);
            nodePtr = activeGraph->addNode(std::move(containerProc));
        } else if (toolName == "VST2Plugin") {
            // VST2 opens a file chooser - no node created here
            loadVST2Plugin(dropPos.toFloat());
            repaint();
            return;
        } else if (toolName == "VST3Plugin") {
            loadVST3Plugin(dropPos.toFloat());
            repaint();
            return;
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
    // Handle Container Preset files dragged from browser
    // =========================================================================
    if (dragId.startsWith("CONTAINERPRESET:")) {
        // Block loading container presets inside an existing container
        if (isInsideContainer())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Not Supported",
                "Container presets can only be loaded onto the main rack.\n\nDive out first, then drop the container.",
                "OK");
            repaint();
            return;
        }

        juce::String presetPath = dragId.substring(16);
        juce::File presetFile(presetPath);
        if (presetFile.existsAsFile()) {
            auto containerProc = std::make_unique<ContainerProcessor>();
            containerProc->setContainerName("Container " + juce::String(++processor.containerCounter));
            containerProc->setParentProcessor(&processor);
            containerProc->loadPreset(presetFile);

            auto nodePtr = processor.mainGraph->addNode(std::move(containerProc));
            if (nodePtr) {
                nodePtr->properties.set("x", (double)dropPos.x);
                nodePtr->properties.set("y", (double)dropPos.y);
                markDirty();
            }
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

// =============================================================================
// HELPER: Try a fresh in-process scan of a plugin file to get correct metadata.
// Used when stored metadata has a stale/wrong uniqueId (e.g. from a fallback
// scan entry) that causes createPluginInstance() to fail.
// Returns true + fills freshDesc if scan found at least one valid description.
// =============================================================================
static bool freshScanPlugin(juce::AudioPluginFormatManager& formatManager,
                            juce::KnownPluginList& knownPluginList,
                            const juce::PluginDescription& staleDesc,
                            juce::PluginDescription& freshDesc)
{
    // Find the format handler for this plugin
    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i) {
        auto* fmt = formatManager.getFormat(i);
        if (fmt->getName() == staleDesc.pluginFormatName) {
            format = fmt;
            break;
        }
    }
    if (!format) return false;

    // Do a fresh in-process scan of this specific file
    juce::OwnedArray<juce::PluginDescription> results;
    format->findAllTypesForFile(results, staleDesc.fileOrIdentifier);

    if (results.isEmpty()) return false;

    // Remove stale entry and add fresh ones
    // (stale entry has wrong uniqueId → can't load)
    auto existingTypes = knownPluginList.getTypes();
    for (const auto& existing : existingTypes) {
        if (existing.fileOrIdentifier.equalsIgnoreCase(staleDesc.fileOrIdentifier) &&
            existing.pluginFormatName == staleDesc.pluginFormatName) {
            knownPluginList.removeType(existing);
        }
    }
    knownPluginList.clearBlacklistedFiles();

    // Add all freshly scanned descriptions
    for (auto* desc : results) {
        // Preserve vendor from stale entry if the fresh scan returned empty vendor
        if (desc->manufacturerName.isEmpty() && staleDesc.manufacturerName.isNotEmpty())
            desc->manufacturerName = staleDesc.manufacturerName;

        knownPluginList.addType(*desc);
    }

    // Return the first result (usually the main processor)
    freshDesc = *results[0];
    return true;
}

void GraphCanvas::addPluginAtPosition(const juce::PluginDescription& description, juce::Point<int> position) {
    juce::String error;

    // SAFE DEFAULTS: When ASIO is off, getSampleRate()/getBlockSize() return 0.
    // Many VST3 plugins (Serum 2, Nexus, etc.) refuse to load with 0/0.
    double sr = processor.getSampleRate();
    int bs = processor.getBlockSize();
    if (sr <= 0.0) sr = 44100.0;
    if (bs <= 0) bs = 512;

    auto instance = processor.formatManager.createPluginInstance(
        description, sr, bs, error);

    // =========================================================================
    // FIX: If first attempt fails, try a fresh in-process scan and retry.
    // This handles plugins whose stored metadata has a stale/wrong uniqueId
    // (e.g. from a fallback scan that crashed). The fresh scan gets the real
    // VST3 component ID, allowing createPluginInstance() to succeed.
    // This fixes Serum 2, Nexus 5, and other plugins that were "blocked".
    // =========================================================================
    if (!instance) {
        juce::PluginDescription freshDesc;
        if (freshScanPlugin(processor.formatManager, processor.knownPluginList,
                            description, freshDesc))
        {
            error.clear();
            instance = processor.formatManager.createPluginInstance(
                freshDesc, sr, bs, error);
        }
    }

    if (instance) {
        auto meteringProc = std::make_unique<MeteringProcessor>(std::move(instance));
        auto* activeGraph = getActiveGraph();
        if (!activeGraph) return;
        auto nodePtr = activeGraph->addNode(std::move(meteringProc));

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
