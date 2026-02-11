// ==============================================================================
//  WiringCanvas_Paint.cpp
//  OnStage — Painting: grid, wires, nodes, pins, buttons
//
//  Matches Colosseum's visual language:
//    • Dark grid background
//    • Bezier wires (blue for audio, green for sidechain)
//    • Rounded nodes with title bar
//    • B (bypass, green/red), E (editor, gold), X (delete, red) buttons
//    • I/O nodes show level meters
//    • PreAmp nodes: no E button (has inline slider instead)
//    • Recorder nodes: custom on-surface GUI (record/stop/waveform/meters)
//    • Guitar nodes: deep purple title bar and body
//
//  FIX: Sidechain pins (green) now always visible for DynamicEQ/Compressor
// ==============================================================================

#include "WiringCanvas.h"

// ==============================================================================
//  Main paint
// ==============================================================================

void WiringCanvas::paint (juce::Graphics& g)
{
    g.fillAll (WiringStyle::colBackground);

    // --- Grid ----------------------------------------------------------------
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    for (int x = 0; x < getWidth(); x += WiringStyle::gridSize)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += WiringStyle::gridSize)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    auto& graph = stageGraph.getGraph();

    // Ensure cache is populated
    if (nodeCache.empty() && graph.getNumNodes() > 0)
        rebuildNodeCache();

    // --- Draw connections (behind nodes) ------------------------------------
    for (auto& conn : graph.getConnections())
    {
        auto* srcNode = graph.getNodeForId (conn.source.nodeID);
        auto* dstNode = graph.getNodeForId (conn.destination.nodeID);
        if (! srcNode || ! dstNode) continue;
        if (! shouldShowNode (srcNode) || ! shouldShowNode (dstNode)) continue;

        PinID srcPin { srcNode->nodeID, conn.source.channelIndex,      false };
        PinID dstPin { dstNode->nodeID, conn.destination.channelIndex, true  };

        auto start = getPinPos (srcNode, srcPin);
        auto end   = getPinPos (dstNode, dstPin);

        // Determine wire color
        juce::Colour wireColor = WiringStyle::colWireIdle;
        float thickness = 2.0f;

        bool isHovered = (conn == hoveredConnection);

        if (! srcNode->isBypassed() && ! dstNode->isBypassed())
        {
            // Check if this is a sidechain connection
            bool isSidechain = false;
            if (conn.destination.channelIndex >= 2)
            {
                auto* dstCache = getCached (dstNode->nodeID);
                if (dstCache && dstCache->hasSidechain)
                    isSidechain = true;
            }

            wireColor = isSidechain ? WiringStyle::colPinSidechain
                                    : WiringStyle::colPinAudio;
            thickness = 2.5f;

            // Brighten if signal present (check I/O meters)
            auto* srcCache = getCached (srcNode->nodeID);
            if (srcCache && srcCache->isAudioInput)
            {
                int ch = conn.source.channelIndex;
                if (ch < 32 && stageGraph.inputRms[ch].load (std::memory_order_relaxed) > 0.001f)
                {
                    wireColor = wireColor.brighter (0.5f);
                    thickness = 3.5f;
                }
            }
            else
            {
                // For effect→effect, assume signal present when not bypassed
                wireColor = wireColor.brighter (0.2f);
            }
        }

        if (isHovered)
        {
            wireColor = WiringStyle::colWireHover;
            thickness = 3.5f;
        }

        drawWire (g, start, end, wireColor, thickness);
    }

    // --- Draw active drag cable ----------------------------------------------
    if (dragCable.active)
        drawWire (g, getPinCenter (dragCable.sourcePin),
                  dragCable.currentPos, dragCable.color, 2.5f);

    // --- Draw nodes ----------------------------------------------------------
    for (auto* node : graph.getNodes())
    {
        if (! shouldShowNode (node)) continue;
        drawNode (g, node);
    }
}

// ==============================================================================
//  Draw a single node
// ==============================================================================

void WiringCanvas::drawNode (juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto bounds = getNodeBounds (node);
    auto* cache = getCached (node->nodeID);

    // --- Recorder nodes get fully custom drawing -----------------------------
    if (cache && cache->isRecorder)
    {
        drawRecorderNode (g, node, bounds);
        drawNodePins (g, node);
        return;
    }

    bool isIO = cache && (cache->isAudioInput || cache->isAudioOutput || cache->isPlayback);

    // --- Determine colours (guitar nodes get purple theme) -------------------
    bool isGuitar = false;
    if (! isIO && cache && cache->effectNode)
        isGuitar = (cache->effectNode->getNodeCategory() == "Guitar");

    juce::Colour bodyCol, titleCol, borderCol;

    if (node->isBypassed())
    {
        bodyCol   = WiringStyle::colNodeBodyBypassed;
        titleCol  = WiringStyle::colNodeTitleBypassed;
        borderCol = juce::Colours::grey.darker();
    }
    else if (isIO)
    {
        bodyCol   = WiringStyle::colIONodeBody;
        titleCol  = WiringStyle::colNodeTitle;
        borderCol = WiringStyle::colNodeBorder;
    }
    else if (isGuitar)
    {
        bodyCol   = WiringStyle::colGuitarNodeBody;
        titleCol  = WiringStyle::colGuitarNodeTitle;
        borderCol = WiringStyle::colGuitarNodeBorder;
    }
    else
    {
        bodyCol   = WiringStyle::colNodeBody;
        titleCol  = WiringStyle::colNodeTitle;
        borderCol = WiringStyle::colNodeBorder;
    }

    // --- Body ----------------------------------------------------------------
    g.setColour (bodyCol);
    g.fillRoundedRectangle (bounds, WiringStyle::nodeRounding);

    // --- Border --------------------------------------------------------------
    g.setColour (borderCol);
    g.drawRoundedRectangle (bounds, WiringStyle::nodeRounding, 2.0f);

    // --- Title bar -----------------------------------------------------------
    auto titleBounds = bounds.removeFromTop (WiringStyle::nodeTitleHeight);
    g.setColour (titleCol);
    g.fillRoundedRectangle (titleBounds, WiringStyle::nodeRounding);
    // Square off the bottom corners of the title bar
    g.fillRect (titleBounds.removeFromBottom (WiringStyle::nodeRounding));

    // --- Title text ----------------------------------------------------------
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    juce::String title = cache ? cache->displayName : "Unknown";
    g.drawText (title, titleBounds.reduced (5, 0),
                juce::Justification::centredLeft, true);

    // --- I/O level meters (inside body of Audio Input / Audio Output) --------
    // FIXED: Meters now aligned to the RIGHT side of the node
    if (cache && (cache->isAudioInput || cache->isAudioOutput))
    {
        auto meterArea = bounds.reduced (6, 4);
        auto* rmsArray = cache->isAudioInput ? stageGraph.inputRms
                                              : stageGraph.outputRms;

        auto* proc = node->getProcessor();
        int numCh = cache->isAudioInput ? proc->getTotalNumOutputChannels()
                                        : proc->getTotalNumInputChannels();
        numCh = juce::jmin (numCh, 8);  // cap for display

        if (numCh > 0 && meterArea.getHeight() > 4)
        {
            float barW = juce::jmin (8.0f, (meterArea.getWidth() - 2.0f) / numCh);
            float barH = meterArea.getHeight();

            // Calculate total width of all meters and align to the right
            float totalMetersWidth = numCh * barW + (numCh - 1);  // bars + gaps
            float startX = meterArea.getRight() - totalMetersWidth;

            for (int ch = 0; ch < numCh; ++ch)
            {
                float level = juce::jlimit (0.0f, 1.0f,
                    rmsArray[ch].load (std::memory_order_relaxed));

                auto bar = juce::Rectangle<float> (
                    startX + ch * (barW + 1), meterArea.getY(),
                    barW, barH);

                g.setColour (juce::Colour (25, 25, 30));
                g.fillRect (bar);

                if (level > 0.0f)
                {
                    float fillH = level * barH;
                    juce::Colour mCol = level < 0.7f ? juce::Colours::limegreen
                                      : (level < 0.9f ? juce::Colours::yellow
                                                       : juce::Colours::red);
                    g.setColour (mCol);
                    g.fillRect (bar.getX(), bar.getBottom() - fillH, barW, fillH);
                }
            }
        }
    }

    // --- Draw pins -----------------------------------------------------------
    drawNodePins (g, node);

    // --- Draw buttons (B / E / X) — only on effect nodes ---------------------
    if (cache && cache->effectNode && ! cache->isRecorder)
        drawNodeButtons (g, node);

    // --- Draw ON/OFF toggle for I/O + Playback nodes -------------------------
    if (cache && (cache->isAudioInput || cache->isAudioOutput || cache->isPlayback))
    {
        auto nb = getNodeBounds (node);
        nb.removeFromTop (WiringStyle::nodeTitleHeight);
        float btnY = nb.getBottom() - WiringStyle::btnMargin - WiringStyle::btnHeight;
        float btnX = nb.getX() + WiringStyle::btnMargin;

        auto toggleRect = juce::Rectangle<float> (btnX, btnY,
            WiringStyle::btnWidth * 1.5f, WiringStyle::btnHeight);
        g.setColour (node->isBypassed() ? juce::Colours::grey.darker()
                                         : juce::Colours::green);
        g.fillRoundedRectangle (toggleRect, 3.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText (node->isBypassed() ? "OFF" : "ON", toggleRect,
                    juce::Justification::centred);
    }
}

// ==============================================================================
//  Draw Recorder node — full custom on-surface GUI
//
//  Layout (360×160):
//    TOP ROW (26px):  Editable name textbox (left) + Sync toggle (right)
//    MIDDLE ROW (44px): Record btn | Stop btn | Time display | Level meters
//    BOTTOM (remaining): Coast-to-coast waveform + X (delete) button
// ==============================================================================

void WiringCanvas::drawRecorderNode (juce::Graphics& g,
                                      juce::AudioProcessorGraph::Node* node,
                                      juce::Rectangle<float> bounds)
{
    auto* cache = getCached (node->nodeID);
    if (! cache || ! cache->recorder) return;

    auto* recorder = cache->recorder;
    bool isRecording = recorder->isCurrentlyRecording();

    // --- Body ----------------------------------------------------------------
    g.setColour (WiringStyle::colNodeBody);
    g.fillRoundedRectangle (bounds, WiringStyle::nodeRounding);

    // Recording border glow
    g.setColour (isRecording ? juce::Colours::red.withAlpha (0.7f)
                              : WiringStyle::colNodeBorder);
    g.drawRoundedRectangle (bounds, WiringStyle::nodeRounding, isRecording ? 2.5f : 2.0f);

    auto contentArea = bounds.reduced (8, 6);

    // ─────────────────────────────────────────────────────────────────────────
    // TOP ROW: Name Textbox + Sync Toggle
    // ─────────────────────────────────────────────────────────────────────────
    auto topRow = contentArea.removeFromTop (24);

    // Name textbox area
    auto nameBoxArea = topRow.removeFromLeft (230).reduced (0, 1);
    g.setColour (juce::Colour (45, 45, 50));
    g.fillRoundedRectangle (nameBoxArea, 4.0f);
    g.setColour (juce::Colours::grey);
    g.drawRoundedRectangle (nameBoxArea, 4.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (13.0f));
    juce::String displayName = recorder->getRecorderName();
    if (displayName.length() > 28) displayName = displayName.substring (0, 25) + "...";
    g.drawText (displayName, nameBoxArea.reduced (8, 0), juce::Justification::centredLeft);

    // Sync mode toggle (right side)
    auto syncArea = topRow.removeFromRight (65);
    bool syncMode = recorder->isSyncMode();
    g.setColour (syncMode ? juce::Colour (0, 180, 180) : juce::Colour (80, 80, 80));
    g.fillRoundedRectangle (syncArea.reduced (2), 4.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText (syncMode ? "SYNC" : "INDEP", syncArea, juce::Justification::centred);

    // Folder button (between name and sync)
    auto folderArea = topRow.removeFromRight (22).reduced (1);
    g.setColour (juce::Colour (60, 60, 65));
    g.fillRoundedRectangle (folderArea, 3.0f);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (12.0f));
    g.drawText (juce::String::charToString (0x1F4C2), folderArea, juce::Justification::centred);

    contentArea.removeFromTop (4);

    // ─────────────────────────────────────────────────────────────────────────
    // MIDDLE ROW: Record / Stop buttons + Time Display + Level Meters
    // ─────────────────────────────────────────────────────────────────────────
    auto controlRow = contentArea.removeFromTop (40);

    // Record button: rounded rect with red circle
    auto recordBtnArea = controlRow.removeFromLeft (46).reduced (3);
    g.setColour (isRecording ? juce::Colour (80, 20, 20) : juce::Colour (50, 50, 55));
    g.fillRoundedRectangle (recordBtnArea, 8.0f);
    g.setColour (isRecording ? juce::Colours::red.darker() : juce::Colours::grey);
    g.drawRoundedRectangle (recordBtnArea, 8.0f, 1.5f);

    float circleSize = recordBtnArea.getHeight() * 0.45f;
    auto circleArea = recordBtnArea.withSizeKeepingCentre (circleSize, circleSize);
    g.setColour (isRecording ? juce::Colours::red : juce::Colour (180, 50, 50));
    g.fillEllipse (circleArea);

    if (isRecording)
    {
        g.setColour (juce::Colours::red.withAlpha (0.2f));
        g.fillEllipse (circleArea.expanded (5));
    }

    controlRow.removeFromLeft (6);

    // Stop button: square with blue square inside
    auto stopBtnArea = controlRow.removeFromLeft (46).reduced (3);
    g.setColour (juce::Colour (50, 50, 55));
    g.fillRoundedRectangle (stopBtnArea, 4.0f);
    g.setColour (juce::Colours::grey);
    g.drawRoundedRectangle (stopBtnArea, 4.0f, 1.5f);

    float squareSize = stopBtnArea.getHeight() * 0.4f;
    auto squareArea = stopBtnArea.withSizeKeepingCentre (squareSize, squareSize);
    g.setColour (juce::Colour (30, 144, 255));  // Dodger blue
    g.fillRect (squareArea);

    controlRow.removeFromLeft (10);

    // Time display
    double recordingSeconds = recorder->getRecordingLengthSeconds();
    int hours   = (int)(recordingSeconds / 3600.0);
    int minutes = (int)(recordingSeconds / 60.0) % 60;
    int seconds = (int)(recordingSeconds) % 60;
    int tenths  = (int)((recordingSeconds - (int)recordingSeconds) * 10);

    juce::String timeStr;
    if (hours > 0)
        timeStr = juce::String::formatted ("%d:%02d:%02d", hours, minutes, seconds);
    else
        timeStr = juce::String::formatted ("%02d:%02d.%d", minutes, seconds, tenths);

    auto timeArea = controlRow.removeFromLeft (100);
    g.setColour (isRecording ? juce::Colours::lightgreen : juce::Colour (150, 150, 150));
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText (timeStr, timeArea, juce::Justification::centred);

    // Level meters (vertical stereo, right side)
    auto meterArea = controlRow.removeFromRight (30).reduced (2, 4);
    float meterW = (meterArea.getWidth() - 3) / 2.0f;
    float meterH = meterArea.getHeight();

    float levelL = juce::jlimit (0.0f, 1.0f, recorder->getLeftLevel());
    float levelR = juce::jlimit (0.0f, 1.0f, recorder->getRightLevel());

    // Left meter
    auto meterL = juce::Rectangle<float> (meterArea.getX(), meterArea.getY(), meterW, meterH);
    g.setColour (juce::Colour (25, 25, 30));
    g.fillRect (meterL);
    if (levelL > 0.0f)
    {
        float fillH = levelL * meterH;
        juce::Colour mCol = levelL < 0.7f ? juce::Colours::limegreen
                          : (levelL < 0.9f ? juce::Colours::yellow : juce::Colours::red);
        g.setColour (mCol);
        g.fillRect (meterL.getX(), meterL.getBottom() - fillH, meterW, fillH);
    }

    // Right meter
    auto meterR = juce::Rectangle<float> (meterArea.getX() + meterW + 3, meterArea.getY(),
                                           meterW, meterH);
    g.setColour (juce::Colour (25, 25, 30));
    g.fillRect (meterR);
    if (levelR > 0.0f)
    {
        float fillH = levelR * meterH;
        juce::Colour mCol = levelR < 0.7f ? juce::Colours::limegreen
                          : (levelR < 0.9f ? juce::Colours::yellow : juce::Colours::red);
        g.setColour (mCol);
        g.fillRect (meterR.getX(), meterR.getBottom() - fillH, meterW, fillH);
    }

    // X (delete) button — right of meters
    auto xBtnArea = controlRow.removeFromRight (22).reduced (1, 10);
    g.setColour (juce::Colours::darkred);
    g.fillRoundedRectangle (xBtnArea, 3.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("X", xBtnArea, juce::Justification::centred);

    contentArea.removeFromTop (4);

    // ─────────────────────────────────────────────────────────────────────────
    // BOTTOM: Coast-to-Coast Waveform Display
    // ─────────────────────────────────────────────────────────────────────────
    auto waveformArea = contentArea.reduced (0, 2);

    // Waveform background
    g.setColour (juce::Colour (18, 18, 22));
    g.fillRoundedRectangle (waveformArea, 5.0f);

    // Center line
    float centerY = waveformArea.getCentreY();
    g.setColour (juce::Colour (60, 60, 70));
    g.drawHorizontalLine ((int) centerY, waveformArea.getX() + 2,
                          waveformArea.getRight() - 2);

    // Get waveform data (1 sample per pixel)
    int waveWidth = (int) waveformArea.getWidth() - 4;
    auto waveData = recorder->getWaveformData (waveWidth);

    if (! waveData.empty())
    {
        float halfH = (waveformArea.getHeight() - 8) * 0.48f;
        float startX = waveformArea.getX() + 2;

        // Build waveform path
        juce::Path wavePath;
        wavePath.startNewSubPath (startX, centerY);

        // Top edge (max values)
        for (int i = 0; i < waveWidth && i < (int) waveData.size(); ++i)
        {
            float maxV = std::max (waveData[i].maxL, waveData[i].maxR);
            wavePath.lineTo (startX + i, centerY - (maxV * halfH));
        }

        // Bottom edge (min values, reverse)
        for (int i = waveWidth - 1; i >= 0 && i < (int) waveData.size(); --i)
        {
            float minV = std::min (waveData[i].minL, waveData[i].minR);
            wavePath.lineTo (startX + i, centerY - (minV * halfH));
        }

        wavePath.closeSubPath();

        // Fill waveform
        juce::Colour waveCol = isRecording ? juce::Colour (0, 200, 255)
                                            : juce::Colour (100, 100, 120);
        g.setColour (waveCol.withAlpha (0.5f));
        g.fillPath (wavePath);

        // Waveform outline
        g.setColour (waveCol);
        g.strokePath (wavePath, juce::PathStrokeType (1.0f));
    }

    // Waveform border
    g.setColour (juce::Colour (70, 70, 80));
    g.drawRoundedRectangle (waveformArea, 5.0f, 1.0f);

    // Show filename if recording exists
    if (recorder->hasRecording() && ! isRecording)
    {
        juce::String fname = recorder->getLastRecordingFile().getFileName();
        if (fname.length() > 45) fname = "..." + fname.substring (fname.length() - 42);
        g.setColour (juce::Colours::grey.withAlpha (0.7f));
        g.setFont (juce::Font (10.0f));
        g.drawText (fname, waveformArea.reduced (6, 0).removeFromBottom (14),
                    juce::Justification::centredLeft);
    }
}

// ==============================================================================
//  Draw pins on a node  (audio only — blue / green for sidechain)
//  FIX: Always draw 4 input pins for nodes with sidechain (green pins visible)
// ==============================================================================

void WiringCanvas::drawNodePins (juce::Graphics& g,
                                  juce::AudioProcessorGraph::Node* node)
{
    auto* proc = node->getProcessor();
    if (! proc) return;

    auto* cache = getCached (node->nodeID);
    
    // Get reported channel counts
    int numIn  = proc->getTotalNumInputChannels();
    int numOut = proc->getTotalNumOutputChannels();
    
    // FIX: For nodes with sidechain, always show 4 input pins (2 main + 2 sidechain)
    if (cache && cache->hasSidechain && numIn < 4)
    {
        numIn = 4;
    }

    // Input pins (top)
    for (int i = 0; i < numIn; ++i)
    {
        PinID p { node->nodeID, i, true };
        auto pos = getPinPos (node, p);
        bool highlighted = (highlightPin == p);
        juce::Colour color = getPinColor (p, node);
        drawPin (g, pos, color, false, highlighted);
    }

    // Output pins (bottom) — Recorder has 0 outputs, so nothing drawn
    for (int i = 0; i < numOut; ++i)
    {
        PinID p { node->nodeID, i, false };
        auto pos = getPinPos (node, p);
        bool highlighted = (highlightPin == p);
        juce::Colour color = getPinColor (p, node);
        drawPin (g, pos, color, false, highlighted);
    }
}

// ==============================================================================
//  Draw a single pin dot
// ==============================================================================

void WiringCanvas::drawPin (juce::Graphics& g, juce::Point<float> pos,
                             juce::Colour color, bool hovered, bool highlighted)
{
    float size = WiringStyle::pinSize;
    if (hovered || highlighted) size *= 1.3f;

    // Yellow highlight when dragging a cable over a valid target
    bool isValidTarget = false;
    if (dragCable.active && highlighted)
        isValidTarget = canConnect (dragCable.sourcePin, highlightPin);

    if (isValidTarget)
    {
        g.setColour (WiringStyle::colPinValidTarget.withAlpha (0.6f));
        g.fillEllipse (pos.x - size / 2 - 3, pos.y - size / 2 - 3,
                       size + 6, size + 6);
        g.setColour (WiringStyle::colPinValidTarget);
        g.fillEllipse (pos.x - size / 2, pos.y - size / 2, size, size);
        g.setColour (juce::Colours::white);
        g.drawEllipse (pos.x - size / 2, pos.y - size / 2, size, size, 2.0f);
    }
    else
    {
        g.setColour (color);
        g.fillEllipse (pos.x - size / 2, pos.y - size / 2, size, size);
        g.setColour (juce::Colours::white);
        g.drawEllipse (pos.x - size / 2, pos.y - size / 2, size, size, 1.5f);
    }
}

// ==============================================================================
//  Draw the B / E / X buttons at the bottom of an effect node
//  PreAmp nodes: skip E button (inline slider replaces the editor popup)
//  Recorder nodes: handled separately in drawRecorderNode
// ==============================================================================

void WiringCanvas::drawNodeButtons (juce::Graphics& g,
                                     juce::AudioProcessorGraph::Node* node)
{
    auto nb = getNodeBounds (node);
    nb.removeFromTop (WiringStyle::nodeTitleHeight);

    auto* cache = getCached (node->nodeID);
    bool isPreAmp = cache && cache->effectNode
                    && cache->effectNode->getEffectType() == "PreAmp";

    float btnY = nb.getBottom() - WiringStyle::btnMargin - WiringStyle::btnHeight;
    float btnX = nb.getX() + WiringStyle::btnMargin;

    // --- B (Bypass) ----------------------------------------------------------
    auto bypassRect = juce::Rectangle<float> (btnX, btnY,
        WiringStyle::btnWidth, WiringStyle::btnHeight);
    g.setColour (node->isBypassed() ? juce::Colours::red : juce::Colours::lightgreen);
    g.fillRoundedRectangle (bypassRect, 3.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("B", bypassRect, juce::Justification::centred);
    btnX += WiringStyle::btnWidth + WiringStyle::btnSpacing;

    // --- E (Editor) — skip for PreAmp (has inline slider) --------------------
    if (! isPreAmp)
    {
        auto editRect = juce::Rectangle<float> (btnX, btnY,
            WiringStyle::btnWidth, WiringStyle::btnHeight);
        g.setColour (WiringStyle::colEditor);  // Gold
        g.fillRoundedRectangle (editRect, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawText ("E", editRect, juce::Justification::centred);
    }
    btnX += WiringStyle::btnWidth + WiringStyle::btnSpacing;

    // --- X (Delete) ----------------------------------------------------------
    auto deleteRect = juce::Rectangle<float> (btnX, btnY,
        WiringStyle::btnWidth, WiringStyle::btnHeight);
    g.setColour (juce::Colours::darkred);
    g.fillRoundedRectangle (deleteRect, 3.0f);
    g.setColour (juce::Colours::white);
    g.drawText ("X", deleteRect, juce::Justification::centred);
}

// ==============================================================================
//  Draw a bezier wire  (output → input, curves downward)
// ==============================================================================

void WiringCanvas::drawWire (juce::Graphics& g,
                              juce::Point<float> start, juce::Point<float> end,
                              juce::Colour col, float thickness)
{
    juce::Path p;
    p.startNewSubPath (start);
    p.cubicTo (start.x, start.y + 50, end.x, end.y - 50, end.x, end.y);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (thickness,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ==============================================================================
//  Button bounds helper  (index: 0=B, 1=E, 2=X)
// ==============================================================================

juce::Rectangle<float> WiringCanvas::getButtonRect (
    juce::Rectangle<float> nodeBounds, int index)
{
    nodeBounds.removeFromTop (WiringStyle::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - WiringStyle::btnMargin - WiringStyle::btnHeight;
    float btnX = nodeBounds.getX() + WiringStyle::btnMargin
                 + index * (WiringStyle::btnWidth + WiringStyle::btnSpacing);
    return { btnX, btnY, WiringStyle::btnWidth, WiringStyle::btnHeight };
}
