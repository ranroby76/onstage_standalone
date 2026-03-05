
// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Mouse.cpp
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!
// FIX: Updated all startTimer calls for MultiTimer API (timerID, intervalMs)
// FIX: Added RecorderProcessor button handling
// FIX: Fixed recorder button hit areas, added folder button
// NEW: Added ManualSampler, AutoSampler, MidiPlayer button handling

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
#include "ManualSamplerProcessor.h"
#include "AutoSamplerProcessor.h"
#include "MidiPlayerProcessor.h"
#include "CCStepperProcessor.h"
#include "CCStepperEditorComponent.h"
#include "TransientSplitterProcessor.h"
#include "TransientSplitterEditorComponent.h"
#include "LatcherProcessor.h"
#include "LatcherEditorComponent.h"
#include "MidiMultiFilterProcessor.h"
#include "MidiMultiFilterEditorComponent.h"
#include "AutoSamplerEditorComponent.h"
#include "MidiSelectors.h"
#include "TransportOverrideComponent.h"
#include "ContainerProcessor.h"

// =============================================================================
// Helper: toggle bypass on whichever graph (main or container inner) is active.
// processor.toggleBypass() only operates on mainGraph — this handles both.
// =============================================================================
static void toggleBypassOnGraph(juce::AudioProcessorGraph* ag,
                                juce::AudioProcessorGraph::NodeID nodeID)
{
    if (!ag) return;
    auto* node = ag->getNodeForId(nodeID);
    if (!node) return;

    bool newBypassState = !node->isBypassed();
    if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor()))
        meteringProc->setFrozen(newBypassState);
    node->setBypassed(newBypassState);
}

// Helper: safely remove a node from whichever graph is active.
// processor.removeNode() only operates on mainGraph.
static void removeNodeFromGraph(SubterraneumAudioProcessor& processor,
                                juce::AudioProcessorGraph* ag,
                                juce::AudioProcessorGraph::NodeID nodeID,
                                bool insideContainer)
{
    if (!ag) return;
    if (insideContainer)
    {
        processor.suspendProcessing(true);
        ag->removeNode(nodeID);
        processor.suspendProcessing(false);
    }
    else
    {
        processor.removeNode(nodeID);
    }
}

void GraphCanvas::mouseDown(const juce::MouseEvent& e)
{
    // =========================================================================
    // Container header click detection (pixel space — must be first)
    // =========================================================================
    if (isInsideContainer())
    {
        auto headerRect = juce::Rectangle<float>(0.0f, 0.0f, (float)getWidth(), containerHeaderHeight);
        if (headerRect.contains(e.position))
        {
            if (!e.mods.isLeftButtonDown()) return;

            const float btnH   = 30.0f;
            const float btnY   = (containerHeaderHeight - btnH) / 2.0f;
            const float margin = 8.0f;
            const float w      = (float)getWidth();

            // ← Rack button
            auto rackBtnRect = juce::Rectangle<float>(margin, btnY, 80.0f, btnH);
            if (rackBtnRect.contains(e.position))
            {
                diveOut();
                return;
            }

            // Save / Load buttons (right side)
            float saveX = w - margin - 65.0f;
            float loadX = saveX - margin - 65.0f;

            auto saveRect = juce::Rectangle<float>(saveX, btnY, 65.0f, btnH);
            if (saveRect.contains(e.position) && !containerStack.empty())
            {
                auto* containerProc = containerStack.back();
                auto defaultFolder = ContainerProcessor::getEffectiveDefaultFolder();
                defaultFolder.createDirectory();

                containerFileChooser = std::make_unique<juce::FileChooser>(
                    "Save Container Preset",
                    defaultFolder.getChildFile(containerProc->getContainerName() + ".container"),
                    "*.container");

                juce::Component::SafePointer<GraphCanvas> safeThis(this);
                containerFileChooser->launchAsync(
                    juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [safeThis, containerProc](const juce::FileChooser& fc)
                    {
                        if (!safeThis) return;
                        auto file = fc.getResult();
                        if (file != juce::File{})
                            containerProc->savePreset(file);
                    });
                return;
            }

            auto loadRect = juce::Rectangle<float>(loadX, btnY, 65.0f, btnH);
            if (loadRect.contains(e.position) && !containerStack.empty())
            {
                auto* containerProc = containerStack.back();
                auto defaultFolder = ContainerProcessor::getEffectiveDefaultFolder();

                containerFileChooser = std::make_unique<juce::FileChooser>(
                    "Load Container Preset",
                    defaultFolder,
                    "*.container");

                juce::Component::SafePointer<GraphCanvas> safeThis(this);
                containerFileChooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safeThis, containerProc](const juce::FileChooser& fc)
                    {
                        auto* self = static_cast<GraphCanvas*>(safeThis.getComponent());
                        if (!self) return;
                        auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            self->processor.suspendProcessing(true);
                            containerProc->loadPreset(file);
                            self->processor.suspendProcessing(false);
                            self->rebuildNodeTypeCache();
                            self->markDirty();
                            self->repaint();
                        }
                    });
                return;
            }

            // Name label click — show AlertWindow to rename
            {
                float nameX = margin + 80.0f + 12.0f;
                float nameW = loadX - nameX - 12.0f;
                auto nameRect = juce::Rectangle<float>(nameX, btnY, nameW, btnH);
                if (nameRect.contains(e.position) && !containerStack.empty())
                {
                    auto* containerProc = containerStack.back();
                    juce::Component::SafePointer<GraphCanvas> safeThis(this);

                    auto* alertWindow = new juce::AlertWindow(
                        "Rename Container",
                        "Enter a new name for this container:",
                        juce::MessageBoxIconType::NoIcon);
                    alertWindow->addTextEditor("name", containerProc->getContainerName(), "Name:");
                    alertWindow->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
                    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                    alertWindow->enterModalState(true,
                        juce::ModalCallbackFunction::create(
                            [safeThis, alertWindow, containerProc](int result)
                            {
                                if (result == 1 && safeThis)
                                {
                                    auto newName = alertWindow->getTextEditorContents("name").trim();
                                    if (newName.isNotEmpty())
                                    {
                                        containerProc->setContainerName(newName);
                                        safeThis->markDirty();
                                        safeThis->repaint();
                                    }
                                }
                            }), true);
                    return;
                }
            }

            // Anything else in the header — consume without acting
            return;
        }
    }

    // =========================================================================
    // NEW: Check minimap and scrollbar hits FIRST (in local coords, not virtual)
    // =========================================================================
    if (e.mods.isLeftButtonDown())
    {
        if (isPointInMinimap(e.position))
        {
            isDraggingMinimap = true;
            navigateMinimapTo(e.position);
            return;
        }

        if (isPointInHScrollbar(e.position))
        {
            isDraggingHScrollbar = true;
            scrollbarDragStartOffset = e.position.x;
            scrollbarDragStartPan = panOffsetX;
            return;
        }

        if (isPointInVScrollbar(e.position))
        {
            isDraggingVScrollbar = true;
            scrollbarDragStartOffset = e.position.y;
            scrollbarDragStartPan = panOffsetY;
            return;
        }
    }

    auto pos = toVirtual(e.position);
    // Screen-space click position for popup menu placement
    auto screenClickPos = e.getScreenPosition();

    // Check for pin or connection
    auto pinAtPos = findPinAt(pos);
    if (pinAtPos.isValid())
    {
        if (e.mods.isRightButtonDown())
        {
            showPinInfo(pinAtPos, pos);
            return;
        }

        dragCable.active = true;
        dragCable.sourcePin = pinAtPos;
        dragCable.currentDragPos = pos;
        dragCable.dragColor = getPinColor(pinAtPos, getActiveGraph() ? getActiveGraph()->getNodeForId(pinAtPos.nodeID) : nullptr);

        // CRITICAL FIX: Start high-frequency timer ONCE when drag begins
        startTimer(MouseInteractionTimerID, 16);  // 60 Hz for smooth dragging
        return;
    }

    auto connAtPos = getConnectionAt(pos);
    if (connAtPos.source.nodeID.uid != 0)
    {
        if (e.mods.isRightButtonDown())
        {
            showWireMenu(connAtPos, pos);
            return;
        }
        else if (e.getNumberOfClicks() == 2)
        {
            if (auto* ag = getActiveGraph())
                ag->removeConnection(connAtPos);
            markDirty();
            return;
        }
    }

    // Check for node interaction
    if (auto* node = findNodeAt(pos))
    {
        auto nodeBounds = getNodeBounds(node);

        // Use cached type info
        auto* cache = getCachedNodeType(node->nodeID);
        MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
        // =========================================================================
        // CRITICAL FIX: Use isInstrument() instead of getPluginDescription()
        // getPluginDescription() freezes some plugins when called!
        // =========================================================================
        bool isInstrument = cache ? cache->isInstrument
                                  : (meteringProc && meteringProc->isInstrument());

        bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
        bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());

        // =========================================================================
        // TITLE BAR RIGHT-CLICK MENU (light gray area at top of node)
        // Available for all node types - provides Disconnect All and Delete options
        // =========================================================================
        auto titleBarArea = juce::Rectangle<float>(
            nodeBounds.getX(),
            nodeBounds.getY(),
            nodeBounds.getWidth(),
            Style::nodeTitleHeight
        );

        if (e.mods.isRightButtonDown() && titleBarArea.contains(pos))
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Disconnect All Wires");
            menu.addSeparator();
            menu.addItem(2, "Delete");

            auto nodeID = node->nodeID;
            auto* capturedGraph = getActiveGraph();
            bool insideContainer = isInsideContainer();
            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this, nodeID, capturedGraph, insideContainer](int result)
                {
                    if (result == 1)
                    {
                        // Disconnect all wires from this node
                        if (capturedGraph)
                        {
                            if (auto* targetNode = capturedGraph->getNodeForId(nodeID))
                            {
                                disconnectNode(targetNode);
                                markDirty();
                                needsRepaint = true;
                            }
                        }
                    }
                    else if (result == 2)
                    {
                        // Delete node — use active graph to avoid touching mainGraph when inside container
                        if (capturedGraph)
                        {
                            if (auto* targetNode = capturedGraph->getNodeForId(nodeID))
                            {
                                // Check if it's a recorder and stop recording first
                                auto* recorderProc = dynamic_cast<RecorderProcessor*>(targetNode->getProcessor());
                                if (recorderProc && recorderProc->isCurrentlyRecording())
                                    recorderProc->stopRecording();

                                // Close plugin window if exists
                                auto windowIt = activePluginWindows.find(nodeID);
                                if (windowIt != activePluginWindows.end())
                                {
                                    activePluginWindows.erase(windowIt);
                                }

                                // Disconnect all wires
                                disconnectNode(targetNode);

                                // Remove node from the correct graph
                                if (insideContainer)
                                {
                                    // Suspend audio processing around inner graph mutation
                                    processor.suspendProcessing(true);
                                    capturedGraph->removeNode(nodeID);
                                    processor.suspendProcessing(false);
                                }
                                else
                                {
                                    removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                                }
                                updateParentSelector();
                                markDirty();
                            }
                        }
                    }
                });
            return;
        }

        // SimpleConnector button handling (system tool)
        SimpleConnectorProcessor* simpleConnector = cache ? cache->simpleConnector
                                                           : dynamic_cast<SimpleConnectorProcessor*>(node->getProcessor());
        if (simpleConnector)
        {
            nodeBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            // M button (Mute)
            auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

            // X button (Delete)
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            // Volume knob area (center of node content)
            auto knobArea = nodeBounds.reduced(15, 10);

            // Right-click: Show tooltips only
            if (e.mods.isRightButtonDown())
            {

                if (muteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("M - Mute/Unmute audio output", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (deleteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("X - Delete this connector module", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (knobArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Volume - Drag up/down to adjust gain (-inf to +25dB)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Right-click elsewhere on node - no action (consistent with tooltip-only policy)
                return;
            }

            // Left-click: Handle buttons
            if (muteRect.contains(pos))
            {
                simpleConnector->toggleMute();
                needsRepaint = true;
                return;
            }

            if (deleteRect.contains(pos))
            {
                // CRITICAL FIX: Close window and disconnect wires BEFORE removing node
                auto nodeID = node->nodeID;

                // Step 1: Close the plugin window if it exists
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                {
                    activePluginWindows.erase(windowIt);
                }

                // Step 2: Disconnect all wires connected to this node
                disconnectNode(node);

                // Step 3: Now safely remove the node
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // If clicking anywhere else on the node, start dragging
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>(
                (float)node->properties["x"],
                (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }

        // =====================================================================
        // Container button handling — E (dive in), M, T, X
        // =====================================================================
        ContainerProcessor* containerProc = cache ? cache->container
                                                   : dynamic_cast<ContainerProcessor*>(node->getProcessor());
        if (containerProc)
        {
            auto contentBounds = nodeBounds;
            contentBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = contentBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            // E button (dive into container)
            auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

            // M button (mute)
            auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

            // T button (transport override)
            auto transportRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

            // X button (delete)
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            // Right-click: tooltips
            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (editRect.contains(pos))      { launchTip("E - Dive into container (open internal graph)"); return; }
                if (muteRect.contains(pos))      { launchTip("M - Mute/Unmute container audio output"); return; }
                if (transportRect.contains(pos)) { launchTip("T - Per-container transport override"); return; }
                if (deleteRect.contains(pos))    { launchTip("X - Delete container from rack"); return; }
                return;
            }

            // Left-click: button actions
            if (editRect.contains(pos))
            {
                diveIntoContainer(containerProc);
                return;
            }

            if (muteRect.contains(pos))
            {
                containerProc->toggleMute();
                needsRepaint = true;
                return;
            }

            if (transportRect.contains(pos))
            {
                showContainerTransportOverride(node);
                return;
            }

            if (deleteRect.contains(pos))
            {
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // If clicking anywhere else on the node, start dragging
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>(
                (float)node->properties["x"],
                (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }

        // FIX: StereoMeter and MIDI Monitor button handling (system tools - X button only)
        StereoMeterProcessor* stereoMeter = cache ? cache->stereoMeter
                                                   : dynamic_cast<StereoMeterProcessor*>(node->getProcessor());
        MidiMonitorProcessor* midiMonitor = cache ? cache->midiMonitor
                                                   : dynamic_cast<MidiMonitorProcessor*>(node->getProcessor());

        if (stereoMeter || midiMonitor)
        {
            nodeBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            // X button (Delete) - ONLY button for these modules
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            // Display area (for tooltip)
            auto displayArea = nodeBounds.reduced(6, 4);

            // Right-click: Show tooltips only
            if (e.mods.isRightButtonDown())
            {

                if (deleteRect.contains(pos))
                {
                    juce::String tooltipText = stereoMeter ? "X - Delete Stereo Meter module"
                                                           : "X - Delete MIDI Monitor module";
                    auto* tooltip = new StatusToolTip(tooltipText, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (displayArea.contains(pos))
                {
                    juce::String tooltipText = stereoMeter
                        ? "Level Meters - Shows L/R audio levels with peak hold"
                        : "MIDI Display - Shows incoming MIDI messages by channel";
                    auto* tooltip = new StatusToolTip(tooltipText, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Right-click elsewhere on node - no action
                return;
            }

            // Left-click: Delete button
            if (deleteRect.contains(pos))
            {
                // Close window and disconnect wires BEFORE removing node
                auto nodeID = node->nodeID;

                // Step 1: Close window if exists
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                {
                    activePluginWindows.erase(windowIt);
                }

                // Step 2: Disconnect all wires
                disconnectNode(node);

                // Step 3: Remove node
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // If clicking anywhere else on the node, start dragging
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>(
                (float)node->properties["x"],
                (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }

        // =========================================================================
        // RECORDER button handling (Record/Stop/Folder/Sync/Delete + Name editing)
        // FIX: Fixed button hit areas, added folder button
        // =========================================================================
        RecorderProcessor* recorder = cache ? cache->recorder
                                             : dynamic_cast<RecorderProcessor*>(node->getProcessor());
        if (recorder)
        {
            // Calculate button areas based on the recorder layout
            // Must match exactly with GraphCanvas_Paint.cpp
            auto contentArea = nodeBounds;
            contentArea.removeFromTop(Style::nodeTitleHeight);  // Skip title bar
            auto recorderArea = contentArea.reduced(8, 6);

            // ROW 1: Name textbox (full width)
            auto nameRow = recorderArea.removeFromTop(24);
            auto nameBoxArea = nameRow.reduced(0, 2);

            recorderArea.removeFromTop(4);

            // ROW 2: Record/Stop/Folder buttons (44px each + 4px gaps)
            auto controlRow = recorderArea.removeFromTop(44);
            auto recordBtnArea = controlRow.removeFromLeft(44).reduced(3);
            controlRow.removeFromLeft(4);
            auto stopBtnArea = controlRow.removeFromLeft(44).reduced(3);
            controlRow.removeFromLeft(4);
            auto folderBtnArea = controlRow.removeFromLeft(44).reduced(3);

            // Skip time row
            recorderArea.removeFromTop(4);
            recorderArea.removeFromTop(28);  // Time row
            recorderArea.removeFromTop(4);

            // SYNC button at bottom
            auto syncRow = recorderArea.removeFromBottom(26);
            auto syncArea = syncRow.reduced(20, 2);

            // Right-click: Show tooltips for buttons or context menu
            if (e.mods.isRightButtonDown())
            {

                // Record button tooltip
                if (recordBtnArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Record - Start recording audio to file", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Stop button tooltip
                if (stopBtnArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Stop - Stop recording and save file", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Folder button tooltip
                if (folderBtnArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Open Folder - Open recording folder in file explorer", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Name box tooltip
                if (nameBoxArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Recording Name - Click to edit filename prefix", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Sync toggle tooltip
                if (syncArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Sync Mode - When ON, all synced recorders start/stop together", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // X button tooltip
                float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
                float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
                auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                if (deleteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("X - Delete recorder module", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Right-click elsewhere on recorder - no action (tooltip-only policy)
                return;
            }

            // Left-click: Handle buttons

            // Name textbox click - open name editor
            if (nameBoxArea.contains(pos))
            {
                auto* editor = new juce::TextEditor();
                editor->setSize(300, 24);
                editor->setText(recorder->getRecorderName());
                editor->selectAll();
                editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(45, 45, 50));
                editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);

                // Capture recorder pointer for lambda
                auto* recPtr = recorder;
                editor->onReturnKey = [editor, recPtr]() {
                    recPtr->setRecorderName(editor->getText());
                    if (auto* callout = editor->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                editor->onEscapeKey = [editor]() {
                    if (auto* callout = editor->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };

                juce::CallOutBox::launchAsynchronously(
                    std::unique_ptr<juce::Component>(editor),
                    juce::Rectangle<int>(
                        virtualToScreen(nameBoxArea.getCentreX(), nameBoxArea.getCentreY()).x,
                        virtualToScreen(nameBoxArea.getCentreX(), nameBoxArea.getCentreY()).y,
                        1, 1),
                    nullptr);
                return;
            }

            // Sync toggle click
            if (syncArea.contains(pos))
            {
                recorder->setSyncMode(!recorder->isSyncMode());
                needsRepaint = true;
                return;
            }

            // Record button click
            if (recordBtnArea.contains(pos))
            {
                if (!recorder->isCurrentlyRecording())
                {
                    recorder->startRecording();
                }
                needsRepaint = true;
                return;
            }

            // Stop button click
            if (stopBtnArea.contains(pos))
            {
                if (recorder->isCurrentlyRecording())
                {
                    recorder->stopRecording();
                }
                needsRepaint = true;
                return;
            }

            // Folder button click - open recording folder
            if (folderBtnArea.contains(pos))
            {
                recorder->openRecordingFolder();
                return;
            }

            // X button (Delete) - bottom left corner
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (deleteRect.contains(pos))
            {
                // Stop recording if active before deleting
                if (recorder->isCurrentlyRecording())
                    recorder->stopRecording();

                auto nodeID = node->nodeID;

                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                {
                    activePluginWindows.erase(windowIt);
                }

                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // If clicking anywhere else on the node, start dragging
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>(
                (float)node->properties["x"],
                (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END RECORDER button handling
        // =========================================================================

        // =========================================================================
        // MANUAL SAMPLER button handling (ARM, Name editing, Folder, Delete)
        // =========================================================================
        ManualSamplerProcessor* manualSampler = cache ? cache->manualSampler
                                                       : dynamic_cast<ManualSamplerProcessor*>(node->getProcessor());
        if (manualSampler)
        {
            auto contentArea = nodeBounds;
            contentArea.removeFromTop(Style::nodeTitleHeight);
            auto recArea = contentArea.reduced(8, 6);

            // ROW 1: Name box
            auto nameRow = recArea.removeFromTop(24);
            nameRow.removeFromRight(70); // file count
            auto nameBoxArea = nameRow.reduced(0, 2);
            recArea.removeFromTop(4);

            // ROW 2: ARM button
            auto controlRow = recArea.removeFromTop(44);
            auto armBtnArea = controlRow.removeFromLeft(50).reduced(3);

            // X button at very bottom
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            // F button (folder)
            auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (nameBoxArea.contains(pos))  { launchTip("Family Name - Click to edit filename prefix"); return; }
                if (armBtnArea.contains(pos))   { launchTip("ARM - Arm/disarm MIDI-triggered recording"); return; }
                if (folderRect.contains(pos))   { launchTip("F - Open recording folder in file explorer"); return; }
                if (deleteRect.contains(pos))   { launchTip("X - Delete this Manual Sampler module"); return; }
                return;
            }

            if (nameBoxArea.contains(pos))
            {
                auto* editor = new juce::TextEditor();
                editor->setSize(300, 24);
                editor->setText(manualSampler->getFamilyName());
                editor->selectAll();
                editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(45, 45, 50));
                editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
                auto* sampPtr = manualSampler;
                editor->onReturnKey = [editor, sampPtr]() {
                    sampPtr->setFamilyName(editor->getText());
                    if (auto* callout = editor->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                editor->onEscapeKey = [editor]() {
                    if (auto* callout = editor->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                juce::CallOutBox::launchAsynchronously(
                    std::unique_ptr<juce::Component>(editor),
                    juce::Rectangle<int>(virtualToScreen(nameBoxArea.getCentreX(), nameBoxArea.getCentreY()).x,
                                         virtualToScreen(nameBoxArea.getCentreX(), nameBoxArea.getCentreY()).y, 1, 1),
                    nullptr);
                return;
            }

            if (armBtnArea.contains(pos))
            {
                manualSampler->setArmed(!manualSampler->getArmed());
                needsRepaint = true;
                return;
            }

            if (folderRect.contains(pos))
            {
                manualSampler->openRecordingFolder();
                return;
            }

            if (deleteRect.contains(pos))
            {
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END MANUAL SAMPLER button handling
        // =========================================================================

        // =========================================================================
        // AUTO SAMPLER button handling (Start/Stop, E editor, Name, Folder, Delete)
        // =========================================================================
        AutoSamplerProcessor* autoSampler = cache ? cache->autoSampler
                                                    : dynamic_cast<AutoSamplerProcessor*>(node->getProcessor());
        if (autoSampler)
        {
            auto contentArea = nodeBounds;
            contentArea.removeFromTop(Style::nodeTitleHeight);
            auto recArea = contentArea.reduced(8, 6);

            // ROW 1: Name box
            auto nameRow = recArea.removeFromTop(24);
            nameRow.removeFromRight(90); // progress
            auto nameBoxArea = nameRow.reduced(0, 2);
            recArea.removeFromTop(4);

            // ROW 2: Start/Stop button
            auto controlRow = recArea.removeFromTop(44);
            auto startBtnArea = controlRow.removeFromLeft(50).reduced(3);

            // Bottom buttons: E (editor) + F (folder) + X (delete)
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            auto editorRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (nameBoxArea.contains(pos))   { launchTip("Family Name - Click to edit filename prefix for samples"); return; }
                if (startBtnArea.contains(pos))  { launchTip("Start/Stop - Begin or halt the auto sampling sequence"); return; }
                if (editorRect.contains(pos))    { launchTip("E - Open note/velocity editor and SFZ settings"); return; }
                if (folderRect.contains(pos))    { launchTip("F - Open recording folder in file explorer"); return; }
                if (deleteRect.contains(pos))    { launchTip("X - Delete this Auto Sampler module"); return; }
                return;
            }

            if (nameBoxArea.contains(pos))
            {
                auto* editor = new juce::TextEditor();
                editor->setSize(300, 24);
                editor->setText(autoSampler->getFamilyName());
                editor->selectAll();
                editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(45, 45, 50));
                editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
                auto* sampPtr = autoSampler;
                editor->onReturnKey = [editor, sampPtr]() {
                    sampPtr->setFamilyName(editor->getText());
                    if (auto* callout = editor->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                editor->onEscapeKey = [editor]() {
                    if (auto* callout = editor->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                juce::CallOutBox::launchAsynchronously(
                    std::unique_ptr<juce::Component>(editor),
                    juce::Rectangle<int>(virtualToScreen(nameBoxArea.getCentreX(), nameBoxArea.getCentreY()).x,
                                         virtualToScreen(nameBoxArea.getCentreX(), nameBoxArea.getCentreY()).y, 1, 1),
                    nullptr);
                return;
            }

            if (startBtnArea.contains(pos))
            {
                if (autoSampler->isRunning())
                    autoSampler->stopAutoSampling();
                else
                {
                    if (!autoSampler->startAutoSampling())
                    {
                        juce::String err = autoSampler->getLastError();
                        if (err.contains("Connect"))
                        {
                            // No valid chain - show error tooltip
                            auto* tooltip = new StatusToolTip(err, false);
                            juce::CallOutBox::launchAsynchronously(
                                std::unique_ptr<juce::Component>(tooltip),
                                juce::Rectangle<int>(screenClickPos.x,
                                                     screenClickPos.y, 1, 1),
                                nullptr);
                        }
                        else
                        {
                            // No notes configured - open the editor
                            showAutoSamplerEditor(autoSampler);
                        }
                    }
                }
                needsRepaint = true;
                return;
            }

            if (editorRect.contains(pos))
            {
                showAutoSamplerEditor(autoSampler);
                return;
            }

            if (folderRect.contains(pos))
            {
                autoSampler->openRecordingFolder();
                return;
            }

            if (deleteRect.contains(pos))
            {
                if (autoSampler->isRunning()) autoSampler->stopAutoSampling();
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END AUTO SAMPLER button handling
        // =========================================================================

        // =========================================================================
        // MIDI PLAYER button handling
        // All hit areas computed exactly as in Paint (after title bar removal)
        // =========================================================================
        MidiPlayerProcessor* midiPlayer = cache ? cache->midiPlayer
                                                  : dynamic_cast<MidiPlayerProcessor*>(node->getProcessor());
        if (midiPlayer)
        {
            // --- Compute layout IDENTICALLY to GraphCanvas_Paint.cpp ---
            // In Paint: bounds already has title removed, then reduced(8,6)
            auto contentBounds = nodeBounds;
            contentBounds.removeFromTop(Style::nodeTitleHeight);
            auto area = contentBounds.reduced(8, 6);

            // TOP ROW: filename + load + info
            auto topRow = area.removeFromTop(26);
            auto infoBtnArea = topRow.removeFromRight(34);
            topRow.removeFromRight(4);
            auto loadBtnArea = topRow.removeFromRight(68);
            topRow.removeFromRight(6);

            area.removeFromTop(6);

            // MIDDLE ROW: transport + BPM + channel dots
            auto controlRow = area.removeFromTop(44);
            auto playBtnArea = controlRow.removeFromLeft(50).reduced(3);
            controlRow.removeFromLeft(4);
            auto stopBtnArea = controlRow.removeFromLeft(50).reduced(3);
            controlRow.removeFromLeft(4);
            auto loopBtnArea = controlRow.removeFromLeft(50).reduced(3);
            controlRow.removeFromLeft(10);
            auto bpmArea = controlRow.removeFromLeft(60);
            controlRow.removeFromLeft(4);
            auto syncBtnArea = controlRow.removeFromLeft(32).reduced(2, 6);

            area.removeFromTop(6);

            // BOTTOM: time + slider
            auto bottomArea = area;
            bottomArea.removeFromTop(16); // time row
            bottomArea.removeFromTop(4);
            auto sliderArea = bottomArea.reduced(0, 1);
            float trackX = sliderArea.getX() + 6;
            float trackW = sliderArea.getWidth() - 12;

            // E + X buttons (from drawNodeButtons: after title removal)
            float btnY = contentBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = contentBounds.getX() + Style::bottomBtnMargin;
            auto editBtnArea = juce::Rectangle<float>(btnX - 4, btnY - 4, 28, 28);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto deleteBtnArea = juce::Rectangle<float>(btnX - 4, btnY - 4, 28, 28);

            // Right-click: Show tooltips
            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (editBtnArea.contains(pos))   { launchTip("E - Open channel info / mute table"); return; }
                if (deleteBtnArea.contains(pos)) { launchTip("X - Delete this MIDI Player module"); return; }
                if (playBtnArea.contains(pos))   { launchTip("Play - Start MIDI file playback"); return; }
                if (stopBtnArea.contains(pos))   { launchTip("Stop - Stop playback and reset position"); return; }
                if (loopBtnArea.contains(pos))   { launchTip("Loop - Toggle loop playback on/off"); return; }
                if (bpmArea.contains(pos))       { launchTip("BPM - Drag up/down to set manual tempo"); return; }
                if (syncBtnArea.contains(pos))   { launchTip("SYNC - Sync tempo to master clock"); return; }
                if (loadBtnArea.contains(pos))   { launchTip("Load - Browse and load a .mid file"); return; }
                if (infoBtnArea.contains(pos))   { launchTip("Info - Show MIDI file channel summary"); return; }
                return;
            }

            // ---- HIT TESTING (E/X first, then others) ----

            // E button (channel info/mute popup)
            if (editBtnArea.contains(pos))
            {
                showMidiPlayerChannelInfo(midiPlayer);
                return;
            }

            // X button (delete)
            if (deleteBtnArea.contains(pos))
            {
                midiPlayer->stop();
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // LOAD button
            if (loadBtnArea.contains(pos))
            {
                juce::File startDir = lastBrowsedDirectory.isDirectory()
                    ? lastBrowsedDirectory
                    : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

                midiFileChooser = std::make_unique<juce::FileChooser>(
                    "Select MIDI File",
                    startDir,
                    "*.mid;*.midi;*.MID;*.MIDI");

                juce::Component::SafePointer<GraphCanvas> safeThis(this);
                auto* playerPtr = midiPlayer;

                midiFileChooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safeThis, playerPtr](const juce::FileChooser& fc)
                    {
                        if (!safeThis) return;
                        auto result = fc.getResult();
                        if (result.existsAsFile())
                        {
                            safeThis->lastBrowsedDirectory = result.getParentDirectory();
                            safeThis->saveLastBrowsedDirectory();
                            playerPtr->loadFile(result);
                            safeThis->needsRepaint = true;
                        }
                    });
                return;
            }

            // Top E button (info/channel editor)
            if (infoBtnArea.contains(pos))
            {
                showMidiPlayerChannelInfo(midiPlayer);
                return;
            }

            // PLAY/PAUSE
            if (playBtnArea.contains(pos))
            {
                if (midiPlayer->isPlaying()) midiPlayer->pause();
                else midiPlayer->play();
                needsRepaint = true;
                return;
            }

            // STOP
            if (stopBtnArea.contains(pos))
            {
                midiPlayer->stop();
                needsRepaint = true;
                return;
            }

            // LOOP toggle
            if (loopBtnArea.contains(pos))
            {
                midiPlayer->setLooping(!midiPlayer->isLooping());
                needsRepaint = true;
                return;
            }

            // SYNC button
            if (syncBtnArea.contains(pos))
            {
                midiPlayer->setSyncToMaster(!midiPlayer->isSyncToMaster());
                needsRepaint = true;
                return;
            }

            // BPM drag (drag only, no toggle)
            if (bpmArea.contains(pos))
            {
                // Dragging BPM disables sync
                if (midiPlayer->isSyncToMaster())
                {
                    midiPlayer->setSyncToMaster(false);
                    midiPlayer->setTempoOverride(true);
                    midiPlayer->setTempoOverrideBpm(midiPlayer->getCurrentBpm());
                }
                midiBpmDragging = true;
                midiBpmDragPlayer = midiPlayer;
                midiBpmDragStartY = pos.y;
                midiBpmDragStartValue = midiPlayer->getTempoOverrideBpm();
                needsRepaint = true;
                return;
            }

            // Slider seek
            if (sliderArea.contains(pos))
            {
                float normalised = juce::jlimit(0.0f, 1.0f, (pos.x - trackX) / trackW);
                midiPlayer->setPositionNormalized((double)normalised);
                midiSliderDragging = true;
                midiSliderDragPlayer = midiPlayer;
                midiSliderTrackX = trackX;
                midiSliderTrackW = trackW;
                needsRepaint = true;
                return;
            }

            // Fall through: drag node
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END MIDI PLAYER button handling
        // =========================================================================

        // =========================================================================
        // STEP SEQ button handling (Play/Stop, BPM drag, Sync, E editor, Delete)
        // =========================================================================
        CCStepperProcessor* ccStepper = cache ? cache->ccStepper
                                                : dynamic_cast<CCStepperProcessor*>(node->getProcessor());
        if (ccStepper)
        {
            auto stepSeqContent = nodeBounds;
            stepSeqContent.removeFromTop(Style::nodeTitleHeight);
            auto ssArea = stepSeqContent.reduced(8, 6);

            // TOP ROW hit areas (must match Paint.cpp layout exactly)
            auto ssTopRow = ssArea.removeFromTop(28);
            auto ssPlayBtnArea = ssTopRow.removeFromLeft(50).reduced(2);
            ssTopRow.removeFromLeft(6);
            auto ssBpmArea = ssTopRow.removeFromLeft(70);
            ssTopRow.removeFromLeft(4);
            auto ssSyncBtnArea = ssTopRow.removeFromLeft(32).reduced(1, 3);

            // Bottom buttons (E + X)
            float ssBtnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float ssBtnX = nodeBounds.getX() + Style::bottomBtnMargin;
            auto ssEditRect = juce::Rectangle<float>(ssBtnX, ssBtnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            ssBtnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto ssDeleteRect = juce::Rectangle<float>(ssBtnX, ssBtnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (ssEditRect.contains(pos))    { launchTip("E - Open CC Step Sequencer editor"); return; }
                if (ssDeleteRect.contains(pos))  { launchTip("X - Delete this Step Sequencer module"); return; }
                if (ssPlayBtnArea.contains(pos)) { launchTip("Play/Stop - Toggle sequencer playback"); return; }
                if (ssBpmArea.contains(pos))     { launchTip("BPM - Drag up/down to set manual tempo"); return; }
                if (ssSyncBtnArea.contains(pos)) { launchTip("SYNC - Sync tempo to master clock"); return; }
                return;
            }

            // FIX: Check bottom buttons BEFORE other areas
            if (ssEditRect.contains(pos))
            {
                showStepSeqEditor(node);
                return;
            }

            if (ssDeleteRect.contains(pos))
            {
                ccStepper->stop();
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // PLAY/STOP
            if (ssPlayBtnArea.contains(pos))
            {
                ccStepper->togglePlayStop();
                needsRepaint = true;
                return;
            }

            // SYNC button
            if (ssSyncBtnArea.contains(pos))
            {
                ccStepper->setSyncToMasterBpm(!ccStepper->isSyncToMasterBpm());
                needsRepaint = true;
                return;
            }

            // BPM drag start (only if not synced to master)
            if (ssBpmArea.contains(pos) && !ccStepper->isSyncToMasterBpm())
            {
                stepSeqBpmDragging = true;
                stepSeqBpmDragProcessor = ccStepper;
                stepSeqBpmDragStartY = pos.y;
                stepSeqBpmDragStartValue = ccStepper->getBpm();
                needsRepaint = true;
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END STEP SEQ button handling
        // =========================================================================

        // =========================================================================
        // TRANSIENT SPLITTER button handling (E editor + X delete)
        // =========================================================================
        TransientSplitterProcessor* transientSplitter = cache ? cache->transientSplitter
                                                : dynamic_cast<TransientSplitterProcessor*>(node->getProcessor());
        if (transientSplitter)
        {
            float tsBtnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float tsBtnX = nodeBounds.getX() + Style::bottomBtnMargin;
            auto tsEditRect = juce::Rectangle<float>(tsBtnX, tsBtnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            tsBtnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto tsDeleteRect = juce::Rectangle<float>(tsBtnX, tsBtnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };
                if (tsEditRect.contains(pos))   { launchTip("E - Open Transient Splitter editor"); return; }
                if (tsDeleteRect.contains(pos))  { launchTip("X - Delete this Transient Splitter"); return; }
                return;
            }

            if (tsEditRect.contains(pos))
            {
                showTransientSplitterEditor(transientSplitter);
                return;
            }

            if (tsDeleteRect.contains(pos))
            {
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END TRANSIENT SPLITTER button handling
        // =========================================================================

        // =========================================================================
        // MIDI MULTI FILTER button handling (E, P, X buttons)
        // =========================================================================
        MidiMultiFilterProcessor* midiMultiFilter = dynamic_cast<MidiMultiFilterProcessor*>(node->getProcessor());
        if (midiMultiFilter)
        {
            nodeBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            auto mmfEditRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto mmfPassRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto mmfDeleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (mmfEditRect.contains(pos))   { launchTip("E - Open MIDI Multi Filter editor"); return; }
                if (mmfPassRect.contains(pos))    { launchTip("P - Pass-through (bypass all filtering, yellow when active)"); return; }
                if (mmfDeleteRect.contains(pos))  { launchTip("X - Delete this MIDI Multi Filter"); return; }
                return;
            }

            if (mmfEditRect.contains(pos))
            {
                showMidiMultiFilterEditor(node);
                return;
            }

            if (mmfPassRect.contains(pos))
            {
                midiMultiFilter->passThrough = !midiMultiFilter->passThrough;
                needsRepaint = true;
                repaint();
                return;
            }

            if (mmfDeleteRect.contains(pos))
            {
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END MIDI MULTI FILTER button handling
        // =========================================================================

        // =========================================================================
        // LATCHER button handling (E editor, X delete, All Off, pad clicks)
        // =========================================================================
        LatcherProcessor* latcher = dynamic_cast<LatcherProcessor*>(node->getProcessor());
        if (latcher)
        {
            auto latcherContent = nodeBounds;
            latcherContent.removeFromTop(Style::nodeTitleHeight);
            auto lArea = latcherContent.reduced(8, 6);

            // "All Off" button — upper right of content area
            float allOffW = 42.0f;
            float allOffH = 16.0f;
            auto allOffRect = juce::Rectangle<float>(
                lArea.getRight() - allOffW, lArea.getY(), allOffW, allOffH);

            // Grid area (below All Off, above bottom buttons)
            auto lGridArea = lArea;
            lGridArea.removeFromTop(allOffH + 2.0f);
            lGridArea.removeFromBottom(Style::bottomBtnHeight + Style::bottomBtnMargin + 2);

            // Bottom buttons (E + X)
            float lBtnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float lBtnX = nodeBounds.getX() + Style::bottomBtnMargin;
            auto lEditRect = juce::Rectangle<float>(lBtnX, lBtnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            lBtnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto lDeleteRect = juce::Rectangle<float>(lBtnX, lBtnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (allOffRect.contains(pos))   { launchTip("All Off - Release all latched pads"); return; }
                if (lEditRect.contains(pos))     { launchTip("E - Open Latcher pad editor"); return; }
                if (lDeleteRect.contains(pos))   { launchTip("X - Delete this Latcher module"); return; }
                if (lGridArea.contains(pos))     { launchTip("Click pads to toggle latch on/off"); return; }
                return;
            }

            // All Off button
            if (allOffRect.contains(pos))
            {
                latcher->allNotesOff();
                needsRepaint = true;
                repaint();
                return;
            }

            // Check bottom buttons BEFORE grid
            if (lEditRect.contains(pos))
            {
                showLatcherEditor(node);
                return;
            }

            if (lDeleteRect.contains(pos))
            {
                latcher->allNotesOff();
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }

            // PAD GRID clicks - toggle pads directly from the node
            if (lGridArea.contains(pos))
            {
                float padW = lGridArea.getWidth() / 4.0f;
                float padH = lGridArea.getHeight() / 4.0f;

                int col = (int)((pos.x - lGridArea.getX()) / padW);
                int row = (int)((pos.y - lGridArea.getY()) / padH);
                col = juce::jlimit(0, 3, col);
                row = juce::jlimit(0, 3, row);
                int padIndex = row * 4 + col;

                latcher->togglePadFromUI(padIndex);
                needsRepaint = true;
                repaint();
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END LATCHER button handling
        // =========================================================================

        // =========================================================================
        // REGULAR PLUGIN NODE button handling (E, CH, M, P, X buttons)
        // Right-click = tooltip only, Left-click = action
        // =========================================================================
        if (!(isAudioInput || isAudioOutput))
        {
            // Calculate all button areas first
            auto buttonBounds = nodeBounds;
            buttonBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = buttonBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            // Build button rectangles based on which buttons exist
            juce::Rectangle<float> editRect, chRect, muteRect, passRect, transportRect, loadRect, deleteRect;
            bool hasEditBtn = meteringProc && meteringProc->hasEditor();
            bool hasChBtn = isInstrument && meteringProc;
            bool hasPassBtn = !isInstrument && meteringProc;
            bool hasTransportBtn = (meteringProc != nullptr);
            bool hasLoadBtn = meteringProc && meteringProc->isVST2();

            if (hasEditBtn)
            {
                editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            }

            if (hasChBtn)
            {
                chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            }

            muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

            if (hasPassBtn)
            {
                passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            }

            if (hasTransportBtn)
            {
                transportRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            }

            if (hasLoadBtn)
            {
                loadRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            }

            deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            // Right-click: Show tooltips only
            if (e.mods.isRightButtonDown())
            {

                if (hasEditBtn && editRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("E - Open plugin editor window", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (hasChBtn && chRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("CH - MIDI channel filter (select which channels this instrument receives)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (muteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("M - Mute/Bypass plugin (yellow when muted)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (hasPassBtn && passRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("P - Pass-through mode (bypass processing, pass audio through)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (hasTransportBtn && transportRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("T - Per-plugin transport override (custom tempo/time signature)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (hasLoadBtn && loadRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("L - Load/Replace VST2 plugin from file", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                if (deleteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("X - Delete plugin from rack", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }

                // Right-click elsewhere on plugin node - no action (tooltip-only policy)
                return;
            }

            // Left-click: Handle button actions
            if (hasEditBtn && editRect.contains(pos))
            {
                openPluginWindow(node);
                return;
            }

            if (hasChBtn && chRect.contains(pos))
            {
                showMidiChannelFilter(node);
                return;
            }

            if (muteRect.contains(pos))
            {
                toggleBypassOnGraph(getActiveGraph(), node->nodeID);
                needsRepaint = true;
                return;
            }

            if (hasPassBtn && passRect.contains(pos))
            {
                meteringProc->togglePassThrough();
                needsRepaint = true;
                return;
            }

            if (hasTransportBtn && transportRect.contains(pos))
            {
                showTransportOverride(node);
                return;
            }

            if (hasLoadBtn && loadRect.contains(pos))
            {
                reloadVST2Node(node);
                return;
            }

            if (deleteRect.contains(pos))
            {
                // CRITICAL FIX: Close window and disconnect wires BEFORE removing node
                auto nodeID = node->nodeID;

                // Step 1: Close the plugin window if it exists
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                {
                    activePluginWindows.erase(windowIt);
                }

                // Step 2: Disconnect all wires connected to this node
                disconnectNode(node);

                // Step 3: Now safely remove the node
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
                return;
            }
        }

        // =========================================================================
        // AUDIO I/O NODE button handling (ON/OFF toggle + +/- when inside container)
        // Right-click = tooltip only, Left-click = action
        // =========================================================================
        if (isAudioInput || isAudioOutput)
        {
            auto ioBounds = nodeBounds;
            ioBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = ioBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            auto toggleRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth * 1.5f, Style::bottomBtnHeight);

            // +/- bus buttons (only inside container, audio nodes only)
            float pmBtnW = Style::bottomBtnWidth;
            float pmBtnH = Style::bottomBtnHeight;
            float addX   = btnX + Style::bottomBtnWidth * 1.5f + Style::bottomBtnSpacing * 2.0f;
            auto addRect = juce::Rectangle<float>(addX, btnY, pmBtnW, pmBtnH);
            auto subRect = juce::Rectangle<float>(addX + pmBtnW + Style::bottomBtnSpacing, btnY, pmBtnW, pmBtnH);

            // Right-click: Show tooltip only
            if (e.mods.isRightButtonDown())
            {
                if (toggleRect.contains(pos))
                {
                    juce::String tooltipText = isAudioInput
                        ? "ON/OFF - Enable/Disable audio input from hardware"
                        : "ON/OFF - Enable/Disable audio output to hardware";
                    auto* tooltip = new StatusToolTip(tooltipText, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                if (isInsideContainer() && addRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("+ Add a stereo channel pair to this I/O", true);
                    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10, screenClickPos.y + 10, 1, 1), nullptr);
                    return;
                }
                if (isInsideContainer() && subRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("- Remove last channel pair from this I/O", true);
                    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10, screenClickPos.y + 10, 1, 1), nullptr);
                    return;
                }
                // Right-click elsewhere on I/O node - no action
                return;
            }

            // Left-click: Toggle ON/OFF
            if (toggleRect.contains(pos))
            {
                toggleBypassOnGraph(getActiveGraph(), node->nodeID);
                needsRepaint = true;
                return;
            }

            // Left-click: +/- bus buttons inside container
            if (isInsideContainer() && !containerStack.empty())
            {
                auto* containerProc = containerStack.back();

                if (addRect.contains(pos))
                {
                    processor.suspendProcessing(true);
                    if (isAudioInput)
                        containerProc->addInputBus();
                    else
                        containerProc->addOutputBus();
                    processor.suspendProcessing(false);
                    rebuildNodeTypeCache();
                    needsRepaint = true;
                    return;
                }

                if (subRect.contains(pos))
                {
                    processor.suspendProcessing(true);
                    if (isAudioInput)
                    {
                        int n = containerProc->getNumContainerInputBuses();
                        if (n > 1) containerProc->removeInputBus(n - 1);
                    }
                    else
                    {
                        int n = containerProc->getNumContainerOutputBuses();
                        if (n > 1) containerProc->removeOutputBus(n - 1);
                    }
                    processor.suspendProcessing(false);
                    rebuildNodeTypeCache();
                    needsRepaint = true;
                    return;
                }
            }
        }

        // Dragging logic
        if (!e.mods.isRightButtonDown())
        {
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>(
                (float)node->properties["x"],
                (float)node->properties["y"]);

            // CRITICAL FIX: Start high-frequency timer ONCE when drag begins
            startTimer(MouseInteractionTimerID, 16);  // 60 Hz for smooth dragging
        }
    }
    else if (e.mods.isRightButtonDown())
    {
        // FIX #3: Store right-click position for new node placement
        lastRightClickPos = pos;

        // Empty canvas - show add plugin menu
        showPluginMenu();
    }
    else if (e.mods.isLeftButtonDown())
    {
        // Left click on empty canvas space — start drag-to-pan
        isPanning = true;
        panMouseScreenStart = e.getScreenPosition();
        panStartOffsetX = panOffsetX;
        panStartOffsetY = panOffsetY;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

void GraphCanvas::showNodeContextMenu(juce::AudioProcessorGraph::Node* node, juce::Point<float> /*pos*/)
{
    auto* cache = getCachedNodeType(node->nodeID);
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
    ContainerProcessor* containerProc = cache ? cache->container : dynamic_cast<ContainerProcessor*>(node->getProcessor());

    // Check if this is an I/O node
    bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
    bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());
    bool isMidiInput   = cache ? cache->isMidiInput   : (node == processor.midiInputNode.get());
    bool isMidiOutput  = cache ? cache->isMidiOutput  : (node == processor.midiOutputNode.get());
    bool isIONode = isAudioInput || isAudioOutput || isMidiInput || isMidiOutput;

    juce::PopupMenu menu;

    // Only show "Open Editor" for regular plugins with editors
    if (!isIONode && meteringProc && meteringProc->hasEditor())
    {
        menu.addItem(1, "Open Editor");
        menu.addSeparator();
    }

    // Don't show mute/bypass for I/O nodes
    if (!isIONode)
    {
        menu.addItem(2, "Mute/Bypass");
        menu.addSeparator();
    }

    menu.addItem(3, "Disconnect All Wires");
    menu.addSeparator();

    // Container-specific menu items
    if (containerProc)
    {
        menu.addItem(20, "Dive Into Container");
        menu.addItem(21, "Rename Container...");
        menu.addSeparator();
        menu.addItem(22, "Save Container Preset...");
        menu.addItem(23, "Load Container Preset...");
        menu.addSeparator();
    }

    menu.addItem(10, "Delete");

    // Keep file chooser alive for async callbacks
    auto fileChooserPtr = std::make_shared<std::unique_ptr<juce::FileChooser>>();

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, node, meteringProc, containerProc, fileChooserPtr](int result)
        {
            if (result == 1 && meteringProc)
            {
                openPluginWindow(node);
            }
            else if (result == 2)
            {
                toggleBypassOnGraph(getActiveGraph(), node->nodeID);
                needsRepaint = true;
            }
            else if (result == 3)
            {
                // Disconnect all wires connected to this node
                disconnectNode(node);
            }
            else if (result == 10)
            {
                // CRITICAL FIX: Close window and disconnect wires BEFORE removing node
                auto nodeID = node->nodeID;

                // Step 1: Close the plugin window if it exists
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                {
                    activePluginWindows.erase(windowIt);
                }

                // Step 2: Disconnect all wires connected to this node
                disconnectNode(node);

                // Step 3: Now safely remove the node
                removeNodeFromGraph(processor, getActiveGraph(), nodeID, isInsideContainer());
                updateParentSelector();
                markDirty();
            }
            // === Container operations ===
            else if (result == 20 && containerProc)
            {
                diveIntoContainer(containerProc);
            }
            else if (result == 21 && containerProc)
            {
                auto currentName = containerProc->getContainerName();
                auto* alertWindow = new juce::AlertWindow("Rename Container",
                    "Enter a new name for this container:", juce::MessageBoxIconType::QuestionIcon);
                alertWindow->addTextEditor("name", currentName, "Name:");
                alertWindow->addButton("OK", 1);
                alertWindow->addButton("Cancel", 0);

                alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
                    [containerProc, alertWindow, this](int result) {
                        if (result == 1)
                        {
                            auto newName = alertWindow->getTextEditorContents("name");
                            if (newName.isNotEmpty())
                            {
                                containerProc->setContainerName(newName);
                                rebuildNodeTypeCache();
                                markDirty();
                            }
                        }
                        delete alertWindow;
                    }), true);
            }
            else if (result == 22 && containerProc)
            {
                auto defaultFolder = ContainerProcessor::getEffectiveDefaultFolder();
                defaultFolder.createDirectory();

                *fileChooserPtr = std::make_unique<juce::FileChooser>(
                    "Save Container Preset",
                    defaultFolder.getChildFile(containerProc->getContainerName() + ".container"),
                    "*.container");

                (*fileChooserPtr)->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [containerProc](const juce::FileChooser& fc) {
                        auto file = fc.getResult();
                        if (file != juce::File{})
                            containerProc->savePreset(file);
                    });
            }
            else if (result == 23 && containerProc)
            {
                auto defaultFolder = ContainerProcessor::getEffectiveDefaultFolder();

                *fileChooserPtr = std::make_unique<juce::FileChooser>(
                    "Load Container Preset",
                    defaultFolder,
                    "*.container");

                (*fileChooserPtr)->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [containerProc, this](const juce::FileChooser& fc) {
                        auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            containerProc->loadPreset(file);
                            rebuildNodeTypeCache();
                            markDirty();
                        }
                    });
            }
        });
}

void GraphCanvas::disconnectNode(juce::AudioProcessorGraph::Node* node)
{
    auto* activeGraph = getActiveGraph();
    if (!node || !activeGraph) return;

    // Get all connections
    auto connections = activeGraph->getConnections();

    // Find and remove all connections involving this node
    for (const auto& conn : connections)
    {
        if (conn.source.nodeID == node->nodeID || conn.destination.nodeID == node->nodeID)
        {
            activeGraph->removeConnection(conn);
        }
    }

    markDirty();
}

void GraphCanvas::mouseMove(const juce::MouseEvent& e)
{
    highlightPin = findPinAt(toVirtual(e.position));
    hoveredConnection = getConnectionAt(toVirtual(e.position));
    // Don't force repaint here - timerCallback will check if state changed
}

// =============================================================================
// Magnetic Snap: Align dragged node with connected neighbors
// Snaps to horizontal (Y-align) or vertical (X-align edge-to-edge) when close.
// Hold Shift during drag to temporarily disable snapping.
// =============================================================================
void GraphCanvas::applyMagneticSnap(juce::AudioProcessorGraph::Node* draggedNode, float& x, float& y)
{
    auto* ag = getActiveGraph();
    if (!draggedNode || !ag) return;

    auto draggedBounds = getNodeBounds(draggedNode);
    float dragW = draggedBounds.getWidth();
    float dragH = draggedBounds.getHeight();

    bool snappedX = false;
    bool snappedY = false;
    float bestSnapDistX = magneticSnapThreshold + 1.0f;
    float bestSnapDistY = magneticSnapThreshold + 1.0f;
    float snapX = x;
    float snapY = y;

    // Collect connected node IDs
    auto nodeID = draggedNode->nodeID;

    for (auto& conn : ag->getConnections())
    {
        juce::AudioProcessorGraph::NodeID otherID;

        if (conn.source.nodeID == nodeID)
            otherID = conn.destination.nodeID;
        else if (conn.destination.nodeID == nodeID)
            otherID = conn.source.nodeID;
        else
            continue;

        auto* otherNode = ag->getNodeForId(otherID);
        if (!otherNode) continue;

        auto otherBounds = getNodeBounds(otherNode);
        float ox = otherBounds.getX();
        float oy = otherBounds.getY();
        float ow = otherBounds.getWidth();
        float oh = otherBounds.getHeight();

        // --- Horizontal alignment (snap Y): top-to-top ---
        float distTopTop = std::abs(y - oy);
        if (distTopTop < bestSnapDistY && distTopTop < magneticSnapThreshold) {
            bestSnapDistY = distTopTop;
            snapY = oy;
            snappedY = true;
        }

        // --- Horizontal alignment: center-to-center Y ---
        float dragCenterY = y + dragH * 0.5f;
        float otherCenterY = oy + oh * 0.5f;
        float distCenterY = std::abs(dragCenterY - otherCenterY);
        if (distCenterY < bestSnapDistY && distCenterY < magneticSnapThreshold) {
            bestSnapDistY = distCenterY;
            snapY = otherCenterY - dragH * 0.5f;
            snappedY = true;
        }

        // --- Vertical snap: right edge of dragged → left edge of other (flow: left→right) ---
        float dragRight = x + dragW;
        float gap1 = std::abs(dragRight - ox);             // snug against left
        float gap2 = std::abs(dragRight + 10.0f - ox);     // 10px gap

        if (gap1 < bestSnapDistX && gap1 < magneticSnapThreshold) {
            bestSnapDistX = gap1;
            snapX = ox - dragW;
            snappedX = true;
        }
        if (gap2 < bestSnapDistX && gap2 < magneticSnapThreshold) {
            bestSnapDistX = gap2;
            snapX = ox - dragW - 10.0f;
            snappedX = true;
        }

        // --- Vertical snap: left edge of dragged → right edge of other (flow: right→left) ---
        float otherRight = ox + ow;
        float gap3 = std::abs(x - otherRight);
        float gap4 = std::abs(x - (otherRight + 10.0f));

        if (gap3 < bestSnapDistX && gap3 < magneticSnapThreshold) {
            bestSnapDistX = gap3;
            snapX = otherRight;
            snappedX = true;
        }
        if (gap4 < bestSnapDistX && gap4 < magneticSnapThreshold) {
            bestSnapDistX = gap4;
            snapX = otherRight + 10.0f;
            snappedX = true;
        }

        // --- Vertical alignment: left-to-left (stacking) ---
        float distLeftLeft = std::abs(x - ox);
        if (distLeftLeft < bestSnapDistX && distLeftLeft < magneticSnapThreshold) {
            bestSnapDistX = distLeftLeft;
            snapX = ox;
            snappedX = true;
        }
    }

    if (snappedX) x = snapX;
    if (snappedY) y = snapY;
}

void GraphCanvas::mouseDrag(const juce::MouseEvent& e)
{
    // =========================================================================
    // NEW: Minimap drag — navigate by dragging viewport in minimap
    // =========================================================================
    if (isDraggingMinimap)
    {
        navigateMinimapTo(e.position);
        return;
    }

    // =========================================================================
    // NEW: Scrollbar drag
    // =========================================================================
    if (isDraggingHScrollbar)
    {
        float visW = getVisibleWidth();
        float maxPan = virtualCanvasWidth - visW;
        if (maxPan > 0.0f)
        {
            auto hBar = getHScrollbarRect();
            float thumbRatio = visW / virtualCanvasWidth;
            float thumbW = juce::jmax(20.0f, hBar.getWidth() * thumbRatio);
            float trackRange = hBar.getWidth() - thumbW;

            float delta = e.position.x - scrollbarDragStartOffset;
            float panDelta = (delta / trackRange) * maxPan;
            panOffsetX = juce::jlimit(0.0f, maxPan, scrollbarDragStartPan + panDelta);
            repaint();
        }
        return;
    }

    if (isDraggingVScrollbar)
    {
        float visH = getVisibleHeight();
        float maxPan = virtualCanvasHeight - visH;
        if (maxPan > 0.0f)
        {
            auto vBar = getVScrollbarRect();
            float thumbRatio = visH / virtualCanvasHeight;
            float thumbH = juce::jmax(20.0f, vBar.getHeight() * thumbRatio);
            float trackRange = vBar.getHeight() - thumbH;

            float delta = e.position.y - scrollbarDragStartOffset;
            float panDelta = (delta / trackRange) * maxPan;
            panOffsetY = juce::jlimit(0.0f, maxPan, scrollbarDragStartPan + panDelta);
            repaint();
        }
        return;
    }

    // Drag-to-pan: move view by screen delta
    if (isPanning)
    {
        auto screenDelta = e.getScreenPosition() - panMouseScreenStart;
        panOffsetX = panStartOffsetX - (float)screenDelta.x / zoomLevel;
        panOffsetY = panStartOffsetY - (float)screenDelta.y / zoomLevel;
        clampPanOffset();
        repaint();
        return;
    }

    if (midiSliderDragging && midiSliderDragPlayer)
    {
        auto pos = toVirtual(e.position);
        float normalised = juce::jlimit(0.0f, 1.0f, (pos.x - midiSliderTrackX) / midiSliderTrackW);
        midiSliderDragPlayer->setPositionNormalized((double)normalised);
        needsRepaint = true;
        return;
    }

    if (midiBpmDragging && midiBpmDragPlayer)
    {
        float deltaY = midiBpmDragStartY - e.position.y;  // Drag up = increase
        double newBpm = midiBpmDragStartValue + deltaY * 0.5;  // 0.5 BPM per pixel
        newBpm = juce::jlimit(20.0, 400.0, newBpm);
        midiBpmDragPlayer->setTempoOverrideBpm(newBpm);
        needsRepaint = true;
        return;
    }

    // Step Seq BPM drag
    if (stepSeqBpmDragging && stepSeqBpmDragProcessor)
    {
        float deltaY = stepSeqBpmDragStartY - e.position.y;
        double newBpm = stepSeqBpmDragStartValue + deltaY * 0.5;
        stepSeqBpmDragProcessor->setBpm(newBpm);
        needsRepaint = true;
        return;
    }

    if (dragCable.active)
    {
        dragCable.currentDragPos = toVirtual(e.position);
        highlightPin = findPinAt(toVirtual(e.position));
        needsRepaint = true;
        // CRITICAL FIX: DO NOT restart timer here! It's already running at 60Hz from mouseDown
        // Restarting hundreds of times causes high CPU usage
    }
    else if (draggingNodeID.uid != 0)
    {
        if (auto* node = getActiveGraph() ? getActiveGraph()->getNodeForId(draggingNodeID) : nullptr)
        {
            auto p = toVirtual(e.position) - nodeDragOffset;
            float clampedX = juce::jmax(10.0f, juce::jmin(p.x, virtualCanvasWidth  - Style::nodeWidth  - 10.0f));
            float clampedY = juce::jmax(10.0f, juce::jmin(p.y, virtualCanvasHeight - Style::nodeHeight - 10.0f));

            // Magnetic snap: align with connected nodes when close
            if (!e.mods.isShiftDown())  // Hold Shift to disable snap
                applyMagneticSnap(node, clampedX, clampedY);

            node->properties.set("x", clampedX);
            node->properties.set("y", clampedY);
            needsRepaint = true;
            // CRITICAL FIX: DO NOT restart timer here! It's already running at 60Hz from mouseDown
            // Restarting hundreds of times causes high CPU usage
        }
    }
}

void GraphCanvas::mouseUp(const juce::MouseEvent& e)
{
    // =========================================================================
    // NEW: End minimap / scrollbar drag
    // =========================================================================
    if (isDraggingMinimap)
    {
        isDraggingMinimap = false;
        return;
    }
    if (isDraggingHScrollbar)
    {
        isDraggingHScrollbar = false;
        repaint();
        return;
    }
    if (isDraggingVScrollbar)
    {
        isDraggingVScrollbar = false;
        repaint();
        return;
    }

    // End drag-to-pan
    if (isPanning)
    {
        isPanning = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    if (midiSliderDragging)
    {
        midiSliderDragging = false;
        midiSliderDragPlayer = nullptr;
        needsRepaint = true;
    }

    if (midiBpmDragging)
    {
        midiBpmDragging = false;
        midiBpmDragPlayer = nullptr;
        needsRepaint = true;
    }

    if (stepSeqBpmDragging)
    {
        stepSeqBpmDragging = false;
        stepSeqBpmDragProcessor = nullptr;
        needsRepaint = true;
    }

    if (dragCable.active)
    {
        auto hovered = findPinAt(toVirtual(e.position));
        if (hovered.isValid() && canConnect(dragCable.sourcePin, hovered))
            createConnection(dragCable.sourcePin, hovered);

        dragCable.active = false;
        needsRepaint = true;
    }

    draggingNodeID = juce::AudioProcessorGraph::NodeID();

    // CRITICAL FIX: Return to main timer when drag ends (managed by MainTimerID)
    // Note: MainTimerID timer is always running, no need to restart
    // MouseInteractionTimerID will automatically be managed by timerCallback
    stopTimer(MouseInteractionTimerID);
}

void GraphCanvas::mouseDoubleClick(const juce::MouseEvent& e)
{
    // FIX: Only open plugin window on actual double-click, not while dragging or on pins
    // Prevent spurious opens when hovering/dragging
    if (dragCable.active || draggingNodeID.uid != 0)
        return;

    // Don't open if double-clicking on pins or connections
    if (findPinAt(toVirtual(e.position)).isValid())
        return;

    if (getConnectionAt(toVirtual(e.position)).source.nodeID.uid != 0)
        return;

    // FIX: Don't open plugin windows for I/O nodes (Audio In/Out, MIDI In/Out)
    // Double-clicking these nodes should do nothing - prevents them from disappearing
    if (auto* node = findNodeAt(toVirtual(e.position)))
    {
        auto* cache = getCachedNodeType(node->nodeID);
        bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
        bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());
        bool isMidiInput   = cache ? cache->isMidiInput   : (node == processor.midiInputNode.get());
        bool isMidiOutput  = cache ? cache->isMidiOutput  : (node == processor.midiOutputNode.get());

        // Exclude I/O nodes from double-click plugin window opening
        if (isAudioInput || isAudioOutput || isMidiInput || isMidiOutput)
            return;

        // Exclude system tool nodes (they have their own UI, not plugin editors)
        auto* proc = node->getProcessor();
        if (dynamic_cast<SimpleConnectorProcessor*>(proc)
            || dynamic_cast<StereoMeterProcessor*>(proc)
            || dynamic_cast<MidiMonitorProcessor*>(proc)
            || dynamic_cast<RecorderProcessor*>(proc)
            || dynamic_cast<ManualSamplerProcessor*>(proc)
            || dynamic_cast<AutoSamplerProcessor*>(proc)
            || dynamic_cast<MidiPlayerProcessor*>(proc)
            || dynamic_cast<CCStepperProcessor*>(proc)
            || dynamic_cast<TransientSplitterProcessor*>(proc)
            || dynamic_cast<LatcherProcessor*>(proc))
            return;

        // Double-click container → dive into its inner graph
        if (auto* container = dynamic_cast<ContainerProcessor*>(proc))
        {
            diveIntoContainer(container);
            return;
        }

        // Safe to open plugin window for regular plugin nodes
        openPluginWindow(node);
    }
}

// MIDI Channel Filter Window - Shows channel selector UI for instruments
void GraphCanvas::showMidiChannelFilter(juce::AudioProcessorGraph::Node* node) {
    if (!node) return;

    auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor());
    if (!meteringProc) return;

    if (!meteringProc->getInnerPlugin()) return;

    // =========================================================================
    // CRITICAL FIX: Use isInstrument() instead of getPluginDescription()
    // getPluginDescription() freezes some plugins when called!
    // =========================================================================
    if (!meteringProc->isInstrument()) return;

    // Get node bounds to position popup near the CH button
    auto nodeBounds = getNodeBounds(node);
    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

    // Adjust for E button if it exists
    if (meteringProc->hasEditor()) {
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // CH button position in component coordinates
    auto chButtonCenter = juce::Point<int>((int)(btnX + Style::bottomBtnWidth / 2), (int)(btnY + Style::bottomBtnHeight / 2));

    // Use MessageManager::callAsync to ensure UI operations happen on message thread
    juce::MessageManager::callAsync([this, meteringProc, chButtonCenter]() {
        auto* selector = new MidiChannelSelector(meteringProc, [this]() {
            // Callback when mask changes - trigger repaint if needed
            needsRepaint = true;
        });

        // Convert to screen coordinates
        juce::Rectangle<int> targetArea(
            virtualToScreen((float)chButtonCenter.x, (float)chButtonCenter.y).x,
            virtualToScreen((float)chButtonCenter.x, (float)chButtonCenter.y).y,
            1, 1);

        juce::CallOutBox::launchAsynchronously(
            std::unique_ptr<juce::Component>(selector),
            targetArea,
            nullptr);
    });
}

// =========================================================================
// Transport Override popup - Shows custom tempo/time-sig controls per plugin
// =========================================================================
void GraphCanvas::showTransportOverride(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;

    auto* proc = node->getProcessor();
    auto* meteringProc = dynamic_cast<MeteringProcessor*>(proc);
    if (!meteringProc) return;

    auto* transportComp = new TransportOverrideComponent(meteringProc);

    auto nodeBounds = getNodeBounds(node);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(transportComp),
        juce::Rectangle<int>(
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getY()).y,
            1, 1),
        nullptr);
}

// =========================================================================
// Container Transport Override popup
// =========================================================================
void GraphCanvas::showContainerTransportOverride(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;

    auto* containerProc = dynamic_cast<ContainerProcessor*>(node->getProcessor());
    if (!containerProc) return;

    auto* transportComp = new ContainerTransportOverrideComponent(containerProc);

    auto nodeBounds = getNodeBounds(node);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(transportComp),
        juce::Rectangle<int>(
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getY()).y,
            1, 1),
        nullptr);
}

// =========================================================================
// VST2 default folder - reads from settings, falls back to platform default
// =========================================================================
juce::File GraphCanvas::getVST2DefaultFolder() const
{
    if (auto* settings = processor.appProperties.getUserSettings())
    {
        // Check VST2Folders setting (from Plugin Folders panel)
        juce::String savedPaths = settings->getValue("VST2Folders", "");
        if (savedPaths.isNotEmpty())
        {
            auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
            for (const auto& path : folders)
            {
                juce::File folder(path);
                if (folder.exists() && folder.isDirectory())
                    return folder;
            }
        }
    }

    // Platform defaults
    #if JUCE_WINDOWS
    juce::File defaultDir("C:\\Program Files\\VstPlugins");
    if (!defaultDir.exists())
        defaultDir = juce::File("C:\\Program Files\\Steinberg\\VstPlugins");
    if (!defaultDir.exists())
        defaultDir = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    return defaultDir;
    #elif JUCE_MAC
    return juce::File("/Library/Audio/Plug-Ins/VST");
    #else
    return juce::File("/usr/lib/vst");
    #endif
}

// =========================================================================
// MIDI Player Channel Info Popup
// Shows a table of channel numbers and their assigned instruments
// =========================================================================
void GraphCanvas::showMidiPlayerChannelInfo(MidiPlayerProcessor* midiPlayer)
{
    if (!midiPlayer) return;

    auto& table = midiPlayer->getChannelInfoTable();

    int rowH = 26;
    int headerH = 28;
    int tableW = 420;
    int rows = (int)table.size();
    if (rows == 0) rows = 1;
    int totalH = headerH + rows * rowH + 12;

    // Interactive panel with mute toggles
    struct MutePanel : public juce::Component
    {
        std::vector<MidiPlayerProcessor::ChannelInfo> info;
        MidiPlayerProcessor* player = nullptr;

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(30, 30, 38));

            int y = 4;
            int headerH = 24;
            int muteColX = 4;
            int muteColW = 30;
            int chX = muteColX + muteColW + 4;

            // Header
            g.setColour(juce::Colour(50, 50, 65));
            g.fillRect(4, y, getWidth() - 8, headerH);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText("M",          muteColX, y, muteColW, headerH, juce::Justification::centred);
            g.drawText("CH",         chX,  y, 32,  headerH, juce::Justification::centredLeft);
            g.drawText("PRG",        chX + 36, y, 40,  headerH, juce::Justification::centredLeft);
            g.drawText("BANK",       chX + 80, y, 50,  headerH, juce::Justification::centredLeft);
            g.drawText("INSTRUMENT", chX + 134, y, 160, headerH, juce::Justification::centredLeft);
            g.drawText("NOTES",      chX + 302, y, 50, headerH, juce::Justification::centredRight);

            y += headerH;

            if (info.empty())
            {
                g.setColour(juce::Colours::grey);
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::italic)));
                g.drawText("No instruments found", 0, y, getWidth(), 26, juce::Justification::centred);
                return;
            }

            int rowH = 26;
            for (int i = 0; i < (int)info.size(); i++)
            {
                auto& ch = info[i];
                bool muted = player ? player->isChannelMuted(ch.channel) : false;

                // Alternate row background
                if (i % 2 == 1) {
                    g.setColour(juce::Colour(38, 38, 48));
                    g.fillRect(4, y, getWidth() - 8, rowH);
                }

                // Muted row overlay
                if (muted) {
                    g.setColour(juce::Colour(60, 20, 20).withAlpha(0.3f));
                    g.fillRect(4, y, getWidth() - 8, rowH);
                }

                // Mute toggle button
                auto muteRect = juce::Rectangle<int>(muteColX + 4, y + 3, muteColW - 8, rowH - 6);
                g.setColour(muted ? juce::Colour(180, 50, 50) : juce::Colour(50, 70, 50));
                g.fillRoundedRectangle(muteRect.toFloat(), 3.0f);
                g.setColour(muted ? juce::Colours::white : juce::Colour(80, 180, 80));
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText(muted ? "M" : "", muteRect, juce::Justification::centred);
                if (!muted) {
                    // Draw small speaker icon / active indicator
                    g.setColour(juce::Colour(80, 180, 80));
                    auto dot = muteRect.toFloat().withSizeKeepingCentre(6, 6);
                    g.fillEllipse(dot);
                }

                // Channel number
                bool isDrums = (ch.channel == 9);
                g.setColour(muted ? juce::Colour(100, 60, 60) : (isDrums ? juce::Colour(255, 160, 40) : juce::Colour(80, 200, 120)));
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
                g.drawText(juce::String(ch.channel + 1), chX, y, 32, rowH, juce::Justification::centredLeft);

                // Program number
                g.setColour(muted ? juce::Colour(100, 100, 110) : juce::Colour(180, 180, 200));
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                if (ch.program >= 0)
                    g.drawText(juce::String(ch.program), chX + 36, y, 40, rowH, juce::Justification::centredLeft);
                else
                    g.drawText("--", chX + 36, y, 40, rowH, juce::Justification::centredLeft);

                // Bank
                if (ch.bankMSB > 0 || ch.bankLSB > 0)
                    g.drawText(juce::String(ch.bankMSB) + "/" + juce::String(ch.bankLSB),
                               chX + 80, y, 50, rowH, juce::Justification::centredLeft);
                else
                    g.drawText("0/0", chX + 80, y, 50, rowH, juce::Justification::centredLeft);

                // Instrument name
                g.setColour(muted ? juce::Colour(120, 100, 100) : (isDrums ? juce::Colour(255, 200, 120) : juce::Colours::white));
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                g.drawText(ch.instrumentName, chX + 134, y, 160, rowH, juce::Justification::centredLeft, true);

                // Note count
                g.setColour(muted ? juce::Colour(80, 80, 90) : juce::Colour(140, 140, 160));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.drawText(juce::String(ch.noteCount), chX + 302, y, 50, rowH, juce::Justification::centredRight);

                y += rowH;
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (!player || info.empty()) return;

            int headerH = 24;
            int rowH = 26;
            int muteColX = 4;
            int muteColW = 30;

            int clickY = (int)e.position.y - 4 - headerH;
            if (clickY < 0) return;

            int rowIndex = clickY / rowH;
            if (rowIndex < 0 || rowIndex >= (int)info.size()) return;

            // Check if click is in mute column
            if (e.position.x >= muteColX && e.position.x <= muteColX + muteColW)
            {
                int ch = info[rowIndex].channel;
                player->setChannelMuted(ch, !player->isChannelMuted(ch));
                repaint();
            }
        }
    };

    auto* mutePanel = new MutePanel();
    mutePanel->info = table;
    mutePanel->player = midiPlayer;
    mutePanel->setSize(tableW, totalH);

    auto screenBounds = getScreenBounds();
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(mutePanel),
        juce::Rectangle<int>(screenBounds.getX() + getWidth() / 2,
                             screenBounds.getY() + getHeight() / 2, 1, 1),
        nullptr);
}

// =========================================================================
// Auto Sampler Editor popup
// =========================================================================
void GraphCanvas::showAutoSamplerEditor(AutoSamplerProcessor* autoSampler)
{
    if (!autoSampler) return;

    auto* editorComp = new AutoSamplerEditorComponent(autoSampler);

    auto screenBounds = getScreenBounds();
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(screenBounds.getX() + getWidth() / 2,
                             screenBounds.getY() + getHeight() / 2, 1, 1),
        nullptr);
}

// =========================================================================
// Load VST2 plugin from file browser → create new node at position
// =========================================================================
void GraphCanvas::loadVST2Plugin(juce::Point<float> position)
{
    #if JUCE_PLUGINHOST_VST
    auto startFolder = getVST2DefaultFolder();

    #if JUCE_WINDOWS
    juce::String filter = "*.dll";
    #elif JUCE_MAC
    juce::String filter = "*.vst";
    #else
    juce::String filter = "*.so";
    #endif

    vst2FileChooser = std::make_unique<juce::FileChooser>(
        "Select VST2 Plugin",
        startFolder,
        filter);

    juce::Component::SafePointer<GraphCanvas> safeThis(this);
    auto nodePos = position;

    vst2FileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, nodePos](const juce::FileChooser& fc)
        {
            if (!safeThis) return;

            auto result = fc.getResult();
            if (!result.exists()) return;

            // Save last browsed directory
            safeThis->lastBrowsedDirectory = result.getParentDirectory();
            safeThis->saveLastBrowsedDirectory();

            // Use VSTPluginFormat to scan this single file for descriptions
            juce::VSTPluginFormat vstFormat;
            juce::OwnedArray<juce::PluginDescription> descriptions;
            vstFormat.findAllTypesForFile(descriptions, result.getFullPathName());

            if (descriptions.size() == 0)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "VST2 Load Failed",
                    "Could not identify a valid VST2 plugin in:\n" + result.getFullPathName());
                return;
            }

            // Use first description (most plugins have exactly one)
            auto& desc = *descriptions[0];

            juce::String error;
            auto instance = safeThis->processor.formatManager.createPluginInstance(
                desc,
                safeThis->processor.getSampleRate(),
                safeThis->processor.getBlockSize(),
                error);

            if (!instance)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "VST2 Load Failed",
                    "Failed to load VST2 plugin:\n" + result.getFileName() + "\n\nError: " + error);
                return;
            }

            // Wrap in MeteringProcessor and add to active graph
            auto meteringProc = std::make_unique<MeteringProcessor>(std::move(instance));
            auto* ag1 = safeThis->getActiveGraph();
            auto nodePtr = ag1 ? ag1->addNode(std::move(meteringProc))
                               : safeThis->processor.mainGraph->addNode(std::move(meteringProc));

            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)nodePos.x);
                nodePtr->properties.set("y", (double)nodePos.y);
                nodePtr->properties.set("manuallyLoaded", true);  // FIX 1: Mark for L button
                safeThis->updateParentSelector();
                safeThis->markDirty();
            }
        });
    #endif
}

// =========================================================================
// Reload/Replace VST2 on existing node (L button)
// Creates new node at same position, deletes old node
// =========================================================================
void GraphCanvas::reloadVST2Node(juce::AudioProcessorGraph::Node* node)
{
    #if JUCE_PLUGINHOST_VST
    if (!node) return;

    auto oldNodeID = node->nodeID;
    float posX = (float)(double)node->properties["x"];
    float posY = (float)(double)node->properties["y"];

    auto startFolder = getVST2DefaultFolder();

    #if JUCE_WINDOWS
    juce::String filter = "*.dll";
    #elif JUCE_MAC
    juce::String filter = "*.vst";
    #else
    juce::String filter = "*.so";
    #endif

    vst2FileChooser = std::make_unique<juce::FileChooser>(
        "Replace VST2 Plugin",
        startFolder,
        filter);

    juce::Component::SafePointer<GraphCanvas> safeThis(this);

    vst2FileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, oldNodeID, posX, posY](const juce::FileChooser& fc)
        {
            if (!safeThis) return;

            auto result = fc.getResult();
            if (!result.exists()) return;

            // Save last browsed directory
            safeThis->lastBrowsedDirectory = result.getParentDirectory();
            safeThis->saveLastBrowsedDirectory();

            // Scan the selected file
            juce::VSTPluginFormat vstFormat;
            juce::OwnedArray<juce::PluginDescription> descriptions;
            vstFormat.findAllTypesForFile(descriptions, result.getFullPathName());

            if (descriptions.size() == 0)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "VST2 Load Failed",
                    "Could not identify a valid VST2 plugin in:\n" + result.getFullPathName());
                return;
            }

            juce::String error;
            auto instance = safeThis->processor.formatManager.createPluginInstance(
                *descriptions[0],
                safeThis->processor.getSampleRate(),
                safeThis->processor.getBlockSize(),
                error);

            if (!instance)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "VST2 Load Failed",
                    "Failed to load VST2 plugin:\n" + result.getFileName() + "\n\nError: " + error);
                return;
            }

            // Delete old node from the active graph
            auto* reloadAg = safeThis->getActiveGraph();
            if (!reloadAg) reloadAg = safeThis->processor.mainGraph.get();
            if (auto* oldNode = reloadAg->getNodeForId(oldNodeID))
            {
                // Close plugin window if exists
                auto windowIt = safeThis->activePluginWindows.find(oldNodeID);
                if (windowIt != safeThis->activePluginWindows.end())
                    safeThis->activePluginWindows.erase(windowIt);

                safeThis->disconnectNode(oldNode);
                if (safeThis->isInsideContainer())
                {
                    safeThis->processor.suspendProcessing(true);
                    reloadAg->removeNode(oldNodeID);
                    safeThis->processor.suspendProcessing(false);
                }
                else
                {
                    safeThis->processor.removeNode(oldNodeID);
                }
            }

            // Create new node at same position
            auto meteringProc = std::make_unique<MeteringProcessor>(std::move(instance));
            auto nodePtr = reloadAg->addNode(std::move(meteringProc));

            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)posX);
                nodePtr->properties.set("y", (double)posY);
                safeThis->updateParentSelector();
                safeThis->markDirty();
            }
        });
    #endif
}

// =========================================================================
// Load VST3 plugin from file browser → create new node at position
// Allows loading blacklisted or unscanned VST3 plugins manually
// =========================================================================
void GraphCanvas::loadVST3Plugin(juce::Point<float> position)
{
    #if JUCE_PLUGINHOST_VST3
    auto startFolder = getVST3DefaultFolder();

    #if JUCE_WINDOWS
    juce::String filter = "*.vst3";
    #elif JUCE_MAC
    juce::String filter = "*.vst3";
    #else
    juce::String filter = "*.vst3";
    #endif

    pluginFileChooser = std::make_unique<juce::FileChooser>(
        "Select VST3 Plugin",
        startFolder,
        filter);

    juce::Component::SafePointer<GraphCanvas> safeThis(this);
    auto nodePos = position;

    pluginFileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
        [safeThis, nodePos](const juce::FileChooser& fc)
        {
            if (!safeThis) return;

            auto result = fc.getResult();
            if (!result.exists()) return;

            // Save last browsed directory
            safeThis->lastBrowsedDirectory = result.getParentDirectory();
            safeThis->saveLastBrowsedDirectory();

            // Use VST3PluginFormat to scan this single file for descriptions
            juce::VST3PluginFormat vst3Format;
            juce::OwnedArray<juce::PluginDescription> descriptions;
            vst3Format.findAllTypesForFile(descriptions, result.getFullPathName());

            if (descriptions.size() == 0)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "VST3 Load Failed",
                    "Could not identify a valid VST3 plugin in:\n" + result.getFullPathName());
                return;
            }

            // If multiple types found (e.g. instrument + effect variants), let user choose
            juce::PluginDescription* chosen = descriptions[0];
            if (descriptions.size() > 1)
            {
                // For now, use the first one — could show a popup later
                DBG("VST3 has " + juce::String(descriptions.size()) + " types, using first: " + descriptions[0]->name);
            }

            juce::String error;
            auto instance = safeThis->processor.formatManager.createPluginInstance(
                *chosen,
                safeThis->processor.getSampleRate(),
                safeThis->processor.getBlockSize(),
                error);

            if (!instance)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "VST3 Load Failed",
                    "Failed to load VST3 plugin:\n" + result.getFileName() + "\n\nError: " + error);
                return;
            }

            // Wrap in MeteringProcessor and add to active graph
            auto meteringProc = std::make_unique<MeteringProcessor>(std::move(instance));
            auto* ag2 = safeThis->getActiveGraph();
            auto nodePtr = ag2 ? ag2->addNode(std::move(meteringProc))
                               : safeThis->processor.mainGraph->addNode(std::move(meteringProc));

            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)nodePos.x);
                nodePtr->properties.set("y", (double)nodePos.y);
                nodePtr->properties.set("manuallyLoaded", true);  // FIX 1: Mark for L button
                safeThis->updateParentSelector();
                safeThis->markDirty();
            }
        });
    #endif
}

// =========================================================================
// VST3 default folder helper
// =========================================================================
juce::File GraphCanvas::getVST3DefaultFolder() const
{
    if (auto* settings = processor.appProperties.getUserSettings())
    {
        juce::String savedPaths = settings->getValue("VST3Folders", "");
        if (savedPaths.isNotEmpty())
        {
            auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
            for (const auto& path : folders)
            {
                juce::File folder(path);
                if (folder.exists() && folder.isDirectory())
                    return folder;
            }
        }
    }

    #if JUCE_WINDOWS
    return juce::File("C:\\Program Files\\Common Files\\VST3");
    #elif JUCE_MAC
    return juce::File("/Library/Audio/Plug-Ins/VST3");
    #else
    return juce::File("/usr/lib/vst3");
    #endif
}

// =============================================================================
// Step Seq Editor Popup
// =============================================================================
void GraphCanvas::showStepSeqEditor(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;

    auto* ccStepper = dynamic_cast<CCStepperProcessor*>(node->getProcessor());
    if (!ccStepper) return;

    auto* editorComp = new CCStepperEditorComponent(ccStepper);

    auto nodeBounds = getNodeBounds(node);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getY()).y,
            1, 1),
        nullptr);
}

void GraphCanvas::showTransientSplitterEditor(TransientSplitterProcessor* proc)
{
    if (!proc) return;

    auto* editorComp = new TransientSplitterEditorComponent(proc);

    // Find the node for positioning
    auto* ag = getActiveGraph();
    if (!ag) return;
    for (auto* node : ag->getNodes())
    {
        if (node->getProcessor() == proc)
        {
            auto nodeBounds = getNodeBounds(node);

            juce::CallOutBox::launchAsynchronously(
                std::unique_ptr<juce::Component>(editorComp),
                juce::Rectangle<int>(
                    virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
                    virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getY()).y,
                    1, 1),
                nullptr);
            return;
        }
    }

    // Fallback: launch centered
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(getScreenBounds().getCentreX(), getScreenBounds().getCentreY(), 1, 1),
        nullptr);
}

void GraphCanvas::showMidiMultiFilterEditor(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;
    auto* mmf = dynamic_cast<MidiMultiFilterProcessor*>(node->getProcessor());
    if (!mmf) return;

    auto* editorComp = mmf->createEditor();
    auto nodeBounds = getNodeBounds(node);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).y,
            1, 1),
        nullptr);
}

void GraphCanvas::showLatcherEditor(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;
    auto* latcher = dynamic_cast<LatcherProcessor*>(node->getProcessor());
    if (!latcher) return;

    auto* editorComp = new LatcherEditorComponent(latcher);
    auto nodeBounds = getNodeBounds(node);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).y,
            1, 1),
        nullptr);
}

// =============================================================================
// NEW: Mouse Wheel — scroll canvas or zoom
// Ctrl+Wheel = zoom, Shift+Wheel = horizontal scroll, plain = vertical scroll
// =============================================================================
void GraphCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl + wheel = zoom (centered on mouse position)
    if (e.mods.isCtrlDown())
    {
        float zoomDelta = wheel.deltaY * 0.15f;
        float newZoom = juce::jlimit(0.10f, 2.0f, zoomLevel + zoomDelta);

        if (std::abs(newZoom - zoomLevel) > 0.001f)
        {
            // Zoom toward mouse position: keep the virtual point under cursor fixed
            auto mouseVirtual = toVirtual(e.position);

            zoomLevel = newZoom;
            // NOTE: No setTransform() — zoom is paint-level only

            // Adjust pan so mouseVirtual stays under cursor
            // virtual = pixel/zoom + pan → pan = virtual - pixel/zoom
            panOffsetX = mouseVirtual.x - e.position.x / zoomLevel;
            panOffsetY = mouseVirtual.y - e.position.y / zoomLevel;
            clampPanOffset();

            repaint();
            if (auto* parent = getParentComponent())
                parent->repaint();
        }
        return;
    }

    // Scroll speed in virtual canvas units
    float scrollSpeed = 120.0f;

    if (e.mods.isShiftDown())
    {
        // Shift + wheel = horizontal scroll
        panOffsetX -= wheel.deltaY * scrollSpeed;
        clampPanOffset();
        repaint();
    }
    else
    {
        // Plain wheel = vertical scroll (deltaX for horizontal if trackpad)
        panOffsetY -= wheel.deltaY * scrollSpeed;
        panOffsetX -= wheel.deltaX * scrollSpeed;
        clampPanOffset();
        repaint();
    }
}

// =============================================================================
// NEW: Navigate minimap — set pan so viewport centers on clicked point
// =============================================================================
void GraphCanvas::navigateMinimapTo(juce::Point<float> localClickPos)
{
    auto mmRect = getMinimapRect();

    // Convert click position to virtual canvas position
    float relX = (localClickPos.x - mmRect.getX()) / mmRect.getWidth();
    float relY = (localClickPos.y - mmRect.getY()) / mmRect.getHeight();

    relX = juce::jlimit(0.0f, 1.0f, relX);
    relY = juce::jlimit(0.0f, 1.0f, relY);

    float targetVirtualX = relX * virtualCanvasWidth;
    float targetVirtualY = relY * virtualCanvasHeight;

    // Center the viewport on the clicked position
    float visW = getVisibleWidth();
    float visH = getVisibleHeight();

    panOffsetX = targetVirtualX - visW * 0.5f;
    panOffsetY = targetVirtualY - visH * 0.5f;
    clampPanOffset();

    repaint();
}
