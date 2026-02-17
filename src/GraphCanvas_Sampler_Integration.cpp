// #D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Sampler_Integration.cpp
//
// INTEGRATION GUIDE: Adding Manual Sampler + Auto Sampler to GraphCanvas
// Follow the same pattern as GraphCanvas_Recorder_Integration.cpp
//
// =============================================================================

// =============================================================================
// STEP 1: Add includes at top of GraphCanvas.h
// =============================================================================
// #include "ManualSamplerProcessor.h"
// #include "AutoSamplerProcessor.h"
// #include "AutoSamplerEditorComponent.h"

// =============================================================================
// STEP 2: Add to NodeTypeCache struct in GraphCanvas.h
// =============================================================================
// Add these members alongside the existing recorder member:
//     ManualSamplerProcessor* manualSampler = nullptr;
//     AutoSamplerProcessor* autoSampler = nullptr;

// =============================================================================
// STEP 3: Add member variables to GraphCanvas class in GraphCanvas.h
// =============================================================================
// Alongside hasRecorder:
//     bool hasSampler = false;
//
// For Auto Sampler editor file choosers (alongside vst2FileChooser):
//     std::unique_ptr<juce::FileChooser> samplerFolderChooser;

// =============================================================================
// STEP 4: Add method declarations to GraphCanvas.h
// =============================================================================
//     void showAutoSamplerEditor(juce::AudioProcessorGraph::Node* node);

// =============================================================================
// STEP 5: Update getNodeBounds() in GraphCanvas.cpp (where Recorder bounds are)
// Add these cases after the RecorderProcessor case:
// =============================================================================
/*
    else if (dynamic_cast<ManualSamplerProcessor*>(proc))
    {
        // Manual Sampler: same size as Recorder (4x width, 3x height)
        nodeHeight = Style::nodeHeight * 3.0f;   // 60 * 3 = 180px
        nodeWidth = Style::minNodeWidth * 4.0f;   // ~624px
    }
    else if (dynamic_cast<AutoSamplerProcessor*>(proc))
    {
        // Auto Sampler: same size as Recorder (4x width, 3x height)
        nodeHeight = Style::nodeHeight * 3.0f;   // 60 * 3 = 180px
        nodeWidth = Style::minNodeWidth * 4.0f;   // ~624px
    }
*/

// =============================================================================
// STEP 6: Add drawing code to GraphCanvas_Paint.cpp
// After the Recorder drawing code, add both sampler draw blocks:
// =============================================================================

/*
        // =====================================================================
        // MANUAL SAMPLER - 480×180 module
        // Layout:
        //   TOP: Editable family name textbox (left) + Armed toggle (right)
        //   MIDDLE: Status | Current Note display | Time | Level Meters
        //   BOTTOM: Coast-to-coast waveform display
        // =====================================================================
        if (auto* manualSampler = dynamic_cast<ManualSamplerProcessor*>(proc))
        {
            bool armed = manualSampler->getArmed();
            bool recording = manualSampler->isCurrentlyRecording();
            auto contentArea = bounds.reduced(8, 6);
            
            // ─────────────────────────────────────────────────────────────────
            // TOP ROW: Family Name + Armed Toggle
            // ─────────────────────────────────────────────────────────────────
            auto topRow = contentArea.removeFromTop(26);
            
            // Family name textbox
            auto nameBoxArea = topRow.removeFromLeft(320).reduced(0, 2);
            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(nameBoxArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(nameBoxArea, 4.0f, 1.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(13.0f));
            juce::String displayName = manualSampler->getFamilyName();
            if (displayName.length() > 35) displayName = displayName.substring(0, 32) + "...";
            g.drawText(displayName, nameBoxArea.reduced(8, 0), juce::Justification::centredLeft);
            
            // Armed toggle
            auto armedArea = topRow.removeFromRight(80);
            g.setColour(armed ? juce::Colour(200, 50, 50) : juce::Colour(80, 80, 80));
            g.fillRoundedRectangle(armedArea.reduced(2), 4.0f);
            if (armed && !recording) {
                // Pulsing glow when armed
                g.setColour(juce::Colours::red.withAlpha(0.15f));
                g.fillRoundedRectangle(armedArea.reduced(-2), 6.0f);
            }
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(armed ? (recording ? "REC" : "ARMED") : "DISARMED", 
                       armedArea, juce::Justification::centred);
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────────
            // MIDDLE ROW: Status + Note Display + Time + Meters
            // ─────────────────────────────────────────────────────────────────
            auto controlRow = contentArea.removeFromTop(48);
            
            // Status indicator (circle)
            auto statusArea = controlRow.removeFromLeft(48).reduced(8);
            if (recording) {
                g.setColour(juce::Colours::red);
                g.fillEllipse(statusArea);
                g.setColour(juce::Colours::red.withAlpha(0.3f));
                g.fillEllipse(statusArea.expanded(4));
            } else if (armed) {
                g.setColour(juce::Colour(200, 50, 50).withAlpha(0.6f));
                g.fillEllipse(statusArea);
            } else {
                g.setColour(juce::Colour(60, 60, 60));
                g.fillEllipse(statusArea);
            }
            
            controlRow.removeFromLeft(8);
            
            // Current note display
            auto noteArea = controlRow.removeFromLeft(100);
            int lastNote = manualSampler->getLastRecordedNote();
            int lastVel = manualSampler->getLastRecordedVelocity();
            
            if (recording) {
                // Show note being recorded (not yet saved)
                g.setColour(juce::Colours::lightgreen);
                g.setFont(juce::Font(20.0f, juce::Font::bold));
                // Note: currentNote is private, but we can show state
                g.drawText("Recording...", noteArea, juce::Justification::centredLeft);
            } else if (lastNote >= 0) {
                g.setColour(juce::Colour(180, 180, 200));
                g.setFont(juce::Font(14.0f));
                juce::String noteStr = ManualSamplerProcessor::midiNoteToName(lastNote) 
                                     + " V" + juce::String(lastVel);
                g.drawText("Last: " + noteStr, noteArea, juce::Justification::centredLeft);
            } else {
                g.setColour(juce::Colour(100, 100, 120));
                g.setFont(juce::Font(13.0f));
                g.drawText("Waiting...", noteArea, juce::Justification::centredLeft);
            }
            
            controlRow.removeFromLeft(8);
            
            // Files count
            auto countArea = controlRow.removeFromLeft(80);
            g.setColour(juce::Colour(150, 150, 170));
            g.setFont(juce::Font(12.0f));
            g.drawText("Files: " + juce::String(manualSampler->getTotalFilesRecorded()),
                       countArea, juce::Justification::centredLeft);
            
            // Level meters (right side, same as Recorder)
            auto meterArea = controlRow.removeFromRight(36).reduced(2, 6);
            float meterW = (meterArea.getWidth() - 3) / 2.0f;
            float meterH = meterArea.getHeight();
            
            float levelL = juce::jlimit(0.0f, 1.0f, manualSampler->getLeftLevel());
            float levelR = juce::jlimit(0.0f, 1.0f, manualSampler->getRightLevel());
            
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
            
            auto meterR = juce::Rectangle<float>(meterArea.getX() + meterW + 3, meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterR);
            if (levelR > 0.0f) {
                float fillH = levelR * meterH;
                juce::Colour mCol = levelR < 0.7f ? juce::Colours::limegreen 
                                  : (levelR < 0.9f ? juce::Colours::yellow : juce::Colours::red);
                g.setColour(mCol);
                g.fillRect(meterR.getX(), meterR.getBottom() - fillH, meterW, fillH);
            }
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────────
            // BOTTOM: Waveform Display (identical to Recorder)
            // ─────────────────────────────────────────────────────────────────
            auto waveformArea = contentArea.reduced(0, 2);
            g.setColour(juce::Colour(18, 18, 22));
            g.fillRoundedRectangle(waveformArea, 6.0f);
            
            float centerY = waveformArea.getCentreY();
            g.setColour(juce::Colour(60, 60, 70));
            g.drawHorizontalLine((int)centerY, waveformArea.getX() + 2, waveformArea.getRight() - 2);
            
            int waveWidth = (int)waveformArea.getWidth() - 4;
            auto waveData = manualSampler->getWaveformData(waveWidth);
            
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
                
                juce::Colour waveCol = recording ? juce::Colour(255, 80, 80) : juce::Colour(100, 100, 120);
                g.setColour(waveCol.withAlpha(0.5f));
                g.fillPath(wavePath);
                g.setColour(waveCol);
                g.strokePath(wavePath, juce::PathStrokeType(1.0f));
            }
            
            g.setColour(juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(waveformArea, 6.0f, 1.0f);
        }
*/

/*
        // =====================================================================
        // AUTO SAMPLER - 480×180 module
        // Layout:
        //   TOP: Family name (left) + Progress "12/100" (right)
        //   MIDDLE: Start btn | Stop btn | Current Note | Time | Meters
        //   BOTTOM: Waveform
        // =====================================================================
        if (auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(proc))
        {
            bool running = autoSampler->isRunning();
            auto currentState = autoSampler->getCurrentState();
            auto contentArea = bounds.reduced(8, 6);
            
            // ─────────────────────────────────────────────────────────────────
            // TOP ROW: Family Name + Progress
            // ─────────────────────────────────────────────────────────────────
            auto topRow = contentArea.removeFromTop(26);
            
            // Family name textbox
            auto nameBoxArea = topRow.removeFromLeft(280).reduced(0, 2);
            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(nameBoxArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(nameBoxArea, 4.0f, 1.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(13.0f));
            juce::String displayName = autoSampler->getFamilyName();
            if (displayName.length() > 30) displayName = displayName.substring(0, 27) + "...";
            g.drawText(displayName, nameBoxArea.reduced(8, 0), juce::Justification::centredLeft);
            
            // Progress display
            auto progressArea = topRow.removeFromRight(120);
            int idx = autoSampler->getCurrentNoteIndex();
            int total = autoSampler->getTotalNotes();
            
            if (running) {
                g.setColour(juce::Colours::lightgreen);
                g.setFont(juce::Font(14.0f, juce::Font::bold));
                g.drawText(juce::String(idx + 1) + " / " + juce::String(total),
                           progressArea, juce::Justification::centredRight);
            } else if (currentState == AutoSamplerProcessor::DONE) {
                g.setColour(juce::Colour(100, 200, 255));
                g.setFont(juce::Font(13.0f, juce::Font::bold));
                g.drawText("DONE (" + juce::String(total) + ")", 
                           progressArea, juce::Justification::centredRight);
            } else {
                g.setColour(juce::Colour(120, 120, 140));
                g.setFont(juce::Font(13.0f));
                g.drawText("Idle", progressArea, juce::Justification::centredRight);
            }
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────────
            // MIDDLE ROW: Start/Stop + Note + Time + Meters
            // ─────────────────────────────────────────────────────────────────
            auto controlRow = contentArea.removeFromTop(48);
            
            // START button (green play triangle)
            auto startBtnArea = controlRow.removeFromLeft(56).reduced(4);
            g.setColour(running ? juce::Colour(30, 80, 30) : juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(startBtnArea, 10.0f);
            g.setColour(running ? juce::Colours::green.darker() : juce::Colours::grey);
            g.drawRoundedRectangle(startBtnArea, 10.0f, 1.5f);
            
            // Play triangle
            float triSize = startBtnArea.getHeight() * 0.4f;
            auto triCenter = startBtnArea.getCentre();
            juce::Path tri;
            tri.addTriangle(triCenter.x - triSize * 0.4f, triCenter.y - triSize * 0.5f,
                            triCenter.x - triSize * 0.4f, triCenter.y + triSize * 0.5f,
                            triCenter.x + triSize * 0.6f, triCenter.y);
            g.setColour(running ? juce::Colours::limegreen : juce::Colour(80, 180, 80));
            g.fillPath(tri);
            
            controlRow.removeFromLeft(8);
            
            // STOP button (blue square)
            auto stopBtnArea = controlRow.removeFromLeft(56).reduced(4);
            g.setColour(juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(stopBtnArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(stopBtnArea, 4.0f, 1.5f);
            
            float squareSize = stopBtnArea.getHeight() * 0.45f;
            auto squareArea = stopBtnArea.withSizeKeepingCentre(squareSize, squareSize);
            g.setColour(juce::Colour(30, 144, 255));
            g.fillRect(squareArea);
            
            controlRow.removeFromLeft(12);
            
            // Current note display
            auto noteArea = controlRow.removeFromLeft(120);
            int curNote = autoSampler->getCurrentNote();
            int curVel = autoSampler->getCurrentVelocity();
            
            if (running && curNote >= 0) {
                g.setColour(juce::Colours::lightgreen);
                g.setFont(juce::Font(18.0f, juce::Font::bold));
                juce::String noteStr = AutoSamplerProcessor::midiNoteToName(curNote)
                                     + " V" + juce::String(curVel);
                g.drawText(noteStr, noteArea, juce::Justification::centredLeft);
            } else {
                g.setColour(juce::Colour(100, 100, 120));
                g.setFont(juce::Font(13.0f));
                g.drawText("Ready", noteArea, juce::Justification::centredLeft);
            }
            
            // Level meters (right side)
            auto meterArea = controlRow.removeFromRight(36).reduced(2, 6);
            float meterW = (meterArea.getWidth() - 3) / 2.0f;
            float meterH = meterArea.getHeight();
            
            float levelL = juce::jlimit(0.0f, 1.0f, autoSampler->getLeftLevel());
            float levelR = juce::jlimit(0.0f, 1.0f, autoSampler->getRightLevel());
            
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
            
            auto meterR = juce::Rectangle<float>(meterArea.getX() + meterW + 3, meterArea.getY(), meterW, meterH);
            g.setColour(juce::Colour(25, 25, 30));
            g.fillRect(meterR);
            if (levelR > 0.0f) {
                float fillH = levelR * meterH;
                juce::Colour mCol = levelR < 0.7f ? juce::Colours::limegreen 
                                  : (levelR < 0.9f ? juce::Colours::yellow : juce::Colours::red);
                g.setColour(mCol);
                g.fillRect(meterR.getX(), meterR.getBottom() - fillH, meterW, fillH);
            }
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────────
            // BOTTOM: Waveform (identical to Recorder/ManualSampler)
            // ─────────────────────────────────────────────────────────────────
            auto waveformArea = contentArea.reduced(0, 2);
            g.setColour(juce::Colour(18, 18, 22));
            g.fillRoundedRectangle(waveformArea, 6.0f);
            
            float centerY = waveformArea.getCentreY();
            g.setColour(juce::Colour(60, 60, 70));
            g.drawHorizontalLine((int)centerY, waveformArea.getX() + 2, waveformArea.getRight() - 2);
            
            int waveWidth = (int)waveformArea.getWidth() - 4;
            auto waveData = autoSampler->getWaveformData(waveWidth);
            
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
                
                juce::Colour waveCol = running ? juce::Colour(100, 255, 100) : juce::Colour(100, 100, 120);
                g.setColour(waveCol.withAlpha(0.5f));
                g.fillPath(wavePath);
                g.setColour(waveCol);
                g.strokePath(wavePath, juce::PathStrokeType(1.0f));
            }
            
            g.setColour(juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(waveformArea, 6.0f, 1.0f);
        }
*/

// =============================================================================
// STEP 7: Add click handling in GraphCanvas_Mouse.cpp (or GraphCanvas_Input.cpp)
// After the Recorder click handling block, add:
// =============================================================================
/*
    // =========================================================================
    // Handle Manual Sampler interactions
    // =========================================================================
    if (auto* manualSampler = dynamic_cast<ManualSamplerProcessor*>(clickedNode->getProcessor())) {
        auto nodeBounds = getNodeBounds(clickedNode);
        auto localClick = e.position - nodeBounds.getPosition();
        auto contentArea = nodeBounds.reduced(8, 6);
        auto topRow = contentArea.removeFromTop(26);
        
        // Family name textbox (click to edit)
        auto nameBoxArea = topRow.removeFromLeft(320).reduced(0, 2);
        nameBoxArea = nameBoxArea.translated(8, 6);
        
        if (nameBoxArea.contains(localClick)) {
            // Show rename dialog (same pattern as Recorder name editor)
            // Use showRecorderNameEditor() pattern but call:
            //   manualSampler->setFamilyName(editor.getText());
            return;
        }
        
        // Armed toggle
        auto armedArea = topRow.removeFromRight(80);
        armedArea = armedArea.translated(8, 6);
        if (armedArea.contains(localClick)) {
            manualSampler->setArmed(!manualSampler->getArmed());
            repaint();
            return;
        }
    }
    
    // =========================================================================
    // Handle Auto Sampler interactions
    // =========================================================================
    if (auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(clickedNode->getProcessor())) {
        auto nodeBounds = getNodeBounds(clickedNode);
        auto localClick = e.position - nodeBounds.getPosition();
        auto contentArea = nodeBounds.reduced(8, 6);
        auto topRow = contentArea.removeFromTop(26);
        
        // Family name textbox (click to edit)
        auto nameBoxArea = topRow.removeFromLeft(280).reduced(0, 2);
        nameBoxArea = nameBoxArea.translated(8, 6);
        
        if (nameBoxArea.contains(localClick)) {
            // Show rename dialog
            return;
        }
        
        contentArea.removeFromTop(6);
        auto controlRow = contentArea.removeFromTop(48);
        
        // Start button
        auto startBtnArea = controlRow.removeFromLeft(56).reduced(4);
        startBtnArea = startBtnArea.translated(8, 38);
        
        if (startBtnArea.contains(localClick)) {
            if (!autoSampler->isRunning()) {
                if (!autoSampler->startAutoSampling()) {
                    // Show error - no valid entries
                    juce::NativeMessageBox::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Auto Sampler",
                        "No valid note/velocity entries found.\n"
                        "Click the E button to configure the note list.");
                }
                repaint();
            }
            return;
        }
        
        controlRow.removeFromLeft(8);
        
        // Stop button
        auto stopBtnArea = controlRow.removeFromLeft(56).reduced(4);
        stopBtnArea = stopBtnArea.translated(8 + 56 + 8, 38);
        
        if (stopBtnArea.contains(localClick)) {
            if (autoSampler->isRunning()) {
                autoSampler->stopAutoSampling();
                repaint();
            }
            return;
        }
    }
*/

// =============================================================================
// STEP 8: Add drawNodeButtons handling in GraphCanvas_Paint.cpp
// In drawNodeButtons(), before the X (delete) button section:
// Samplers only show folder (📁) and X buttons, Auto Sampler also shows E
// =============================================================================
/*
    // For system tools (Recorder, ManualSampler, AutoSampler):
    // They don't have E/M/P/T/L plugin buttons.
    // ManualSampler: [folder][X]
    // AutoSampler: [E][folder][X]
    
    if (auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(proc))
    {
        // E button (edit note list)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(0xFF50C878));  // Green
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }
    
    if (dynamic_cast<ManualSamplerProcessor*>(proc) || dynamic_cast<AutoSamplerProcessor*>(proc))
    {
        // Folder button
        auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(180, 160, 80));  // Gold
        g.fillRoundedRectangle(folderRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("F", folderRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }
*/

// =============================================================================
// STEP 9: Add button click handling in GraphCanvas_Mouse.cpp
// In the button hit-testing section, add E and F button handlers:
// =============================================================================
/*
    // Auto Sampler E button → open editor popup
    if (auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(proc))
    {
        if (editRect.contains(pos))
        {
            showAutoSamplerEditor(node);
            return;
        }
    }
    
    // Folder button for samplers
    if (auto* manualSampler = dynamic_cast<ManualSamplerProcessor*>(proc))
    {
        if (folderRect.contains(pos))
        {
            manualSampler->openRecordingFolder();
            return;
        }
    }
    if (auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(proc))
    {
        if (folderRect.contains(pos))
        {
            autoSampler->openRecordingFolder();
            return;
        }
    }
*/

// =============================================================================
// STEP 10: Implement showAutoSamplerEditor() in GraphCanvas_Mouse.cpp
// (Or in a separate integration file) - shows CallOutBox with editor
// =============================================================================
/*
void GraphCanvas::showAutoSamplerEditor(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;
    
    auto* autoSampler = dynamic_cast<AutoSamplerProcessor*>(node->getProcessor());
    if (!autoSampler) return;
    
    auto* editorComp = new AutoSamplerEditorComponent(autoSampler);
    
    auto nodeBounds = getNodeBounds(node);
    auto screenBounds = getScreenBounds();
    
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(
            screenBounds.getX() + (int)nodeBounds.getCentreX(),
            screenBounds.getY() + (int)nodeBounds.getY(),
            1, 1),
        nullptr);
}
*/

// =============================================================================
// STEP 11: Update CMakeLists.txt
// Add to the source files list:
// =============================================================================
/*
    src/ManualSamplerProcessor.h
    src/ManualSamplerProcessor.cpp
    src/AutoSamplerProcessor.h
    src/AutoSamplerProcessor.cpp
    src/AutoSamplerEditorComponent.h
*/

// =============================================================================
// STEP 12: Add sampler folder settings to PluginManagerTab_Folders.cpp
// Add a "Auto-Sampler Output Folder" section similar to the Recording folder
// =============================================================================
/*
    // In the folders panel, add:
    // Label: "Auto-Sampler Output Folder"
    // Path display + Browse button
    // On browse: set ManualSamplerProcessor::setGlobalDefaultFolder() 
    //            AND AutoSamplerProcessor::setGlobalDefaultFolder()
    // Save to settings: "AutoSamplerFolder" key
*/
