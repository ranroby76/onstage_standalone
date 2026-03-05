
// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Paint.cpp
// FIXED: Added RecorderProcessor visualization
// FIX: Added folder button to recorder, fixed layout
// NEW: Added ManualSampler, AutoSampler, MidiPlayer visualization

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
#include "ManualSamplerProcessor.h"
#include "AutoSamplerProcessor.h"
#include "MidiPlayerProcessor.h"
#include "CCStepperProcessor.h"
#include "TransientSplitterProcessor.h"
#include "LatcherProcessor.h"
#include "MidiMultiFilterProcessor.h"
#include "ContainerProcessor.h"  // FIX 2
#include "ContainerProcessor.h"

void GraphCanvas::paint(juce::Graphics& g)
{
    // Dark purple background when inside a container, normal otherwise
    if (isInsideContainer())
        g.fillAll(juce::Colour(28, 14, 42));
    else
        g.fillAll(Style::colBackground);

    // Save pixel-coord state so overlays can restore it later (bulletproof)
    g.saveState();

    // =========================================================================
    // Paint-level zoom + pan: virtual coord (vx,vy) → pixel ((vx-panX)*zoom, (vy-panY)*zoom)
    // Component stays within parent bounds — no setTransform overflow
    // =========================================================================
    g.addTransform(juce::AffineTransform::scale(zoomLevel)
                    .translated(-panOffsetX * zoomLevel, -panOffsetY * zoomLevel));

    // Grid across the full virtual canvas
    // Skip lines when they'd be too dense at low zoom (minimum 8px apart on screen)
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    int gridSize = 40;
    while (gridSize * zoomLevel < 8.0f) gridSize *= 2;  // double grid spacing when too dense
    for (int x = 0; x < (int)virtualCanvasWidth; x += gridSize)
        g.drawVerticalLine(x, 0.0f, virtualCanvasHeight);
    for (int y = 0; y < (int)virtualCanvasHeight; y += gridSize)
        g.drawHorizontalLine(y, 0.0f, virtualCanvasWidth);

    if (!processor.mainGraph)
    {
        g.restoreState();
        drawScrollbars(g);
        drawMinimap(g);
        return;
    }

    auto* ag = getActiveGraph();
    if (!ag)
    {
        g.restoreState();
        drawScrollbars(g);
        drawMinimap(g);
        return;
    }

    // FIX: verifyPositions() moved to rebuildNodeTypeCache() — runs once per structure change,
    // not 20 times/sec during meter animation

    // Ensure cache is valid
    if (nodeTypeCache.empty() && ag->getNumNodes() > 0)
        const_cast<GraphCanvas*>(this)->rebuildNodeTypeCache();

    // FIX: Use cached connections instead of getConnections() which copies
    // the entire std::vector on every call — saves heap allocation churn at 20fps
    for (auto& connection : cachedConnections)
    {
        auto* srcNode = ag->getNodeForId(connection.source.nodeID);
        auto* dstNode = ag->getNodeForId(connection.destination.nodeID);
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
    for (auto* node : ag->getNodes())
    {
        if (!shouldShowNode(node)) continue;
        auto bounds = getNodeBounds(node);
        bool selected = false;

        auto* cache = getCachedNodeType(node->nodeID);

        bool isAudioInput  = cache ? cache->isAudioInput  : (node == processor.audioInputNode.get());
        bool isAudioOutput = cache ? cache->isAudioOutput : (node == processor.audioOutputNode.get());
        bool isMidiInput   = cache ? cache->isMidiInput   : (node == processor.midiInputNode.get());
        bool isMidiOutput  = cache ? cache->isMidiOutput  : (node == processor.midiOutputNode.get());

        // Main body - use darker background if bypassed
        juce::Colour bodyCol = node->isBypassed() ? Style::colNodeBodyBypassed : Style::colNodeBody;

        // Container nodes get a brighter grey body to stand out
        if (cache && cache->container)
            bodyCol = node->isBypassed() ? juce::Colour(55, 55, 60) : juce::Colour(75, 80, 90);

        g.setColour(bodyCol);
        g.fillRoundedRectangle(bounds.toFloat(), Style::nodeRounding);

        // Border
        juce::Colour borderCol = selected ? juce::Colours::yellow
                                         : (node->isBypassed() ? juce::Colours::grey.darker() : Style::colNodeBorder);
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds.toFloat(), Style::nodeRounding, 2.0f);

        // Title bar
        auto titleBounds = bounds.removeFromTop(Style::nodeTitleHeight);
        juce::Colour titleCol = node->isBypassed() ? Style::colNodeTitleBypassed : Style::colNodeTitle;
        if (cache && cache->container)
            titleCol = node->isBypassed() ? juce::Colour(60, 65, 75) : juce::Colour(50, 90, 130);
        g.setColour(titleCol);
        g.fillRoundedRectangle(titleBounds.toFloat(), Style::nodeRounding);
        g.fillRect(titleBounds.removeFromBottom(Style::nodeRounding).toFloat());

        // Title text
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

        // FREEZE FIX: Use cached name - some plugins freeze when getName() is called!
        juce::String title = cache ? cache->pluginName : "Unknown";

        g.drawText(title, titleBounds.reduced(5, 0), juce::Justification::centredLeft, true);

        // Auto Sampling chain LED: white rectangle with red circle
        if (cache && cache->inSamplingChain)
        {
            float ledW = 14.0f, ledH = 10.0f;
            float ledX = titleBounds.getRight() - ledW - 5.0f;
            float ledY = titleBounds.getCentreY() - ledH / 2.0f;
            auto ledRect = juce::Rectangle<float>(ledX, ledY, ledW, ledH);

            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(ledRect, 2.0f);

            float circleSize = 6.0f;
            g.setColour(juce::Colours::red);
            g.fillEllipse(ledRect.getCentreX() - circleSize / 2.0f,
                          ledRect.getCentreY() - circleSize / 2.0f,
                          circleSize, circleSize);
        }

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
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
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

            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain).withName(juce::Font::getDefaultMonospacedFontName())));
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

                    juce::Colour textColor;
                    if (event.isCC)
                        textColor = juce::Colour(80, 200, 255);   // Cyan for CC
                    else
                        textColor = event.isNoteOn ? juce::Colour(0, 255, 100) : juce::Colour(255, 150, 0);
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
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::italic)));
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
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
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
            g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
            g.drawText(timeStr, timeRow, juce::Justification::centred);

            contentArea.removeFromTop(4);

            // ROW 5 (reserve at bottom): SYNC Button
            auto syncRow = contentArea.removeFromBottom(26);
            bool syncMode = recorder->isSyncMode();
            auto syncArea = syncRow.reduced(20, 2);
            g.setColour(syncMode ? juce::Colour(0, 180, 180) : juce::Colour(80, 80, 80));
            g.fillRoundedRectangle(syncArea, 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
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
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.drawText(fname, waveformArea.reduced(4, 0).removeFromBottom(12), juce::Justification::centredLeft);
            }
        }
        // =====================================================================
        // END RECORDER VISUALIZATION
        // =====================================================================

        // =====================================================================
        // MANUAL SAMPLER VISUALIZATION (480×180)
        // Armed button, status, note display, file count, meters, waveform
        // =====================================================================
        if (auto* manualSampler = dynamic_cast<ManualSamplerProcessor*>(proc))
        {
            bool armed = manualSampler->getArmed();
            bool recording = manualSampler->isCurrentlyRecording();
            auto contentArea = bounds.reduced(8, 6);

            // ROW 1: Family name textbox + file count
            auto nameRow = contentArea.removeFromTop(24);
            auto countArea = nameRow.removeFromRight(70);
            auto nameBoxArea = nameRow.reduced(0, 2);

            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(nameBoxArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(nameBoxArea, 4.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawText(manualSampler->getFamilyName(), nameBoxArea.reduced(6, 0), juce::Justification::centredLeft);

            g.setColour(juce::Colour(120, 120, 140));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(juce::String(manualSampler->getTotalFilesRecorded()) + " files", countArea, juce::Justification::centredRight);

            contentArea.removeFromTop(4);

            // ROW 2: Armed button + Status + Note display + Meters
            auto controlRow = contentArea.removeFromTop(44);

            // ARMED button (50x38)
            auto armBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(armed ? (recording ? juce::Colour(80, 20, 20) : juce::Colour(60, 40, 10)) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(armBtnArea, 8.0f);
            g.setColour(armed ? (recording ? juce::Colours::red : juce::Colours::orange) : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(armBtnArea, 8.0f, 1.5f);

            if (recording) {
                float circleSize = armBtnArea.getHeight() * 0.45f;
                auto circleArea = armBtnArea.withSizeKeepingCentre(circleSize, circleSize);
                g.setColour(juce::Colours::red);
                g.fillEllipse(circleArea);
                g.setColour(juce::Colours::red.withAlpha(0.25f));
                g.fillEllipse(circleArea.expanded(4));
            } else {
                g.setColour(armed ? juce::Colours::orange : juce::Colour(100, 100, 120));
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText("ARM", armBtnArea, juce::Justification::centred);
            }

            controlRow.removeFromLeft(8);

            // Note display
            auto noteArea = controlRow.removeFromLeft(80);
            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(noteArea, 4.0f);

            if (recording) {
                g.setColour(juce::Colours::lightgreen);
                g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
                g.drawText(ManualSamplerProcessor::midiNoteToName(manualSampler->getLastRecordedNote()), noteArea, juce::Justification::centred);
            } else if (armed) {
                g.setColour(juce::Colours::orange.withAlpha(0.6f));
                g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::italic)));
                g.drawText("Waiting...", noteArea, juce::Justification::centred);
            } else {
                g.setColour(juce::Colour(80, 80, 100));
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                g.drawText("--", noteArea, juce::Justification::centred);
            }

            // Level meters on right
            auto meterArea = controlRow.removeFromRight(28).reduced(2, 4);
            float meterW = (meterArea.getWidth() - 2) / 2.0f;
            float meterH = meterArea.getHeight();
            float levelL = juce::jlimit(0.0f, 1.0f, manualSampler->getLeftLevel());
            float levelR = juce::jlimit(0.0f, 1.0f, manualSampler->getRightLevel());

            auto meterL = juce::Rectangle<float>(meterArea.getX(), meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterL);
            if (levelL > 0.0f) {
                float fillH = levelL * meterH;
                g.setColour(levelL < 0.7f ? juce::Colours::limegreen : (levelL < 0.9f ? juce::Colours::yellow : juce::Colours::red));
                g.fillRect(meterL.getX(), meterL.getBottom() - fillH, meterW, fillH);
            }
            auto meterR = juce::Rectangle<float>(meterArea.getX() + meterW + 2, meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterR);
            if (levelR > 0.0f) {
                float fillH = levelR * meterH;
                g.setColour(levelR < 0.7f ? juce::Colours::limegreen : (levelR < 0.9f ? juce::Colours::yellow : juce::Colours::red));
                g.fillRect(meterR.getX(), meterR.getBottom() - fillH, meterW, fillH);
            }

            contentArea.removeFromTop(4);

            // ROW 3: Waveform display (remaining space)
            auto waveformArea = contentArea.reduced(0, 2);
            g.setColour(juce::Colour(18, 18, 22));
            g.fillRoundedRectangle(waveformArea, 6.0f);
            g.setColour(juce::Colour(60, 60, 70));
            g.drawHorizontalLine((int)waveformArea.getCentreY(), waveformArea.getX() + 2, waveformArea.getRight() - 2);
            g.setColour(juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(waveformArea, 6.0f, 1.0f);

            // Silence threshold line
            float threshDB = manualSampler->getSilenceThresholdDb();
            float threshLinear = std::pow(10.0f, threshDB / 20.0f);
            float threshY = waveformArea.getCentreY() - (threshLinear * (waveformArea.getHeight() - 8) * 0.48f);
            g.setColour(juce::Colours::red.withAlpha(0.3f));
            g.drawHorizontalLine((int)threshY, waveformArea.getX() + 2, waveformArea.getRight() - 2);
        }
        // =====================================================================
        // END MANUAL SAMPLER VISUALIZATION
        // =====================================================================

        // =====================================================================
        // AUTO SAMPLER VISUALIZATION (480×180)
        // Progress, Start/Stop, current note, meters, waveform
        // =====================================================================
        if (auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(proc))
        {
            bool running = autoSampler->isRunning();
            auto contentArea = bounds.reduced(8, 6);

            // ROW 1: Family name textbox + progress
            auto nameRow = contentArea.removeFromTop(24);
            auto progressArea = nameRow.removeFromRight(90);
            auto nameBoxArea = nameRow.reduced(0, 2);

            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(nameBoxArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(nameBoxArea, 4.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawText(autoSampler->getFamilyName(), nameBoxArea.reduced(6, 0), juce::Justification::centredLeft);

            g.setColour(running ? juce::Colours::lightgreen : juce::Colour(120, 120, 140));
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText(juce::String(autoSampler->getCurrentNoteIndex()) + " / " + juce::String(autoSampler->getTotalNotes()),
                       progressArea, juce::Justification::centredRight);

            contentArea.removeFromTop(4);

            // ROW 2: Start/Stop + E button indicator + current note + meters
            auto controlRow = contentArea.removeFromTop(44);

            // Start/Stop button (50x38)
            auto startBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(running ? juce::Colour(20, 60, 20) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(startBtnArea, 8.0f);
            g.setColour(running ? juce::Colours::green.darker() : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(startBtnArea, 8.0f, 1.5f);

            if (running) {
                float sqSize = startBtnArea.getHeight() * 0.35f;
                auto sq = startBtnArea.withSizeKeepingCentre(sqSize, sqSize);
                g.setColour(juce::Colour(60, 140, 220));
                g.fillRect(sq);
            } else {
                float triH = startBtnArea.getHeight() * 0.45f;
                auto center = startBtnArea.getCentre();
                juce::Path tri;
                tri.addTriangle(
                    center.x - triH * 0.35f, center.y - triH * 0.5f,
                    center.x - triH * 0.35f, center.y + triH * 0.5f,
                    center.x + triH * 0.55f, center.y);
                g.setColour(juce::Colour(80, 200, 80));
                g.fillPath(tri);
            }

            controlRow.removeFromLeft(8);

            // Current note display
            auto noteArea = controlRow.removeFromLeft(80);
            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(noteArea, 4.0f);

            if (running) {
                g.setColour(juce::Colours::lightgreen);
                g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
                g.drawText(AutoSamplerProcessor::midiNoteToName(autoSampler->getCurrentNote()), noteArea, juce::Justification::centred);
            } else {
                g.setColour(juce::Colour(80, 80, 100));
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                int total = autoSampler->getTotalNotes();
                g.drawText(total > 0 ? juce::String(total) + " notes" : "No notes", noteArea, juce::Justification::centred);
            }

            // Level meters on right
            auto meterArea = controlRow.removeFromRight(28).reduced(2, 4);
            float meterW = (meterArea.getWidth() - 2) / 2.0f;
            float meterH = meterArea.getHeight();
            float levelL = juce::jlimit(0.0f, 1.0f, autoSampler->getLeftLevel());
            float levelR = juce::jlimit(0.0f, 1.0f, autoSampler->getRightLevel());

            auto meterL = juce::Rectangle<float>(meterArea.getX(), meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterL);
            if (levelL > 0.0f) {
                float fillH = levelL * meterH;
                g.setColour(levelL < 0.7f ? juce::Colours::limegreen : (levelL < 0.9f ? juce::Colours::yellow : juce::Colours::red));
                g.fillRect(meterL.getX(), meterL.getBottom() - fillH, meterW, fillH);
            }
            auto meterR = juce::Rectangle<float>(meterArea.getX() + meterW + 2, meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterR);
            if (levelR > 0.0f) {
                float fillH = levelR * meterH;
                g.setColour(levelR < 0.7f ? juce::Colours::limegreen : (levelR < 0.9f ? juce::Colours::yellow : juce::Colours::red));
                g.fillRect(meterR.getX(), meterR.getBottom() - fillH, meterW, fillH);
            }

            contentArea.removeFromTop(4);

            // ROW 3: Waveform display / Instructions
            auto waveformArea = contentArea.reduced(0, 2);
            g.setColour(juce::Colour(18, 18, 22));
            g.fillRoundedRectangle(waveformArea, 6.0f);
            g.setColour(juce::Colour(60, 60, 70));
            g.drawHorizontalLine((int)waveformArea.getCentreY(), waveformArea.getX() + 2, waveformArea.getRight() - 2);
            g.setColour(juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(waveformArea, 6.0f, 1.0f);

            // Show instructions when idle
            if (!running && autoSampler->getTotalNotes() == 0) {
                g.setColour(juce::Colour(90, 90, 110));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.drawText("1. Connect MIDI out to VSTi chain",
                           waveformArea.reduced(8, 0).removeFromTop(waveformArea.getHeight() / 3 + 4),
                           juce::Justification::centredLeft);
                g.drawText("2. Press E to configure notes",
                           waveformArea.reduced(8, 0).withTrimmedTop(waveformArea.getHeight() / 3.0f),
                           juce::Justification::centredLeft);
                g.drawText("3. Press Play to auto-sample",
                           waveformArea.reduced(8, 0).removeFromBottom(waveformArea.getHeight() / 3 + 4),
                           juce::Justification::centredLeft);
            }

            // Progress bar at bottom of waveform
            if (running && autoSampler->getTotalNotes() > 0) {
                float progress = (float)autoSampler->getCurrentNoteIndex() / (float)autoSampler->getTotalNotes();
                auto progBar = waveformArea.removeFromBottom(4);
                g.setColour(juce::Colour(40, 180, 80).withAlpha(0.6f));
                g.fillRect(progBar.getX(), progBar.getY(), progBar.getWidth() * progress, progBar.getHeight());
            }
        }
        // =====================================================================
        // END AUTO SAMPLER VISUALIZATION
        // =====================================================================

        // =====================================================================
        // MIDI PLAYER VISUALIZATION (480×180)
        // Filename + LOAD, Transport buttons, BPM, Channel dots, Slider
        // =====================================================================
        if (auto* midiPlayer = dynamic_cast<MidiPlayerProcessor*>(proc))
        {
            bool playing = midiPlayer->isPlaying();
            bool paused = midiPlayer->isPaused();
            bool hasFile = midiPlayer->hasFileLoaded();
            bool looping = midiPlayer->isLooping();
            auto contentArea = bounds.reduced(8, 6);

            // TOP ROW: Filename + Load + Info buttons
            auto topRow = contentArea.removeFromTop(26);
            auto infoBtnArea = topRow.removeFromRight(34);
            topRow.removeFromRight(4);
            auto loadBtnArea = topRow.removeFromRight(68);
            topRow.removeFromRight(6);
            auto fileArea = topRow;

            // Filename display
            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(fileArea, 4.0f);
            g.setColour(juce::Colour(60, 60, 70));
            g.drawRoundedRectangle(fileArea, 4.0f, 1.0f);

            if (hasFile) {
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(13.0f)));
                juce::String fname = midiPlayer->getFileName();
                if (fname.length() > 35) fname = fname.substring(0, 32) + "...";
                g.drawText(fname, fileArea.reduced(8, 0), juce::Justification::centredLeft);
            } else {
                g.setColour(juce::Colour(100, 100, 120));
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::italic)));
                g.drawText("No file loaded", fileArea.reduced(8, 0), juce::Justification::centredLeft);
            }

            // LOAD button
            g.setColour(juce::Colour(60, 100, 160));
            g.fillRoundedRectangle(loadBtnArea, 5.0f);
            g.setColour(juce::Colour(80, 130, 200));
            g.drawRoundedRectangle(loadBtnArea, 5.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText("LOAD", loadBtnArea, juce::Justification::centred);

            // E button (editor - channel mute table)
            g.setColour(hasFile ? juce::Colours::cyan.darker() : juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(infoBtnArea, 5.0f);
            g.setColour(hasFile ? juce::Colour(60, 180, 200) : juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(infoBtnArea, 5.0f, 1.0f);
            g.setColour(hasFile ? juce::Colours::black : juce::Colours::grey);
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText("E", infoBtnArea, juce::Justification::centred);

            contentArea.removeFromTop(6);

            // MIDDLE ROW: Transport + BPM + Channel dots
            auto controlRow = contentArea.removeFromTop(44);

            // PLAY/PAUSE button (50x38)
            auto playBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(playing ? juce::Colour(20, 70, 20) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(playBtnArea, 8.0f);
            g.setColour(playing ? juce::Colours::green.darker() : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(playBtnArea, 8.0f, 1.5f);

            if (playing) {
                float barW = 4.0f, barH = playBtnArea.getHeight() * 0.45f;
                float cx = playBtnArea.getCentreX(), cy = playBtnArea.getCentreY();
                g.setColour(juce::Colours::limegreen);
                g.fillRect(cx - barW - 1.5f, cy - barH / 2, barW, barH);
                g.fillRect(cx + 1.5f, cy - barH / 2, barW, barH);
            } else {
                float triH = playBtnArea.getHeight() * 0.5f;
                auto center = playBtnArea.getCentre();
                juce::Path tri;
                tri.addTriangle(
                    center.x - triH * 0.35f, center.y - triH * 0.5f,
                    center.x - triH * 0.35f, center.y + triH * 0.5f,
                    center.x + triH * 0.55f, center.y);
                g.setColour(paused ? juce::Colours::yellow : juce::Colour(80, 200, 80));
                g.fillPath(tri);
            }

            if (playing) {
                g.setColour(juce::Colours::green.withAlpha(0.12f));
                g.fillRoundedRectangle(playBtnArea.expanded(3), 10.0f);
            }

            controlRow.removeFromLeft(4);

            // STOP button (50x38)
            auto stopBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(stopBtnArea, 8.0f);
            g.setColour(juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(stopBtnArea, 8.0f, 1.5f);
            float sqSize = stopBtnArea.getHeight() * 0.38f;
            auto sq = stopBtnArea.withSizeKeepingCentre(sqSize, sqSize);
            g.setColour(juce::Colour(60, 140, 220));
            g.fillRect(sq);

            controlRow.removeFromLeft(4);

            // LOOP toggle (50x38)
            auto loopBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(looping ? juce::Colour(20, 60, 80) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(loopBtnArea, 8.0f);
            g.setColour(looping ? juce::Colour(0, 180, 220) : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(loopBtnArea, 8.0f, 1.5f);
            g.setColour(looping ? juce::Colour(0, 220, 255) : juce::Colour(100, 100, 120));
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText("LOOP", loopBtnArea, juce::Justification::centred);

            controlRow.removeFromLeft(10);

            // BPM Knob Display (drag up/down to change tempo)
            auto bpmArea = controlRow.removeFromLeft(60);
            double bpm = midiPlayer->getCurrentBpm();
            bool synced = midiPlayer->isSyncToMaster();

            g.setColour(synced ? juce::Colour(30, 30, 35) : juce::Colour(40, 35, 20));
            g.fillRoundedRectangle(bpmArea, 4.0f);
            g.setColour(synced ? juce::Colour(60, 60, 70) : juce::Colour(180, 140, 40));
            g.drawRoundedRectangle(bpmArea, 4.0f, 1.0f);

            g.setColour(synced ? juce::Colour(200, 200, 220) : juce::Colours::orange);
            g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
            g.drawText(juce::String(bpm, 1), bpmArea.reduced(4, 0), juce::Justification::centred);
            g.setColour(juce::Colour(120, 120, 140));
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.drawText("BPM", bpmArea.reduced(2, 1), juce::Justification::bottomRight);

            // Up/down arrows to hint draggable
            float arrowX = bpmArea.getRight() - 8;
            float arrowMidY = bpmArea.getCentreY();
            g.setColour(synced ? juce::Colour(80, 80, 100) : juce::Colour(180, 140, 40).withAlpha(0.6f));
            juce::Path upArrow, downArrow;
            upArrow.addTriangle(arrowX, arrowMidY - 3, arrowX - 3, arrowMidY - 7, arrowX + 3, arrowMidY - 7);
            downArrow.addTriangle(arrowX, arrowMidY + 3, arrowX - 3, arrowMidY + 7, arrowX + 3, arrowMidY + 7);
            g.fillPath(upArrow);
            g.fillPath(downArrow);

            controlRow.removeFromLeft(4);

            // SYNC button
            auto syncBtnArea = controlRow.removeFromLeft(32).reduced(2, 6);
            g.setColour(synced ? juce::Colour(20, 60, 80) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(syncBtnArea, 4.0f);
            g.setColour(synced ? juce::Colour(0, 180, 220) : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(syncBtnArea, 4.0f, 1.0f);
            g.setColour(synced ? juce::Colour(0, 220, 255) : juce::Colour(100, 100, 120));
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.drawText("SYNC", syncBtnArea, juce::Justification::centred);

            controlRow.removeFromLeft(6);

            // Channel Activity Dots (2 rows x 8)
            auto chArea = controlRow;
            if (chArea.getWidth() > 70)
            {
                float dotSize = 8.0f, dotSpacing = 11.0f;
                float startX = chArea.getX();
                float topY = chArea.getCentreY() - dotSpacing + 1;

                for (int row = 0; row < 2; row++)
                {
                    for (int col = 0; col < 8; col++)
                    {
                        int ch = row * 8 + col;
                        float x = startX + col * dotSpacing;
                        float y = topY + row * (dotSpacing + 2);
                        auto dotRect = juce::Rectangle<float>(x, y, dotSize, dotSize);

                        bool muted = midiPlayer->isChannelMuted(ch);

                        if (muted) {
                            // Muted channel: red X dot
                            g.setColour(juce::Colour(80, 30, 30));
                            g.fillEllipse(dotRect);
                            g.setColour(juce::Colour(180, 60, 60));
                            g.drawLine(dotRect.getX() + 2, dotRect.getY() + 2,
                                       dotRect.getRight() - 2, dotRect.getBottom() - 2, 1.5f);
                            g.drawLine(dotRect.getRight() - 2, dotRect.getY() + 2,
                                       dotRect.getX() + 2, dotRect.getBottom() - 2, 1.5f);
                        } else if (midiPlayer->isChannelActive(ch)) {
                            juce::Colour dotColor = (ch == 9) ? juce::Colour(255, 160, 40) : juce::Colour(60, 220, 100);
                            g.setColour(dotColor);
                            g.fillEllipse(dotRect);
                            g.setColour(dotColor.withAlpha(0.3f));
                            g.fillEllipse(dotRect.expanded(2));
                        } else {
                            g.setColour(juce::Colour(40, 40, 48));
                            g.fillEllipse(dotRect);
                        }
                    }
                }
            }

            contentArea.removeFromTop(6);

            // BOTTOM: Time + Slider
            auto bottomArea = contentArea;
            auto timeRow = bottomArea.removeFromTop(16);

            double currentSec = midiPlayer->getCurrentTimeSeconds();
            double totalSec = midiPlayer->getTotalTimeSeconds();
            int curMin = (int)(currentSec / 60.0), curSec2 = (int)currentSec % 60;
            int totMin = (int)(totalSec / 60.0), totSec2 = (int)totalSec % 60;

            g.setColour(playing ? juce::Colours::lightgreen : juce::Colour(180, 180, 200));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText(juce::String::formatted("%d:%02d", curMin, curSec2),
                       timeRow.removeFromLeft(45), juce::Justification::centredLeft);
            g.setColour(juce::Colour(100, 100, 120));
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(juce::String::formatted("/ %d:%02d", totMin, totSec2),
                       timeRow.removeFromLeft(55), juce::Justification::centredLeft);

            // Current section from markers
            auto& mkrs = midiPlayer->getMarkers();
            if (!mkrs.empty()) {
                juce::String currentSection;
                double curTick = midiPlayer->getCurrentTick();
                for (int mi = (int)mkrs.size() - 1; mi >= 0; mi--) {
                    if (curTick >= mkrs[mi].tick) { currentSection = mkrs[mi].name; break; }
                }
                if (currentSection.isNotEmpty()) {
                    g.setColour(juce::Colour(150, 180, 220));
                    g.setFont(juce::Font(juce::FontOptions(10.0f)));
                    g.drawText(currentSection, timeRow, juce::Justification::centredRight);
                }
            }

            bottomArea.removeFromTop(4);

            // POSITION SLIDER
            auto sliderArea = bottomArea.reduced(0, 1);
            float trackH = 10.0f;
            float trackY = sliderArea.getCentreY() - trackH / 2;
            float trackX = sliderArea.getX() + 6;
            float trackW = sliderArea.getWidth() - 12;
            auto trackRect = juce::Rectangle<float>(trackX, trackY, trackW, trackH);

            g.setColour(juce::Colour(20, 20, 25));
            g.fillRoundedRectangle(trackRect, 5.0f);
            g.setColour(juce::Colour(50, 50, 60));
            g.drawRoundedRectangle(trackRect, 5.0f, 1.0f);

            double pos = midiPlayer->getPositionNormalized();
            float fillW = (float)(pos * trackW);

            if (fillW > 0.5f) {
                auto fillRect = juce::Rectangle<float>(trackX, trackY, fillW, trackH);
                g.setGradientFill(juce::ColourGradient(
                    juce::Colour(20, 60, 140), trackX, trackY,
                    juce::Colour(40, 140, 255), trackX + fillW, trackY, false));
                g.fillRoundedRectangle(fillRect, 5.0f);
            }

            // Marker ticks
            if (!mkrs.empty() && midiPlayer->getTotalTicks() > 0) {
                double totalT = midiPlayer->getTotalTicks();
                g.setColour(juce::Colour(200, 200, 100).withAlpha(0.5f));
                for (auto& marker : mkrs) {
                    float mx = trackX + (float)(marker.tick / totalT) * trackW;
                    g.drawVerticalLine((int)mx, trackY - 2, trackY + trackH + 2);
                }
            }

            // Thumb
            float thumbX = trackX + fillW;
            float thumbRadius = 8.0f;
            auto thumbRect = juce::Rectangle<float>(
                thumbX - thumbRadius, sliderArea.getCentreY() - thumbRadius,
                thumbRadius * 2, thumbRadius * 2);

            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.fillEllipse(thumbRect.translated(1, 1));
            g.setColour(playing ? juce::Colour(60, 180, 255) : juce::Colour(160, 160, 180));
            g.fillEllipse(thumbRect);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.fillEllipse(thumbRect.reduced(3));
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawEllipse(thumbRect, 1.5f);
        }
        // =====================================================================
        // END MIDI PLAYER VISUALIZATION
        // =====================================================================

        // =====================================================================
        // STEP SEQ - Compact transport-only node
        // Layout:
        //   TOP:    Play/Stop + BPM display + Sync button + Slot counter
        //   BOTTOM: E + X buttons (drawn by drawNodeButtons)
        // =====================================================================
        if (auto* ccStepper = dynamic_cast<CCStepperProcessor*>(proc))
        {
            bool stepSeqPlaying = ccStepper->isPlaying();
            auto contentArea = bounds.reduced(8, 6);

            // --- SINGLE ROW: Transport + BPM + Sync + Counter ---
            auto topRow = contentArea.removeFromTop(28);

            // PLAY/STOP button
            auto playBtnArea = topRow.removeFromLeft(50).reduced(2);
            g.setColour(stepSeqPlaying ? juce::Colour(20, 70, 20) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(playBtnArea, 6.0f);
            g.setColour(stepSeqPlaying ? juce::Colours::green.darker() : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(playBtnArea, 6.0f, 1.0f);

            if (stepSeqPlaying) {
                float sqSize = playBtnArea.getHeight() * 0.38f;
                auto sq = playBtnArea.withSizeKeepingCentre(sqSize, sqSize);
                g.setColour(juce::Colour(60, 140, 220));
                g.fillRect(sq);
            } else {
                float triH = playBtnArea.getHeight() * 0.5f;
                auto center = playBtnArea.getCentre();
                juce::Path tri;
                tri.addTriangle(
                    center.x - triH * 0.35f, center.y - triH * 0.5f,
                    center.x - triH * 0.35f, center.y + triH * 0.5f,
                    center.x + triH * 0.55f, center.y);
                g.setColour(juce::Colour(80, 200, 80));
                g.fillPath(tri);
            }

            if (stepSeqPlaying) {
                g.setColour(juce::Colours::green.withAlpha(0.12f));
                g.fillRoundedRectangle(playBtnArea.expanded(2), 8.0f);
            }

            topRow.removeFromLeft(6);

            // BPM display (draggable)
            auto bpmArea = topRow.removeFromLeft(70);
            double bpmVal = ccStepper->isSyncToMasterBpm() ? ccStepper->getEffectiveBpm() : ccStepper->getBpm();

            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(bpmArea, 4.0f);
            g.setColour(juce::Colour(60, 60, 70));
            g.drawRoundedRectangle(bpmArea, 4.0f, 1.0f);

            g.setColour(juce::Colour(200, 200, 220));
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawText(juce::String(bpmVal, 1), bpmArea.reduced(4, 0), juce::Justification::centredLeft);
            g.setColour(juce::Colour(120, 120, 140));
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.drawText("BPM", bpmArea.reduced(4, 2), juce::Justification::bottomRight);

            topRow.removeFromLeft(4);

            // SYNC button
            auto syncBtnArea = topRow.removeFromLeft(32).reduced(1, 3);
            bool synced = ccStepper->isSyncToMasterBpm();
            g.setColour(synced ? juce::Colour(30, 90, 30) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(syncBtnArea, 4.0f);
            g.setColour(synced ? juce::Colours::limegreen.darker() : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(syncBtnArea, 4.0f, 1.0f);
            g.setColour(synced ? juce::Colours::limegreen : juce::Colour(120, 120, 140));
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.drawText("SYN", syncBtnArea, juce::Justification::centred);

            topRow.removeFromLeft(6);

            // Slot counter
            int enabledCount = 0;
            for (int si = 0; si < CCStepperProcessor::MaxSlots; si++)
                if (ccStepper->getSlot(si).enabled) enabledCount++;

            g.setColour(juce::Colour(140, 140, 160));
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(juce::String(enabledCount) + "/16",
                       topRow, juce::Justification::centredRight);
        }
        // =====================================================================
        // END STEP SEQ VISUALIZATION
        // =====================================================================


        // =====================================================================
        // LATCHER - 4x4 pad grid compact visualization
        // =====================================================================
        if (auto* latcher = dynamic_cast<LatcherProcessor*>(proc))
        {
            auto contentArea = bounds.reduced(8, 6);

            // "All Off" button in upper right corner of content area
            float allOffW = 42.0f;
            float allOffH = 16.0f;
            auto allOffRect = juce::Rectangle<float>(
                contentArea.getRight() - allOffW,
                contentArea.getY(),
                allOffW, allOffH);

            // Count latched pads for color
            int latchedCount = 0;
            for (int i = 0; i < LatcherProcessor::NumPads; i++)
                if (latcher->isPadLatched(i)) latchedCount++;

            g.setColour(latchedCount > 0 ? juce::Colour(0xffCC3333) : juce::Colour(60, 60, 60));
            g.fillRoundedRectangle(allOffRect, 3.0f);
            g.setColour(latchedCount > 0 ? juce::Colours::white : juce::Colours::grey);
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText("ALL OFF", allOffRect, juce::Justification::centred);

            // Grid area — below All Off, above bottom buttons
            auto gridArea = contentArea;
            gridArea.removeFromTop(allOffH + 2.0f);
            gridArea.removeFromBottom(Style::bottomBtnHeight + Style::bottomBtnMargin + 2);

            float padW = gridArea.getWidth() / 4.0f;
            float padH = gridArea.getHeight() / 4.0f;
            float gap = 2.0f;

            for (int row = 0; row < 4; row++)
            {
                for (int col = 0; col < 4; col++)
                {
                    int padIndex = row * 4 + col;
                    bool latched = latcher->isPadLatched(padIndex);

                    auto padRect = juce::Rectangle<float>(
                        gridArea.getX() + col * padW + gap / 2,
                        gridArea.getY() + row * padH + gap / 2,
                        padW - gap,
                        padH - gap);

                    if (latched)
                    {
                        // ON: golden fill + glow
                        g.setColour(juce::Colour(0xffB8860B));
                        g.fillRoundedRectangle(padRect, 3.0f);
                        g.setColour(juce::Colour(0xffFFD700).withAlpha(0.3f));
                        g.fillRoundedRectangle(padRect.expanded(1), 4.0f);
                        // Golden border
                        g.setColour(juce::Colour(0xffFFD700));
                        g.drawRoundedRectangle(padRect, 3.0f, 1.0f);
                        // Golden text
                        g.setColour(juce::Colour(0xffFFD700));
                    }
                    else
                    {
                        // OFF: light gray fill
                        g.setColour(juce::Colour(180, 180, 185));
                        g.fillRoundedRectangle(padRect, 3.0f);
                        // Subtle darker border
                        g.setColour(juce::Colour(140, 140, 145));
                        g.drawRoundedRectangle(padRect, 3.0f, 0.8f);
                        // Black text
                        g.setColour(juce::Colours::black);
                    }

                    // Pad number label
                    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                    g.drawText(juce::String(padIndex + 1), padRect, juce::Justification::centred);
                }
            }

            if (latchedCount > 0)
            {
                g.setColour(juce::Colour(0xffFFD700).withAlpha(0.06f));
                g.fillRoundedRectangle(bounds.reduced(2), Style::nodeRounding);
            }
        }
        // =====================================================================
        // END LATCHER VISUALIZATION
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

    // =========================================================================
    // NEW: Draw overlays in raw component pixel coords
    // restoreState() guarantees pixel-space regardless of zoom/pan
    // =========================================================================
    g.restoreState();

    drawScrollbars(g);
    drawMinimap(g);

    // Draw container header on top of everything when inside a container
    if (isInsideContainer())
        drawContainerHeader(g);
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
    auto* node = getActiveGraph() ? getActiveGraph()->getNodeForId(pinID.nodeID) : nullptr;
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

    // =========================================================================
    // MidiMultiFilter: E (editor popup) + P (pass-through) + X (delete)
    // =========================================================================
    MidiMultiFilterProcessor* midiMultiFilter = cache ? cache->midiMultiFilter
                                                       : dynamic_cast<MidiMultiFilterProcessor*>(proc);
    if (midiMultiFilter)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (editor)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // P button (pass-through)
        bool isPass = midiMultiFilter->passThrough;
        auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(isPass ? juce::Colours::yellow : juce::Colours::grey.darker());
        g.fillRoundedRectangle(passRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("P", passRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // X button (delete)
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // =========================================================================
    // Container: E (dive in) + CH (MIDI channel) + M (mute) + T (transport) + X (delete)
    // =========================================================================
    ContainerProcessor* containerProc = cache ? cache->container
                                               : dynamic_cast<ContainerProcessor*>(proc);
    if (containerProc)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (dive in)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // M button (mute)
        auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(containerProc->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("M", muteRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // T button (transport sync)
        bool synced = containerProc->isTransportSynced();
        auto transportRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(synced ? juce::Colours::yellow.darker(0.4f) : juce::Colours::yellow);
        g.fillRoundedRectangle(transportRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("T", transportRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // X button (delete)
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // Latcher: E (editor popup) + X (delete)
    if (dynamic_cast<LatcherProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    if (simpleConnector)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(simpleConnector->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("M", muteRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
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
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // Manual Sampler: F (folder) + X (delete)
    if (dynamic_cast<ManualSamplerProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // F button (folder)
        auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRoundedRectangle(folderRect, 3.0f);
        g.setColour(juce::Colour(200, 180, 100));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("F", folderRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // Auto Sampler: E (editor) + F (folder) + X (delete)
    if (dynamic_cast<AutoSamplerProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (editor)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // F button (folder)
        auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRoundedRectangle(folderRect, 3.0f);
        g.setColour(juce::Colour(200, 180, 100));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("F", folderRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // MIDI Player: E (editor/mute) + X (delete)
    if (dynamic_cast<MidiPlayerProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (channel mute table)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // Step Seq: E (editor popup) + X (delete)
    if (dynamic_cast<CCStepperProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (editor)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }

    // Transient Splitter: E (editor popup) + X (delete)
    if (dynamic_cast<TransientSplitterProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (editor)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect2 = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect2, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect2, juce::Justification::centred);
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
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    if (isInstrument && meteringProc)
    {
        auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::orange.darker());
        g.fillRoundedRectangle(chRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));

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
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("M", muteRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    if (!isInstrument && meteringProc)
    {
        bool passThrough = meteringProc->isPassThrough();
        auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(passThrough ? juce::Colours::yellow : juce::Colours::grey.darker());
        g.fillRoundedRectangle(passRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("P", passRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // T button (transport override) - all plugin nodes
    if (meteringProc)
    {
        bool synced = meteringProc->isTransportSynced();
        auto transportRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(synced ? juce::Colours::yellow.darker(0.4f) : juce::Colours::yellow);
        g.fillRoundedRectangle(transportRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("T", transportRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // L button (load/reload VST2) - VST2 plugin nodes only
    if (meteringProc && meteringProc->isVST2())
    {
        auto loadRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(0xFF4DA6FF));  // Blue for VST2
        g.fillRoundedRectangle(loadRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("L", loadRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(juce::Colours::darkred);
    g.fillRoundedRectangle(deleteRect, 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
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
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText(node->isBypassed() ? "OFF" : "ON", toggleRect, juce::Justification::centred);

    // =========================================================================
    // +/- bus buttons — only for audio I/O nodes when inside a container
    // =========================================================================
    if (isInsideContainer())
    {
        auto* cache = getCachedNodeType(node->nodeID);
        bool isAudioIn  = cache && cache->isAudioInput;
        bool isAudioOut = cache && cache->isAudioOutput;

        if (isAudioIn || isAudioOut)
        {
            float pmBtnW = Style::bottomBtnWidth;
            float pmBtnH = Style::bottomBtnHeight;
            float addX = btnX + Style::bottomBtnWidth * 1.5f + Style::bottomBtnSpacing * 2.0f;

            // + button
            auto addRect = juce::Rectangle<float>(addX, btnY, pmBtnW, pmBtnH);
            g.setColour(juce::Colour(50, 160, 80));
            g.fillRoundedRectangle(addRect, 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText("+", addRect, juce::Justification::centred);
            addX += pmBtnW + Style::bottomBtnSpacing;

            // - button
            auto subRect = juce::Rectangle<float>(addX, btnY, pmBtnW, pmBtnH);
            g.setColour(juce::Colour(160, 60, 50));
            g.fillRoundedRectangle(subRect, 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText("-", subRect, juce::Justification::centred);
        }
    }
}

// =============================================================================
// Container header — drawn in pixel space as the top bar when inside a container
// Layout (left → right):
//   [← Rack]  [Name TextEditor - managed as child component]  [title label]  [Load] [Save]
// =============================================================================
void GraphCanvas::drawContainerHeader(juce::Graphics& g)
{
    float w = (float)getWidth();
    float h = containerHeaderHeight;

    // Background
    g.setColour(juce::Colour(45, 18, 68));
    g.fillRect(0.0f, 0.0f, w, h);

    // Bottom border line
    g.setColour(juce::Colour(140, 70, 200));
    g.fillRect(0.0f, h - 1.5f, w, 1.5f);

    const float btnH   = 30.0f;
    const float btnY   = (h - btnH) / 2.0f;
    const float margin = 8.0f;

    // ---- ← Rack button ----
    auto rackBtnRect = juce::Rectangle<float>(margin, btnY, 80.0f, btnH);
    g.setColour(juce::Colour(80, 40, 110));
    g.fillRoundedRectangle(rackBtnRect, 5.0f);
    g.setColour(juce::Colour(200, 150, 255));
    g.drawRoundedRectangle(rackBtnRect, 5.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(juce::CharPointer_UTF8("\xe2\x86\x90 Rack"), rackBtnRect, juce::Justification::centred);

    // ---- Name label (clickable — shows AlertWindow to rename) ----
    float saveX  = w - margin - 65.0f;
    float loadX  = saveX - margin - 65.0f;
    float nameX  = margin + 80.0f + 12.0f;
    float nameW  = loadX - nameX - 12.0f;

    juce::String currentName = containerStack.empty()
        ? juce::String("Container")
        : containerStack.back()->getContainerName();

    auto nameRect = juce::Rectangle<float>(nameX, btnY, nameW, btnH);

    // Draw name background — slightly highlighted to hint it's clickable
    g.setColour(juce::Colour(70, 30, 100));
    g.fillRoundedRectangle(nameRect, 4.0f);
    g.setColour(juce::Colour(140, 80, 190));
    g.drawRoundedRectangle(nameRect, 4.0f, 1.0f);

    // Draw pencil icon hint + name
    g.setColour(juce::Colour(220, 180, 255));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(currentName + "  \xe2\x9c\x8e", nameRect.reduced(6, 0), juce::Justification::centredLeft, true);

    // ---- Load button ----
    auto loadRect = juce::Rectangle<float>(loadX, btnY, 65.0f, btnH);
    g.setColour(juce::Colour(35, 80, 120));
    g.fillRoundedRectangle(loadRect, 5.0f);
    g.setColour(juce::Colour(100, 180, 255));
    g.drawRoundedRectangle(loadRect, 5.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText("Load", loadRect, juce::Justification::centred);

    // ---- Save button ----
    auto saveRect = juce::Rectangle<float>(saveX, btnY, 65.0f, btnH);
    g.setColour(juce::Colour(35, 100, 50));
    g.fillRoundedRectangle(saveRect, 5.0f);
    g.setColour(juce::Colour(100, 220, 130));
    g.drawRoundedRectangle(saveRect, 5.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText("Save", saveRect, juce::Justification::centred);
}

// =============================================================================
// NEW: Minimap overlay — shows full canvas with node dots and viewport rect
// Drawn in component PIXEL coords (after undoing zoom+pan transform)
// =============================================================================
void GraphCanvas::drawMinimap(juce::Graphics& g)
{
    auto mmRect = getMinimapRect();

    // Background
    g.setColour(juce::Colour(0xDD101018));
    g.fillRoundedRectangle(mmRect, 4.0f);

    // Border
    g.setColour(juce::Colour(0xFF505060));
    g.drawRoundedRectangle(mmRect, 4.0f, 1.0f);

    // Scale factors: virtual canvas -> minimap pixels
    float scaleX = mmRect.getWidth()  / virtualCanvasWidth;
    float scaleY = mmRect.getHeight() / virtualCanvasHeight;

    // Draw grid hint (every 4800 virtual units = one "old canvas" block)
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    for (int gx = 0; gx <= 4; ++gx)
    {
        float lx = mmRect.getX() + gx * (4800.0f * scaleX);
        g.drawVerticalLine((int)lx, mmRect.getY(), mmRect.getBottom());
    }
    for (int gy = 0; gy <= 4; ++gy)
    {
        float ly = mmRect.getY() + gy * (3200.0f * scaleY);
        g.drawHorizontalLine((int)ly, mmRect.getX(), mmRect.getRight());
    }

    // Draw nodes as small colored dots
    if (processor.mainGraph)
    {
        auto* mmGraph = getActiveGraph();
        if (!mmGraph) mmGraph = processor.mainGraph.get();
        for (auto* node : mmGraph->getNodes())
        {
            if (!shouldShowNode(node)) continue;

            auto bounds = getNodeBounds(node);
            float nx = mmRect.getX() + bounds.getCentreX() * scaleX;
            float ny = mmRect.getY() + bounds.getCentreY() * scaleY;

            // Color based on type
            auto* cache = getCachedNodeType(node->nodeID);
            juce::Colour dotColor = juce::Colour(0xFF00AAFF);  // default blue

            if (cache)
            {
                if (cache->isIO)
                    dotColor = juce::Colour(0xFFFFD700);  // gold for I/O
                else if (cache->isInstrument)
                    dotColor = juce::Colour(0xFF00FF88);  // green for instruments
                else if (cache->simpleConnector || cache->stereoMeter || cache->midiMonitor ||
                         cache->recorder || cache->manualSampler || cache->autoSampler ||
                         cache->midiPlayer || cache->ccStepper || cache->transientSplitter ||
                         cache->latcher)
                    dotColor = juce::Colour(0xFFFF8800);  // orange for system tools
                else if (node->isBypassed())
                    dotColor = juce::Colour(0xFF666666);  // gray for bypassed
            }

            float dotSize = 4.0f;
            g.setColour(dotColor);
            g.fillEllipse(nx - dotSize / 2, ny - dotSize / 2, dotSize, dotSize);
        }
    }

    // Draw viewport rectangle (current visible area)
    float visW = getVisibleWidth();
    float visH = getVisibleHeight();

    float vpX = mmRect.getX() + panOffsetX * scaleX;
    float vpY = mmRect.getY() + panOffsetY * scaleY;
    float vpW = visW * scaleX;
    float vpH = visH * scaleY;

    // Clamp viewport rect to minimap bounds
    vpW = juce::jmin(vpW, mmRect.getWidth());
    vpH = juce::jmin(vpH, mmRect.getHeight());

    auto vpRect = juce::Rectangle<float>(vpX, vpY, vpW, vpH);

    // Viewport fill (semi-transparent)
    g.setColour(juce::Colour(0x20FFFFFF));
    g.fillRect(vpRect);

    // Viewport border (bright)
    g.setColour(juce::Colour(0xCCFFFFFF));
    g.drawRect(vpRect, 1.5f);

    // Label
    g.setColour(juce::Colour(0x99FFFFFF));
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    int zoomPct = juce::roundToInt(zoomLevel * 100.0f);
    g.drawText("MAP " + juce::String(zoomPct) + "%",
               mmRect.getX() + 4, mmRect.getY() + 2, 80, 12,
               juce::Justification::centredLeft);
}

// =============================================================================
// NEW: Scrollbars — horizontal and vertical, drawn in component PIXEL coords
// =============================================================================
void GraphCanvas::drawScrollbars(juce::Graphics& g)
{
    float visW = getVisibleWidth();   // virtual visible area (for thumb ratio)
    float visH = getVisibleHeight();

    // ---- Horizontal Scrollbar ----
    {
        auto hBar = getHScrollbarRect();

        // Track background
        g.setColour(juce::Colour(0xAA181820));
        g.fillRect(hBar);
        g.setColour(juce::Colour(0xFF383840));
        g.drawRect(hBar, 1.0f);

        // Thumb
        float thumbRatio = visW / virtualCanvasWidth;
        if (thumbRatio < 1.0f)
        {
            float thumbW = juce::jmax(20.0f, hBar.getWidth() * thumbRatio);
            float maxPan = virtualCanvasWidth - visW;
            float thumbX = hBar.getX();
            if (maxPan > 0.0f)
                thumbX += (panOffsetX / maxPan) * (hBar.getWidth() - thumbW);

            auto thumbRect = juce::Rectangle<float>(thumbX, hBar.getY() + 2, thumbW, hBar.getHeight() - 4);

            g.setColour(isDraggingHScrollbar ? juce::Colour(0xFFAABBCC) : juce::Colour(0xFF667788));
            g.fillRoundedRectangle(thumbRect, 3.0f);
        }
    }

    // ---- Vertical Scrollbar ----
    {
        auto vBar = getVScrollbarRect();

        // Track background
        g.setColour(juce::Colour(0xAA181820));
        g.fillRect(vBar);
        g.setColour(juce::Colour(0xFF383840));
        g.drawRect(vBar, 1.0f);

        // Thumb
        float thumbRatio = visH / virtualCanvasHeight;
        if (thumbRatio < 1.0f)
        {
            float thumbH = juce::jmax(20.0f, vBar.getHeight() * thumbRatio);
            float maxPan = virtualCanvasHeight - visH;
            float thumbY = vBar.getY();
            if (maxPan > 0.0f)
                thumbY += (panOffsetY / maxPan) * (vBar.getHeight() - thumbH);

            auto thumbRect = juce::Rectangle<float>(vBar.getX() + 2, thumbY, vBar.getWidth() - 4, thumbH);

            g.setColour(isDraggingVScrollbar ? juce::Colour(0xFFAABBCC) : juce::Colour(0xFF667788));
            g.fillRoundedRectangle(thumbRect, 3.0f);
        }
    }

    // ---- Corner square (bottom-right where scrollbars meet) ----
    float pixW = (float)getWidth();
    float pixH = (float)getHeight();
    g.setColour(juce::Colour(0xFF181820));
    g.fillRect(juce::Rectangle<float>(pixW - scrollbarThickness, pixH - scrollbarThickness,
                                       scrollbarThickness, scrollbarThickness));
}
