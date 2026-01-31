#include "AudioSettingsTab.h"

// Forward declaration for editor
class SubterraneumAudioProcessorEditor;

AudioSettingsTab::AudioSettingsTab(SubterraneumAudioProcessor& p) : processor(p) { 
    deviceManager = SubterraneumAudioProcessor::standaloneDeviceManager; 
    
    addAndMakeVisible(driverGroup); 
    driverGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::grey);
    driverGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white); 
    
    addAndMakeVisible(deviceLabel); 
    addAndMakeVisible(deviceCombo); 
    deviceCombo.addListener(this); 
    
    addAndMakeVisible(controlPanelBtn); 
    controlPanelBtn.addListener(this);
    
    // Hide control panel button on non-Windows platforms (ASIO-specific feature)
    #if !JUCE_WINDOWS
    controlPanelBtn.setVisible(false);
    #endif
    
    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    
    addAndMakeVisible(midiGroup); 
    midiGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::magenta); 
    midiGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::magenta); 
    
    addAndMakeVisible(midiViewport); 
    midiViewport.setViewedComponent(&midiContent, false);
    
    if (deviceManager) { 
        deviceManager->addChangeListener(this); 
        enforceDriverType(); 
        
        juce::String savedDevice = processor.getSavedAudioDeviceName();
        if (savedDevice.isEmpty()) {
            deviceManager->closeAudioDevice();
            DBG("Starting with audio device disabled (no saved device or OFF)");
        } else {
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName = savedDevice;
            setup.outputDeviceName = savedDevice;
            setup.useDefaultInputChannels = true;
            setup.useDefaultOutputChannels = true;
            deviceManager->setAudioDeviceSetup(setup, true);
            DBG("Restored saved audio device: " << savedDevice);
        }
        
        updateMidiList();
        updateDeviceList();
        updateStatusLabel();
    } 
}

AudioSettingsTab::~AudioSettingsTab() { 
    if (deviceManager) deviceManager->removeChangeListener(this); 
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
    
    auto driverArea = area.removeFromTop(100); 
    driverGroup.setBounds(driverArea);
    driverArea.reduce(10, 25); 
    
    auto row1 = driverArea.removeFromTop(30); 
    deviceLabel.setBounds(row1.removeFromLeft(100)); 
    deviceCombo.setBounds(row1.removeFromLeft(250)); 
    
    #if JUCE_WINDOWS
    row1.removeFromLeft(10); 
    controlPanelBtn.setBounds(row1.removeFromLeft(100));
    #endif
    
    driverArea.removeFromTop(10);
    statusLabel.setBounds(driverArea.removeFromTop(25));
    
    area.removeFromTop(20);
    
    midiGroup.setBounds(area); 
    midiViewport.setBounds(area.reduced(10, 25));
    if (midiContent.getHeight() > 0) 
        midiContent.setSize(midiViewport.getWidth() - 15, midiContent.getHeight()); 
}

void AudioSettingsTab::changeListenerCallback(juce::ChangeBroadcaster* source) { 
    if (source == deviceManager) { 
        updateDeviceList();
        updateMidiList();
        updateStatusLabel();
    } 
}

void AudioSettingsTab::enforceDriverType() { 
    if (!deviceManager) return;
    
    #if JUCE_WINDOWS 
    const juce::String targetType = "ASIO";
    #elif JUCE_MAC 
    const juce::String targetType = "CoreAudio";
    #elif JUCE_LINUX
    // On Linux, prefer JACK if available, otherwise ALSA
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
            deviceManager->setCurrentAudioDeviceType(targetType, true);
        } 
    } 
}

void AudioSettingsTab::updateDeviceList() { 
    deviceCombo.clear(juce::dontSendNotification); 
    if (!deviceManager) return; 
    
    deviceCombo.addItem("OFF (No Audio Device)", 1);
    
    // Ensure we're using the correct driver type for this platform
    #if JUCE_WINDOWS
    if (deviceManager->getCurrentAudioDeviceType() != "ASIO") {
        deviceManager->setCurrentAudioDeviceType("ASIO", true);
    }
    #elif JUCE_MAC
    if (deviceManager->getCurrentAudioDeviceType() != "CoreAudio") {
        deviceManager->setCurrentAudioDeviceType("CoreAudio", true);
    }
    #endif
    
    if (auto* type = deviceManager->getCurrentDeviceTypeObject()) { 
        type->scanForDevices(); 
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

void AudioSettingsTab::updateStatusLabel() {
    if (!deviceManager) return;
    
    if (auto* device = deviceManager->getCurrentAudioDevice()) {
        auto inputNames = device->getInputChannelNames();
        auto outputNames = device->getOutputChannelNames();
        
        juce::String status = "Active: " + device->getName();
        status += " | SR: " + juce::String((int)device->getCurrentSampleRate()) + " Hz";
        status += " | Buf: " + juce::String(device->getCurrentBufferSizeSamples());
        status += " | I/O: " + juce::String(inputNames.size()) + "/" + juce::String(outputNames.size());
        statusLabel.setText(status, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    } else {
        statusLabel.setText("Audio Device: OFF", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
}

void AudioSettingsTab::updateMidiList() { 
    midiRows.clear(); 
    midiContent.deleteAllChildren();
    
    auto midiInputs = juce::MidiInput::getAvailableDevices(); 
    int y = 0;
    int rowH = 30;
    
    if (midiInputs.size() == 0) { 
        auto* noDevLabel = new juce::Label("nodev", "No MIDI Inputs Found");
        noDevLabel->setJustificationType(juce::Justification::centred); 
        noDevLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
        noDevLabel->setBounds(0, 0, 300, 30); 
        midiContent.addAndMakeVisible(noDevLabel); 
        y = 30;
    } else {
        auto* userSettings = processor.appProperties.getUserSettings();
        
        for (auto& info : midiInputs) { 
            auto row = std::make_unique<MidiRow>();
            row->identifier = info.identifier;
            row->deviceName = info.name;
            
            juce::String maskKey = "MidiMask_" + info.identifier.replaceCharacters(" :/\\", "____");
            row->channelMask = userSettings ? userSettings->getIntValue(maskKey, 0) : 0;
            
            bool shouldEnable = (row->channelMask != 0);
            deviceManager->setMidiInputDeviceEnabled(info.identifier, shouldEnable);
            
            row->deviceNameLabel = std::make_unique<juce::Label>("lbl", info.name); 
            row->deviceNameLabel->setJustificationType(juce::Justification::centredLeft); 
            midiContent.addAndMakeVisible(row->deviceNameLabel.get()); 
            
            row->channelButton = std::make_unique<juce::TextButton>();
            row->channelButton->onClick = [this, rowPtr = row.get()]() {
                openMidiChannelSelector(rowPtr);
            };
            midiContent.addAndMakeVisible(row->channelButton.get());
            
            updateMidiRowButton(row.get());
            
            row->deviceNameLabel->setBounds(5, y, 220, rowH); 
            row->channelButton->setBounds(230, y + 2, 80, rowH - 4);
            
            midiRows.push_back(std::move(row)); 
            y += rowH;
        } 
    } 
    midiContent.setSize(320, std::max(y, 30));
    
    if (auto* p = getParentComponent()) p->resized();
    else resized(); 
}

void AudioSettingsTab::updateMidiRowButton(MidiRow* row) {
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
        btnColor = juce::Colours::cyan;
        textColor = juce::Colours::black;
    } else {
        text = juce::String(enabledCount) + " CH";
        btnColor = juce::Colours::cyan.darker();
        textColor = juce::Colours::black;
    }
    
    row->channelButton->setButtonText(text);
    row->channelButton->setColour(juce::TextButton::buttonColourId, btnColor);
    row->channelButton->setColour(juce::TextButton::textColourOffId, textColor);
}

void AudioSettingsTab::openMidiChannelSelector(MidiRow* row) {
    if (!row) return;
    
    auto selector = std::make_unique<MidiInputChannelSelector>(
        row->deviceName, 
        row->channelMask, 
        [this, row](int newMask) {
            row->channelMask = newMask;
            
            bool shouldEnable = (newMask != 0);
            deviceManager->setMidiInputDeviceEnabled(row->identifier, shouldEnable);
            
            auto* userSettings = processor.appProperties.getUserSettings();
            if (userSettings) {
                juce::String maskKey = "MidiMask_" + row->identifier.replaceCharacters(" :/\\", "____");
                userSettings->setValue(maskKey, newMask);
                userSettings->saveIfNeeded();
            }
            
            updateMidiRowButton(row);
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
            processor.updateIOChannelCount();  // Update I/O nodes when device closed
            updateStatusLabel();
            DBG("Audio device set to OFF");
        } else { 
            juce::String deviceName = cb->getText();
            
            // First, get the device type to query available channels
            auto* deviceType = deviceManager->getCurrentDeviceTypeObject();
            if (!deviceType) return;
            
            // Scan and find this device
            deviceType->scanForDevices();
            auto deviceNames = deviceType->getDeviceNames();
            int deviceIndex = deviceNames.indexOf(deviceName);
            
            // Create setup with ALL channels enabled
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName = deviceName;
            setup.outputDeviceName = deviceName;
            
            // Get total available channels for this device
            auto inputChannels = deviceType->getDeviceNames(true);  // input devices
            auto outputChannels = deviceType->getDeviceNames(false); // output devices
            
            // Enable ALL channels by setting the bitmask to all 1s
            // Use BigInteger for devices with many channels
            juce::BigInteger allInputs, allOutputs;
            allInputs.setRange(0, 256, true);  // Enable up to 256 input channels
            allOutputs.setRange(0, 256, true); // Enable up to 256 output channels
            
            setup.inputChannels = allInputs;
            setup.outputChannels = allOutputs;
            setup.useDefaultInputChannels = false;  // Don't use defaults, use our selection
            setup.useDefaultOutputChannels = false;
            
            auto result = deviceManager->setAudioDeviceSetup(setup, true);
            
            if (result.isEmpty()) {
                // Log actual channel count
                if (auto* device = deviceManager->getCurrentAudioDevice()) {
                    auto inNames = device->getInputChannelNames();
                    auto outNames = device->getOutputChannelNames();
                    DBG("Device opened: " << deviceName);
                    DBG("  Available Inputs: " << inNames.size());
                    DBG("  Available Outputs: " << outNames.size());
                    DBG("  Active Inputs: " << device->getActiveInputChannels().countNumberOfSetBits());
                    DBG("  Active Outputs: " << device->getActiveOutputChannels().countNumberOfSetBits());
                }
                processor.saveAudioSettings();
                
                // Force update I/O nodes with new channel count
                processor.updateIOChannelCount();
                
                DBG("Audio device changed and saved: " << deviceName);
            } else {
                DBG("Failed to set audio device: " << result);
            }
            
            updateStatusLabel();
        } 
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
}
