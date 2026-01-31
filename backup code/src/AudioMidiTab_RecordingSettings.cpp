// #D:\Workspace\Subterraneum_plugins_daw\src\AudioMidiTab_RecordingSettings.cpp
//
// INTEGRATION GUIDE: Adding Recording Settings to Audio/MIDI Tab
// These settings are relocated from the removed Studio tab
//
// Add these UI components and handlers to your existing AudioMidiTab
// =============================================================================

// =============================================================================
// HEADER ADDITIONS (AudioMidiTab.h or equivalent)
// Add these member declarations:
// =============================================================================
/*
    // Recording Settings Section
    juce::GroupComponent recordingSettingsGroup { "recordingGroup", "Recording Settings" };
    
    juce::Label recordingFolderLabel { "folderLabel", "Recording Folder:" };
    juce::TextButton setRecordingFolderBtn { "Set Folder..." };
    juce::Label currentFolderLabel { "currentFolder", "" };
    
    // Tempo Section
    juce::GroupComponent tempoGroup { "tempoGroup", "Tempo" };
    juce::Slider tempoSlider;
    juce::Label tempoLabel { "tempoLabel", "BPM:" };
    juce::Label tempoValueLabel { "tempoValue", "120.00" };
    juce::TextButton tapTempoBtn { "Tap Tempo" };
    
    // Time Signature Section
    juce::GroupComponent timeSignatureGroup { "timeSigGroup", "Time Signature" };
    juce::ComboBox numeratorCombo;
    juce::Label slashLabel { "slash", "/" };
    juce::ComboBox denominatorCombo;
    
    // Metronome Section
    juce::GroupComponent metronomeGroup { "metronomeGroup", "Metronome" };
    juce::ToggleButton metronomeEnabledBtn { "Metronome On" };
    juce::Slider metronomeVolumeSlider;
    juce::Label metronomeVolumeLabel { "metVol", "Volume:" };
    
    // Tap tempo timing
    juce::Array<juce::int64> tapTimes;
    
    // File chooser
    std::unique_ptr<juce::FileChooser> folderChooser;
*/

// =============================================================================
// CONSTRUCTOR ADDITIONS
// Add these in the constructor after other UI setup:
// =============================================================================
/*
    // =========================================================================
    // Recording Settings Section
    // =========================================================================
    addAndMakeVisible(recordingSettingsGroup);
    
    addAndMakeVisible(recordingFolderLabel);
    recordingFolderLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(setRecordingFolderBtn);
    setRecordingFolderBtn.onClick = [this]() { chooseRecordingFolder(); };
    setRecordingFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue);
    
    addAndMakeVisible(currentFolderLabel);
    currentFolderLabel.setFont(juce::Font(11.0f));
    currentFolderLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    
    // Load saved recording folder
    loadRecordingFolder();
    
    // =========================================================================
    // Tempo Section
    // =========================================================================
    addAndMakeVisible(tempoGroup);
    
    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(20.0, 300.0, 0.01);
    tempoSlider.setValue(processor.tempo);
    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tempoSlider.onValueChange = [this]() {
        processor.tempo = tempoSlider.getValue();
        tempoValueLabel.setText(juce::String(processor.tempo, 2), juce::dontSendNotification);
    };
    
    addAndMakeVisible(tempoLabel);
    tempoLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(tempoValueLabel);
    tempoValueLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    tempoValueLabel.setJustificationType(juce::Justification::centred);
    tempoValueLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    
    addAndMakeVisible(tapTempoBtn);
    tapTempoBtn.onClick = [this]() { handleTapTempo(); };
    tapTempoBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
    
    // =========================================================================
    // Time Signature Section
    // =========================================================================
    addAndMakeVisible(timeSignatureGroup);
    
    addAndMakeVisible(numeratorCombo);
    for (int i = 1; i <= 16; ++i)
        numeratorCombo.addItem(juce::String(i), i);
    numeratorCombo.setSelectedId(processor.timeSignatureNumerator, juce::dontSendNotification);
    numeratorCombo.onChange = [this]() {
        processor.timeSignatureNumerator = numeratorCombo.getSelectedId();
    };
    
    addAndMakeVisible(slashLabel);
    slashLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    slashLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(denominatorCombo);
    denominatorCombo.addItem("2", 2);
    denominatorCombo.addItem("4", 4);
    denominatorCombo.addItem("8", 8);
    denominatorCombo.addItem("16", 16);
    denominatorCombo.setSelectedId(processor.timeSignatureDenominator, juce::dontSendNotification);
    denominatorCombo.onChange = [this]() {
        processor.timeSignatureDenominator = denominatorCombo.getSelectedId();
    };
    
    // =========================================================================
    // Metronome Section
    // =========================================================================
    addAndMakeVisible(metronomeGroup);
    
    addAndMakeVisible(metronomeEnabledBtn);
    metronomeEnabledBtn.setToggleState(processor.metronomeEnabled, juce::dontSendNotification);
    metronomeEnabledBtn.onClick = [this]() {
        processor.metronomeEnabled = metronomeEnabledBtn.getToggleState();
    };
    
    addAndMakeVisible(metronomeVolumeLabel);
    metronomeVolumeLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(metronomeVolumeSlider);
    metronomeVolumeSlider.setRange(0.0, 1.0, 0.01);
    metronomeVolumeSlider.setValue(processor.metronomeVolume);
    metronomeVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    metronomeVolumeSlider.onValueChange = [this]() {
        processor.metronomeVolume = (float)metronomeVolumeSlider.getValue();
    };
*/

// =============================================================================
// RESIZED ADDITIONS
// Add layout code in resized():
// =============================================================================
/*
    // After existing layout code, add a new section for recording/tempo settings
    // Adjust based on your existing layout
    
    auto recordingArea = area.removeFromTop(200);  // Adjust height as needed
    
    // Recording folder row
    auto recordingRow = recordingArea.removeFromTop(70);
    recordingSettingsGroup.setBounds(recordingRow);
    recordingRow = recordingRow.reduced(10, 20);
    
    auto folderRow1 = recordingRow.removeFromTop(25);
    recordingFolderLabel.setBounds(folderRow1.removeFromLeft(120));
    setRecordingFolderBtn.setBounds(folderRow1.removeFromLeft(100));
    
    auto folderRow2 = recordingRow.removeFromTop(20);
    folderRow2.removeFromLeft(120);
    currentFolderLabel.setBounds(folderRow2);
    
    recordingArea.removeFromTop(10);
    
    // Tempo, Time Signature, Metronome in a row
    auto settingsRow = recordingArea.removeFromTop(100);
    
    // Tempo group
    auto tempoArea = settingsRow.removeFromLeft(180);
    tempoGroup.setBounds(tempoArea);
    tempoArea = tempoArea.reduced(10, 20);
    
    auto tempoRow1 = tempoArea.removeFromTop(25);
    tempoLabel.setBounds(tempoRow1.removeFromLeft(40));
    tempoValueLabel.setBounds(tempoRow1.removeFromLeft(80));
    tapTempoBtn.setBounds(tempoRow1);
    
    tempoSlider.setBounds(tempoArea.removeFromTop(30));
    
    settingsRow.removeFromLeft(10);
    
    // Time Signature group
    auto timeSigArea = settingsRow.removeFromLeft(120);
    timeSignatureGroup.setBounds(timeSigArea);
    timeSigArea = timeSigArea.reduced(10, 25);
    
    auto timeSigRow = timeSigArea.removeFromTop(30);
    numeratorCombo.setBounds(timeSigRow.removeFromLeft(40));
    slashLabel.setBounds(timeSigRow.removeFromLeft(20));
    denominatorCombo.setBounds(timeSigRow.removeFromLeft(40));
    
    settingsRow.removeFromLeft(10);
    
    // Metronome group
    auto metronomeArea = settingsRow.removeFromLeft(200);
    metronomeGroup.setBounds(metronomeArea);
    metronomeArea = metronomeArea.reduced(10, 20);
    
    metronomeEnabledBtn.setBounds(metronomeArea.removeFromTop(25));
    
    auto metVolRow = metronomeArea.removeFromTop(25);
    metronomeVolumeLabel.setBounds(metVolRow.removeFromLeft(60));
    metronomeVolumeSlider.setBounds(metVolRow);
*/

// =============================================================================
// HELPER METHODS
// Add these methods to the class:
// =============================================================================
/*
void AudioMidiTab::chooseRecordingFolder() {
    folderChooser = std::make_unique<juce::FileChooser>(
        "Select Recording Folder",
        processor.recordingFolder.exists() ? processor.recordingFolder 
                                           : juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        ""
    );
    
    folderChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.exists() && result.isDirectory()) {
                processor.recordingFolder = result;
                updateFolderLabel();
                saveRecordingFolder();
            }
        }
    );
}

void AudioMidiTab::loadRecordingFolder() {
    if (auto* settings = processor.appProperties.getUserSettings()) {
        juce::String folderPath = settings->getValue("RecordingFolder", "");
        if (folderPath.isNotEmpty()) {
            processor.recordingFolder = juce::File(folderPath);
        } else {
            processor.recordingFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                            .getChildFile("Colosseum Recordings");
        }
    }
    updateFolderLabel();
}

void AudioMidiTab::saveRecordingFolder() {
    if (auto* settings = processor.appProperties.getUserSettings()) {
        settings->setValue("RecordingFolder", processor.recordingFolder.getFullPathName());
        settings->saveIfNeeded();
    }
}

void AudioMidiTab::updateFolderLabel() {
    juce::String path = processor.recordingFolder.getFullPathName();
    if (path.length() > 50) {
        path = "..." + path.substring(path.length() - 47);
    }
    currentFolderLabel.setText(path, juce::dontSendNotification);
}

void AudioMidiTab::handleTapTempo() {
    juce::int64 now = juce::Time::currentTimeMillis();
    
    // Clear old taps (more than 2 seconds ago)
    while (tapTimes.size() > 0 && (now - tapTimes[0]) > 2000) {
        tapTimes.remove(0);
    }
    
    tapTimes.add(now);
    
    // Need at least 2 taps to calculate tempo
    if (tapTimes.size() >= 2) {
        // Calculate average interval
        double totalInterval = 0.0;
        for (int i = 1; i < tapTimes.size(); ++i) {
            totalInterval += (tapTimes[i] - tapTimes[i-1]);
        }
        double avgInterval = totalInterval / (tapTimes.size() - 1);
        
        // Convert to BPM (60000ms / interval)
        double newTempo = 60000.0 / avgInterval;
        newTempo = juce::jlimit(20.0, 300.0, newTempo);
        
        processor.tempo = newTempo;
        tempoSlider.setValue(newTempo, juce::dontSendNotification);
        tempoValueLabel.setText(juce::String(newTempo, 2), juce::dontSendNotification);
    }
}
*/

// =============================================================================
// PROCESSOR ADDITIONS (PluginProcessor.h)
// Make sure these members exist in the processor:
// =============================================================================
/*
    // Recording folder (used by all Recorders)
    juce::File recordingFolder;
    
    // Tempo
    double tempo = 120.0;
    
    // Time Signature
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    
    // Metronome
    bool metronomeEnabled = false;
    float metronomeVolume = 0.5f;
*/
