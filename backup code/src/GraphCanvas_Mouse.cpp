// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Mouse.cpp
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!
// FIX: Updated all startTimer calls for MultiTimer API (timerID, intervalMs)

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
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
            if (muteRect.contains(pos))
            {
                simpleConnector->toggleMute();
                needsRepaint = true;
                return;
            }
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            
            // X button (Delete)
            auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
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
            
            // Right-click: Show tooltip
            if (e.mods.isRightButtonDown())
            {
                if (deleteRect.contains(pos))
                {
                    auto screenBounds = getScreenBounds();
                    auto* tooltip = new StatusToolTip("X - Delete Module", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(
                            screenBounds.getX() + (int)pos.x + 10, 
                            screenBounds.getY() + (int)pos.y + 10, 
                            1, 1),
                        nullptr);
                    return;
                }
                // If right-click on node but not on button, show context menu
                showNodeContextMenu(node, pos);
                return;
            }
            else
            {
                // Left-click: Delete
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
        }

        // FIX 3 & FIX 4: Audio I/O nodes only have ON/OFF button, no regular buttons
        if (!(isAudioInput || isAudioOutput))
        {
            // Button checks
            nodeBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            // FIX 4: Right-click on buttons shows tooltips instead of triggering actions
            if (e.mods.isRightButtonDown())
            {
                // FIX 1: Convert component coordinates to screen coordinates for tooltips
                auto screenBounds = getScreenBounds();
                
                // E button tooltip
                if (meteringProc && meteringProc->hasEditor())
                {
                    auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                    if (editRect.contains(pos))
                    {
                        auto* tooltip = new StatusToolTip("E - Open Plugin Editor", true);
                        juce::CallOutBox::launchAsynchronously(
                            std::unique_ptr<juce::Component>(tooltip),
                            juce::Rectangle<int>(
                                screenBounds.getX() + (int)pos.x + 10, 
                                screenBounds.getY() + (int)pos.y + 10, 
                                1, 1),
                            nullptr);
                        return;
                    }
                    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
                }

                // CH button tooltip (instruments only)
                if (isInstrument && meteringProc)
                {
                    auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                    if (chRect.contains(pos))
                    {
                        auto* tooltip = new StatusToolTip("CH - MIDI Channel Filter", true);
                        juce::CallOutBox::launchAsynchronously(
                            std::unique_ptr<juce::Component>(tooltip),
                            juce::Rectangle<int>(
                                screenBounds.getX() + (int)pos.x + 10, 
                                screenBounds.getY() + (int)pos.y + 10, 
                                1, 1),
                            nullptr);
                        return;
                    }
                    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
                }

                // M button tooltip
                auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                if (muteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("M - Mute/Bypass Plugin", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(
                            screenBounds.getX() + (int)pos.x + 10, 
                            screenBounds.getY() + (int)pos.y + 10, 
                            1, 1),
                            nullptr);
                    return;
                }
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

                // P button tooltip (effects only)
                if (!isInstrument && meteringProc)
                {
                    auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                    if (passRect.contains(pos))
                    {
                        auto* tooltip = new StatusToolTip("P - Pass-Through (Dry/Wet)", true);
                        juce::CallOutBox::launchAsynchronously(
                            std::unique_ptr<juce::Component>(tooltip),
                            juce::Rectangle<int>(
                                screenBounds.getX() + (int)pos.x + 10, 
                                screenBounds.getY() + (int)pos.y + 10, 
                                1, 1),
                            nullptr);
                        return;
                    }
                    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
                }

                // X button tooltip
                auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                if (deleteRect.contains(pos))
                {
                    auto* tooltip = new StatusToolTip("X - Delete Plugin", true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(
                            screenBounds.getX() + (int)pos.x + 10, 
                            screenBounds.getY() + (int)pos.y + 10, 
                            1, 1),
                            nullptr);
                    return;
                }
                
                // If right-click on node but not on button, show context menu
                showNodeContextMenu(node, pos);
                return;
            }
            else
            {
                // LEFT-CLICK: Execute button actions
                // E button
                if (meteringProc && meteringProc->hasEditor())
                {
                    auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                    if (editRect.contains(pos))
                    {
                        openPluginWindow(node);
                        return;
                    }
                    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
                }

                // CH button (MIDI channel filter - instruments only)
                if (isInstrument && meteringProc)
                {
                    auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                    if (chRect.contains(pos))
                    {
                        showMidiChannelFilter(node);
                        return;
                    }
                    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
                }

                // M button
                auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                if (muteRect.contains(pos))
                {
                    processor.toggleBypass(node->nodeID);
                    needsRepaint = true;
                    return;
                }
                btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

                // P button (pass-through - effects only)
                if (!isInstrument && meteringProc)
                {
                    auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
                    if (passRect.contains(pos))
                    {
                        meteringProc->togglePassThrough();
                        needsRepaint = true;
                        return;
                    }
                    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
                }

                // X button (delete)
                auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
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
        }

        // FIX 3: Audio I/O node ON/OFF toggle button
        if (isAudioInput || isAudioOutput)
        {
            nodeBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            auto toggleRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth * 1.5f, Style::bottomBtnHeight);
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
