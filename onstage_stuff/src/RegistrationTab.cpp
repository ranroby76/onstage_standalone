#include "RegistrationTab.h"
#include "PluginProcessor.h"

RegistrationTab::RegistrationTab(SubterraneumAudioProcessor& p) : processor(p) {
    // Check registration on startup
    RegistrationManager::getInstance().checkRegistration();
    
    // Status label
    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    statusLabel.setJustificationType(juce::Justification::centred);
    
    // Machine ID display
    addAndMakeVisible(machineIdLabel);
    machineIdLabel.setFont(juce::Font(14.0f));
    machineIdLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(machineIdValue);
    machineIdValue.setFont(juce::Font(16.0f, juce::Font::bold));
    machineIdValue.setJustificationType(juce::Justification::centredLeft);
    machineIdValue.setColour(juce::Label::textColourId, juce::Colours::orange);
    machineIdValue.setText(RegistrationManager::getInstance().getMachineIDString(), juce::dontSendNotification);
    
    // Copy ID button
    addAndMakeVisible(copyIdBtn);
    copyIdBtn.addListener(this);
    
    // Serial input
    addAndMakeVisible(serialLabel);
    serialLabel.setFont(juce::Font(14.0f));
    serialLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(serialInput);
    serialInput.setFont(juce::Font(16.0f));
    serialInput.setJustification(juce::Justification::centred);
    serialInput.setInputRestrictions(20, "0123456789-");
    
    addAndMakeVisible(registerBtn);
    registerBtn.addListener(this);
    registerBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    
    // Info labels
    addAndMakeVisible(infoLabel);
    infoLabel.setFont(juce::Font(12.0f));
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    infoLabel.setText("To purchase a license, visit our website with your Machine ID.", juce::dontSendNotification);
    
    addAndMakeVisible(demoInfoLabel);
    demoInfoLabel.setFont(juce::Font(13.0f));
    demoInfoLabel.setJustificationType(juce::Justification::centred);
    demoInfoLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    
    // Update display
    updateStatus();
    
    // Start timer to update status periodically
    startTimer(1000);
}

RegistrationTab::~RegistrationTab() {
    stopTimer();
}

void RegistrationTab::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground);
    
    // Draw header
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(0, 0, getWidth(), 60);
    
    // Title
    g.setColour(juce::Colours::gold);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("PRODUCT REGISTRATION", 0, 15, getWidth(), 30, juce::Justification::centred);
    
    // Draw registration box
    auto boxArea = getLocalBounds().reduced(50).withTrimmedTop(80);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(boxArea.toFloat(), 10.0f);
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(boxArea.toFloat(), 10.0f, 1.0f);
}

void RegistrationTab::resized() {
    auto area = getLocalBounds();
    area.removeFromTop(70);  // Header
    
    auto boxArea = area.reduced(50).withTrimmedTop(20);
    auto contentArea = boxArea.reduced(30);
    
    // Status at top
    statusLabel.setBounds(contentArea.removeFromTop(40));
    contentArea.removeFromTop(20);
    
    // Machine ID row
    auto idRow = contentArea.removeFromTop(30);
    machineIdLabel.setBounds(idRow.removeFromLeft(100));
    idRow.removeFromLeft(10);
    machineIdValue.setBounds(idRow.removeFromLeft(150));
    idRow.removeFromLeft(10);
    copyIdBtn.setBounds(idRow.removeFromLeft(80));
    
    contentArea.removeFromTop(30);
    
    // Demo limitations info (if not registered)
    demoInfoLabel.setBounds(contentArea.removeFromTop(50));
    contentArea.removeFromTop(20);
    
    // Serial input row
    serialLabel.setBounds(contentArea.removeFromTop(25));
    contentArea.removeFromTop(5);
    
    auto inputRow = contentArea.removeFromTop(35);
    int inputWidth = juce::jmin(300, inputRow.getWidth() - 120);
    int startX = (inputRow.getWidth() - inputWidth - 100) / 2;
    serialInput.setBounds(inputRow.getX() + startX, inputRow.getY(), inputWidth, 35);
    registerBtn.setBounds(inputRow.getX() + startX + inputWidth + 10, inputRow.getY(), 90, 35);
    
    contentArea.removeFromTop(30);
    
    // Info label at bottom
    infoLabel.setBounds(contentArea.removeFromTop(30));
}

void RegistrationTab::buttonClicked(juce::Button* b) {
    if (b == &registerBtn) {
        attemptRegistration();
    }
    else if (b == &copyIdBtn) {
        juce::SystemClipboard::copyTextToClipboard(RegistrationManager::getInstance().getMachineIDString());
        copyIdBtn.setButtonText("Copied!");
        juce::Timer::callAfterDelay(1500, [this]() {
            if (copyIdBtn.isShowing())
                copyIdBtn.setButtonText("Copy ID");
        });
    }
}

void RegistrationTab::timerCallback() {
    // Update demo mode timing
    RegistrationManager::getInstance().updateDemoMode();
    
    // Update status display
    updateStatus();
}

void RegistrationTab::updateStatus() {
    bool registered = RegistrationManager::getInstance().isRegistered();
    
    if (registered) {
        statusLabel.setText("REGISTERED - Thank you!", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        serialInput.setEnabled(false);
        registerBtn.setEnabled(false);
        demoInfoLabel.setText("", juce::dontSendNotification);
    }
    else {
        statusLabel.setText("DEMO MODE - Unregistered", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        serialInput.setEnabled(true);
        registerBtn.setEnabled(true);
        
        // Show demo limitations
        demoInfoLabel.setText(
            "DEMO LIMITATIONS:\n"
            "Audio inputs and outputs will be silenced for 3 seconds every 15 seconds.\n"
            "Register to remove this limitation.",
            juce::dontSendNotification
        );
    }
}

void RegistrationTab::attemptRegistration() {
    juce::String serial = serialInput.getText().trim();
    
    if (serial.isEmpty()) {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Registration", "Please enter a serial number.");
        return;
    }
    
    bool success = RegistrationManager::getInstance().tryRegister(serial);
    
    if (success) {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Registration Successful", 
            "Thank you for registering Colosseum!\n\nAll demo limitations have been removed.");
        updateStatus();
    }
    else {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Registration Failed", 
            "The serial number is invalid.\n\n"
            "Please check your serial number and try again.\n"
            "Make sure you're using the serial generated for this Machine ID.");
        serialInput.clear();
    }
}
