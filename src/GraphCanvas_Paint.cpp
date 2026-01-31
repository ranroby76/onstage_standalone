// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Paint.cpp
// FIXED: Added RecorderProcessor visualization
// FIX: Added folder button to recorder, fixed layout

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"

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
                // FIX: Light purple color for wire hover (unique hover indicator)
                wireColor = juce::Colour(0xFFCC88FF);  // Light purple
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
        if (auto* stereoMeter = dynamic_cast<StereoMeterProcessor*>(proc))
        {
            float leftLevel = stereoMeter->getLeftLevel();
            float rightLevel = stereoMeter->getRightLevel();
            float leftPeak = stereoMeter->getLeftPeak();
            float rightPeak = stereoMeter->getRightPeak();
            
            leftLevel = juce::jlimit(0.0f, 1.0f, leftLevel);
            rightLevel = juce::jlimit(0.0f, 1.0f, rightLevel);
            leftPeak = juce::jlimit(0.0f, 1.0f, leftPeak);
            rightPeak = juce::jlimit(0.0f, 1.0f, rightPeak);
            
            auto meterArea = bounds.reduced(8, 6);
            float meterHeight = meterArea.getHeight();
            float barWidth = (meterArea.getWidth() - 6) / 2.0f;
            
            auto leftBarBounds = juce::Rectangle<float>(meterArea.getX(), meterArea.getY(), barWidth, meterHeight);
            
            g.setColour(juce::Colour(30, 30, 30));
            g.fillRect(leftBarBounds);
            
            if (leftLevel > 0.0f)
            {
                float fillHeight = leftLevel * meterHeight;
                auto leftFillBounds = juce::Rectangle<float>(
                    leftBarBounds.getX(), leftBarBounds.getBottom() - fillHeight, leftBarBounds.getWidth(), fillHeight);
                
                juce::Colour meterColor = leftLevel < 0.75f ? juce::Colours::green 
                                        : (leftLevel < 0.90f ? juce::Colours::yellow : juce::Colours::red);
                g.setColour(meterColor);
                g.fillRect(leftFillBounds);
            }
            
            if (leftPeak > 0.0f)
            {
                float peakY = leftBarBounds.getBottom() - (leftPeak * meterHeight);
                g.setColour(juce::Colours::white);
                g.drawLine(leftBarBounds.getX(), peakY, leftBarBounds.getRight(), peakY, 2.0f);
            }
            
            g.setColour(juce::Colours::grey);
            g.drawRect(leftBarBounds, 1.0f);
            
            auto rightBarBounds = juce::Rectangle<float>(leftBarBounds.getRight() + 6, meterArea.getY(), barWidth, meterHeight);
            
            g.setColour(juce::Colour(30, 30, 30));
            g.fillRect(rightBarBounds);
            
            if (rightLevel > 0.0f)
            {
                float fillHeight = rightLevel * meterHeight;
                auto rightFillBounds = juce::Rectangle<float>(
                    rightBarBounds.getX(), rightBarBounds.getBottom() - fillHeight, rightBarBounds.getWidth(), fillHeight);
                
                juce::Colour meterColor = rightLevel < 0.75f ? juce::Colours::green 
                                        : (rightLevel < 0.90f ? juce::Colours::yellow : juce::Colours::red);
                g.setColour(meterColor);
                g.fillRect(rightFillBounds);
            }
            
            if (rightPeak > 0.0f)
            {
                float peakY = rightBarBounds.getBottom() - (rightPeak * meterHeight);
                g.setColour(juce::Colours::white);
                g.drawLine(rightBarBounds.getX(), peakY, rightBarBounds.getRight(), peakY, 2.0f);
            }
            
            g.setColour(juce::Colours::grey);
            g.drawRect(rightBarBounds, 1.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("L", leftBarBounds.removeFromBottom(12), juce::Justification::centred);
            g.drawText("R", rightBarBounds.removeFromBottom(12), juce::Justification::centred);
            
            if (stereoMeter->isLeftClipping())
            {
                auto leftClipLED = juce::Rectangle<float>(leftBarBounds.getX() + 2, leftBarBounds.getY() + 2, leftBarBounds.getWidth() - 4, 8);
                g.setColour(juce::Colours::red.brighter());
                g.fillEllipse(leftClipLED);
            }
            
            if (stereoMeter->isRightClipping())
            {
                auto rightClipLED = juce::Rectangle<float>(rightBarBounds.getX() + 2, rightBarBounds.getY() + 2, rightBarBounds.getWidth() - 4, 8);
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
        if (auto* midiMonitor = dynamic_cast<MidiMonitorProcessor*>(proc))
        {
            auto events = midiMonitor->getMidiEvents();
            auto displayArea = bounds.reduced(6, 4);
            
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
            float lineHeight = 16.0f;
            float yPos = displayArea.getY();
            
            g.setColour(juce::Colour(25, 25, 25));
            g.fillRect(displayArea);
            
            g.setColour(juce::Colours::grey.darker());
            g.drawRect(displayArea, 1.0f);
            
            bool anyActivity = false;
            
            for (int ch = 0; ch < 16; ++ch)
            {
                const auto& event = events[ch];
                
                if (event.isActive)
                {
                    anyActivity = true;
                    
                    juce::Colour textColor = event.isNoteOn ? juce::Colour(0, 255, 100) : juce::Colour(255, 150, 0);
                    g.setColour(textColor);
                    
                    juce::String text = event.toString();
                    
                    auto textBounds = juce::Rectangle<float>(displayArea.getX() + 4, yPos, displayArea.getWidth() - 8, lineHeight);
                    g.drawText(text, textBounds, juce::Justification::centredLeft, true);
                    
                    yPos += lineHeight;
                    
                    if (yPos + lineHeight > displayArea.getBottom())
                        break;
                }
            }
            
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

        // =====================================================================
        // RECORDER VISUALIZATION - with folder button
        // FIX: Added folder button, fixed button layout
        // =====================================================================
        if (auto* recorder = dynamic_cast<RecorderProcessor*>(proc))
        {
            bool isRecording = recorder->isCurrentlyRecording();
            auto contentArea = bounds.reduced(8, 6);
            
            // ROW 1: Name Textbox (full width)
            auto nameRow = contentArea.removeFromTop(24);
            auto nameBoxArea = nameRow.reduced(0, 2);
            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(nameBoxArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(nameBoxArea, 4.0f, 1.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f));
            juce::String displayName = recorder->getRecorderName();
            if (displayName.length() > 22) displayName = displayName.substring(0, 19) + "...";
            g.drawText(displayName, nameBoxArea.reduced(6, 0), juce::Justification::centredLeft);
            
            contentArea.removeFromTop(4);
            
            // ROW 2: Record/Stop/Folder Buttons + Level Meters on right
            auto controlRow = contentArea.removeFromTop(44);
            
            // RECORD BUTTON (44x38)
            auto recordBtnArea = controlRow.removeFromLeft(44).reduced(3);
            g.setColour(isRecording ? juce::Colour(80, 20, 20) : juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(recordBtnArea, 8.0f);
            g.setColour(isRecording ? juce::Colours::red.darker() : juce::Colours::grey);
            g.drawRoundedRectangle(recordBtnArea, 8.0f, 1.5f);
            
            float circleSize = recordBtnArea.getHeight() * 0.5f;
            auto circleArea = recordBtnArea.withSizeKeepingCentre(circleSize, circleSize);
            g.setColour(isRecording ? juce::Colours::red : juce::Colour(180, 50, 50));
            g.fillEllipse(circleArea);
            
            if (isRecording) {
                g.setColour(juce::Colours::red.withAlpha(0.25f));
                g.fillEllipse(circleArea.expanded(4));
            }
            
            controlRow.removeFromLeft(4);
            
            // STOP BUTTON (44x38)
            auto stopBtnArea = controlRow.removeFromLeft(44).reduced(3);
            g.setColour(juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(stopBtnArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(stopBtnArea, 4.0f, 1.5f);
            
            float squareSize = stopBtnArea.getHeight() * 0.4f;
            auto squareArea = stopBtnArea.withSizeKeepingCentre(squareSize, squareSize);
            g.setColour(juce::Colour(30, 144, 255));
            g.fillRect(squareArea);
            
            controlRow.removeFromLeft(4);
            
            // FOLDER BUTTON (44x38) - opens recording folder
            auto folderBtnArea = controlRow.removeFromLeft(44).reduced(3);
            g.setColour(juce::Colour(60, 60, 65));
            g.fillRoundedRectangle(folderBtnArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(folderBtnArea, 4.0f, 1.5f);
            
            // Draw folder icon
            g.setColour(juce::Colour(200, 180, 100));  // Folder yellow
            float iconSize = folderBtnArea.getHeight() * 0.5f;
            auto iconArea = folderBtnArea.withSizeKeepingCentre(iconSize, iconSize * 0.8f);
            // Folder body
            g.fillRoundedRectangle(iconArea.getX(), iconArea.getY() + iconSize * 0.15f, 
                                   iconSize, iconSize * 0.65f, 2.0f);
            // Folder tab
            g.fillRoundedRectangle(iconArea.getX(), iconArea.getY(), 
                                   iconSize * 0.4f, iconSize * 0.25f, 1.0f);
            
            // LEVEL METERS (on the right side)
            auto meterArea = controlRow.removeFromRight(28).reduced(2, 4);
            float meterW = (meterArea.getWidth() - 2) / 2.0f;
            float meterH = meterArea.getHeight();
            
            float levelL = juce::jlimit(0.0f, 1.0f, recorder->getLeftLevel());
            float levelR = juce::jlimit(0.0f, 1.0f, recorder->getRightLevel());
            
            auto meterL = juce::Rectangle<float>(meterArea.getX(), meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterL);
            if (levelL > 0.0f) {
                float fillH = levelL * meterH;
                juce::Colour mCol = levelL < 0.7f ? juce::Colours::limegreen 
                                  : (levelL < 0.9f ? juce::Colours::yellow : juce::Colours::red);
                g.setColour(mCol);
                g.fillRect(meterL.getX(), meterL.getBottom() - fillH, meterW, fillH);
            }
            
            auto meterR = juce::Rectangle<float>(meterArea.getX() + meterW + 2, meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterR);
            if (levelR > 0.0f) {
                float fillH = levelR * meterH;
                juce::Colour mCol = levelR < 0.7f ? juce::Colours::limegreen 
                                  : (levelR < 0.9f ? juce::Colours::yellow : juce::Colours::red);
                g.setColour(mCol);
                g.fillRect(meterR.getX(), meterR.getBottom() - fillH, meterW, fillH);
            }
            
            contentArea.removeFromTop(4);
            
            // ROW 3: Time Display (centered)
            auto timeRow = contentArea.removeFromTop(28);
            double recordingSeconds = recorder->getRecordingLengthSeconds();
            int hours = (int)(recordingSeconds / 3600.0);
            int minutes = (int)(recordingSeconds / 60.0) % 60;
            int seconds = (int)(recordingSeconds) % 60;
            int tenths = (int)((recordingSeconds - (int)recordingSeconds) * 10);
            
            juce::String timeStr = hours > 0 
                ? juce::String::formatted("%d:%02d:%02d", hours, minutes, seconds)
                : juce::String::formatted("%02d:%02d.%d", minutes, seconds, tenths);
            
            g.setColour(isRecording ? juce::Colours::lightgreen : juce::Colour(150, 150, 150));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText(timeStr, timeRow, juce::Justification::centred);
            
            contentArea.removeFromTop(4);
            
            // ROW 5 (reserve at bottom): SYNC Button
            auto syncRow = contentArea.removeFromBottom(26);
            bool syncMode = recorder->isSyncMode();
            auto syncArea = syncRow.reduced(20, 2);
            g.setColour(syncMode ? juce::Colour(0, 180, 180) : juce::Colour(80, 80, 80));
            g.fillRoundedRectangle(syncArea, 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(syncMode ? "SYNC" : "INDEPENDENT", syncArea, juce::Justification::centred);
            
            contentArea.removeFromBottom(4);
            
            // ROW 4: Waveform Display (remaining space)
            auto waveformArea = contentArea.reduced(0, 2);
            
            g.setColour(juce::Colour(18, 18, 22));
            g.fillRoundedRectangle(waveformArea, 6.0f);
            
            float centerY = waveformArea.getCentreY();
            g.setColour(juce::Colour(60, 60, 70));
            g.drawHorizontalLine((int)centerY, waveformArea.getX() + 2, waveformArea.getRight() - 2);
            
            int waveWidth = (int)waveformArea.getWidth() - 4;
            auto waveData = recorder->getWaveformData(waveWidth);
            
            if (!waveData.empty()) {
                float halfH = (waveformArea.getHeight() - 8) * 0.48f;
                float startX = waveformArea.getX() + 2;
                
                juce::Path wavePath;
                wavePath.startNewSubPath(startX, centerY);
                
                for (int i = 0; i < waveWidth && i < (int)waveData.size(); ++i) {
                    float maxV = std::max(waveData[i].maxL, waveData[i].maxR);
                    wavePath.lineTo(startX + i, centerY - (maxV * halfH));
                }
                
                for (int i = waveWidth - 1; i >= 0 && i < (int)waveData.size(); --i) {
                    float minV = std::min(waveData[i].minL, waveData[i].minR);
                    wavePath.lineTo(startX + i, centerY - (minV * halfH));
                }
                
                wavePath.closeSubPath();
                
                juce::Colour waveCol = isRecording ? juce::Colour(0, 200, 255) : juce::Colour(100, 100, 120);
                g.setColour(waveCol.withAlpha(0.5f));
                g.fillPath(wavePath);
                
                g.setColour(waveCol);
                g.strokePath(wavePath, juce::PathStrokeType(1.0f));
            }
            
            g.setColour(juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(waveformArea, 6.0f, 1.0f);
            
            // Show filename if recording exists
            if (recorder->hasRecording() && !isRecording) {
                juce::String fname = recorder->getLastRecordingFile().getFileName();
                if (fname.length() > 28) fname = "..." + fname.substring(fname.length() - 25);
                g.setColour(juce::Colours::grey.withAlpha(0.7f));
                g.setFont(juce::Font(9.0f));
                g.drawText(fname, waveformArea.reduced(4, 0).removeFromBottom(12), juce::Justification::centredLeft);
            }
        }
        // =====================================================================
        // END RECORDER VISUALIZATION
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
    
    p.cubicTo(start.x, start.y + 50, end.x, end.y - 50, end.x, end.y);

    g.setColour(col);
    g.strokePath(p, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Rectangle<float> GraphCanvas::getPinBounds(const PinInfo& pin, juce::AudioProcessorGraph::Node* node) const
{
    if (!node) return {};
    
    auto* proc = node->getProcessor();
    if (!proc) return {};
    
    auto nodeBounds = getNodeBounds(node);
    
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
    
    float spacing = Style::minPinSpacing;
    float totalWidth = spacing * (totalPins + 1);
    float startX = nodeBounds.getCentreX() - totalWidth / 2.0f;
    float x = startX + spacing * (float)(pinIdx + 1);
    
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
    
    for (int i = 0; i < numIn; ++i) {
        bool isMidi = false;
        PinInfo pinInfo = { node->nodeID, i, true, isMidi };
        auto pinBounds = getPinBounds(pinInfo, node);
        if (pinBounds.isEmpty()) continue;
        
        bool highlighted = (highlightPin == PinID{ node->nodeID, i, true, isMidi });
        
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
    
    SimpleConnectorProcessor* simpleConnector = cache ? cache->simpleConnector 
                                                       : dynamic_cast<SimpleConnectorProcessor*>(proc);
    
    StereoMeterProcessor* stereoMeter = cache ? cache->stereoMeter 
                                               : dynamic_cast<StereoMeterProcessor*>(proc);
    MidiMonitorProcessor* midiMonitor = cache ? cache->midiMonitor
                                               : dynamic_cast<MidiMonitorProcessor*>(proc);
    
    RecorderProcessor* recorder = cache ? cache->recorder
                                         : dynamic_cast<RecorderProcessor*>(proc);
    
    if (simpleConnector)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(simpleConnector->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("M", muteRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    if (stereoMeter || midiMonitor || recorder)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(proc);
    bool isInstrument = cache && cache->isInstrument;

    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

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

    if (isInstrument && meteringProc)
    {
        auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::orange.darker());
        g.fillRoundedRectangle(chRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        
        int mask = meteringProc->getMidiChannelMask();
        juce::String text = "CH";
        if (mask != 0) {
            for (int i = 0; i < 16; ++i) {
                if ((mask >> i) & 1) {
                    text = juce::String(i + 1);
                    break;
                }
            }
        }
        
        g.drawText(text, chRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(node->isBypassed() ? juce::Colours::red : juce::Colours::lightgreen);
    g.fillRoundedRectangle(muteRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("M", muteRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

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

    auto toggleRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth * 1.5f, Style::bottomBtnHeight);
    g.setColour(node->isBypassed() ? juce::Colours::grey.darker() : juce::Colours::green);
    g.fillRoundedRectangle(toggleRect, 3.0f);
    
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(node->isBypassed() ? "OFF" : "ON", toggleRect, juce::Justification::centred);
}
