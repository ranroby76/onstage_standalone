// #D:\Workspace\Subterraneum_plugins_daw\src\AudioSettingsTab.cpp
// MIDI CHANNEL DUPLICATION: Select target channels to duplicate hardware MIDI to
// Default: ALL (0xFFFF) = pass-through unchanged
// Select specific channels (e.g., 2,3,4) = incoming MIDI duplicated to all selected channels

#include "AudioSettingsTab.h"

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
    
    #if !JUCE_WINDOWS
    controlPanelBtn.setVisible(false);
    #endif
    
    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    
    // MIDI Inputs Group (Red Frame)
    addAndMakeVisible(midiInputsGroup); 
    midiInputsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::red); 
    midiInputsGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::red); 
    
    addAndMakeVisible(midiInputsViewport); 
    midiInputsViewport.setViewedComponent(&midiInputsContent, false);
    
    // MIDI Outputs Group (Yellow/Gold Frame)
    addAndMakeVisible(midiOutputsGroup);
    midiOutputsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xffFFD700)); // Gold
    midiOutputsGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xffFFD700)); // Gold
    
    addAndMakeVisible(midiOutputsViewport);
    midiOutputsViewport.setViewedComponent(&midiOutputsContent, false);
    
    if (deviceManager) { 
        // FIX: DON'T register listener yet - prevents feedback loop
        // deviceManager->addChangeListener(this);  // ← MOVED TO END
        
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
            deviceManager->setAudioDeviceSetup(setup, false);  // FIX: false = no forced scan
        }
        
        updateMidiInputsList();
        updateMidiOutputsList();
        updateDeviceList();
        updateStatusLabel();
        
        // FIX: Register changeListener AFTER all initialization complete
        deviceManager->addChangeListener(this);
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
    
    // Split MIDI area into two sections (left: inputs, right: outputs)
    auto midiArea = area;
    int midiSectionWidth = midiArea.getWidth() / 2 - 10;
    
    // MIDI Inputs (Left - Red Frame)
    auto inputsArea = midiArea.removeFromLeft(midiSectionWidth);
    midiInputsGroup.setBounds(inputsArea);
    midiInputsViewport.setBounds(inputsArea.reduced(10, 25));
    if (midiInputsContent.getHeight() > 0)
        midiInputsContent.setSize(midiInputsViewport.getWidth() - 15, midiInputsContent.getHeight());
    
    midiArea.removeFromLeft(20); // Spacing between sections
    
    // MIDI Outputs (Right - Yellow/Gold Frame)
    auto outputsArea = midiArea;
    midiOutputsGroup.setBounds(outputsArea);
    midiOutputsViewport.setBounds(outputsArea.reduced(10, 25));
    if (midiOutputsContent.getHeight() > 0)
        midiOutputsContent.setSize(midiOutputsViewport.getWidth() - 15, midiOutputsContent.getHeight()); 
}

void AudioSettingsTab::changeListenerCallback(juce::ChangeBroadcaster* source) { 
    if (source == deviceManager) { 
        updateDeviceList();
        updateMidiInputsList();
        updateMidiOutputsList();
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
            // FIX: false = no forced scan, prevents ASIO panel popup
            deviceManager->setCurrentAudioDeviceType(targetType, false);
        } 
    } 
}

void AudioSettingsTab::updateDeviceList() { 
    deviceCombo.clear(juce::dontSendNotification); 
    if (!deviceManager) return; 
    
    deviceCombo.addItem("OFF (No Audio Device)", 1);
    
    // FIX: REMOVED redundant type enforcement - enforceDriverType() already handles this
    // This was causing extra scans and ASIO panel popups in a feedback loop
    
    if (auto* type = deviceManager->getCurrentDeviceTypeObject()) { 
        // FIX: REMOVED explicit scanForDevices() - getDeviceNames() is sufficient
        // type->scanForDevices(); 
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

void AudioSettingsTab::updateMidiInputsList() { 
    midiInputRows.clear(); 
    midiInputsContent.deleteAllChildren();
    
    auto midiInputs = juce::MidiInput::getAvailableDevices(); 
    int y = 0;
    int rowH = 30;
    
    if (midiInputs.size() == 0) { 
        auto* noDevLabel = new juce::Label("nodev", "No MIDI Inputs Found");
        noDevLabel->setJustificationType(juce::Justification::centred); 
        noDevLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
        noDevLabel->setBounds(0, 0, 300, 30); 
        midiInputsContent.addAndMakeVisible(noDevLabel); 
        y = 30;
    } else {
        auto* userSettings = processor.appProperties.getUserSettings();
        
        for (auto& info : midiInputs) { 
            auto row = std::make_unique<MidiInputRow>();
            row->identifier = info.identifier;
            row->deviceName = info.name;
            
            juce::String maskKey = "MidiMask_" + info.identifier.replaceCharacters(" :/\\", "____");
            
            // FIX: Default to ALL channels (0xFFFF) = pass-through mode
            // Select specific channels = duplication mode (e.g., Ch1 input → Ch2,3,4 output)
            row->channelMask = userSettings ? userSettings->getIntValue(maskKey, 0xFFFF) : 0xFFFF;
            
            bool shouldEnable = (row->channelMask != 0);
            deviceManager->setMidiInputDeviceEnabled(info.identifier, shouldEnable);
            
            row->deviceNameLabel = std::make_unique<juce::Label>("lbl", info.name); 
            row->deviceNameLabel->setJustificationType(juce::Justification::centredLeft); 
            midiInputsContent.addAndMakeVisible(row->deviceNameLabel.get()); 
            
            row->channelButton = std::make_unique<juce::TextButton>();
            row->channelButton->onClick = [this, rowPtr = row.get()]() {
                openMidiInputChannelSelector(rowPtr);
            };
            midiInputsContent.addAndMakeVisible(row->channelButton.get());
            
            updateMidiInputRowButton(row.get());
            
            row->deviceNameLabel->setBounds(5, y, 220, rowH); 
            row->channelButton->setBounds(230, y + 2, 80, rowH - 4);
            
            midiInputRows.push_back(std::move(row)); 
            y += rowH;
        } 
    } 
    midiInputsContent.setSize(320, std::max(y, 30));
    
    if (auto* p = getParentComponent()) p->resized();
    else resized(); 
}

void AudioSettingsTab::updateMidiOutputsList() {
    midiOutputRows.clear();
    midiOutputsContent.deleteAllChildren();
    
    auto midiOutputs = juce::MidiOutput::getAvailableDevices();
    int y = 0;
    int rowH = 30;
    
    if (midiOutputs.size() == 0) {
        auto* noDevLabel = new juce::Label("nodev", "No MIDI Outputs Found");
        noDevLabel->setJustificationType(juce::Justification::centred);
        noDevLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
        noDevLabel->setBounds(0, 0, 300, 30);
        midiOutputsContent.addAndMakeVisible(noDevLabel);
        y = 30;
    } else {
        auto* userSettings = processor.appProperties.getUserSettings();
        
        for (auto& info : midiOutputs) {
            auto row = std::make_unique<MidiOutputRow>();
            row->identifier = info.identifier;
            row->deviceName = info.name;
            
            juce::String maskKey = "MidiOutMask_" + info.identifier.replaceCharacters(" :/\\", "____");
            
            // Default to ALL channels (0xFFFF) = pass-through mode
            row->channelMask = userSettings ? userSettings->getIntValue(maskKey, 0xFFFF) : 0xFFFF;
            
            row->deviceNameLabel = std::make_unique<juce::Label>("lbl", info.name);
            row->deviceNameLabel->setJustificationType(juce::Justification::centredLeft);
            midiOutputsContent.addAndMakeVisible(row->deviceNameLabel.get());
            
            row->channelButton = std::make_unique<juce::TextButton>();
            row->channelButton->onClick = [this, rowPtr = row.get()]() {
                openMidiOutputChannelSelector(rowPtr);
            };
            midiOutputsContent.addAndMakeVisible(row->channelButton.get());
            
            updateMidiOutputRowButton(row.get());
            
            row->deviceNameLabel->setBounds(5, y, 220, rowH);
            row->channelButton->setBounds(230, y + 2, 80, rowH - 4);
            
            midiOutputRows.push_back(std::move(row));
            y += rowH;
        }
    }
    midiOutputsContent.setSize(320, std::max(y, 30));
    
    if (auto* p = getParentComponent()) p->resized();
    else resized();
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
        btnColor = juce::Colour(0xff8B0000); // Dark red
        textColor = juce::Colours::white;
    } else if (enabledCount == 1) {
        for (int i = 0; i < 16; ++i) {
            if ((mask >> i) & 1) {
                text = "CH " + juce::String(i + 1);
                break;
            }
        }
        btnColor = juce::Colour(0xff8B0000); // Dark red
        textColor = juce::Colours::white;
    } else {
        text = juce::String(enabledCount) + " CH";
        btnColor = juce::Colour(0xff8B0000); // Dark red
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
        btnColor = juce::Colour(0xffFFD700); // Gold
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
        btnColor = juce::Colour(0xff9B7A00); // Dark gold
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
            
            // TODO: Update processor to route MIDI output through selected channels
            
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
            int deviceIndex = deviceNames.indexOf(deviceName);
            
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName = deviceName;
            setup.outputDeviceName = deviceName;
            
            auto inputChannels = deviceType->getDeviceNames(true);
            auto outputChannels = deviceType->getDeviceNames(false);
            
            juce::BigInteger allInputs, allOutputs;
            allInputs.setRange(0, 256, true);
            allOutputs.setRange(0, 256, true);
            
            setup.inputChannels = allInputs;
            setup.outputChannels = allOutputs;
            setup.useDefaultInputChannels = false;
            setup.useDefaultOutputChannels = false;
            
            auto result = deviceManager->setAudioDeviceSetup(setup, true);
            
            if (result.isEmpty()) {
                if (auto* device = deviceManager->getCurrentAudioDevice()) {
                    auto inNames = device->getInputChannelNames();
                    auto outNames = device->getOutputChannelNames();
                }
                processor.saveAudioSettings();
                processor.updateIOChannelCount();
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