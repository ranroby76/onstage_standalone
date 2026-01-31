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
    
    addAndMakeVisible(exportBtn);
    exportBtn.addListener(this);
    exportBtn.setEnabled(false);
    exportBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    
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
    
    // ==========================================================================
    // Info Label
    // ==========================================================================
    addAndMakeVisible(infoLabel);
    infoLabel.setFont(juce::Font(11.0f));
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    infoLabel.setText("Recording captures master output at 24-bit/44.1kHz", juce::dontSendNotification);
    
    // Start timer for UI updates
    startTimer(50);
}

StudioTab::~StudioTab() {
    stopTimer();
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
    recordingTop.removeFromLeft(20);
    exportBtn.setBounds(recordingTop.removeFromLeft(100));
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
    
    auto tempoTopRow = tempoInner.removeFromTop(35);
    tempoLabel.setBounds(tempoTopRow.removeFromLeft(40));
    tempoTopRow.removeFromLeft(10);
    tempoValueLabel.setBounds(tempoTopRow.removeFromLeft(80));
    tempoTopRow.removeFromLeft(20);
    tapTempoBtn.setBounds(tempoTopRow.removeFromLeft(60));
    
    tempoInner.removeFromTop(10);
    tempoSlider.setBounds(tempoInner.removeFromTop(30));
    
    // Time Signature section
    timeSignatureGroup.setBounds(timeSigArea);
    auto timeSigInner = timeSigArea.reduced(15, 25);
    
    auto timeSigRow = timeSigInner.removeFromTop(40);
    int comboWidth = 60;
    int totalWidth = comboWidth * 2 + 30;
    int startX = (timeSigRow.getWidth() - totalWidth) / 2;
    
    numeratorCombo.setBounds(timeSigRow.getX() + startX, timeSigRow.getY(), comboWidth, 35);
    slashLabel.setBounds(timeSigRow.getX() + startX + comboWidth, timeSigRow.getY(), 30, 35);
    denominatorCombo.setBounds(timeSigRow.getX() + startX + comboWidth + 30, timeSigRow.getY(), comboWidth, 35);
    
    area.removeFromTop(15);
    
    // Metronome section
    auto metronomeArea = area.removeFromTop(80);
    metronomeGroup.setBounds(metronomeArea);
    auto metronomeInner = metronomeArea.reduced(15, 25);
    
    metronomeBtn.setBounds(metronomeInner.removeFromLeft(100));
    metronomeInner.removeFromLeft(20);
    metronomeVolumeSlider.setBounds(metronomeInner);
    
    area.removeFromTop(10);
    
    // Info label at bottom
    infoLabel.setBounds(area.removeFromTop(20));
}

void StudioTab::buttonClicked(juce::Button* b) {
    if (b == &recordBtn) {
        startRecording();
    }
    else if (b == &stopBtn) {
        stopRecording();
    }
    else if (b == &exportBtn) {
        exportRecording();
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
        exportBtn.setEnabled(false);
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
    exportBtn.setEnabled(true);  // Enable to show file location
    recordingLED.setActive(false);
    
    recordingStatusLabel.setText("Recorded " + formatTime(elapsed), juce::dontSendNotification);
    recordingStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
}

void StudioTab::exportRecording() {
    // With ThreadedWriter, file is already saved - just show user where it is
    juce::File recordingFile = processor.getLastRecordingFile();
    
    if (recordingFile.existsAsFile()) {
        // Ask user if they want to reveal in file browser or copy elsewhere
        int result = juce::NativeMessageBox::showYesNoCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "Recording Saved",
            "Recording saved to:\n" + recordingFile.getFullPathName() + 
            "\n\nWould you like to reveal the file in Explorer/Finder?",
            nullptr, nullptr);
        
        if (result == 1) {  // Yes
            recordingFile.revealToUser();
        }
    } else {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "No Recording", "No recording file found. Please record something first.");
    }
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
