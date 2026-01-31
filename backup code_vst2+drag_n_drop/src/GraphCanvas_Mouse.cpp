// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Mouse.cpp
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!
// FIX: Updated all startTimer calls for MultiTimer API (timerID, intervalMs)
// FIX: Added RecorderProcessor button handling
// FIX: Fixed recorder button hit areas, added folder button

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
#include "MidiSelectors.h"

void GraphCanvas::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.position;

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
        dragCable.dragColor = getPinColor(pinAtPos, processor.mainGraph->getNodeForId(pinAtPos.nodeID));
        
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
            processor.mainGraph->removeConnection(connAtPos);
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
            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this, nodeID](int result)
                {
                    if (result == 1)
                    {
                        // Disconnect all wires from this node
                        if (auto* targetNode = processor.mainGraph->getNodeForId(nodeID))
                        {
                            disconnectNode(targetNode);
                            markDirty();
                            needsRepaint = true;
                        }
                    }
                    else if (result == 2)
                    {
                        // Delete node
                        if (auto* targetNode = processor.mainGraph->getNodeForId(nodeID))
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
                            
                            // Remove node
                            processor.removeNode(nodeID);
                            updateParentSelector();
                            markDirty();
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
                auto screenBounds = getScreenBounds();
                
                if (muteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("M - Mute/Unmute audio output", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                if (deleteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("X - Delete this connector module", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                if (knobArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Volume - Drag up/down to adjust gain (-inf to +25dB)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
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
                processor.removeNode(nodeID);
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
                auto screenBounds = getScreenBounds();
                
                if (deleteRect.contains(pos))
                {
                    juce::String tooltipText = stereoMeter ? "X - Delete Stereo Meter module" 
                                                           : "X - Delete MIDI Monitor module";
                    auto* tooltip = new StatusToolTip(tooltipText, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
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
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
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
                processor.removeNode(nodeID);
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
                auto screenBounds = getScreenBounds();
                
                // Record button tooltip
                if (recordBtnArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Record - Start recording audio to file", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                // Stop button tooltip
                if (stopBtnArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Stop - Stop recording and save file", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                // Folder button tooltip
                if (folderBtnArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Open Folder - Open recording folder in file explorer", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                // Name box tooltip
                if (nameBoxArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Recording Name - Click to edit filename prefix", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                // Sync toggle tooltip
                if (syncArea.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("Sync Mode - When ON, all synced recorders start/stop together", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
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
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
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
                auto screenBounds = getScreenBounds();
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
                        screenBounds.getX() + (int)nameBoxArea.getCentreX(),
                        screenBounds.getY() + (int)nameBoxArea.getCentreY(),
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
                processor.removeNode(nodeID);
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
            juce::Rectangle<float> editRect, chRect, muteRect, passRect, deleteRect;
            bool hasEditBtn = meteringProc && meteringProc->hasEditor();
            bool hasChBtn = isInstrument && meteringProc;
            bool hasPassBtn = !isInstrument && meteringProc;
            
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
            
            deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            
            // Right-click: Show tooltips only
            if (e.mods.isRightButtonDown())
            {
                auto screenBounds = getScreenBounds();
                
                if (hasEditBtn && editRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("E - Open plugin editor window", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                if (hasChBtn && chRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("CH - MIDI channel filter (select which channels this instrument receives)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                if (muteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("M - Mute/Bypass plugin (yellow when muted)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                if (hasPassBtn && passRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("P - Pass-through mode (bypass processing, pass audio through)", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                if (deleteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("X - Delete plugin from rack", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
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
                processor.toggleBypass(node->nodeID);
                needsRepaint = true;
                return;
            }

            if (hasPassBtn && passRect.contains(pos))
            {
                meteringProc->togglePassThrough();
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
                processor.removeNode(nodeID);
                updateParentSelector();
                markDirty();
                return;
            }
        }

        // =========================================================================
        // AUDIO I/O NODE button handling (ON/OFF toggle)
        // Right-click = tooltip only, Left-click = action
        // =========================================================================
        if (isAudioInput || isAudioOutput)
        {
            auto ioBounds = nodeBounds;
            ioBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = ioBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            auto toggleRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth * 1.5f, Style::bottomBtnHeight);
            
            // Right-click: Show tooltip only
            if (e.mods.isRightButtonDown())
            {
                if (toggleRect.contains(pos))
                {
                    auto screenBounds = getScreenBounds();
                    juce::String tooltipText = isAudioInput 
                        ? "ON/OFF - Enable/Disable audio input from hardware"
                        : "ON/OFF - Enable/Disable audio output to hardware";
                    auto* tooltip = new StatusToolTip(tooltipText, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenBounds.getX() + (int)pos.x + 10, 
                                             screenBounds.getY() + (int)pos.y + 10, 1, 1),
                        nullptr);
                    return;
                }
                
                // Right-click elsewhere on I/O node - no action
                return;
            }
            
            // Left-click: Toggle
            if (toggleRect.contains(pos))
            {
                processor.toggleBypass(node->nodeID);
                needsRepaint = true;
                return;
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
        lastRightClickPos = e.position;
        
        // Empty canvas - show add plugin menu
        showPluginMenu();
    }
}

void GraphCanvas::showNodeContextMenu(juce::AudioProcessorGraph::Node* node, juce::Point<float> pos)
{
    auto* cache = getCachedNodeType(node->nodeID);
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
    
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
    menu.addItem(10, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options(), 
        [this, node, meteringProc](int result)
        {
            if (result == 1 && meteringProc)
            {
                openPluginWindow(node);
            }
            else if (result == 2)
            {
                processor.toggleBypass(node->nodeID);
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
                processor.removeNode(nodeID);
                updateParentSelector();
                markDirty();
            }
        });
}

void GraphCanvas::disconnectNode(juce::AudioProcessorGraph::Node* node)
{
    if (!node || !processor.mainGraph) return;
    
    // Get all connections
    auto connections = processor.mainGraph->getConnections();
    
    // Find and remove all connections involving this node
    for (const auto& conn : connections)
    {
        if (conn.source.nodeID == node->nodeID || conn.destination.nodeID == node->nodeID)
        {
            processor.mainGraph->removeConnection(conn);
        }
    }
    
    markDirty();
}

void GraphCanvas::mouseMove(const juce::MouseEvent& e)
{
    highlightPin = findPinAt(e.position);
    hoveredConnection = getConnectionAt(e.position);
    // Don't force repaint here - timerCallback will check if state changed
}

void GraphCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (dragCable.active)
    {
        dragCable.currentDragPos = e.position;
        highlightPin = findPinAt(e.position);
        needsRepaint = true;
        // CRITICAL FIX: DO NOT restart timer here! It's already running at 60Hz from mouseDown
        // Restarting hundreds of times causes high CPU usage
    }
    else if (draggingNodeID.uid != 0)
    {
        if (auto* node = processor.mainGraph->getNodeForId(draggingNodeID))
        {
            auto p = e.position - nodeDragOffset;
            float clampedX = juce::jmax(10.0f, juce::jmin(p.x, (float)getWidth()  - Style::nodeWidth  - 10.0f));
            float clampedY = juce::jmax(10.0f, juce::jmin(p.y, (float)getHeight() - Style::nodeHeight - 10.0f));
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
    if (dragCable.active)
    {
        auto hovered = findPinAt(e.position);
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
    if (findPinAt(e.position).isValid())
        return;
        
    if (getConnectionAt(e.position).source.nodeID.uid != 0)
        return;
    
    // FIX: Don't open plugin windows for I/O nodes (Audio In/Out, MIDI In/Out)
    // Double-clicking these nodes should do nothing - prevents them from disappearing
    if (auto* node = findNodeAt(e.position))
    {
        auto* cache = getCachedNodeType(node->nodeID);
        bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
        bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());
        bool isMidiInput   = cache ? cache->isMidiInput   : (node == processor.midiInputNode.get());
        bool isMidiOutput  = cache ? cache->isMidiOutput  : (node == processor.midiOutputNode.get());
        
        // Exclude I/O nodes from double-click plugin window opening
        if (isAudioInput || isAudioOutput || isMidiInput || isMidiOutput)
            return;
            
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
    auto chButtonCenter = juce::Point<int>((int)btnX + Style::bottomBtnWidth / 2, (int)btnY + Style::bottomBtnHeight / 2);
    
    // Use MessageManager::callAsync to ensure UI operations happen on message thread
    juce::MessageManager::callAsync([this, meteringProc, chButtonCenter]() {
        auto* selector = new MidiChannelSelector(meteringProc, [this]() {
            // Callback when mask changes - trigger repaint if needed
            needsRepaint = true;
        });
        
        // Convert to screen coordinates
        auto screenBounds = getScreenBounds();
        juce::Rectangle<int> targetArea(
            screenBounds.getX() + chButtonCenter.x, 
            screenBounds.getY() + chButtonCenter.y, 
            1, 1);
        
        juce::CallOutBox::launchAsynchronously(
            std::unique_ptr<juce::Component>(selector),
            targetArea,
            nullptr);
    });
}
