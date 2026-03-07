// AudioSettingsTab.cpp
// MIDI CHANNEL DUPLICATION: Select target channels to duplicate hardware MIDI to
// FIX: Added Tempo, Time Signature, and Metronome sections from removed StudioTab
// FIX: Redesigned layout - Time Sig narrow on left, Tempo/Metronome controls on single rows
// FIX: Added timeSigValueLabel to display current time signature prominently
// FIX: Added recording folder selection button (right-aligned in Driver Settings)
// FIX: Added ASIO latency info (In/Out/Total) to status label
// FIX: Green slider for metronome (matches frame color)

#include "AudioSettingsTab.h"
#include "RecorderProcessor.h"

class SubterraneumAudioProcessorEditor;

AudioSettingsTab::AudioSettingsTab(SubterraneumAudioProcessor& p) : processor(p) { 
    deviceManager = SubterraneumAudioProcessor::standaloneDeviceManager; 
    
    // =========================================================================
    // Driver Settings Group
    // =========================================================================
    addAndMakeVisible(driverGroup); 
    driverGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::grey);
    driverGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white); 
    
    addAndMakeVisible(deviceLabel); 
    addAndMakeVisible(deviceCombo); 
    deviceCombo.addListener(this); 
    
    addAndMakeVisible(controlPanelBtn); 
    controlPanelBtn.addListener(this);
    
    #if !JUCE_WINDOWS
    controlPanelBtn.setVisible(false);
    #endif
    
    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    
    // Recording folder button and label (right-aligned)
    addAndMakeVisible(recordingFolderBtn);
    recordingFolderBtn.addListener(this);
    recordingFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    
    addAndMakeVisible(recordingFolderLabel);
    recordingFolderLabel.setFont(juce::Font(11.0f));
    recordingFolderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    recordingFolderLabel.setJustificationType(juce::Justification::centredRight);
    updateRecordingFolderLabel();
    
    // Default patch buttons
    addAndMakeVisible(saveDefaultBtn);
    saveDefaultBtn.addListener(this);
    saveDefaultBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(40, 80, 40));
    saveDefaultBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgreen);
    saveDefaultBtn.setTooltip("Right-click for info");
    saveDefaultBtn.infoText = "Saves the current rack state and audio\nsettings as the default patch loaded\nautomatically on startup.";
    
    addAndMakeVisible(clearDefaultBtn);
    clearDefaultBtn.addListener(this);
    clearDefaultBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(80, 40, 40));
    clearDefaultBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(255, 150, 150));
    
    addAndMakeVisible(defaultPatchLabel);
    defaultPatchLabel.setFont(juce::Font(11.0f));
    defaultPatchLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    defaultPatchLabel.setJustificationType(juce::Justification::centredRight);
    updateDefaultPatchLabel();
    
    // =========================================================================
    // Tempo Section Setup (moved from StudioTab)
    // =========================================================================
    addAndMakeVisible(tempoGroup);
    tempoGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::orange);
    tempoGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::orange);
    
    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(20.0, 300.0, 0.01);
    tempoSlider.setValue(120.0);
    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tempoSlider.addListener(this);
    tempoSlider.setLookAndFeel(&goldSliderLookAndFeel);
    
    addAndMakeVisible(tempoLabel);
    tempoLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(tempoValueLabel);
    tempoValueLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    tempoValueLabel.setJustificationType(juce::Justification::centred);
    tempoValueLabel.setColour(juce::Label::textColourId, juce::Colours::orange);

    // Make tempo value clickable — single click opens text editor
    tempoValueLabel.setEditable(true, true, false);
    tempoValueLabel.setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::orange);
    tempoValueLabel.setColour(juce::TextEditor::textColourId, juce::Colours::orange);
    tempoValueLabel.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
    tempoValueLabel.setColour(juce::TextEditor::highlightColourId, juce::Colours::orange.withAlpha(0.3f));
    tempoValueLabel.setColour(juce::TextEditor::outlineColourId, juce::Colours::orange);
    tempoValueLabel.onTextChange = [this]()
    {
        auto text = tempoValueLabel.getText().trim();
        double newTempo = text.getDoubleValue();

        // Clamp to slider range (20-300 BPM)
        newTempo = juce::jlimit(20.0, 300.0, newTempo);

        // Round to 1 decimal place (e.g. 75.6)
        newTempo = std::round(newTempo * 10.0) / 10.0;

        currentTempo = newTempo;
        tempoSlider.setValue(newTempo, juce::dontSendNotification);
        processor.masterTempo.store(currentTempo);
        tempoValueLabel.setText(juce::String(newTempo, 1), juce::dontSendNotification);
    };
    
    addAndMakeVisible(tapTempoBtn);
    tapTempoBtn.addListener(this);
    tapTempoBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
    
    // =========================================================================
    // Time Signature Section Setup (moved from StudioTab)
    // FIX: Added prominent value label display
    // =========================================================================
    addAndMakeVisible(timeSignatureGroup);
    timeSignatureGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::cyan);
    timeSignatureGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::cyan);
    
    // FIX: Time signature value label - large white text on left
    addAndMakeVisible(timeSigValueLabel);
    timeSigValueLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    timeSigValueLabel.setJustificationType(juce::Justification::centredLeft);
    timeSigValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible(numeratorCombo);
    for (int i = 1; i <= 16; ++i)
        numeratorCombo.addItem(juce::String(i), i);
    numeratorCombo.setSelectedId(4);
    numeratorCombo.addListener(this);
    numeratorCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    numeratorCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(40, 40, 45));
    numeratorCombo.setColour(juce::ComboBox::outlineColourId, juce::Colours::cyan.darker());
    
    addAndMakeVisible(slashLabel);
    slashLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    slashLabel.setJustificationType(juce::Justification::centred);
    slashLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible(denominatorCombo);
    denominatorCombo.addItem("2", 2);
    denominatorCombo.addItem("4", 4);
    denominatorCombo.addItem("8", 8);
    denominatorCombo.addItem("16", 16);
    denominatorCombo.setSelectedId(4);
    denominatorCombo.addListener(this);
    denominatorCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    denominatorCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(40, 40, 45));
    denominatorCombo.setColour(juce::ComboBox::outlineColourId, juce::Colours::cyan.darker());
    
    // =========================================================================
    // Metronome Section Setup (moved from StudioTab)
    // =========================================================================
    addAndMakeVisible(metronomeGroup);
    metronomeGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::lightgreen);
    metronomeGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::lightgreen);
    
    addAndMakeVisible(metronomeBtn);
    metronomeBtn.addListener(this);
    
    addAndMakeVisible(metronomeVolumeSlider);
    metronomeVolumeSlider.setRange(0.0, 1.0, 0.01);
    metronomeVolumeSlider.setValue(0.7);
    metronomeVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    metronomeVolumeSlider.setLookAndFeel(&greenSliderLookAndFeel);  // FIX: Green slider to match frame
    
    addAndMakeVisible(metronomeVolumeLabel);
    metronomeVolumeLabel.setJustificationType(juce::Justification::centredRight);
    
    // =========================================================================
    // Initialize device manager
    // =========================================================================
    if (deviceManager) { 
        enforceDriverType(); 
        
        juce::String savedDevice = processor.getSavedAudioDeviceName();
        if (savedDevice.isEmpty()) {
            deviceManager->closeAudioDevice();
        } else {
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName = savedDevice;
            setup.outputDeviceName = savedDevice;
            setup.useDefaultInputChannels = true;
            setup.useDefaultOutputChannels = true;
            deviceManager->setAudioDeviceSetup(setup, false);
        }
        
        // OnStage: No MIDI routing (effects-only mode)
        updateDeviceList();
        updateStatusLabel();
        
        deviceManager->addChangeListener(this);
    }
    
    // Load saved recording folder
    auto* userSettings = processor.appProperties.getUserSettings();
    if (userSettings) {
        juce::String savedFolder = userSettings->getValue("DefaultRecordingFolder", "");
        if (savedFolder.isNotEmpty()) {
            juce::File folder(savedFolder);
            if (folder.exists()) {
                RecorderProcessor::setGlobalDefaultFolder(folder);
            }
        }
        
    }
    updateRecordingFolderLabel();
}

AudioSettingsTab::~AudioSettingsTab() { 
    if (deviceManager) deviceManager->removeChangeListener(this); 
    tempoSlider.setLookAndFeel(nullptr);
    metronomeVolumeSlider.setLookAndFeel(nullptr);
}

void AudioSettingsTab::paint(juce::Graphics& g) { 
    g.fillAll(Style::colBackground);
    if (!deviceManager) { 
        g.setColour(juce::Colours::red); 
        g.setFont(20.0f);
        g.drawText("Audio Settings only available in Standalone Mode", getLocalBounds(), juce::Justification::centred, true);
    } 
}

void AudioSettingsTab::resized() { 
    if (!deviceManager) return;
    
    auto area = getLocalBounds().reduced(10); 
    
    // =========================================================================
    // Driver Settings - Top row (increased height for folder controls)
    // =========================================================================
    auto driverArea = area.removeFromTop(160); 
    driverGroup.setBounds(driverArea);
    driverArea.reduce(10, 25); 
    
    auto row1 = driverArea.removeFromTop(30); 
    
    // Left side: Device controls
    deviceLabel.setBounds(row1.removeFromLeft(100)); 
    deviceCombo.setBounds(row1.removeFromLeft(250)); 
    
    #if JUCE_WINDOWS
    row1.removeFromLeft(10); 
    controlPanelBtn.setBounds(row1.removeFromLeft(100));
    #endif
    
    // Right side: Recording folder button only (OnStage: no MIDI reconnect or sampler)
    auto folderBtn = row1.removeFromRight(155);
    recordingFolderBtn.setBounds(folderBtn);
    
    driverArea.removeFromTop(5);
    
    // Second row: Status label (left) and recording folder path label (right)
    auto row2 = driverArea.removeFromTop(20);
    
    // Recording folder label on the right
    auto folderLabelArea = row2.removeFromRight(350);
    recordingFolderLabel.setBounds(folderLabelArea);
    
    // Status label on the left
    statusLabel.setBounds(row2);
    
    driverArea.removeFromTop(4);
    
    // Third row: Default patch buttons (left) + label (right)
    auto row3 = driverArea.removeFromTop(24);
    saveDefaultBtn.setBounds(row3.removeFromLeft(130));
    row3.removeFromLeft(5);
    clearDefaultBtn.setBounds(row3.removeFromLeft(100));
    row3.removeFromLeft(10);
    defaultPatchLabel.setBounds(row3);
    
    area.removeFromTop(10);
    
    // =========================================================================
    // Time Signature, Tempo, Metronome - Second row (side by side)
    // Time Sig is narrow (1/6), Tempo takes 2/5, Metronome takes remaining
    // =========================================================================
    auto tempoRow = area.removeFromTop(70);
    int totalWidth = tempoRow.getWidth();
    int timeSigWidth = totalWidth / 6;          // Narrow time signature
    int tempoWidth = (totalWidth * 2) / 5;      // Tempo section
    
    // Time Signature section (narrow, on left)
    // Layout: [Value Label "4/4"] [numerator combo] [/] [denominator combo]
    auto timeSigArea = tempoRow.removeFromLeft(timeSigWidth);
    timeSignatureGroup.setBounds(timeSigArea);
    auto timeSigInner = timeSigArea.reduced(8, 20);
    
    // Value label on left (large "4/4" display)
    timeSigValueLabel.setBounds(timeSigInner.removeFromLeft(50));
    
    timeSigInner.removeFromLeft(4);
    
    // Combo boxes on right
    int comboWidth = 38;
    int slashWidth = 14;
    
    auto timeSigControlArea = timeSigInner;
    numeratorCombo.setBounds(timeSigControlArea.removeFromLeft(comboWidth));
    slashLabel.setBounds(timeSigControlArea.removeFromLeft(slashWidth));
    denominatorCombo.setBounds(timeSigControlArea.removeFromLeft(comboWidth));
    
    tempoRow.removeFromLeft(10);
    
    // Tempo section - single row: [BPM label] [slider] [value] [TAP]
    auto tempoArea = tempoRow.removeFromLeft(tempoWidth);
    tempoGroup.setBounds(tempoArea);
    auto tempoInner = tempoArea.reduced(10, 20);
    
    auto tempoControlRow = tempoInner;
    tempoLabel.setBounds(tempoControlRow.removeFromLeft(35));
    tempoControlRow.removeFromLeft(5);
    
    // TAP button on far right
    auto tapBtnArea = tempoControlRow.removeFromRight(50);
    tapTempoBtn.setBounds(tapBtnArea.reduced(0, 2));
    
    tempoControlRow.removeFromRight(8);
    
    // Value label next to TAP (wider to fit typed input)
    auto valueLabelArea = tempoControlRow.removeFromRight(75);
    tempoValueLabel.setBounds(valueLabelArea);
    
    tempoControlRow.removeFromRight(8);
    
    // Slider takes remaining space
    tempoSlider.setBounds(tempoControlRow);
    
    tempoRow.removeFromLeft(10);
    
    // Metronome section - single row: [Volume label] [slider] [Enable checkbox]
    auto metronomeArea = tempoRow;
    metronomeGroup.setBounds(metronomeArea);
    auto metronomeInner = metronomeArea.reduced(10, 20);
    
    auto metronomeControlRow = metronomeInner;
    metronomeVolumeLabel.setBounds(metronomeControlRow.removeFromLeft(45));
    metronomeControlRow.removeFromLeft(5);
    
    // Enable checkbox on far right
    auto enableBtnArea = metronomeControlRow.removeFromRight(70);
    metronomeBtn.setBounds(enableBtnArea.reduced(0, 2));
    
    metronomeControlRow.removeFromRight(8);
    
    // Slider takes remaining space
    metronomeVolumeSlider.setBounds(metronomeControlRow);
    
    // OnStage: No MIDI routing UI (effects-only mode)
}

void AudioSettingsTab::changeListenerCallback(juce::ChangeBroadcaster* source) { 
    if (source == deviceManager) { 
        updateDeviceList();
        updateStatusLabel();
    } 
}

void AudioSettingsTab::sliderValueChanged(juce::Slider* s) {
    if (s == &tempoSlider) {
        currentTempo = tempoSlider.getValue();
        updateTempoDisplay();
        processor.masterTempo.store(currentTempo);
    }
}

void AudioSettingsTab::updateTempoDisplay() {
    tempoValueLabel.setText(juce::String(currentTempo, 1), juce::dontSendNotification);
}

void AudioSettingsTab::updateTimeSigDisplay() {
    timeSigValueLabel.setText(juce::String(timeSigNumerator) + "/" + juce::String(timeSigDenominator), 
                              juce::dontSendNotification);
}

void AudioSettingsTab::selectRecordingFolder() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Default Recording Folder",
        RecorderProcessor::getEffectiveDefaultFolder(),
        "");
    
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.exists() && result.isDirectory()) {
                RecorderProcessor::setGlobalDefaultFolder(result);
                
                // Save to settings
                auto* userSettings = processor.appProperties.getUserSettings();
                if (userSettings) {
                    userSettings->setValue("DefaultRecordingFolder", result.getFullPathName());
                    userSettings->saveIfNeeded();
                }
                
                updateRecordingFolderLabel();
            }
        });
}

void AudioSettingsTab::updateRecordingFolderLabel() {
    auto folder = RecorderProcessor::getEffectiveDefaultFolder();
    juce::String path = folder.getFullPathName();
    
    // Truncate if too long
    if (path.length() > 45) {
        path = "..." + path.substring(path.length() - 42);
    }
    
    recordingFolderLabel.setText("Rec: " + path, juce::dontSendNotification);
}

void AudioSettingsTab::selectSamplerFolder() {
    // Sampler tools removed from OnStage
}

void AudioSettingsTab::updateSamplerFolderLabel() {
    // OnStage: Sampler tools removed (effects-only mode)
}

void AudioSettingsTab::saveAsDefault() {
    auto defaultFile = processor.getDefaultPatchFile();
    
    // Ensure parent directory exists
    defaultFile.getParentDirectory().createDirectory();
    
    processor.saveUserPreset(defaultFile);
    
    // Also save current audio device name as default
    processor.saveAudioSettings();
    
    updateDefaultPatchLabel();
    
    // Flash the button to confirm
    saveDefaultBtn.setButtonText("Saved!");
    saveDefaultBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(20, 120, 20));
    
    auto safeThis = juce::Component::SafePointer<AudioSettingsTab>(this);
    juce::Timer::callAfterDelay(1500, [safeThis]() {
        if (safeThis) {
            safeThis->saveDefaultBtn.setButtonText("Save as Default");
            safeThis->saveDefaultBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(40, 80, 40));
        }
    });
}

void AudioSettingsTab::clearDefault() {
    auto defaultFile = processor.getDefaultPatchFile();
    
    if (defaultFile.existsAsFile()) {
        defaultFile.deleteFile();
        
        updateDefaultPatchLabel();
        
        // Flash the button to confirm
        clearDefaultBtn.setButtonText("Cleared!");
        clearDefaultBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(120, 20, 20));
        
        auto safeThis = juce::Component::SafePointer<AudioSettingsTab>(this);
        juce::Timer::callAfterDelay(1500, [safeThis]() {
            if (safeThis) {
                safeThis->clearDefaultBtn.setButtonText("Clear Default");
                safeThis->clearDefaultBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(80, 40, 40));
            }
        });
    }
}

void AudioSettingsTab::updateDefaultPatchLabel() {
    auto defaultFile = processor.getDefaultPatchFile();
    
    if (defaultFile.existsAsFile()) {
        auto modTime = defaultFile.getLastModificationTime();
        juce::String timeStr = modTime.toString(true, true, false);
        defaultPatchLabel.setText("Default patch saved: " + timeStr, juce::dontSendNotification);
        defaultPatchLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        clearDefaultBtn.setEnabled(true);
    } else {
        defaultPatchLabel.setText("No default patch set", juce::dontSendNotification);
        defaultPatchLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        clearDefaultBtn.setEnabled(false);
    }
}

void AudioSettingsTab::handleTapTempo() {
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    // Reset if more than 2 seconds since last tap
    if (now - lastTapTime > 2.0) {
        tapTimes.clear();
    }
    
    tapTimes.push_back(now);
    lastTapTime = now;
    
    // Keep only last 8 taps
    while (tapTimes.size() > 8) {
        tapTimes.erase(tapTimes.begin());
    }
    
    // Calculate average tempo from tap intervals
    if (tapTimes.size() >= 2) {
        double totalInterval = 0.0;
        int intervalCount = 0;
        
        for (size_t i = 1; i < tapTimes.size(); ++i) {
            double interval = tapTimes[i] - tapTimes[i - 1];
            if (interval > 0.1 && interval < 2.0) {
                totalInterval += interval;
                intervalCount++;
            }
        }
        
        if (intervalCount > 0) {
            double avgInterval = totalInterval / intervalCount;
            double bpm = 60.0 / avgInterval;
            bpm = juce::jlimit(20.0, 300.0, bpm);
            
            currentTempo = bpm;
            tempoSlider.setValue(bpm, juce::dontSendNotification);
            updateTempoDisplay();
            processor.masterTempo.store(currentTempo);
        }
    }
}

void AudioSettingsTab::enforceDriverType() { 
    if (!deviceManager) return;
    
    #if JUCE_WINDOWS 
    const juce::String targetType = "ASIO";
    #elif JUCE_MAC 
    const juce::String targetType = "CoreAudio";
    #elif JUCE_LINUX
    juce::String targetType = "ALSA";
    auto availableTypes = deviceManager->getAvailableDeviceTypes();
    for (auto* type : availableTypes) {
        if (type->getTypeName() == "JACK") {
            targetType = "JACK";
            break;
        }
    }
    #else 
    const juce::String targetType = "";
    #endif
    
    if (targetType.isNotEmpty()) { 
        auto currentType = deviceManager->getCurrentAudioDeviceType();
        if (currentType != targetType) { 
            deviceManager->setCurrentAudioDeviceType(targetType, false);
        } 
    } 
}

void AudioSettingsTab::updateDeviceList() { 
    deviceCombo.clear(juce::dontSendNotification); 
    if (!deviceManager) return; 
    
    deviceCombo.addItem("OFF (No Audio Device)", 1);
    
    if (auto* type = deviceManager->getCurrentDeviceTypeObject()) { 
        auto deviceNames = type->getDeviceNames();
        
        int itemId = 2;
        for (auto& name : deviceNames) {
            deviceCombo.addItem(name, itemId++);
        }
        
        if (auto* current = deviceManager->getCurrentAudioDevice()) { 
            deviceCombo.setText(current->getName(), juce::dontSendNotification);
        } else { 
            deviceCombo.setSelectedId(1, juce::dontSendNotification);
        } 
    } else { 
        deviceCombo.setSelectedId(1, juce::dontSendNotification);
    } 
}

// =============================================================================
// FIX: Added ASIO latency info (In/Out/Total) to status label
// =============================================================================
void AudioSettingsTab::updateStatusLabel() {
    if (!deviceManager) return;
    
    if (auto* device = deviceManager->getCurrentAudioDevice()) {
        auto inputNames = device->getInputChannelNames();
        auto outputNames = device->getOutputChannelNames();
        
        juce::String status = "Active: " + device->getName();
        status += " | SR: " + juce::String((int)device->getCurrentSampleRate()) + " Hz";
        status += " | Buf: " + juce::String(device->getCurrentBufferSizeSamples());
        status += " | I/O: " + juce::String(inputNames.size()) + "/" + juce::String(outputNames.size());
        
        // FIX: Add latency info
        double sr = device->getCurrentSampleRate();
        int inLat = device->getInputLatencyInSamples();
        int outLat = device->getOutputLatencyInSamples();
        int bufSize = device->getCurrentBufferSizeSamples();
        if (sr > 0) {
            double inMs  = (inLat + bufSize) * 1000.0 / sr;
            double outMs = (outLat + bufSize) * 1000.0 / sr;
            double totalMs = inMs + outMs;
            status += " | Latency In: " + juce::String(inMs, 1) + "ms Out: " + juce::String(outMs, 1) + "ms (Total: " + juce::String(totalMs, 1) + "ms)";
        }
        
        statusLabel.setText(status, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    } else {
        statusLabel.setText("Audio Device: OFF", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
}

void AudioSettingsTab::updateMidiInputsList() { 
    // OnStage: MIDI routing removed (effects-only mode)
}

void AudioSettingsTab::updateMidiOutputsList() {
    // OnStage: MIDI routing removed (effects-only mode)
}

void AudioSettingsTab::updateMidiInputRowButton(MidiInputRow* row) {
    if (!row || !row->channelButton) return;
    
    int mask = row->channelMask;
    int enabledCount = 0;
    for (int i = 0; i < 16; ++i) 
        if ((mask >> i) & 1) enabledCount++;
    
    juce::String text;
    juce::Colour btnColor;
    juce::Colour textColor;
    
    if (enabledCount == 0) {
        text = "OFF";
        btnColor = juce::Colours::grey.darker();
        textColor = juce::Colours::lightgrey;
    } else if (enabledCount == 16) {
        text = "ALL";
        btnColor = juce::Colour(0xff8B0000);
        textColor = juce::Colours::white;
    } else if (enabledCount == 1) {
        for (int i = 0; i < 16; ++i) {
            if ((mask >> i) & 1) {
                text = "CH " + juce::String(i + 1);
                break;
            }
        }
        btnColor = juce::Colour(0xff8B0000);
        textColor = juce::Colours::white;
    } else {
        text = juce::String(enabledCount) + " CH";
        btnColor = juce::Colour(0xff8B0000);
        textColor = juce::Colours::white;
    }
    
    row->channelButton->setButtonText(text);
    row->channelButton->setColour(juce::TextButton::buttonColourId, btnColor);
    row->channelButton->setColour(juce::TextButton::textColourOffId, textColor);
}

void AudioSettingsTab::openMidiInputChannelSelector(MidiInputRow* row) {
    if (!row) return;
    
    auto selector = std::make_unique<MidiInputChannelSelector>(
        row->deviceName, 
        row->channelMask, 
        [this, row](int newMask) {
            int oldMask = row->channelMask;
            row->channelMask = newMask;
            
            bool wasEnabled = (oldMask != 0);
            bool nowEnabled = (newMask != 0);
            
            if (wasEnabled != nowEnabled) {
                deviceManager->setMidiInputDeviceEnabled(row->identifier, nowEnabled);
            }
            
            auto* userSettings = processor.appProperties.getUserSettings();
            if (userSettings) {
                juce::String maskKey = "MidiMask_" + row->identifier.replaceCharacters(" :/\\", "____");
                userSettings->setValue(maskKey, newMask);
                userSettings->saveIfNeeded();
            }
            
            processor.updateHardwareMidiChannelMasks();
            
            updateMidiInputRowButton(row);
        }
    );
    
    juce::CallOutBox::launchAsynchronously(std::move(selector), row->channelButton->getScreenBounds(), nullptr);
}

void AudioSettingsTab::updateMidiOutputRowButton(MidiOutputRow* row) {
    if (!row || !row->channelButton) return;
    
    int mask = row->channelMask;
    int enabledCount = 0;
    for (int i = 0; i < 16; ++i)
        if ((mask >> i) & 1) enabledCount++;
    
    juce::String text;
    juce::Colour btnColor;
    juce::Colour textColor;
    
    if (enabledCount == 0) {
        text = "OFF";
        btnColor = juce::Colours::grey.darker();
        textColor = juce::Colours::lightgrey;
    } else if (enabledCount == 16) {
        text = "ALL";
        btnColor = juce::Colour(0xffFFD700);
        textColor = juce::Colours::black;
    } else if (enabledCount == 1) {
        for (int i = 0; i < 16; ++i) {
            if ((mask >> i) & 1) {
                text = "CH " + juce::String(i + 1);
                break;
            }
        }
        btnColor = juce::Colours::green;
        textColor = juce::Colours::black;
    } else {
        text = juce::String(enabledCount) + " CH";
        btnColor = juce::Colour(0xff9B7A00);
        textColor = juce::Colours::black;
    }
    
    row->channelButton->setButtonText(text);
    row->channelButton->setColour(juce::TextButton::buttonColourId, btnColor);
    row->channelButton->setColour(juce::TextButton::textColourOffId, textColor);
}

void AudioSettingsTab::openMidiOutputChannelSelector(MidiOutputRow* row) {
    if (!row) return;
    
    auto selector = std::make_unique<MidiInputChannelSelector>(
        row->deviceName + " Output",
        row->channelMask,
        [this, row](int newMask) {
            row->channelMask = newMask;
            
            auto* userSettings = processor.appProperties.getUserSettings();
            if (userSettings) {
                juce::String maskKey = "MidiOutMask_" + row->identifier.replaceCharacters(" :/\\", "____");
                userSettings->setValue(maskKey, newMask);
                userSettings->saveIfNeeded();
            }
            
            updateMidiOutputRowButton(row);
        }
    );
    
    juce::CallOutBox::launchAsynchronously(std::move(selector), row->channelButton->getScreenBounds(), nullptr);
}

void AudioSettingsTab::comboBoxChanged(juce::ComboBox* cb) { 
    if (!deviceManager) return;
    
    if (cb == &deviceCombo) { 
        int selectedId = cb->getSelectedId();
        
        if (selectedId == 1) { 
            deviceManager->closeAudioDevice();
            processor.saveAudioSettings();
            processor.updateIOChannelCount();
            updateStatusLabel();
        } else { 
            juce::String deviceName = cb->getText();
            
            auto* deviceType = deviceManager->getCurrentDeviceTypeObject();
            if (!deviceType) return;
            
            deviceType->scanForDevices();
            auto deviceNames = deviceType->getDeviceNames();
            
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName = deviceName;
            setup.outputDeviceName = deviceName;
            
            juce::BigInteger allInputs, allOutputs;
            allInputs.setRange(0, 256, true);
            allOutputs.setRange(0, 256, true);
            
            setup.inputChannels = allInputs;
            setup.outputChannels = allOutputs;
            setup.useDefaultInputChannels = false;
            setup.useDefaultOutputChannels = false;
            
            auto result = deviceManager->setAudioDeviceSetup(setup, true);
            
            if (result.isEmpty()) {
                processor.saveAudioSettings();
                processor.updateIOChannelCount();
            }
            
            updateStatusLabel();
        } 
    }
    else if (cb == &numeratorCombo) {
        timeSigNumerator = numeratorCombo.getSelectedId();
        processor.masterTimeSigNumerator.store(timeSigNumerator);
        updateTimeSigDisplay();
    }
    else if (cb == &denominatorCombo) {
        timeSigDenominator = denominatorCombo.getSelectedId();
        processor.masterTimeSigDenominator.store(timeSigDenominator);
        updateTimeSigDisplay();
    }
}

void AudioSettingsTab::buttonClicked(juce::Button* b) { 
    if (!deviceManager) return;
    
    if (b == &controlPanelBtn) { 
        if (auto* device = deviceManager->getCurrentAudioDevice()) { 
            if (device->hasControlPanel()) 
                device->showControlPanel();
        } 
    }
    else if (b == &tapTempoBtn) {
        handleTapTempo();
    }
    else if (b == &metronomeBtn) {
        // TODO: processor.metronomeEnabled.store(metronomeBtn.getToggleState());
        // Metronome not yet implemented in processor
    }
    else if (b == &recordingFolderBtn) {
        selectRecordingFolder();
    }
    else if (b == &saveDefaultBtn) {
        saveAsDefault();
    }
    else if (b == &clearDefaultBtn) {
        clearDefault();
    }
}




void AudioSettingsTab::reconnectMidiDevices()
{
    // OnStage: MIDI routing removed (effects-only mode)
}
