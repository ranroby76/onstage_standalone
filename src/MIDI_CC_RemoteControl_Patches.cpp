// =============================================================================
// PATCH FILE: MIDI CC Remote Control for Workspaces and Instruments
// =============================================================================
// This file contains all the sections that need to be modified in the large
// source files. Each section is clearly marked with the target file and location.
//
// CC MAPPING:
//   Workspaces 1-16:   CC 102-117  (value > 63 = switch)
//   Instruments 1-32:  CC 20-51    (value > 63 = select/toggle)
//   Any MIDI channel accepted.
// =============================================================================


// #############################################################################
// PATCH 1: PluginProcessor.h — Add to PUBLIC section (after instrumentSelectorMultiMode)
//
// Location: After line "bool instrumentSelectorMultiMode = true;"
//           (around line 31334 in colosseum_full_code.txt)
// #############################################################################

    // =========================================================================
    // MIDI CC Remote Control — fixed CC mapping for live performance
    // Workspaces 1-16:  CC 102-117 (value > 63 triggers switch)
    // Instruments 1-32: CC 20-51   (value > 63 triggers select/toggle)
    // =========================================================================
    static constexpr int midiCCWorkspaceBase = 102;    // CC 102 = Workspace 1, CC 117 = Workspace 16
    static constexpr int midiCCInstrumentBase = 20;    // CC 20 = Instrument 1, CC 51 = Instrument 32

    // Pending triggers (set in processBlock audio thread, consumed in UI timers)
    std::atomic<int> pendingWorkspaceSwitch { -1 };
    std::atomic<int> pendingInstrumentSelect { -1 };


// #############################################################################
// PATCH 2: PluginProcessor.cpp — REPLACE entire processBlock function
//
// Location: Around line 29392 in colosseum_full_code.txt
//           Replace from "void SubterraneumAudioProcessor::processBlock"
//           to the closing brace before "// Recording Methods"
// #############################################################################

void SubterraneumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    if (!mainGraph) {
        buffer.clear();
        midiMessages.clear();
        return;
    }
    
    // CPU FIX #1: REMOVED updateDemoMode() call!
    // updateDemoMode() is already called in UI timers (PluginEditor::timerCallback)
    // Just read the atomic state - this is very cheap
    bool demoSilence = RegistrationManager::getInstance().isDemoSilenceActive();
    
    // CPU FIX #2: Throttle meter updates - only calculate RMS every Nth buffer
    bool shouldUpdateMeters = (++meterUpdateCounter >= meterUpdateInterval);
    if (shouldUpdateMeters) {
        meterUpdateCounter = 0;
    }
    
    // If audio input is disabled OR demo silence is active, clear the input buffer
    if (!audioInputEnabled.load() || demoSilence) {
        buffer.clear();
        mainInputRms[0] = 0.0f;
        mainInputRms[1] = 0.0f;
        
        // FIX: Send All-Notes-Off to kill oscillators/stuck instruments
        // This prevents "caged leftover sound" from plugins with internal oscillators
        static int audioOffPanicCounter = 0;
        if (++audioOffPanicCounter >= 10) {  // Send every 10 buffers to ensure it's received
            audioOffPanicCounter = 0;
            for (int ch = 1; ch <= 16; ++ch) {
                midiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                midiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
            }
        }
    } else {
        // Apply input gains (before graph processing)
        for (int ch = 0; ch < buffer.getNumChannels() && ch < maxMixerChannels; ++ch) {
            float gain = inputGains[ch].load();
            if (gain != 1.0f) {
                buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);
            }
        }
        
        // CPU FIX #3: Only calculate input RMS when needed (throttled)
        if (shouldUpdateMeters) {
            if (buffer.getNumChannels() > 0) mainInputRms[0] = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            if (buffer.getNumChannels() > 1) mainInputRms[1] = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        }
    }
    
    // FIX: MIDI input control with All-Notes-Off on disable
    bool midiEnabled = midiInputEnabled.load() && !demoSilence;
    
    if (!midiEnabled) {
        // Clear all incoming MIDI
        midiMessages.clear();
        
        // Send All-Notes-Off on all channels to kill stuck notes
        static int allNotesOffCounter = 0;
        if (++allNotesOffCounter >= 10) {  // Send every 10 buffers to ensure it's received
            allNotesOffCounter = 0;
            for (int ch = 1; ch <= 16; ++ch) {
                midiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                midiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
            }
        }
        
        mainMidiInFlash = false;
    } else {
        // HARDWARE MIDI CHANNEL FILTERING - Apply before virtual keyboard
        // This filters hardware MIDI based on per-device channel masks
        applyHardwareMidiChannelFiltering(midiMessages);
        
        // Process virtual keyboard MIDI (only when MIDI input is enabled)
        keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
        
        // Track MIDI input activity
        if (!midiMessages.isEmpty()) {
            mainMidiInNoteCount++;
            mainMidiInFlash = true;
        } else {
            mainMidiInFlash = false;
        }
    }

    // =========================================================================
    // NEW: MIDI CC interception for Workspace + Instrument remote control
    // Scan for CC 102-117 (workspaces) and CC 20-51 (instruments).
    // Matching CCs are consumed (removed) so they don't reach plugins.
    // =========================================================================
    {
        juce::MidiBuffer filteredMidi;
        
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            bool consumed = false;
            
            if (msg.isController())
            {
                int cc = msg.getControllerNumber();
                int val = msg.getControllerValue();
                
                // Workspace switch: CC 102-117, value > 63 = trigger
                if (cc >= midiCCWorkspaceBase && cc < midiCCWorkspaceBase + maxWorkspaces && val > 63)
                {
                    int wsIndex = cc - midiCCWorkspaceBase;
                    pendingWorkspaceSwitch.store(wsIndex);
                    consumed = true;
                }
                // Instrument select: CC 20-51, value > 63 = trigger
                else if (cc >= midiCCInstrumentBase && cc < midiCCInstrumentBase + 32 && val > 63)
                {
                    int instIndex = cc - midiCCInstrumentBase;
                    pendingInstrumentSelect.store(instIndex);
                    consumed = true;
                }
            }
            
            if (!consumed)
                filteredMidi.addEvent(msg, metadata.samplePosition);
        }
        
        midiMessages.swapWith(filteredMidi);
    }

    // Process the graph with crash protection
    try {
        mainGraph->processBlock(buffer, midiMessages);
    } catch (...) {
        buffer.clear();
        midiMessages.clear();
    }
    
    // If audio output is disabled OR demo silence is active, clear the output buffer
    if (!audioOutputEnabled.load() || demoSilence) {
        buffer.clear();
        mainOutputRms[0] = 0.0f;
        mainOutputRms[1] = 0.0f;
    } else {
        // Apply output gains (after graph processing)
        for (int ch = 0; ch < buffer.getNumChannels() && ch < maxMixerChannels; ++ch) {
            float gain = outputGains[ch].load();
            if (gain != 1.0f) {
                buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);
            }
        }

        // CPU FIX #4: Only calculate output RMS when needed (throttled)
        if (shouldUpdateMeters) {
            if (buffer.getNumChannels() > 0) mainOutputRms[0] = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            if (buffer.getNumChannels() > 1) mainOutputRms[1] = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        }
    }
    
    // Recording - capture final output
    if (isRecording.load() && backgroundWriter != nullptr) {
        backgroundWriter->write(buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    }
}


// #############################################################################
// PATCH 3: PluginEditor — REPLACE timerCallback
//
// Location: Around line 24611 in colosseum_full_code.txt
//           Replace the entire timerCallback function
// #############################################################################

void SubterraneumAudioProcessorEditor::timerCallback() {
    // Update registration LED
    bool isReg = RegistrationManager::getInstance().isRegistered();
    if (registrationLED.getActive() != isReg) {
        registrationLED.setActive(isReg);
    }
    
    RegistrationManager::getInstance().updateDemoMode();
    
    // Update CPU and RAM meters
    #if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        SIZE_T usedMemory = pmc.WorkingSetSize;
        int ramMB = (int)(usedMemory / (1024 * 1024));
        ramLabel.setText("RAM: " + juce::String(ramMB) + "MB", juce::dontSendNotification);
    }
    #elif JUCE_MAC
    struct task_basic_info info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        int ramMB = (int)(info.resident_size / (1024 * 1024));
        ramLabel.setText("RAM: " + juce::String(ramMB) + "MB", juce::dontSendNotification);
    }
    #elif JUCE_LINUX
    std::ifstream statusFile("/proc/self/status");
    std::string line;
    int ramKB = 0;
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> ramKB;
            break;
        }
    }
    ramLabel.setText("RAM: " + juce::String(ramKB / 1024) + "MB", juce::dontSendNotification);
    #else
    int ramMB = juce::SystemStats::getMemorySizeInMegabytes();
    ramLabel.setText("RAM: " + juce::String(ramMB) + "MB", juce::dontSendNotification);
    #endif
    
    double cpuPercent = 0.0;
    if (SubterraneumAudioProcessor::standaloneDeviceManager != nullptr) {
        cpuPercent = SubterraneumAudioProcessor::standaloneDeviceManager->getCpuUsage() * 100.0;
    }
    cpuLabel.setText("CPU: " + juce::String(cpuPercent, 1) + "%", juce::dontSendNotification);
    
    // =========================================================================
    // NEW: Handle pending MIDI CC workspace switch
    // =========================================================================
    int pendingWS = audioProcessor.pendingWorkspaceSwitch.exchange(-1);
    if (pendingWS >= 0 && pendingWS < 16)
    {
        if (audioProcessor.isWorkspaceEnabled(pendingWS) && pendingWS != audioProcessor.getActiveWorkspace())
        {
            graphCanvas.closeAllPluginWindows();
            audioProcessor.switchWorkspace(pendingWS);
            graphCanvas.refreshCache();
            graphCanvas.repaint();
            zoomSlider.setValue((double)audioProcessor.rackZoomLevel, juce::sendNotificationSync);
            updateWorkspaceButtonColors();
            updateInstrumentSelector();  // FIX: Refresh instruments for new workspace
        }
    }
}


// #############################################################################
// PATCH 4: PluginEditor constructor — REPLACE workspace button onClick lambdas
//
// Location: Around line 24393-24414 in colosseum_full_code.txt
//           Replace the for loop that sets up workspaceButtons[i].onClick
// #############################################################################

    // Workspace selector buttons
    for (int i = 0; i < 16; ++i)
    {
        workspaceButtons[i].setButtonText(audioProcessor.getWorkspaceName(i));
        workspaceButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(40, 40, 45));
        workspaceButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(190, 190, 200));
        workspaceButtons[i].onClick = [this, i]() {
            if (!audioProcessor.isWorkspaceEnabled(i)) return;
            graphCanvas.closeAllPluginWindows();
            audioProcessor.switchWorkspace(i);
            graphCanvas.refreshCache();
            graphCanvas.repaint();
            // Sync zoom to restored workspace value
            zoomSlider.setValue((double)audioProcessor.rackZoomLevel, juce::sendNotificationSync);
            updateWorkspaceButtonColors();
            updateInstrumentSelector();  // FIX: Refresh instruments for new workspace
        };
        workspaceButtons[i].onStateChange = [this, i]() {
            // Right-click detection via mouse event
        };
        addAndMakeVisible(workspaceButtons[i]);
        workspaceButtons[i].addMouseListener(this, false);
    }


// #############################################################################
// PATCH 5: PluginEditor — REPLACE showWorkspaceContextMenu
//
// Location: Around line 25052 in colosseum_full_code.txt
//           Replace the entire showWorkspaceContextMenu function
// #############################################################################

void SubterraneumAudioProcessorEditor::showWorkspaceContextMenu(int idx)
{
    juce::PopupMenu menu;
    
    bool isEnabled = audioProcessor.isWorkspaceEnabled(idx);
    bool isActive = (idx == audioProcessor.getActiveWorkspace());
    
    // NEW: Show MIDI CC info at top of context menu
    int midiCC = SubterraneumAudioProcessor::midiCCWorkspaceBase + idx;
    menu.addSectionHeader("MIDI CC " + juce::String(midiCC) + "  (value > 63 = switch)");
    menu.addSeparator();
    
    // Enable / Disable toggle
    if (!isEnabled)
        menu.addItem(1, "Enable");
    else if (!isActive)
        menu.addItem(1, "Disable");
    else
        menu.addItem(1, "Disable", false); // Can't disable active workspace
    
    menu.addItem(2, "Rename");
    
    // Clear — only if enabled
    menu.addItem(3, "Clear", isEnabled);
    
    // Duplicate submenu — only if enabled
    juce::PopupMenu dupMenu;
    for (int i = 0; i < 16; ++i)
    {
        if (i == idx) continue;
        dupMenu.addItem(100 + i, "Workspace " + audioProcessor.getWorkspaceName(i));
    }
    menu.addSubMenu("Duplicate to...", dupMenu, isEnabled);
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&workspaceButtons[idx]),
        [this, idx](int result)
        {
            if (result == 0) return;
            
            if (result == 1)
            {
                // Toggle enable
                bool wasEnabled = audioProcessor.isWorkspaceEnabled(idx);
                audioProcessor.setWorkspaceEnabled(idx, !wasEnabled);
                updateWorkspaceButtonColors();
            }
            else if (result == 2)
            {
                // Rename — popup text editor
                auto* editor = new juce::TextEditor();
                editor->setSize(200, 26);
                editor->setText(audioProcessor.getWorkspaceName(idx));
                editor->selectAll();
                editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(45, 45, 50));
                editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
                editor->setColour(juce::TextEditor::outlineColourId, juce::Colour(80, 80, 90));
                
                auto* okBtn = new juce::TextButton("OK");
                okBtn->setSize(50, 26);
                okBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0, 120, 200));
                
                auto* container = new juce::Component();
                container->setSize(260, 30);
                container->addAndMakeVisible(editor);
                container->addAndMakeVisible(okBtn);
                editor->setBounds(0, 2, 200, 26);
                okBtn->setBounds(206, 2, 50, 26);
                
                auto screenBounds = workspaceButtons[idx].getScreenBounds();
                
                auto* editorPtr = editor;
                auto& procRef = audioProcessor;
                auto updateFn = [this]() { updateWorkspaceButtonColors(); };
                int wsIdx = idx;
                
                auto doRename = [editorPtr, &procRef, wsIdx, updateFn]() {
                    auto newName = editorPtr->getText().trim();
                    if (newName.isNotEmpty())
                        procRef.setWorkspaceName(wsIdx, newName.substring(0, 15));
                    updateFn();
                    if (auto* callout = editorPtr->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                
                okBtn->onClick = doRename;
                editor->onReturnKey = doRename;
                editor->onEscapeKey = [editorPtr]() {
                    if (auto* callout = editorPtr->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                
                juce::CallOutBox::launchAsynchronously(
                    std::unique_ptr<juce::Component>(container),
                    screenBounds, nullptr);
            }
            else if (result == 3)
            {
                // Clear
                graphCanvas.closeAllPluginWindows();
                audioProcessor.clearWorkspace(idx);
                if (idx == audioProcessor.getActiveWorkspace())
                {
                    graphCanvas.refreshCache();
                    graphCanvas.repaint();
                    updateInstrumentSelector();  // FIX: Refresh instruments after clear
                }
                updateWorkspaceButtonColors();
            }
            else if (result >= 100 && result < 116)
            {
                // Duplicate to workspace N
                int dstIdx = result - 100;
                graphCanvas.closeAllPluginWindows();
                audioProcessor.duplicateWorkspace(idx, dstIdx);
                
                if (dstIdx == audioProcessor.getActiveWorkspace())
                {
                    graphCanvas.refreshCache();
                    graphCanvas.repaint();
                    updateInstrumentSelector();  // FIX: Refresh instruments after duplicate
                }
                updateWorkspaceButtonColors();
            }
        });
}
