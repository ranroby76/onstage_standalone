// #D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Recorder_Integration.cpp
// 
// INTEGRATION GUIDE: Adding Recorder to GraphCanvas
// This file contains all the code snippets to add to existing files.
// 
// =============================================================================

// =============================================================================
// STEP 1: Add include at top of GraphCanvas.h
// =============================================================================
// #include "RecorderProcessor.h"

// =============================================================================
// STEP 2: Add to NodeTypeCache struct in GraphCanvas.h (around line 5579)
// =============================================================================
// Add this member:
//     RecorderProcessor* recorder = nullptr;

// =============================================================================
// STEP 3: Update shouldShowNode() in GraphCanvas.cpp 
// Around line 2629, add:
// =============================================================================
/*
    if (dynamic_cast<RecorderProcessor*>(proc)) return true;
*/

// =============================================================================
// STEP 4: Update getNodeBounds() in GraphCanvas.cpp
// Around line 2722, add this case:
// =============================================================================
/*
    else if (dynamic_cast<RecorderProcessor*>(proc))
    {
        // Recorder: 4x width (480px), 3x height (180px)
        nodeHeight = Style::nodeHeight * 3.0f;   // 60 * 3 = 180px
        nodeWidth = Style::minNodeWidth * 4.0f;  // 120 * 4 = 480px
    }
*/

// =============================================================================
// STEP 5: Update buildNodeTypeCache() in GraphCanvas.cpp
// Around line 2405, add:
// =============================================================================
/*
    cache.recorder = dynamic_cast<RecorderProcessor*>(proc);
*/

// =============================================================================
// STEP 6: Add Recorder drawing code to GraphCanvas_Paint.cpp
// After the StereoMeter drawing code (around line 4000), add:
// =============================================================================

// This is the full drawing function for Recorder nodes:
// Dimensions: 480×180 px (4x width, 3x height)
/*
        // =====================================================================
        // RECORDER - 480×180 module
        // Layout:
        //   TOP: Editable name textbox (left) + Sync toggle (right)
        //   MIDDLE: Record btn (rounded+red circle) | Stop btn (square+blue) | Time | Meters
        //   BOTTOM: Coast-to-coast waveform display
        // =====================================================================
        if (auto* recorder = dynamic_cast<RecorderProcessor*>(proc))
        {
            bool isRecording = recorder->isCurrentlyRecording();
            auto contentArea = bounds.reduced(8, 6);
            
            // ─────────────────────────────────────────────────────────────────
            // TOP ROW: Editable Name Textbox + Sync Toggle
            // ─────────────────────────────────────────────────────────────────
            auto topRow = contentArea.removeFromTop(26);
            
            // Name textbox area (draws as editable field - click to edit)
            auto nameBoxArea = topRow.removeFromLeft(320).reduced(0, 2);
            g.setColour(juce::Colour(45, 45, 50));
            g.fillRoundedRectangle(nameBoxArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(nameBoxArea, 4.0f, 1.0f);
            
            // Name text
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(13.0f));
            juce::String displayName = recorder->getRecorderName();
            if (displayName.length() > 35) displayName = displayName.substring(0, 32) + "...";
            g.drawText(displayName, nameBoxArea.reduced(8, 0), juce::Justification::centredLeft);
            
            // Sync mode toggle (right side)
            auto syncArea = topRow.removeFromRight(70);
            bool syncMode = recorder->isSyncMode();
            g.setColour(syncMode ? juce::Colour(0, 180, 180) : juce::Colour(80, 80, 80));
            g.fillRoundedRectangle(syncArea.reduced(2), 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(syncMode ? "SYNC" : "INDEP", syncArea, juce::Justification::centred);
            
            contentArea.removeFromTop(6);
            
            // ─────────────────────────────────────────────────────────────────
            // MIDDLE ROW: Record/Stop Buttons + Time Display + Level Meters
            // ─────────────────────────────────────────────────────────────────
            auto controlRow = contentArea.removeFromTop(48);
            
            // ▶ RECORD BUTTON: Rounded rectangle with red circle inside
            auto recordBtnArea = controlRow.removeFromLeft(56).reduced(4);
            
            // Rounded button background
            g.setColour(isRecording ? juce::Colour(80, 20, 20) : juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(recordBtnArea, 10.0f);
            g.setColour(isRecording ? juce::Colours::red.darker() : juce::Colours::grey);
            g.drawRoundedRectangle(recordBtnArea, 10.0f, 1.5f);
            
            // Red circle inside (centered)
            float circleSize = recordBtnArea.getHeight() * 0.5f;
            auto circleArea = recordBtnArea.withSizeKeepingCentre(circleSize, circleSize);
            g.setColour(isRecording ? juce::Colours::red : juce::Colour(180, 50, 50));
            g.fillEllipse(circleArea);
            
            // Recording glow effect
            if (isRecording) {
                g.setColour(juce::Colours::red.withAlpha(0.25f));
                g.fillEllipse(circleArea.expanded(6));
            }
            
            controlRow.removeFromLeft(8);
            
            // ▶ STOP BUTTON: Square button with blue square inside
            auto stopBtnArea = controlRow.removeFromLeft(56).reduced(4);
            
            // Square button background (slightly rounded corners)
            g.setColour(juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(stopBtnArea, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(stopBtnArea, 4.0f, 1.5f);
            
            // Blue square inside (centered)
            float squareSize = stopBtnArea.getHeight() * 0.45f;
            auto squareArea = stopBtnArea.withSizeKeepingCentre(squareSize, squareSize);
            g.setColour(juce::Colour(30, 144, 255));  // Dodger blue
            g.fillRect(squareArea);
            
            controlRow.removeFromLeft(12);
            
            // ▶ TIME DISPLAY
            double recordingSeconds = recorder->getRecordingLengthSeconds();
            int hours = (int)(recordingSeconds / 3600.0);
            int minutes = (int)(recordingSeconds / 60.0) % 60;
            int seconds = (int)(recordingSeconds) % 60;
            int tenths = (int)((recordingSeconds - (int)recordingSeconds) * 10);
            
            juce::String timeStr;
            if (hours > 0)
                timeStr = juce::String::formatted("%d:%02d:%02d", hours, minutes, seconds);
            else
                timeStr = juce::String::formatted("%02d:%02d.%d", minutes, seconds, tenths);
            
            auto timeArea = controlRow.removeFromLeft(120);
            g.setColour(isRecording ? juce::Colours::lightgreen : juce::Colour(150, 150, 150));
            g.setFont(juce::Font(26.0f, juce::Font::bold));
            g.drawText(timeStr, timeArea, juce::Justification::centred);
            
            // ▶ LEVEL METERS (vertical stereo, right side)
            auto meterArea = controlRow.removeFromRight(36).reduced(2, 6);
            float meterW = (meterArea.getWidth() - 3) / 2.0f;
            float meterH = meterArea.getHeight();
            
            float levelL = juce::jlimit(0.0f, 1.0f, recorder->getLeftLevel());
            float levelR = juce::jlimit(0.0f, 1.0f, recorder->getRightLevel());
            
            // Left meter
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
            
            // Right meter
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
            // BOTTOM: Coast-to-Coast Waveform Display
            // ─────────────────────────────────────────────────────────────────
            auto waveformArea = contentArea.reduced(0, 2);
            
            // Waveform background (dark)
            g.setColour(juce::Colour(18, 18, 22));
            g.fillRoundedRectangle(waveformArea, 6.0f);
            
            // Center line
            float centerY = waveformArea.getCentreY();
            g.setColour(juce::Colour(60, 60, 70));
            g.drawHorizontalLine((int)centerY, waveformArea.getX() + 2, waveformArea.getRight() - 2);
            
            // Get waveform data (1 sample per pixel for coast-to-coast)
            int waveWidth = (int)waveformArea.getWidth() - 4;
            auto waveData = recorder->getWaveformData(waveWidth);
            
            if (!waveData.empty()) {
                float halfH = (waveformArea.getHeight() - 8) * 0.48f;
                float startX = waveformArea.getX() + 2;
                
                // Build waveform path
                juce::Path wavePath;
                wavePath.startNewSubPath(startX, centerY);
                
                // Top edge (max values)
                for (int i = 0; i < waveWidth && i < (int)waveData.size(); ++i) {
                    float maxV = std::max(waveData[i].maxL, waveData[i].maxR);
                    wavePath.lineTo(startX + i, centerY - (maxV * halfH));
                }
                
                // Bottom edge (min values, reverse)
                for (int i = waveWidth - 1; i >= 0 && i < (int)waveData.size(); --i) {
                    float minV = std::min(waveData[i].minL, waveData[i].minR);
                    wavePath.lineTo(startX + i, centerY - (minV * halfH));
                }
                
                wavePath.closeSubPath();
                
                // Fill waveform
                juce::Colour waveCol = isRecording ? juce::Colour(0, 200, 255) : juce::Colour(100, 100, 120);
                g.setColour(waveCol.withAlpha(0.5f));
                g.fillPath(wavePath);
                
                // Waveform outline
                g.setColour(waveCol);
                g.strokePath(wavePath, juce::PathStrokeType(1.0f));
            }
            
            // Waveform border
            g.setColour(juce::Colour(70, 70, 80));
            g.drawRoundedRectangle(waveformArea, 6.0f, 1.0f);
            
            // Show filename if recording exists
            if (recorder->hasRecording() && !isRecording) {
                juce::String fname = recorder->getLastRecordingFile().getFileName();
                if (fname.length() > 50) fname = "..." + fname.substring(fname.length() - 47);
                g.setColour(juce::Colours::grey.withAlpha(0.7f));
                g.setFont(juce::Font(10.0f));
                g.drawText(fname, waveformArea.reduced(6, 0).removeFromBottom(14), 
                          juce::Justification::centredLeft);
            }
        }
*/

// =============================================================================
// STEP 7: Add click handling for Recorder in GraphCanvas_Input.cpp
// In mouseDown or mouseUp handler, add this for Recorder nodes:
// =============================================================================
/*
    // Handle Recorder interactions
    if (auto* recorder = dynamic_cast<RecorderProcessor*>(clickedNode->getProcessor())) {
        auto nodeBounds = getNodeBounds(clickedNode);
        auto localClick = e.position - nodeBounds.getPosition();
        
        // Calculate button areas (must match paint code)
        auto contentArea = nodeBounds.reduced(8, 6);
        auto topRow = contentArea.removeFromTop(26);
        
        // Name textbox area (click to edit)
        auto nameBoxArea = topRow.removeFromLeft(320).reduced(0, 2);
        nameBoxArea = nameBoxArea.translated(8, 6);  // Account for content offset
        
        if (nameBoxArea.contains(localClick)) {
            // Show rename dialog
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withTitle("Rename Recorder")
                    .withMessage("Enter new name:")
                    .withButton("OK")
                    .withButton("Cancel"),
                [this, recorder](int result) {
                    // Note: In practice, use a proper text input dialog
                    // This is simplified - implement proper TextEditor popup
                }
            );
            
            // BETTER: Show inline TextEditor
            // Create a TextEditor at nameBoxArea position, 
            // pre-fill with recorder->getRecorderName(),
            // on return key: recorder->setRecorderName(editor.getText())
            return;
        }
        
        // Sync toggle area
        auto syncArea = topRow.removeFromRight(70);
        syncArea = syncArea.translated(8, 6);
        if (syncArea.contains(localClick)) {
            recorder->setSyncMode(!recorder->isSyncMode());
            repaint();
            return;
        }
        
        contentArea.removeFromTop(6);
        auto controlRow = contentArea.removeFromTop(48);
        
        // Record button area
        auto recordBtnArea = controlRow.removeFromLeft(56).reduced(4);
        recordBtnArea = recordBtnArea.translated(8, 38);  // Offset from node top-left
        
        if (recordBtnArea.contains(localClick)) {
            if (!recorder->isCurrentlyRecording()) {
                recorder->startRecording();
                repaint();
            }
            return;
        }
        
        controlRow.removeFromLeft(8);
        
        // Stop button area  
        auto stopBtnArea = controlRow.removeFromLeft(56).reduced(4);
        stopBtnArea = stopBtnArea.translated(8 + 56 + 8, 38);
        
        if (stopBtnArea.contains(localClick)) {
            if (recorder->isCurrentlyRecording()) {
                recorder->stopRecording();
                repaint();
            }
            return;
        }
    }
*/

// =============================================================================
// STEP 7b: Editable TextEditor for recorder name
// Add this helper method to GraphCanvas class:
// =============================================================================
/*
void GraphCanvas::showRecorderNameEditor(RecorderProcessor* recorder, juce::Rectangle<float> bounds) {
    // Create inline text editor
    auto* editor = new juce::TextEditor();
    editor->setBounds(bounds.toNearestInt());
    editor->setText(recorder->getRecorderName());
    editor->setFont(juce::Font(13.0f));
    editor->selectAll();
    editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(45, 45, 50));
    editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::cyan);
    
    editor->onReturnKey = [this, editor, recorder]() {
        recorder->setRecorderName(editor->getText());
        removeChildComponent(editor);
        delete editor;
        repaint();
    };
    
    editor->onEscapeKey = [this, editor]() {
        removeChildComponent(editor);
        delete editor;
        repaint();
    };
    
    editor->onFocusLost = [this, editor, recorder]() {
        recorder->setRecorderName(editor->getText());
        juce::MessageManager::callAsync([this, editor]() {
            removeChildComponent(editor);
            delete editor;
            repaint();
        });
    };
    
    addAndMakeVisible(editor);
    editor->grabKeyboardFocus();
}
*/

// =============================================================================
// STEP 8: Add to System Tools menu in GraphCanvas (around line 4756)
// =============================================================================
/*
    // In the system tools popup menu, add:
    menu.addItem(4, "Recorder");
    
    // In the menu result handler:
    case 4:  // Recorder
    {
        auto nodePtr = processor.mainGraph->addNode(std::make_unique<RecorderProcessor>());
        if (nodePtr) {
            nodePtr->properties.set("x", menuPos.x);
            nodePtr->properties.set("y", menuPos.y);
            rebuildNodeCache();
            repaint();
        }
        break;
    }
*/

// =============================================================================
// STEP 9: Add timer handling for Recorder refresh (optional, for smooth waveform)
// In timerCallback, add check for recorders similar to StereoMeter
// =============================================================================

// =============================================================================
// STEP 10: Update drawNodeButtons to handle Recorder (no M/bypass buttons)
// Similar to StereoMeter handling - only show X (delete) button
// =============================================================================
