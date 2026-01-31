// #D:\Workspace\Subterraneum_plugins_daw\src\StudioTab.cpp
// CPU OPTIMIZATION: Changed timer from 50ms to 100ms (still responsive for recording time display)
// FIX 2: Added recording folder selection and save file functionality

#include "StudioTab.h"
#include "PluginProcessor.h"

StudioTab::StudioTab(SubterraneumAudioProcessor& p) : processor(p) {
    // ==========================================================================
    // Recording Section Setup
    // ==========================================================================
    addAndMakeVisible(recordingGroup);
    
    addAndMakeVisible(recordBtn);
    recordBtn.addListener(this);
    recordBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    recordBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    
    addAndMakeVisible(stopBtn);
    stopBtn.addListener(this);
    stopBtn.setEnabled(false);
    
    // FIX 2: Set Recording Folder button
    addAndMakeVisible(setFolderBtn);
    setFolderBtn.addListener(this);
    setFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue);
    
    // FIX 2: Save File button (renamed from "Show File")
    addAndMakeVisible(saveFileBtn);
    saveFileBtn.addListener(this);
    saveFileBtn.setEnabled(false);
    saveFileBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    
    addAndMakeVisible(recordingLED);
    
    addAndMakeVisible(recordingStatusLabel);
    recordingStatusLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(recordingTimeLabel);
    recordingTimeLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    recordingTimeLabel.setJustificationType(juce::Justification::centred);
    recordingTimeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    
    addAndMakeVisible(recordingFormatLabel);
    recordingFormatLabel.setFont(juce::Font(11.0f));
    recordingFormatLabel.setJustificationType(juce::Justification::centred);
    recordingFormatLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    
    // FIX 2: Load recording folder preference
    loadRecordingFolder();
    
    // ==========================================================================
    // Tempo Section Setup
    // ==========================================================================
    addAndMakeVisible(tempoGroup);
    
    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(20.0, 300.0, 0.01);
    tempoSlider.setValue(120.0);
    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tempoSlider.addListener(this);
    // FIX #3: Apply gold slider look and feel
    tempoSlider.setLookAndFeel(&goldSliderLookAndFeel);
    
    addAndMakeVisible(tempoLabel);
    tempoLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(tempoValueLabel);
    tempoValueLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    tempoValueLabel.setJustificationType(juce::Justification::centred);
    tempoValueLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    
    addAndMakeVisible(tapTempoBtn);
    tapTempoBtn.addListener(this);
    tapTempoBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
    
    // ==========================================================================
    // Time Signature Section Setup
    // ==========================================================================
    addAndMakeVisible(timeSignatureGroup);
    
    addAndMakeVisible(numeratorCombo);
    for (int i = 1; i <= 16; ++i)
        numeratorCombo.addItem(juce::String(i), i);
    numeratorCombo.setSelectedId(4);
    numeratorCombo.addListener(this);
    
    addAndMakeVisible(slashLabel);
    slashLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    slashLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(denominatorCombo);
    denominatorCombo.addItem("2", 2);
    denominatorCombo.addItem("4", 4);
    denominatorCombo.addItem("8", 8);
    denominatorCombo.addItem("16", 16);
    denominatorCombo.setSelectedId(4);
    denominatorCombo.addListener(this);
    
    // ==========================================================================
    // Metronome Section Setup
    // ==========================================================================
    addAndMakeVisible(metronomeGroup);
    
    addAndMakeVisible(metronomeBtn);
    metronomeBtn.addListener(this);
    
    addAndMakeVisible(metronomeVolumeSlider);
    metronomeVolumeSlider.setRange(0.0, 1.0, 0.01);
    metronomeVolumeSlider.setValue(0.7);
    metronomeVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    // FIX #3: Apply gold slider look and feel
    metronomeVolumeSlider.setLookAndFeel(&goldSliderLookAndFeel);
    
    // ==========================================================================
    // Info Label
    // ==========================================================================
    addAndMakeVisible(infoLabel);
    infoLabel.setFont(juce::Font(11.0f));
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    infoLabel.setText("Recording captures master output at 24-bit/44.1kHz", juce::dontSendNotification);
    
    // CPU FIX: Changed from 50ms to 100ms - still responsive for time display, but less CPU
    startTimer(100);
}

StudioTab::~StudioTab() {
    stopTimer();
    // FIX #3: Clear custom look and feel to avoid dangling pointers
    tempoSlider.setLookAndFeel(nullptr);
    metronomeVolumeSlider.setLookAndFeel(nullptr);
}

void StudioTab::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground);
}

void StudioTab::resized() {
    auto area = getLocalBounds().reduced(10);
    
    // Recording section - top
    auto recordingArea = area.removeFromTop(150);
    recordingGroup.setBounds(recordingArea);
    auto recordingInner = recordingArea.reduced(15, 25);
    
    auto recordingTop = recordingInner.removeFromTop(40);
    recordingLED.setBounds(recordingTop.removeFromLeft(30).reduced(5));
    recordingTop.removeFromLeft(10);
    recordBtn.setBounds(recordingTop.removeFromLeft(70));
    recordingTop.removeFromLeft(10);
    stopBtn.setBounds(recordingTop.removeFromLeft(70));
    recordingTop.removeFromLeft(10);
    // FIX 2: New button layout
    setFolderBtn.setBounds(recordingTop.removeFromLeft(150));
    recordingTop.removeFromLeft(10);
    saveFileBtn.setBounds(recordingTop.removeFromLeft(100));
    recordingTop.removeFromLeft(20);
    recordingStatusLabel.setBounds(recordingTop);
    
    recordingInner.removeFromTop(10);
    recordingTimeLabel.setBounds(recordingInner.removeFromTop(35));
    recordingFormatLabel.setBounds(recordingInner.removeFromTop(20));
    
    area.removeFromTop(15);
    
    // Tempo and Time Signature - side by side
    auto middleArea = area.removeFromTop(120);
    auto tempoArea = middleArea.removeFromLeft(middleArea.getWidth() / 2 - 5);
    middleArea.removeFromLeft(10);
    auto timeSigArea = middleArea;
    
    // Tempo section
    tempoGroup.setBounds(tempoArea);
    auto tempoInner = tempoArea.reduced(15, 25);
    auto tempoSliderArea = tempoInner.removeFromTop(40);
    tempoLabel.setBounds(tempoSliderArea.removeFromLeft(50));
    
    // FIX #3: Make slider 2x wider and center vertically
    // Calculate slider width as percentage of available space, then double it
    int availableWidth = tempoSliderArea.getWidth();
    int sliderWidth = juce::jmin(availableWidth, availableWidth * 2); // Use all available width
    int sliderHeight = 30; // Increased height for better thumb visibility
    int sliderX = tempoSliderArea.getX();
    int sliderY = tempoSliderArea.getY() + (tempoSliderArea.getHeight() - sliderHeight) / 2;
    tempoSlider.setBounds(sliderX, sliderY, sliderWidth, sliderHeight);
    
    tempoInner.removeFromTop(5);
    auto tempoBottom = tempoInner.removeFromTop(35);
    tempoValueLabel.setBounds(tempoBottom.removeFromLeft(100));
    tempoBottom.removeFromLeft(10);
    tapTempoBtn.setBounds(tempoBottom.removeFromLeft(80));
    
    // Time signature section
    timeSignatureGroup.setBounds(timeSigArea);
    auto timeSigInner = timeSigArea.reduced(15, 25);
    timeSigInner.removeFromTop(20);
    auto timeSigCombos = timeSigInner.removeFromTop(30);
    timeSigCombos.removeFromLeft(20);
    numeratorCombo.setBounds(timeSigCombos.removeFromLeft(60));
    timeSigCombos.removeFromLeft(5);
    slashLabel.setBounds(timeSigCombos.removeFromLeft(20));
    timeSigCombos.removeFromLeft(5);
    denominatorCombo.setBounds(timeSigCombos.removeFromLeft(60));
    
    area.removeFromTop(15);
    
    // Metronome section (optional)
    auto metronomeArea = area.removeFromTop(90);
    metronomeGroup.setBounds(metronomeArea);
    auto metronomeInner = metronomeArea.reduced(15, 25);
    metronomeBtn.setBounds(metronomeInner.removeFromTop(25));
    metronomeInner.removeFromTop(5);
    
    // FIX #3: Make slider 2x wider and center vertically in remaining space
    auto volumeSliderArea = metronomeInner.removeFromTop(35); // Increased from 25 to 35 for more space
    int volSliderWidth = juce::jmin(volumeSliderArea.getWidth(), volumeSliderArea.getWidth() * 2);
    int volSliderHeight = 30;
    int volSliderX = volumeSliderArea.getX();
    int volSliderY = volumeSliderArea.getY() + (volumeSliderArea.getHeight() - volSliderHeight) / 2;
    metronomeVolumeSlider.setBounds(volSliderX, volSliderY, volSliderWidth, volSliderHeight);
    
    area.removeFromTop(15);
    infoLabel.setBounds(area.removeFromTop(20));
}

void StudioTab::buttonClicked(juce::Button* b) {
    if (b == &recordBtn) {
        startRecording();
    }
    else if (b == &stopBtn) {
        stopRecording();
    }
    else if (b == &setFolderBtn) {
        // FIX 2: Set recording folder
        setRecordingFolder();
    }
    else if (b == &saveFileBtn) {
        // FIX 2: Save file with custom name/location
        saveRecordingFile();
    }
    else if (b == &tapTempoBtn) {
        handleTapTempo();
    }
}

void StudioTab::sliderValueChanged(juce::Slider* s) {
    if (s == &tempoSlider) {
        currentTempo = tempoSlider.getValue();
        updateTempoDisplay();
        
        // Update processor tempo
        processor.masterTempo.store(currentTempo);
    }
}

void StudioTab::comboBoxChanged(juce::ComboBox* cb) {
    if (cb == &numeratorCombo) {
        timeSigNumerator = numeratorCombo.getSelectedId();
        processor.masterTimeSigNumerator.store(timeSigNumerator);
    }
    else if (cb == &denominatorCombo) {
        timeSigDenominator = denominatorCombo.getSelectedId();
        processor.masterTimeSigDenominator.store(timeSigDenominator);
    }
}

void StudioTab::timerCallback() {
    if (processor.isCurrentlyRecording()) {
        recordingLED.pulse();
        // Update time based on elapsed time since recording started
        double elapsed = (juce::Time::getMillisecondCounterHiRes() - recordingStartTime) / 1000.0;
        recordingTimeLabel.setText(formatTime(elapsed), juce::dontSendNotification);
    }
}

void StudioTab::startRecording() {
    // Start recording in processor
    if (processor.startRecording()) {
        recordingSampleRate = processor.getSampleRate();
        if (recordingSampleRate <= 0) recordingSampleRate = 44100.0;
        
        recordingStartTime = juce::Time::getMillisecondCounterHiRes();
        
        recordBtn.setEnabled(false);
        stopBtn.setEnabled(true);
        saveFileBtn.setEnabled(false);
        recordingLED.setActive(true);
        recordingStatusLabel.setText("Recording...", juce::dontSendNotification);
        recordingStatusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        
        // Update format label with actual sample rate
        recordingFormatLabel.setText("24-bit / " + juce::String((int)recordingSampleRate) + " Hz / Stereo", 
                                      juce::dontSendNotification);
    } else {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Recording Error", "Could not start recording. Check disk space and permissions.");
    }
}

void StudioTab::stopRecording() {
    double elapsed = (juce::Time::getMillisecondCounterHiRes() - recordingStartTime) / 1000.0;
    
    processor.stopRecording();
    
    recordBtn.setEnabled(true);
    stopBtn.setEnabled(false);
    saveFileBtn.setEnabled(true);  // FIX 2: Enable save file button
    recordingLED.setActive(false);
    
    recordingStatusLabel.setText("Recorded " + formatTime(elapsed), juce::dontSendNotification);
    recordingStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
}

// FIX 2: Set default recording folder
void StudioTab::setRecordingFolder() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Recording Folder", 
        recordingFolder.exists() ? recordingFolder : juce::File::getSpecialLocation(juce::File::userMusicDirectory)
    );
    
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    
    chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result != juce::File{}) {
            recordingFolder = result;
            saveRecordingFolderPreference();
            
            juce::String msg = "Recording folder set to:\n" + recordingFolder.getFullPathName();
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "Recording Folder", msg);
        }
    });
}

// FIX 2: Save recording file with custom name and location
void StudioTab::saveRecordingFile() {
    juce::File tempRecordingFile = processor.getLastRecordingFile();
    
    if (!tempRecordingFile.existsAsFile()) {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "No Recording", "No recording file found. Please record something first.");
        return;
    }
    
    // Default to recording folder, or music directory if not set
    juce::File defaultFolder = recordingFolder.exists() ? recordingFolder 
                                                        : juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    
    // Generate default filename with timestamp
    juce::Time now = juce::Time::getCurrentTime();
    juce::String defaultName = "Colosseum_" + now.formatted("%Y%m%d_%H%M%S") + ".wav";
    juce::File defaultFile = defaultFolder.getChildFile(defaultName);
    
    auto chooser = std::make_shared<juce::FileChooser>("Save Recording As", defaultFile, "*.wav");
    
    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
    
    chooser->launchAsync(flags, [this, chooser, tempRecordingFile](const juce::FileChooser& fc) {
        auto destination = fc.getResult();
        if (destination != juce::File{}) {
            // Copy temporary recording to chosen location
            if (tempRecordingFile.copyFileTo(destination)) {
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Recording Saved", 
                    "Recording saved to:\n" + destination.getFullPathName());
            } else {
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Save Error", "Failed to save recording file. Check permissions and disk space.");
            }
        }
    });
}

void StudioTab::updateRecordingTime() {
    // Time is now updated inline in timerCallback using recordingStartTime
}

void StudioTab::updateTempoDisplay() {
    tempoValueLabel.setText(juce::String(currentTempo, 2), juce::dontSendNotification);
}

void StudioTab::handleTapTempo() {
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
            if (interval > 0.1 && interval < 2.0) {  // Valid range
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

juce::String StudioTab::formatTime(double seconds) {
    int hours = (int)(seconds / 3600);
    int mins = (int)(std::fmod(seconds, 3600) / 60);
    int secs = (int)std::fmod(seconds, 60);
    int ms = (int)(std::fmod(seconds, 1.0) * 1000);
    
    return juce::String::formatted("%02d:%02d:%02d.%03d", hours, mins, secs, ms);
}

// FIX 2: Load recording folder preference from settings
void StudioTab::loadRecordingFolder() {
    if (auto* settings = processor.appProperties.getUserSettings()) {
        juce::String folderPath = settings->getValue("recordingFolder", "");
        if (folderPath.isNotEmpty()) {
            recordingFolder = juce::File(folderPath);
            if (!recordingFolder.exists()) {
                // Folder was deleted, reset to music directory
                recordingFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
            }
        } else {
            // Default to music directory
            recordingFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        }
    } else {
        recordingFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    }
}

// FIX 2: Save recording folder preference to settings
void StudioTab::saveRecordingFolderPreference() {
    if (auto* settings = processor.appProperties.getUserSettings()) {
        settings->setValue("recordingFolder", recordingFolder.getFullPathName());
        settings->saveIfNeeded();
    }
}

