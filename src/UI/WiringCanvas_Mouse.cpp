// ==============================================================================
//  WiringCanvas_Mouse.cpp
//  OnStage — Mouse interaction: pin dragging, node dragging, button clicks,
//            recorder on-surface GUI interactions
// ==============================================================================

#include "WiringCanvas.h"

// ==============================================================================
//  mouseDown
// ==============================================================================

void WiringCanvas::mouseDown (const juce::MouseEvent& e)
{
    auto pos = e.position;

    // --- Check pins first (start cable drag) ---------------------------------
    auto pinAtPos = findPinAt (pos);
    if (pinAtPos.isValid())
    {
        if (e.mods.isRightButtonDown())
        {
            // Show pin name tooltip
            showPinTooltip (pinAtPos, pos);
            return;
        }

        dragCable.active    = true;
        dragCable.sourcePin = pinAtPos;
        dragCable.currentPos = pos;
        dragCable.color     = getPinColor (pinAtPos,
            stageGraph.getGraph().getNodeForId (pinAtPos.nodeID));
        startTimer (DragTimer, 16);   // 60 Hz for smooth cable dragging
        return;
    }

    // --- Check wire hit (double-click to delete, right-click for menu) -------
    auto connAtPos = getConnectionAt (pos);
    if (connAtPos.source.nodeID.uid != 0)
    {
        if (e.mods.isRightButtonDown())
        {
            showWireMenu (connAtPos, pos);
            return;
        }
        if (e.getNumberOfClicks() == 2)
        {
            deleteConnectionAt (pos);
            return;
        }
    }

    // --- Check nodes ---------------------------------------------------------
    if (auto* node = findNodeAt (pos))
    {
        auto nodeBounds = getNodeBounds (node);
        auto* cache = getCached (node->nodeID);

        // =================================================================
        // RECORDER NODE — custom click handling
        // =================================================================
        if (cache && cache->isRecorder && cache->recorder)
        {
            auto* recorder = cache->recorder;
            auto localClick = pos;  // pos is already in canvas coords

            // Reconstruct the layout areas (must match drawRecorderNode)
            auto contentArea = nodeBounds.reduced (8, 6);
            auto topRow = contentArea.removeFromTop (24);

            // --- Name textbox click → show inline editor ---------------------
            auto nameBoxArea = topRow.removeFromLeft (230).reduced (0, 1);
            if (nameBoxArea.contains (localClick))
            {
                showRecorderNameEditor (node->nodeID, recorder, nameBoxArea);
                return;
            }

            // --- Sync toggle click -------------------------------------------
            auto syncArea = topRow.removeFromRight (65);
            if (syncArea.contains (localClick))
            {
                recorder->setSyncMode (! recorder->isSyncMode());
                needsRepaint = true;
                return;
            }

            // --- Folder button click -----------------------------------------
            auto folderArea = topRow.removeFromRight (22).reduced (1);
            if (folderArea.contains (localClick))
            {
                recorder->openRecordingFolder();
                return;
            }

            contentArea.removeFromTop (4);
            auto controlRow = contentArea.removeFromTop (40);

            // --- Record button click -----------------------------------------
            auto recordBtnArea = controlRow.removeFromLeft (46).reduced (3);
            if (recordBtnArea.contains (localClick))
            {
                if (! recorder->isCurrentlyRecording())
                {
                    recorder->startRecording();
                    needsRepaint = true;
                }
                return;
            }

            controlRow.removeFromLeft (6);

            // --- Stop button click -------------------------------------------
            auto stopBtnArea = controlRow.removeFromLeft (46).reduced (3);
            if (stopBtnArea.contains (localClick))
            {
                if (recorder->isCurrentlyRecording())
                {
                    recorder->stopRecording();
                    needsRepaint = true;
                }
                return;
            }

            // Skip time display area (must match paint order)
            controlRow.removeFromLeft (10);
            controlRow.removeFromLeft (100);

            // --- Must remove meter area FIRST to match paint order -----------
            controlRow.removeFromRight (30);   // meters

            // --- X (delete) button (now in correct position) -----------------
            auto xBtnArea = controlRow.removeFromRight (22).reduced (1, 10);
            if (xBtnArea.expanded (6).contains (localClick))
            {
                auto nodeID = node->nodeID;
                stageGraph.removeNode (nodeID);
                markDirty();
                return;
            }

            // --- Right-click on recorder → context menu ----------------------
            if (e.mods.isRightButtonDown())
            {
                showNodeContextMenu (node, pos);
                return;
            }

            // --- Fall through to node dragging -------------------------------
            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float> (
                (float) node->properties["x"],
                (float) node->properties["y"]);
            startTimer (DragTimer, 16);
            return;
        }

        // =================================================================
        // STANDARD NODES — existing handling
        // =================================================================

        // --- Title bar right-click → context menu ----------------------------
        auto titleArea = juce::Rectangle<float> (
            nodeBounds.getX(), nodeBounds.getY(),
            nodeBounds.getWidth(), WiringStyle::nodeTitleHeight);

        if (e.mods.isRightButtonDown() && titleArea.contains (pos))
        {
            showNodeContextMenu (node, pos);
            return;
        }

        // --- Effect node buttons (B / E / X) ---------------------------------
        if (cache && cache->effectNode)
        {
            auto bRect = getButtonRect (nodeBounds, 0);
            auto eRect = getButtonRect (nodeBounds, 1);
            auto xRect = getButtonRect (nodeBounds, 2);

            if (bRect.contains (pos))
            {
                // Toggle bypass
                node->setBypassed (! node->isBypassed());
                needsRepaint = true;
                return;
            }
            if (eRect.contains (pos))
            {
                // Open editor window
                openEditorWindow (node);
                return;
            }
            if (xRect.contains (pos))
            {
                // Delete node
                auto nodeID = node->nodeID;
                closeEditorWindow (nodeID);
                stageGraph.removeNode (nodeID);
                markDirty();
                return;
            }
        }

        // --- I/O + Playback node ON/OFF toggle --------------------------------
        if (cache && (cache->isAudioInput || cache->isAudioOutput || cache->isPlayback))
        {
            auto nb = nodeBounds;
            nb.removeFromTop (WiringStyle::nodeTitleHeight);
            float btnY = nb.getBottom() - WiringStyle::btnMargin - WiringStyle::btnHeight;
            float btnX = nb.getX() + WiringStyle::btnMargin;
            auto toggleRect = juce::Rectangle<float> (btnX, btnY,
                WiringStyle::btnWidth * 1.5f, WiringStyle::btnHeight);

            if (toggleRect.contains (pos))
            {
                node->setBypassed (! node->isBypassed());
                needsRepaint = true;
                return;
            }
        }

        // --- Start node dragging ---------------------------------------------
        draggingNodeID = node->nodeID;
        nodeDragOffset = pos - juce::Point<float> (
            (float) node->properties["x"],
            (float) node->properties["y"]);
        startTimer (DragTimer, 16);
        return;
    }

    // --- Right-click on empty canvas → add effect menu -----------------------
    if (e.mods.isRightButtonDown())
    {
        lastRightClickPos = pos;
        showAddEffectMenu();
    }
}

// ==============================================================================
//  mouseDrag
// ==============================================================================

void WiringCanvas::mouseDrag (const juce::MouseEvent& e)
{
    auto pos = e.position;

    // --- Cable dragging ------------------------------------------------------
    if (dragCable.active)
    {
        dragCable.currentPos = pos;
        highlightPin = findPinAt (pos);
        return;
    }

    // --- Node dragging -------------------------------------------------------
    if (draggingNodeID.uid != 0)
    {
        if (auto* node = stageGraph.getGraph().getNodeForId (draggingNodeID))
        {
            auto newPos = pos - nodeDragOffset;
            node->properties.set ("x", (double) newPos.x);
            node->properties.set ("y", (double) newPos.y);
        }
    }
}

// ==============================================================================
//  mouseUp
// ==============================================================================

void WiringCanvas::mouseUp (const juce::MouseEvent&)
{
    // --- Finish cable drag → create connection if valid ----------------------
    if (dragCable.active)
    {
        if (highlightPin.isValid() && canConnect (dragCable.sourcePin, highlightPin))
            createConnection (dragCable.sourcePin, highlightPin);

        dragCable.active = false;
        highlightPin = {};
    }

    // --- Finish node drag ----------------------------------------------------
    draggingNodeID = {};

    // Stop the high-frequency drag timer
    stopTimer (DragTimer);
    markDirty();
}

// ==============================================================================
//  mouseDoubleClick — open editor on effect nodes
// ==============================================================================

void WiringCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* node = findNodeAt (e.position))
    {
        auto* cache = getCached (node->nodeID);

        // Don't open editor for recorder nodes (they have on-surface GUI)
        if (cache && cache->isRecorder)
            return;

        if (cache && cache->effectNode)
            openEditorWindow (node);
    }
}

// ==============================================================================
//  mouseMove — hover effects (highlight pins & wires)
// ==============================================================================

void WiringCanvas::mouseMove (const juce::MouseEvent& e)
{
    auto pos = e.position;

    // Pin hover
    highlightPin = findPinAt (pos);

    // Wire hover
    hoveredConnection = getConnectionAt (pos);
}
// ==============================================================================
//  Pin tooltip — right-click shows channel name
// ==============================================================================

void WiringCanvas::showPinTooltip (const PinID& pin, juce::Point<float> pos)
{
    auto* node = stageGraph.getGraph().getNodeForId (pin.nodeID);
    if (! node) return;

    auto* cache = getCached (pin.nodeID);
    if (! cache) return;

    juce::String label;

    // --- Audio Input node: show hardware input channel name -------------------
    if (cache->isAudioInput)
    {
        if (! pin.isInput && pin.pinIndex < stageGraph.inputChannelNames.size())
            label = stageGraph.inputChannelNames[pin.pinIndex];
        else
            label = "Input " + juce::String (pin.pinIndex + 1);
    }
    // --- Audio Output node: show hardware output channel name -----------------
    else if (cache->isAudioOutput)
    {
        if (pin.isInput && pin.pinIndex < stageGraph.outputChannelNames.size())
            label = stageGraph.outputChannelNames[pin.pinIndex];
        else
            label = "Output " + juce::String (pin.pinIndex + 1);
    }
    // --- Playback node -------------------------------------------------------
    else if (cache->isPlayback)
    {
        label = (pin.pinIndex == 0) ? "L" : "R";
    }
    // --- Effect nodes with sidechain -----------------------------------------
    else if (cache->hasSidechain && pin.isInput && pin.pinIndex >= 2)
    {
        label = (pin.pinIndex == 2) ? "S.C. L" : "S.C. R";
    }
    // --- Normal effect pins --------------------------------------------------
    else
    {
        int numCh = pin.isInput ? node->getProcessor()->getTotalNumInputChannels()
                                : node->getProcessor()->getTotalNumOutputChannels();
        if (numCh == 1)
            label = "Mono";
        else
            label = (pin.pinIndex == 0) ? "L" : "R";
    }

    // Show as a popup menu with a single disabled item (acts as tooltip)
    juce::PopupMenu tooltip;
    tooltip.addItem (1, label, false);  // disabled = not clickable

    auto screenPos = localPointToGlobal (pos.toInt());
    tooltip.showMenuAsync (juce::PopupMenu::Options()
        .withTargetScreenArea ({ screenPos.x - 1, screenPos.y - 1, 2, 2 }));
}
