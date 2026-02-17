// #D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_MidiPlayer_Integration.cpp
//
// INTEGRATION GUIDE: Adding MIDI Player to GraphCanvas
// Complete drop-in code for Paint, Mouse, Core, Menu
//
// =============================================================================

// =============================================================================
// STEP 1: Add includes to GraphCanvas.h and GraphCanvas_Core.cpp
// =============================================================================
// #include "MidiPlayerProcessor.h"

// =============================================================================
// STEP 2: Add to NodeTypeCache struct in GraphCanvas.h
// =============================================================================
//     MidiPlayerProcessor* midiPlayer = nullptr;

// =============================================================================
// STEP 3: Add member to GraphCanvas.h
// =============================================================================
//     bool hasMidiPlayer = false;
//     std::unique_ptr<juce::FileChooser> midiFileChooser;

// =============================================================================
// STEP 4: Update rebuildNodeTypeCache() in GraphCanvas_Core.cpp
// After the autoSampler line, add:
// =============================================================================
/*
        cache.midiPlayer = dynamic_cast<MidiPlayerProcessor*>(proc);
        
        if (cache.midiPlayer) {
            hasMidiPlayer = true;
            LOG("    MidiPlayer detected!");
        }
*/

// Also update the StereoMeterTimerID check:
/*
        if (hasStereoMeter || hasRecorder || hasSampler || hasMidiPlayer)
*/

// =============================================================================
// STEP 5: Update shouldShowNode()
// =============================================================================
/*
    if (dynamic_cast<MidiPlayerProcessor*>(proc)) return true;
*/

// =============================================================================
// STEP 6: Update getNodeBounds()
// =============================================================================
/*
    else if (dynamic_cast<MidiPlayerProcessor*>(proc))
    {
        // MIDI Player: wide node (same as Recorder)
        nodeHeight = Style::nodeHeight * 3.0f;   // 180px
        nodeWidth = Style::minNodeWidth * 4.0f;   // ~624px
    }
*/

// =============================================================================
// STEP 7: Add to right-click menu in GraphCanvas_PluginMenu.cpp
// After the Auto Sampler entry:
// =============================================================================
/*
    m.addItem(8, "MIDI Player");
    
    // In the result handler:
    else if (result == 8)
    {
        LOG("Adding MIDI Player node");
        auto nodePtr = safeThis->processor.mainGraph->addNode(
            std::make_unique<MidiPlayerProcessor>());
        if (nodePtr)
        {
            nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
            nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
            safeThis->markDirty();
            LOG("MIDI Player added successfully");
        }
    }
*/

// =============================================================================
// STEP 8: DRAWING CODE for GraphCanvas_Paint.cpp
// Add after the AutoSampler drawing block
// =============================================================================
/*
        // =====================================================================
        // MIDI PLAYER - 480×180 module
        // Layout:
        //   TOP ROW: Filename + [LOAD] button
        //   MIDDLE ROW: [PLAY][STOP][LOOP] | BPM display | Channel activity dots
        //   BOTTOM: Position slider (full width) + Time display
        // =====================================================================
        if (auto* midiPlayer = dynamic_cast<MidiPlayerProcessor*>(proc))
        {
            bool playing = midiPlayer->isPlaying();
            bool paused = midiPlayer->isPaused();
            bool hasFile = midiPlayer->hasFileLoaded();
            bool looping = midiPlayer->isLooping();
            auto contentArea = bounds.reduced(8, 6);
            
            // ─────────────────────────────────────────────────────────────
            // TOP ROW: Filename + Load Button
            // ─────────────────────────────────────────────────────────────
            auto topRow = contentArea.removeFromTop(26);
            
            // Filename display (dark inset)
            auto fileArea = topRow.removeFromLeft(topRow.getWidth() - 80);
            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(fileArea, 4.0f);
            g.setColour(juce::Colour(60, 60, 70));
            g.drawRoundedRectangle(fileArea, 4.0f, 1.0f);
            
            if (hasFile) {
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(13.0f));
                juce::String fname = midiPlayer->getFileName();
                if (fname.length() > 40) fname = fname.substring(0, 37) + "...";
                g.drawText(fname, fileArea.reduced(8, 0), juce::Justification::centredLeft);
            } else {
                g.setColour(juce::Colour(100, 100, 120));
                g.setFont(juce::Font(12.0f, juce::Font::italic));
                g.drawText("No file loaded", fileArea.reduced(8, 0), juce::Justification::centredLeft);
            }
            
            topRow.removeFromLeft(8);
            
            // LOAD button
            auto loadBtnArea = topRow;
            g.setColour(juce::Colour(60, 100, 160));
            g.fillRoundedRectangle(loadBtnArea, 5.0f);
            g.setColour(juce::Colour(80, 130, 200));
            g.drawRoundedRectangle(loadBtnArea, 5.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("LOAD", loadBtnArea, juce::Justification::centred);
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────
            // MIDDLE ROW: Transport Buttons + BPM + Channel Activity
            // ─────────────────────────────────────────────────────────────
            auto controlRow = contentArea.removeFromTop(44);
            
            // ▶ PLAY/PAUSE button
            auto playBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(playing ? juce::Colour(20, 70, 20) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(playBtnArea, 8.0f);
            g.setColour(playing ? juce::Colours::green.darker() : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(playBtnArea, 8.0f, 1.5f);
            
            if (playing) {
                // Pause icon (two vertical bars)
                float barW = 4.0f;
                float barH = playBtnArea.getHeight() * 0.45f;
                float cx = playBtnArea.getCentreX();
                float cy = playBtnArea.getCentreY();
                g.setColour(juce::Colours::limegreen);
                g.fillRect(cx - barW - 1.5f, cy - barH / 2, barW, barH);
                g.fillRect(cx + 1.5f, cy - barH / 2, barW, barH);
            } else {
                // Play triangle
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
            
            // Glow when playing
            if (playing) {
                g.setColour(juce::Colours::green.withAlpha(0.12f));
                g.fillRoundedRectangle(playBtnArea.expanded(3), 10.0f);
            }
            
            controlRow.removeFromLeft(4);
            
            // ■ STOP button
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
            
            // 🔁 LOOP toggle
            auto loopBtnArea = controlRow.removeFromLeft(50).reduced(3);
            g.setColour(looping ? juce::Colour(20, 60, 80) : juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(loopBtnArea, 8.0f);
            g.setColour(looping ? juce::Colour(0, 180, 220) : juce::Colour(80, 80, 90));
            g.drawRoundedRectangle(loopBtnArea, 8.0f, 1.5f);
            g.setColour(looping ? juce::Colour(0, 220, 255) : juce::Colour(100, 100, 120));
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("LOOP", loopBtnArea, juce::Justification::centred);
            
            controlRow.removeFromLeft(10);
            
            // BPM Display (large, prominent)
            auto bpmArea = controlRow.removeFromLeft(100);
            double bpm = midiPlayer->getCurrentBpm();
            bool tempoOverride = midiPlayer->isTempoOverrideEnabled();
            
            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(bpmArea, 4.0f);
            
            g.setColour(tempoOverride ? juce::Colours::orange : juce::Colour(200, 200, 220));
            g.setFont(juce::Font(22.0f, juce::Font::bold));
            g.drawText(juce::String(bpm, 1), bpmArea.reduced(4, 0), juce::Justification::centredLeft);
            
            g.setColour(juce::Colour(120, 120, 140));
            g.setFont(juce::Font(10.0f));
            g.drawText("BPM", bpmArea.reduced(4, 0), juce::Justification::bottomRight);
            
            controlRow.removeFromLeft(10);
            
            // Channel Activity Dots (16 dots in 2 rows of 8)
            auto chArea = controlRow;
            if (chArea.getWidth() > 80)
            {
                float dotSize = 8.0f;
                float dotSpacing = 11.0f;
                float startX = chArea.getX();
                float topY = chArea.getCentreY() - dotSpacing + 1;
                
                // Channel label
                g.setColour(juce::Colour(80, 80, 100));
                g.setFont(juce::Font(9.0f));
                
                for (int row = 0; row < 2; row++)
                {
                    for (int col = 0; col < 8; col++)
                    {
                        int ch = row * 8 + col;
                        float x = startX + col * dotSpacing;
                        float y = topY + row * (dotSpacing + 2);
                        
                        auto dotRect = juce::Rectangle<float>(x, y, dotSize, dotSize);
                        
                        if (midiPlayer->isChannelActive(ch))
                        {
                            // Active: bright color, drums = orange, others = green
                            juce::Colour dotColor = (ch == 9) 
                                ? juce::Colour(255, 160, 40)   // Drums: orange
                                : juce::Colour(60, 220, 100);  // Others: green
                            g.setColour(dotColor);
                            g.fillEllipse(dotRect);
                            g.setColour(dotColor.withAlpha(0.3f));
                            g.fillEllipse(dotRect.expanded(2));
                        }
                        else
                        {
                            g.setColour(juce::Colour(40, 40, 48));
                            g.fillEllipse(dotRect);
                        }
                    }
                }
            }
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────
            // BOTTOM: Position Slider + Time + Markers
            // ─────────────────────────────────────────────────────────────
            auto bottomArea = contentArea;
            
            // Time display row
            auto timeRow = bottomArea.removeFromTop(18);
            
            // Current time
            double currentSec = midiPlayer->getCurrentTimeSeconds();
            double totalSec = midiPlayer->getTotalTimeSeconds();
            
            int curMin = (int)(currentSec / 60.0);
            int curSec = (int)currentSec % 60;
            int totMin = (int)(totalSec / 60.0);
            int totSec = (int)totalSec % 60;
            
            g.setColour(playing ? juce::Colours::lightgreen : juce::Colour(180, 180, 200));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(juce::String::formatted("%d:%02d", curMin, curSec),
                       timeRow.removeFromLeft(50), juce::Justification::centredLeft);
            
            g.setColour(juce::Colour(100, 100, 120));
            g.setFont(juce::Font(12.0f));
            g.drawText(juce::String::formatted("/ %d:%02d", totMin, totSec),
                       timeRow.removeFromLeft(60), juce::Justification::centredLeft);
            
            // Markers (if any) on the right
            auto& markers = midiPlayer->getMarkers();
            if (!markers.empty())
            {
                g.setColour(juce::Colour(80, 80, 100));
                g.setFont(juce::Font(10.0f));
                
                // Show current section name
                juce::String currentSection = "";
                double curTick = midiPlayer->getCurrentTick();
                for (int mi = (int)markers.size() - 1; mi >= 0; mi--)
                {
                    if (curTick >= markers[mi].tick) {
                        currentSection = markers[mi].name;
                        break;
                    }
                }
                if (currentSection.isNotEmpty())
                {
                    g.setColour(juce::Colour(150, 180, 220));
                    g.drawText(currentSection, timeRow, juce::Justification::centredRight);
                }
            }
            
            bottomArea.removeFromTop(4);
            
            // =====================================================
            // POSITION SLIDER BAR (the main attraction)
            // Full width, tall, with thumb and marker ticks
            // =====================================================
            auto sliderArea = bottomArea.reduced(0, 2);
            float sliderH = sliderArea.getHeight();
            float trackH = 10.0f;
            float trackY = sliderArea.getCentreY() - trackH / 2;
            float trackX = sliderArea.getX() + 6;
            float trackW = sliderArea.getWidth() - 12;
            
            auto trackRect = juce::Rectangle<float>(trackX, trackY, trackW, trackH);
            
            // Track background (dark groove)
            g.setColour(juce::Colour(20, 20, 25));
            g.fillRoundedRectangle(trackRect, 5.0f);
            g.setColour(juce::Colour(50, 50, 60));
            g.drawRoundedRectangle(trackRect, 5.0f, 1.0f);
            
            // Played portion (blue fill)
            double pos = midiPlayer->getPositionNormalized();
            float fillW = (float)(pos * trackW);
            
            if (fillW > 0.5f)
            {
                auto fillRect = juce::Rectangle<float>(trackX, trackY, fillW, trackH);
                
                // Gradient from dark blue to bright blue
                g.setGradientFill(juce::ColourGradient(
                    juce::Colour(20, 60, 140), trackX, trackY,
                    juce::Colour(40, 140, 255), trackX + fillW, trackY,
                    false));
                g.fillRoundedRectangle(fillRect, 5.0f);
            }
            
            // Marker ticks on the track
            if (!markers.empty() && midiPlayer->getTotalTicks() > 0)
            {
                double totalT = midiPlayer->getTotalTicks();
                g.setColour(juce::Colour(200, 200, 100).withAlpha(0.6f));
                
                for (auto& marker : markers)
                {
                    float mx = trackX + (float)(marker.tick / totalT) * trackW;
                    g.drawVerticalLine((int)mx, trackY - 3, trackY + trackH + 3);
                }
            }
            
            // Thumb (large, visible circle)
            float thumbX = trackX + fillW;
            float thumbRadius = 9.0f;
            auto thumbRect = juce::Rectangle<float>(
                thumbX - thumbRadius, sliderArea.getCentreY() - thumbRadius,
                thumbRadius * 2, thumbRadius * 2);
            
            // Thumb shadow
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.fillEllipse(thumbRect.translated(1, 1));
            
            // Thumb body
            g.setColour(playing ? juce::Colour(60, 180, 255) : juce::Colour(160, 160, 180));
            g.fillEllipse(thumbRect);
            
            // Thumb highlight (inner bright spot)
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.fillEllipse(thumbRect.reduced(3));
            
            // Thumb border
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.drawEllipse(thumbRect, 1.5f);
        }
*/

// =============================================================================
// STEP 9: CLICK HANDLING for GraphCanvas_Mouse.cpp
// Add after the AutoSampler click handling block
// =============================================================================
/*
    // =========================================================================
    // Handle MIDI Player interactions
    // =========================================================================
    if (auto* midiPlayer = dynamic_cast<MidiPlayerProcessor*>(clickedNode->getProcessor()))
    {
        auto nodeBounds = getNodeBounds(clickedNode);
        auto localClick = e.position - nodeBounds.getPosition();
        auto contentArea = nodeBounds.reduced(8, 6);
        
        auto topRow = contentArea.removeFromTop(26);
        
        // LOAD button (right side of top row)
        auto fileAreaWidth = topRow.getWidth() - 80;
        topRow.removeFromLeft(fileAreaWidth + 8);
        auto loadBtnArea = topRow;
        loadBtnArea = loadBtnArea.translated(8, 6);
        
        if (loadBtnArea.contains(localClick))
        {
            midiFileChooser = std::make_unique<juce::FileChooser>(
                "Select MIDI File",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.mid;*.midi");
            
            juce::Component::SafePointer<GraphCanvas> safeThis(this);
            auto nodeID = clickedNode->nodeID;
            
            midiFileChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeThis, nodeID](const juce::FileChooser& fc)
                {
                    if (!safeThis) return;
                    auto result = fc.getResult();
                    if (!result.exists()) return;
                    
                    if (auto* node = safeThis->processor.mainGraph->getNodeForId(nodeID))
                    {
                        if (auto* player = dynamic_cast<MidiPlayerProcessor*>(node->getProcessor()))
                        {
                            if (!player->loadFile(result))
                            {
                                juce::NativeMessageBox::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "MIDI Player",
                                    "Failed to load MIDI file:\n" + result.getFileName());
                            }
                            safeThis->repaint();
                        }
                    }
                });
            return;
        }
        
        contentArea.removeFromTop(6);
        auto controlRow = contentArea.removeFromTop(44);
        
        // PLAY/PAUSE button
        auto playBtnArea = controlRow.removeFromLeft(50).reduced(3);
        playBtnArea = playBtnArea.translated(8, 38);
        
        if (playBtnArea.contains(localClick))
        {
            midiPlayer->togglePlayPause();
            repaint();
            return;
        }
        
        controlRow.removeFromLeft(4);
        
        // STOP button
        auto stopBtnArea = controlRow.removeFromLeft(50).reduced(3);
        stopBtnArea = stopBtnArea.translated(8 + 50 + 4, 38);
        
        if (stopBtnArea.contains(localClick))
        {
            midiPlayer->stop();
            repaint();
            return;
        }
        
        controlRow.removeFromLeft(4);
        
        // LOOP toggle
        auto loopBtnArea = controlRow.removeFromLeft(50).reduced(3);
        loopBtnArea = loopBtnArea.translated(8 + 50 + 4 + 50 + 4, 38);
        
        if (loopBtnArea.contains(localClick))
        {
            midiPlayer->setLooping(!midiPlayer->isLooping());
            repaint();
            return;
        }
        
        // BPM area (click to toggle tempo override)
        controlRow.removeFromLeft(10);
        auto bpmArea = controlRow.removeFromLeft(100);
        bpmArea = bpmArea.translated(8 + 50 + 4 + 50 + 4 + 50 + 10, 38);
        
        if (bpmArea.contains(localClick))
        {
            midiPlayer->setTempoOverride(!midiPlayer->isTempoOverrideEnabled());
            repaint();
            return;
        }
        
        // =====================================================
        // POSITION SLIDER DRAG
        // Check if click is in the bottom slider area
        // =====================================================
        contentArea.removeFromTop(6);
        auto bottomArea = contentArea;
        bottomArea.removeFromTop(18);  // Time row
        bottomArea.removeFromTop(4);   // Gap
        auto sliderArea = bottomArea.reduced(0, 2);
        sliderArea = sliderArea.translated(8, 6 + 26 + 6 + 44 + 6 + 18 + 4);
        
        float trackX = sliderArea.getX() + 6;
        float trackW = sliderArea.getWidth() - 12;
        
        // Expanded hit area for the slider (easy to grab)
        auto sliderHitArea = juce::Rectangle<float>(
            trackX - 10, sliderArea.getY() - 5,
            trackW + 20, sliderArea.getHeight() + 10);
        
        if (sliderHitArea.contains(localClick))
        {
            // Calculate normalized position from click X
            float clickX = localClick.x;
            double normalizedPos = (double)(clickX - trackX) / (double)trackW;
            normalizedPos = juce::jlimit(0.0, 1.0, normalizedPos);
            
            midiPlayer->setPositionNormalized(normalizedPos);
            repaint();
            return;
        }
    }
*/

// =============================================================================
// STEP 10: SLIDER DRAG support in mouseDrag()
// Add to the mouseDrag handler for continuous slider dragging:
// =============================================================================
/*
    // MIDI Player slider dragging
    if (draggingSlider && midiPlayerNode)
    {
        auto nodeBounds = getNodeBounds(midiPlayerNode);
        auto contentArea = nodeBounds.reduced(8, 6);
        
        float trackX = contentArea.getX() + 6;
        float trackW = contentArea.getWidth() - 12;
        
        float mouseX = e.position.x - nodeBounds.getX();
        double normalizedPos = (double)(mouseX - 6) / (double)(contentArea.getWidth() - 12);
        normalizedPos = juce::jlimit(0.0, 1.0, normalizedPos);
        
        midiPlayerNode->setPositionNormalized(normalizedPos);
        repaint();
    }
*/

// =============================================================================
// STEP 11: BPM scroll wheel support (optional, nice UX)
// In mouseWheelMove(), add:
// =============================================================================
/*
    // If mouse is over BPM area of MIDI Player, scroll to adjust tempo
    if (midiPlayer && midiPlayer->isTempoOverrideEnabled())
    {
        double delta = wheel.deltaY * 2.0;
        double newBpm = midiPlayer->getTempoOverrideBpm() + delta;
        midiPlayer->setTempoOverrideBpm(newBpm);
        repaint();
    }
*/

// =============================================================================
// STEP 12: drawNodeButtons for MIDI Player
// Only show F (folder) and X (delete) buttons:
// =============================================================================
/*
    if (dynamic_cast<MidiPlayerProcessor*>(proc))
    {
        // F button (open containing folder)
        auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(180, 160, 80));
        g.fillRoundedRectangle(folderRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("F", folderRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }
*/

// =============================================================================
// STEP 13: Add to CMakeLists.txt source list
// =============================================================================
/*
    src/MidiPlayerProcessor.h
    src/MidiPlayerProcessor.cpp
*/
