// =============================================================================
// FIXED BUTTON HANDLING in GraphCanvas::mouseDown() - GraphCanvas_Mouse.cpp
// FIX: Add handling for StereoMeter and MIDI Monitor (X button only, no M button)
// Insert this code AFTER the SimpleConnector handling and BEFORE the regular button handling
// =============================================================================

// After line ~1809 (after SimpleConnector handling), ADD THIS:

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
                startTimer(16);
                return;
            }
        }

// This section replaces or augments the existing button handling code
// The key change is checking for stereoMeter || midiMonitor BEFORE processing regular plugin buttons
