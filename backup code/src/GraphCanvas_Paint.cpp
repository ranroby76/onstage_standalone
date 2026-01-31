// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Paint.cpp

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"

void GraphCanvas::paint(juce::Graphics& g)
{
    g.fillAll(Style::colBackground);

    // Simplified grid - draw fewer lines for better performance
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    const int gridSize = 40; // Larger grid = fewer lines to draw
    for (int x = 0; x < getWidth(); x += gridSize)
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += gridSize)
        g.drawHorizontalLine(y, 0.0f, (float)getWidth());

    if (!processor.mainGraph) return;
    verifyPositions();

    // Ensure cache is valid
    if (nodeTypeCache.empty() && processor.mainGraph->getNumNodes() > 0)
        const_cast<GraphCanvas*>(this)->rebuildNodeTypeCache();

    // Draw connections
    for (auto& connection : processor.mainGraph->getConnections())
    {
        auto* srcNode = processor.mainGraph->getNodeForId(connection.source.nodeID);
        auto* dstNode = processor.mainGraph->getNodeForId(connection.destination.nodeID);
        if (srcNode && dstNode && shouldShowNode(srcNode) && shouldShowNode(dstNode))
        {
            bool isMidi = connection.source.isMIDI();
            PinID srcPin = { srcNode->nodeID, isMidi ? 0 : connection.source.channelIndex, false, isMidi };
            PinID dstPin = { dstNode->nodeID, isMidi ? 0 : connection.destination.channelIndex, true,  isMidi };

            auto start = getPinPos(srcNode, srcPin);
            auto end = getPinPos(dstNode, dstPin);

            juce::Colour wireColor = juce::Colours::darkgrey;
            float thickness = 2.0f;
            bool isHovered = (connection == hoveredConnection);
            bool hasSignal = false;

            if (!srcNode->isBypassed() && !dstNode->isBypassed())
            {
                auto* srcCache = getCachedNodeType(srcNode->nodeID);

                if (isMidi)
                {
                    if (srcCache && srcCache->meteringProc)
                    {
                        hasSignal = srcCache->meteringProc->isMidiOutActive();
                    }
                    else if (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(srcNode->getProcessor()))
                    {
                        hasSignal = processor.mainMidiInFlash.load();
                    }
                }
                else
                {
                    int ch = connection.source.channelIndex;
                    
                    // Only check signal for I/O nodes - MeteringProcessor RMS was removed
                    if (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(srcNode->getProcessor()))
                    {
                        if (ch < 2) hasSignal = (processor.mainInputRms[ch].load() > 0.001f);
                    }
                    // For plugin-to-plugin audio connections, always show active color (no RMS available)
                    else if (srcCache && srcCache->meteringProc)
                    {
                        hasSignal = true; // Assume signal present when not bypassed
                    }
                }

                // FIX: Determine wire color based on whether it's actually a sidechain connection
                // Check if destination channel is in sidechain range (2-3) for correct color
                bool isSidechainConnection = false;
                if (!isMidi && connection.destination.channelIndex >= 2)
                {
                    auto* dstCache = getCachedNodeType(dstNode->nodeID);
                    MeteringProcessor* meteringProc = dstCache ? dstCache->meteringProc 
                                                                : dynamic_cast<MeteringProcessor*>(dstNode->getProcessor());
                    if (meteringProc && meteringProc->hasSidechain())
                    {
                        isSidechainConnection = true;
                    }
                }
                
                wireColor = isMidi ? Style::colPinMidi 
                                   : (isSidechainConnection ? Style::colPinSidechain : Style::colPinAudio);
                thickness = 2.5f;

                if (hasSignal)
                {
                    wireColor = wireColor.brighter(0.5f);
                    thickness = 3.5f;
                }
            }

            if (isHovered)
            {
                wireColor = wireColor.brighter(0.5f);
                thickness = 3.5f;
            }

            drawWire(g, start, end, wireColor, thickness);
        }
    }

    // Draw active drag cable
    if (dragCable.active)
    {
        drawWire(g, getPinCenterPos(dragCable.sourcePin), dragCable.currentDragPos, dragCable.dragColor, 2.5f);
    }

    // Draw nodes
    for (auto* node : processor.mainGraph->getNodes())
    {
        if (!shouldShowNode(node)) continue;
        auto bounds = getNodeBounds(node);
        bool selected = false;

        auto* cache = getCachedNodeType(node->nodeID);
        MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
        // FIX: NEVER call getPluginDescription() - it freezes some plugins
        bool isInstrument = cache && cache->isInstrument;

        bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
        bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());
        bool isMidiInput   = cache ? cache->isMidiInput   : (node == processor.midiInputNode.get());
        bool isMidiOutput  = cache ? cache->isMidiOutput  : (node == processor.midiOutputNode.get());

        // Main body - use darker background if bypassed
        juce::Colour bodyCol = node->isBypassed() ? Style::colNodeBodyBypassed : Style::colNodeBody;
        g.setColour(bodyCol);
        g.fillRoundedRectangle(bounds.toFloat(), Style::nodeRounding);

        // Border
        juce::Colour borderCol = selected ? juce::Colours::yellow 
                                         : (node->isBypassed() ? juce::Colours::grey.darker() : Style::colNodeBorder);
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds.toFloat(), Style::nodeRounding, 2.0f);

        // Title bar
        auto titleBounds = bounds.removeFromTop(Style::nodeTitleHeight);
        g.setColour(node->isBypassed() ? Style::colNodeTitleBypassed : Style::colNodeTitle);
        g.fillRoundedRectangle(titleBounds.toFloat(), Style::nodeRounding);
        g.fillRect(titleBounds.removeFromBottom(Style::nodeRounding).toFloat());

        // Title text
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        
        // FREEZE FIX: Use cached name - some plugins freeze when getName() is called!
        juce::String title = cache ? cache->pluginName : "Unknown";
        
        g.drawText(title, titleBounds.reduced(5, 0), juce::Justification::centredLeft, true);
        
        // Get processor for visualization checks
        auto* proc = node->getProcessor();

        // =====================================================================
        // STEREO METER VISUALIZATION - DO NOT DELETE!
        // =====================================================================
        // StereoMeter shows 2 vertical meter bars (L/R) that display audio levels:
        // - GREEN: 0% to 75% (safe levels)
        // - YELLOW: 75% to 90% (approaching hot)
        // - RED: 90% to 100% (hot/clipping)
        // Input comes from BLUE audio pins (stereo input only, NO outputs)
        // Meter consumes audio signal (does not pass through)
        // =====================================================================
        if (auto* stereoMeter = dynamic_cast<StereoMeterProcessor*>(proc))
        {
            // Get current levels (0.0 to 1.0+, thread-safe atomics)
            float leftLevel = stereoMeter->getLeftLevel();
            float rightLevel = stereoMeter->getRightLevel();
            float leftPeak = stereoMeter->getLeftPeak();
            float rightPeak = stereoMeter->getRightPeak();
            
            // Clamp to 0-1 range for display (anything above 1.0 is clipping)
            leftLevel = juce::jlimit(0.0f, 1.0f, leftLevel);
            rightLevel = juce::jlimit(0.0f, 1.0f, rightLevel);
            leftPeak = juce::jlimit(0.0f, 1.0f, leftPeak);
            rightPeak = juce::jlimit(0.0f, 1.0f, rightPeak);
            
            // Meter drawing area (inside node body, below title)
            auto meterArea = bounds.reduced(8, 6);
            float meterHeight = meterArea.getHeight();
            float barWidth = (meterArea.getWidth() - 6) / 2.0f; // 2 bars with 6px gap
            
            // LEFT meter bar
            auto leftBarBounds = juce::Rectangle<float>(
                meterArea.getX(), 
                meterArea.getY(), 
                barWidth, 
                meterHeight
            );
            
            // Draw LEFT meter background (dark grey)
            g.setColour(juce::Colour(30, 30, 30));
            g.fillRect(leftBarBounds);
            
            // Draw LEFT meter level with gradient
            if (leftLevel > 0.0f)
            {
                float fillHeight = leftLevel * meterHeight;
                auto leftFillBounds = juce::Rectangle<float>(
                    leftBarBounds.getX(),
                    leftBarBounds.getBottom() - fillHeight,
                    leftBarBounds.getWidth(),
                    fillHeight
                );
                
                // Color based on level: Green (0-75%), Yellow (75-90%), Red (90-100%)
                juce::Colour meterColor;
                if (leftLevel < 0.75f)
                    meterColor = juce::Colours::green;
                else if (leftLevel < 0.90f)
                    meterColor = juce::Colours::yellow;
                else
                    meterColor = juce::Colours::red;
                
                g.setColour(meterColor);
                g.fillRect(leftFillBounds);
            }
            
            // Draw LEFT peak hold line (white)
            if (leftPeak > 0.0f)
            {
                float peakY = leftBarBounds.getBottom() - (leftPeak * meterHeight);
                g.setColour(juce::Colours::white);
                g.drawLine(leftBarBounds.getX(), peakY, 
                          leftBarBounds.getRight(), peakY, 2.0f);
            }
            
            // Draw LEFT border
            g.setColour(juce::Colours::grey);
            g.drawRect(leftBarBounds, 1.0f);
            
            // RIGHT meter bar
            auto rightBarBounds = juce::Rectangle<float>(
                leftBarBounds.getRight() + 6, 
                meterArea.getY(), 
                barWidth, 
                meterHeight
            );
            
            // Draw RIGHT meter background (dark grey)
            g.setColour(juce::Colour(30, 30, 30));
            g.fillRect(rightBarBounds);
            
            // Draw RIGHT meter level with gradient
            if (rightLevel > 0.0f)
            {
                float fillHeight = rightLevel * meterHeight;
                auto rightFillBounds = juce::Rectangle<float>(
                    rightBarBounds.getX(),
                    rightBarBounds.getBottom() - fillHeight,
                    rightBarBounds.getWidth(),
                    fillHeight
                );
                
                // Color based on level: Green (0-75%), Yellow (75-90%), Red (90-100%)
                juce::Colour meterColor;
                if (rightLevel < 0.75f)
                    meterColor = juce::Colours::green;
                else if (rightLevel < 0.90f)
                    meterColor = juce::Colours::yellow;
                else
                    meterColor = juce::Colours::red;
                
                g.setColour(meterColor);
                g.fillRect(rightFillBounds);
            }
            
            // Draw RIGHT peak hold line (white)
            if (rightPeak > 0.0f)
            {
                float peakY = rightBarBounds.getBottom() - (rightPeak * meterHeight);
                g.setColour(juce::Colours::white);
                g.drawLine(rightBarBounds.getX(), peakY, 
                          rightBarBounds.getRight(), peakY, 2.0f);
            }
            
            // Draw RIGHT border
            g.setColour(juce::Colours::grey);
            g.drawRect(rightBarBounds, 1.0f);
            
            // Draw L/R labels at bottom
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("L", leftBarBounds.removeFromBottom(12), juce::Justification::centred);
            g.drawText("R", rightBarBounds.removeFromBottom(12), juce::Justification::centred);
            
            // Draw CLIP indicators (red LEDs at top if clipping detected)
            if (stereoMeter->isLeftClipping())
            {
                auto leftClipLED = juce::Rectangle<float>(
                    leftBarBounds.getX() + 2, 
                    leftBarBounds.getY() + 2, 
                    leftBarBounds.getWidth() - 4, 
                    8
                );
                g.setColour(juce::Colours::red.brighter());
                g.fillEllipse(leftClipLED);
            }
            
            if (stereoMeter->isRightClipping())
            {
                auto rightClipLED = juce::Rectangle<float>(
                    rightBarBounds.getX() + 2, 
                    rightBarBounds.getY() + 2, 
                    rightBarBounds.getWidth() - 4, 
                    8
                );
                g.setColour(juce::Colours::red.brighter());
                g.fillEllipse(rightClipLED);
            }
        }
        // =====================================================================
        // END STEREO METER VISUALIZATION
        // =====================================================================
        
        // =====================================================================
        // MIDI MONITOR VISUALIZATION - DO NOT DELETE!
        // =====================================================================
        // MIDI Monitor shows real-time MIDI activity for up to 16 channels:
        // FIX: Simple numeric format: "144, 1, 60, 100" (status, ch, note, vel)
        // Each MIDI channel gets its own horizontal line
        // Module is 6x taller and 2x wider for display space
        // MIDI input only (RED pins), no outputs, MIDI consumed for display
        // =====================================================================
        if (auto* midiMonitor = dynamic_cast<MidiMonitorProcessor*>(proc))
        {
            // Get all MIDI events (array of 16, one per channel)
            auto events = midiMonitor->getMidiEvents();
            
            // Display area (inside node body, below title)
            auto displayArea = bounds.reduced(6, 4);
            
            // Text settings
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
            float lineHeight = 16.0f;
            float yPos = displayArea.getY();
            
            // Draw background (dark grey)
            g.setColour(juce::Colour(25, 25, 25));
            g.fillRect(displayArea);
            
            // Border
            g.setColour(juce::Colours::grey.darker());
            g.drawRect(displayArea, 1.0f);
            
            bool anyActivity = false;
            
            // Draw each channel's MIDI event (skip inactive channels)
            for (int ch = 0; ch < 16; ++ch)
            {
                const auto& event = events[ch];
                
                if (event.isActive)
                {
                    anyActivity = true;
                    
                    // FIX #4: Bright sample-and-hold colors - NO fade-out
                    // Events stay visible until next message on that channel
                    juce::Colour textColor;
                    if (event.isNoteOn)
                        textColor = juce::Colour(0, 255, 100);  // Bright green for Note ON
                    else
                        textColor = juce::Colour(255, 150, 0);  // Bright orange for Note OFF
                    
                    g.setColour(textColor);
                    
                    // FIX: Simple numeric format: "144, 1, 60, 100"
                    juce::String text = event.toString();
                    
                    // Draw text line
                    auto textBounds = juce::Rectangle<float>(
                        displayArea.getX() + 4,
                        yPos,
                        displayArea.getWidth() - 8,
                        lineHeight
                    );
                    g.drawText(text, textBounds, juce::Justification::centredLeft, true);
                    
                    yPos += lineHeight;
                    
                    // Don't overflow display area
                    if (yPos + lineHeight > displayArea.getBottom())
                        break;
                }
            }
            
            // If no activity, show "Waiting for MIDI..."
            if (!anyActivity)
            {
                g.setColour(juce::Colours::grey);
                g.setFont(juce::Font(12.0f, juce::Font::italic));
                g.drawText("Waiting for MIDI...", displayArea, juce::Justification::centred);
            }
        }
        // =====================================================================
        // END MIDI MONITOR VISUALIZATION
        // =====================================================================


        // Draw pins
        drawNodePins(g, node);

        // Draw buttons (only for non-I/O nodes)
        if (!(isAudioInput || isAudioOutput || isMidiInput || isMidiOutput))
        {
            drawNodeButtons(g, node);
        }
        
        // Draw ON/OFF toggle for Audio I/O nodes
        if (isAudioInput || isAudioOutput)
        {
            drawAudioIOToggle(g, node);
        }
    }
}

void GraphCanvas::drawWire(juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, juce::Colour col, float thickness)
{
    juce::Path p;
    p.startNewSubPath(start);
    
    // Use VERTICAL bezier curves for TOP/BOTTOM pin layout (matches getConnectionAt)
    p.cubicTo(start.x, start.y + 50, 
              end.x, end.y - 50, 
              end.x, end.y);

    g.setColour(col);
    g.strokePath(p, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Rectangle<float> GraphCanvas::getPinBounds(const PinInfo& pin, juce::AudioProcessorGraph::Node* node) const
{
    if (!node) return {};
    
    auto* proc = node->getProcessor();
    if (!proc) return {};
    
    auto nodeBounds = getNodeBounds(node);
    
    // FIXED: Instruments have NO audio inputs - only MIDI input
    // Use isInstrument() getter to avoid getPluginDescription() freeze
    int numIn = 0;
    if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
        if (mp->getInnerPlugin() && !mp->isInstrument()) {
            numIn = proc->getTotalNumInputChannels();
        }
    } else {
        numIn = proc->getTotalNumInputChannels();
    }
    
    int numOut = proc->getTotalNumOutputChannels();
    bool acceptsMidi = proc->acceptsMidi();
    bool producesMidi = proc->producesMidi();
    
    int totalInputs = numIn + (acceptsMidi ? 1 : 0);
    int totalOutputs = numOut + (producesMidi ? 1 : 0);
    
    int pinIdx = pin.channelIndex;
    if (pin.isMidi) pinIdx = pin.isInput ? numIn : numOut;
    
    int totalPins = pin.isInput ? totalInputs : totalOutputs;
    if (totalPins == 0) return {};
    
    // Calculate horizontal spacing for pins across top/bottom
    float spacing = Style::minPinSpacing;
    float totalWidth = spacing * (totalPins + 1);
    float startX = nodeBounds.getCentreX() - totalWidth / 2.0f;
    float x = startX + spacing * (float)(pinIdx + 1);
    
    // Position on TOP (inputs) or BOTTOM (outputs)
    float y = pin.isInput ? (nodeBounds.getY() - Style::hookLength) 
                          : (nodeBounds.getBottom() + Style::hookLength);
    
    return juce::Rectangle<float>(x - Style::pinSize/2, y - Style::pinSize/2, Style::pinSize, Style::pinSize);
}

juce::Point<float> GraphCanvas::getPinPos(juce::AudioProcessorGraph::Node* node, const PinID& pinID) const
{
    return getPinBounds({ pinID.nodeID, pinID.pinIndex, pinID.isInput, pinID.isMidi }, node).getCentre();
}

juce::Point<float> GraphCanvas::getPinCenterPos(const PinID& pinID) const
{
    auto* node = processor.mainGraph->getNodeForId(pinID.nodeID);
    if (!node) return {};
    return getPinPos(node, pinID);
}

void GraphCanvas::drawNodePins(juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto* proc = node->getProcessor();
    if (!proc) return;
    
    // FIXED: Instruments have NO audio inputs - use isInstrument() getter
    int numIn = 0;
    if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
        if (mp->getInnerPlugin() && !mp->isInstrument()) {
            numIn = proc->getTotalNumInputChannels();
        }
    } else {
        numIn = proc->getTotalNumInputChannels();
    }
    
    int numOut = proc->getTotalNumOutputChannels();
    bool acceptsMidi = proc->acceptsMidi();
    bool producesMidi = proc->producesMidi();
    
    auto cache = getCachedNodeType(node->nodeID);
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(proc);
    
    // Draw input pins (SKIP for instruments!)
    for (int i = 0; i < numIn; ++i) {
        bool isMidi = false;
        PinInfo pinInfo = { node->nodeID, i, true, isMidi };
        auto pinBounds = getPinBounds(pinInfo, node);
        if (pinBounds.isEmpty()) continue;
        
        bool highlighted = (highlightPin == PinID{ node->nodeID, i, true, isMidi });
        
        // Determine pin color based on sidechain
        juce::Colour pinColor = Style::colPinAudio;
        if (meteringProc && meteringProc->hasSidechain() && i >= 2) {
            pinColor = Style::colPinSidechain;
        }
        
        bool hasSignal = meteringProc && !node->isBypassed() && (meteringProc->getOutputRms(i) > 0.001f);
        if (hasSignal) {
            pinColor = pinColor.brighter(0.5f);
        }
        
        if (highlighted) {
            g.setColour(pinColor.brighter(0.5f));
            g.fillEllipse(pinBounds.expanded(2.0f));
        }
        
        g.setColour(pinColor);
        g.fillEllipse(pinBounds);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(pinBounds, 1.0f);
    }
    
    // MIDI input pin
    if (acceptsMidi) {
        bool isMidi = true;
        PinInfo pinInfo = { node->nodeID, 0, true, isMidi };
        auto pinBounds = getPinBounds(pinInfo, node);
        if (!pinBounds.isEmpty()) {
            bool highlighted = (highlightPin == PinID{ node->nodeID, 0, true, isMidi });
            
            juce::Colour pinColor = Style::colPinMidi;
            bool hasSignal = meteringProc && !node->isBypassed() && meteringProc->isMidiInActive();
            if (hasSignal) {
                pinColor = pinColor.brighter(0.5f);
            }
            
            if (highlighted) {
                g.setColour(pinColor.brighter(0.5f));
                g.fillEllipse(pinBounds.expanded(2.0f));
            }
            
            g.setColour(pinColor);
            g.fillEllipse(pinBounds);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawEllipse(pinBounds, 1.0f);
        }
    }
    
    // Draw output pins
    for (int i = 0; i < numOut; ++i) {
        bool isMidi = false;
        PinInfo pinInfo = { node->nodeID, i, false, isMidi };
        auto pinBounds = getPinBounds(pinInfo, node);
        if (pinBounds.isEmpty()) continue;
        
        bool highlighted = (highlightPin == PinID{ node->nodeID, i, false, isMidi });
        
        juce::Colour pinColor = Style::colPinAudio;
        bool hasSignal = meteringProc && !node->isBypassed() && (meteringProc->getOutputRms(i) > 0.001f);
        if (hasSignal) {
            pinColor = pinColor.brighter(0.5f);
        }
        
        if (highlighted) {
            g.setColour(pinColor.brighter(0.5f));
            g.fillEllipse(pinBounds.expanded(2.0f));
        }
        
        g.setColour(pinColor);
        g.fillEllipse(pinBounds);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(pinBounds, 1.0f);
    }
    
    // MIDI output pin
    if (producesMidi) {
        bool isMidi = true;
        PinInfo pinInfo = { node->nodeID, 0, false, isMidi };
        auto pinBounds = getPinBounds(pinInfo, node);
        if (!pinBounds.isEmpty()) {
            bool highlighted = (highlightPin == PinID{ node->nodeID, 0, false, isMidi });
            
            juce::Colour pinColor = Style::colPinMidi;
            bool hasSignal = meteringProc && !node->isBypassed() && meteringProc->isMidiOutActive();
            if (hasSignal) {
                pinColor = pinColor.brighter(0.5f);
            }
            
            if (highlighted) {
                g.setColour(pinColor.brighter(0.5f));
                g.fillEllipse(pinBounds.expanded(2.0f));
            }
            
            g.setColour(pinColor);
            g.fillEllipse(pinBounds);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawEllipse(pinBounds, 1.0f);
        }
    }
}

void GraphCanvas::drawNodeButtons(juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto nodeBounds = getNodeBounds(node);
    auto* cache = getCachedNodeType(node->nodeID);
    auto* proc = node->getProcessor();
    
    // Check if this is a SimpleConnector (system tool)
    SimpleConnectorProcessor* simpleConnector = cache ? cache->simpleConnector 
                                                       : dynamic_cast<SimpleConnectorProcessor*>(proc);
    
    // FIX: Check if this is a StereoMeter or MIDIMonitor (system tools with no audio output)
    StereoMeterProcessor* stereoMeter = cache ? cache->stereoMeter 
                                               : dynamic_cast<StereoMeterProcessor*>(proc);
    MidiMonitorProcessor* midiMonitor = cache ? cache->midiMonitor
                                               : dynamic_cast<MidiMonitorProcessor*>(proc);
    
    if (simpleConnector)
    {
        // =====================================================================
        // SIMPLE CONNECTOR BUTTONS - Mute/Delete only
        // =====================================================================
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        // M button (Mute)
        auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(simpleConnector->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("M", muteRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        
        // X button (Delete)
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // FIX: Stereo Meter and MIDI Monitor - X button ONLY (no M button)
    // These modules don't produce audio, so mute makes no sense
    if (stereoMeter || midiMonitor)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        // X button (Delete) - ONLY button for these modules
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // Regular MeteringProcessor buttons (plugins)
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(proc);
        // FIX: NEVER call getPluginDescription() - it freezes some plugins
        bool isInstrument = cache && cache->isInstrument;

    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

    // E button (only if has editor)
    if (meteringProc && meteringProc->hasEditor())
    {
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // CH button (instruments only) - Shows selected channel number
    if (isInstrument && meteringProc)
    {
        auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::orange.darker());
        g.fillRoundedRectangle(chRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        
        // Get selected channel from mask (single channel mode)
        int mask = meteringProc->getMidiChannelMask();
        juce::String text = "CH";
        if (mask != 0) {
            // Find which channel is selected (single bit set)
            for (int i = 0; i < 16; ++i) {
                if ((mask >> i) & 1) {
                    text = juce::String(i + 1);  // Show channel number 1-16
                    break;
                }
            }
        }
        
        g.drawText(text, chRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // M button (mute/bypass)
    auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(node->isBypassed() ? juce::Colours::red : juce::Colours::lightgreen);
    g.fillRoundedRectangle(muteRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("M", muteRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    // P button (effects only - pass-through)
    if (!isInstrument && meteringProc)
    {
        bool passThrough = meteringProc->isPassThrough();
        auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(passThrough ? juce::Colours::yellow : juce::Colours::grey.darker());
        g.fillRoundedRectangle(passRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("P", passRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // X button (delete)
    auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(juce::Colours::darkred);
    g.fillRoundedRectangle(deleteRect, 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("X", deleteRect, juce::Justification::centred);
}

void GraphCanvas::drawAudioIOToggle(juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto nodeBounds = getNodeBounds(node);
    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

    // ON/OFF toggle button
    auto toggleRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth * 1.5f, Style::bottomBtnHeight);
    g.setColour(node->isBypassed() ? juce::Colours::grey.darker() : juce::Colours::green);
    g.fillRoundedRectangle(toggleRect, 3.0f);
    
    // FIX: White text for better visibility on green/grey background
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(node->isBypassed() ? "OFF" : "ON", toggleRect, juce::Justification::centred);
}
