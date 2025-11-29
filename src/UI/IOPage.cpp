// **Changes:** 1.  **Constructor:** Initialized `midiRefreshButton` with "Refresh" text and callback. 2.  **`resized()`:** Updated MIDI row to accommodate the new button. 3.  **`restoreSavedSettings()`:** Added logic to check if saved MIDI device exists. If not, it forces "OFF". 4.  **`updateMidiDevices()`:** Refined logic to preserve current selection if still available during refresh. <!-- end list -->

#include "IOPage.h"
#include "../RegistrationManager.h" 
#include "../AppLogger.h"

IOPage::IOPage(AudioEngine& engine, IOSettingsManager& settings)
    : audioEngine(engine), ioSettingsManager(settings)
{
    LOG_INFO("IOPage: Constructor START");
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    setLookAndFeel(goldenLookAndFeel.get());

    addAndMakeVisible(leftViewport);
    leftViewport.setViewedComponent(&leftContainer, false);
    leftViewport.setScrollBarsShown(true, false);
    
    addAndMakeVisible(outputViewport);
    outputViewport.setViewedComponent(&outputCheckboxContainer, false);
    outputViewport.setScrollBarsShown(true, false);

    // --- 1. ASIO ---
    leftContainer.addAndMakeVisible(asioDriverSectionLabel);
    asioDriverSectionLabel.setText("ASIO Audio Device", juce::dontSendNotification);
    asioDriverSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    asioDriverSectionLabel.setFont(juce::Font(18.0f, juce::Font::bold));

    leftContainer.addAndMakeVisible(specificDriverLabel);
    specificDriverLabel.setText("Driver:", juce::dontSendNotification);
    leftContainer.addAndMakeVisible(specificDriverSelector);
    
    // Populate Drivers List (Default to OFF initially)
    auto drivers = audioEngine.getSpecificDrivers("ASIO");
    drivers.insert(0, "OFF"); 
    specificDriverSelector.addItemList(drivers, 1);
    specificDriverSelector.setText("OFF", juce::dontSendNotification);
    
    specificDriverSelector.onChange = [this] { onSpecificDriverChanged(); };

    leftContainer.addAndMakeVisible(controlPanelButton);
    controlPanelButton.setButtonText("Control Panel");
    controlPanelButton.onClick = [this] { audioEngine.openDriverControlPanel(); };

    leftContainer.addAndMakeVisible(deviceInfoLabel);
    deviceInfoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    deviceInfoLabel.setFont(juce::Font(14.0f));
    
    // --- 2. Mics ---
    leftContainer.addAndMakeVisible(performersInputsSectionLabel);
    performersInputsSectionLabel.setText("Microphone Inputs", juce::dontSendNotification);
    performersInputsSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    performersInputsSectionLabel.setFont(juce::Font(18.0f, juce::Font::bold));

    leftContainer.addAndMakeVisible(mic1Label); mic1Label.setText("Mic 1:", juce::dontSendNotification);
    leftContainer.addAndMakeVisible(mic1InputSelector);
    mic1InputSelector.onChange = [this] { 
        audioEngine.selectInputDevice(0, mic1InputSelector.getText());
        ioSettingsManager.saveMicInput(0, mic1InputSelector.getText());
    };

    leftContainer.addAndMakeVisible(mic1MuteToggle); mic1MuteToggle.setButtonText("Mute");
    mic1MuteToggle.setMidiInfo("MIDI: CC 10");
    mic1MuteToggle.onClick = [this] { audioEngine.setMicMute(0, mic1MuteToggle.getToggleState()); };

    leftContainer.addAndMakeVisible(mic1BypassToggle); mic1BypassToggle.setButtonText("FX Bypass");
    mic1BypassToggle.setMidiInfo("MIDI: CC 11");
    mic1BypassToggle.onClick = [this] { audioEngine.setFxBypass(0, mic1BypassToggle.getToggleState()); };

    leftContainer.addAndMakeVisible(mic1Led);

    leftContainer.addAndMakeVisible(mic2Label); mic2Label.setText("Mic 2:", juce::dontSendNotification);
    leftContainer.addAndMakeVisible(mic2InputSelector);
    mic2InputSelector.onChange = [this] { 
        audioEngine.selectInputDevice(1, mic2InputSelector.getText());
        ioSettingsManager.saveMicInput(1, mic2InputSelector.getText());
    };

    leftContainer.addAndMakeVisible(mic2MuteToggle); mic2MuteToggle.setButtonText("Mute");
    mic2MuteToggle.setMidiInfo("MIDI: CC 12");
    mic2MuteToggle.onClick = [this] { audioEngine.setMicMute(1, mic2MuteToggle.getToggleState()); };

    leftContainer.addAndMakeVisible(mic2BypassToggle); mic2BypassToggle.setButtonText("FX Bypass");
    mic2BypassToggle.setMidiInfo("MIDI: CC 13");
    mic2BypassToggle.onClick = [this] { audioEngine.setFxBypass(1, mic2BypassToggle.getToggleState()); };

    leftContainer.addAndMakeVisible(mic2Led);

    // --- 3. Backing ---
    leftContainer.addAndMakeVisible(backingTrackSectionLabel);
    backingTrackSectionLabel.setText("Backing Tracks Routing", juce::dontSendNotification);
    backingTrackSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    backingTrackSectionLabel.setFont(juce::Font(18.0f, juce::Font::bold));

    leftContainer.addAndMakeVisible(mediaPlayerLabel); mediaPlayerLabel.setText("Internal Media Player:", juce::dontSendNotification);
    leftContainer.addAndMakeVisible(mediaPlayerToggle); mediaPlayerToggle.setButtonText("Active");
    mediaPlayerToggle.setToggleState(true, juce::dontSendNotification);
    mediaPlayerToggle.setEnabled(false);
    leftContainer.addAndMakeVisible(mediaPlayerLed);

    leftContainer.addAndMakeVisible(backingGainHeaderLabel);
    backingGainHeaderLabel.setText("Gain", juce::dontSendNotification);
    backingGainHeaderLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    backingGainHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    backingGainHeaderLabel.setJustificationType(juce::Justification::centred);

    for (int i = 0; i < 4; ++i) {
        auto* pair = new PlaybackInputPair();
        pair->label.setText("Input Pair " + juce::String(i + 1), juce::dontSendNotification);
        pair->leftLabel.setText("L:", juce::dontSendNotification);
        pair->rightLabel.setText("R:", juce::dontSendNotification);
        
        pair->leftSelector.onChange = [this, i] { onPlaybackInputChanged(i, true); };
        pair->rightSelector.onChange = [this, i] { onPlaybackInputChanged(i, false); };

        pair->gainSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        pair->gainSlider->setRange(0.0, 2.0, 0.01);
        pair->gainSlider->setValue(1.0);
        pair->gainSlider->setMidiInfo("MIDI: CC " + juce::String(20 + i));
        
        pair->gainSlider->onValueChange = [this, i] { 
            float val = (float)playbackInputPairs[i]->gainSlider->getValue();
            audioEngine.setBackingTrackPairGain(i, val);
            
            auto* p = playbackInputPairs[i];
            int inputIdx = 1 + i*2;
            int chIdx = audioEngine.getBackingTrackInputChannel(inputIdx);
            ioSettingsManager.saveBackingTrackInput(inputIdx, chIdx >= 0, chIdx, 
                                                    p->leftSelector.getText(), p->rightSelector.getText(), val);
        };

        inputsContainer.addAndMakeVisible(pair->label);
        inputsContainer.addAndMakeVisible(pair->leftLabel);
        inputsContainer.addAndMakeVisible(pair->leftSelector);
        inputsContainer.addAndMakeVisible(pair->rightLabel);
        inputsContainer.addAndMakeVisible(pair->rightSelector);
        inputsContainer.addAndMakeVisible(pair->gainSlider.get());
        inputsContainer.addAndMakeVisible(pair->leftLed);
        inputsContainer.addAndMakeVisible(pair->rightLed);

        playbackInputPairs.add(pair);
    }
    leftContainer.addAndMakeVisible(inputsContainer);

    // --- 4. Settings ---
    leftContainer.addAndMakeVisible(vocalSettingsSectionLabel);
    vocalSettingsSectionLabel.setText("Vocal Settings", juce::dontSendNotification);
    vocalSettingsSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    vocalSettingsSectionLabel.setFont(juce::Font(18.0f, juce::Font::bold));

    leftContainer.addAndMakeVisible(latencyLabel);
    latencyLabel.setText("Latency Correction (ms):", juce::dontSendNotification);
    latencySlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    latencySlider->setRange(0.0, 500.0, 1.0);
    latencySlider->setMidiInfo("MIDI: CC 24");
    latencySlider->onValueChange = [this] { 
        audioEngine.setLatencyCorrectionMs((float)latencySlider->getValue());
        ioSettingsManager.saveVocalSettings((float)latencySlider->getValue(), (float)vocalBoostSlider->getValue());
    };
    leftContainer.addAndMakeVisible(latencySlider.get());

    leftContainer.addAndMakeVisible(vocalBoostLabel);
    vocalBoostLabel.setText("Vocal Boost (dB):", juce::dontSendNotification);
    vocalBoostSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    vocalBoostSlider->setRange(0.0, 24.0, 0.1);
    vocalBoostSlider->setMidiInfo("MIDI: CC 25");
    vocalBoostSlider->onValueChange = [this] {
        audioEngine.setVocalBoostDb((float)vocalBoostSlider->getValue());
        ioSettingsManager.saveVocalSettings((float)latencySlider->getValue(), (float)vocalBoostSlider->getValue());
    };
    leftContainer.addAndMakeVisible(vocalBoostSlider.get());

    // --- 5. MIDI ---
    leftContainer.addAndMakeVisible(midiInputLabel); midiInputLabel.setText("MIDI Input:", juce::dontSendNotification);
    leftContainer.addAndMakeVisible(midiInputSelector);
    midiInputSelector.onChange = [this] {
        audioEngine.setMidiInput(midiInputSelector.getText());
        ioSettingsManager.saveMidiDevice(midiInputSelector.getText());
    };
    
    // NEW: Refresh Button
    leftContainer.addAndMakeVisible(midiRefreshButton);
    midiRefreshButton.setButtonText("Refresh");
    midiRefreshButton.onClick = [this] { updateMidiDevices(); };

    // --- 6. Outputs ---
    addAndMakeVisible(outputsSectionLabel);
    outputsSectionLabel.setText("Outputs", juce::dontSendNotification);
    outputsSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    outputsSectionLabel.setFont(juce::Font(18.0f, juce::Font::bold));

    updateInputDevices();
    updateOutputDevices();
    updateMidiDevices();
    
    startTimerHz(20);
    LOG_INFO("IOPage: Constructor END");
}

IOPage::~IOPage()
{
    stopTimer();
    setLookAndFeel(nullptr); 
}

void IOPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF202020));
}

void IOPage::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto leftArea = area.removeFromLeft((int)(area.getWidth() * 0.6f)).reduced(5);
    auto rightArea = area.reduced(5);

    // --- RIGHT ---
    outputsSectionLabel.setBounds(rightArea.removeFromTop(30));
    outputViewport.setBounds(rightArea);
    outputCheckboxContainer.setBounds(0, 0, outputViewport.getWidth(), outputItems.size() * 35);
    int outY = 0;
    for (auto* item : outputItems) {
        item->setBounds(0, outY, outputCheckboxContainer.getWidth(), 35);
        outY += 35;
    }

    // --- LEFT ---
    leftViewport.setBounds(leftArea);
    
    int w = leftViewport.getWidth() - 20;
    int y = 10;
    int gap = 40; 
    
    // Calculate unified geometry
    int comboX = 80;
    int comboRightMargin = 280; 
    int comboWidth = w - comboX - comboRightMargin;
    if (comboWidth < 150) comboWidth = 150; 

    // 1. ASIO
    asioDriverSectionLabel.setBounds(10, y, w, 25);
    y += 30;
    specificDriverLabel.setBounds(10, y, 60, 25);
    
    specificDriverSelector.setBounds(comboX, y, comboWidth, 25);
    controlPanelButton.setBounds(comboX + comboWidth + 10, y, 120, 25);
    y += gap;
    deviceInfoLabel.setBounds(10, y, w, 20); y += gap;

    // 2. Mics
    performersInputsSectionLabel.setBounds(10, y, w, 25);
    y += 30;
    
    mic1Label.setBounds(10, y, 50, 25);
    mic1InputSelector.setBounds(comboX, y, comboWidth, 25);
    int rightControlsX = comboX + comboWidth + 10;
    mic1MuteToggle.setBounds(rightControlsX, y, 60, 25);
    mic1BypassToggle.setBounds(rightControlsX + 70, y, 80, 25);
    mic1Led.setBounds(rightControlsX + 160, y, 20, 20); 
    
    y += 35;

    mic2Label.setBounds(10, y, 50, 25);
    mic2InputSelector.setBounds(comboX, y, comboWidth, 25);
    mic2MuteToggle.setBounds(rightControlsX, y, 60, 25);
    mic2BypassToggle.setBounds(rightControlsX + 70, y, 80, 25);
    mic2Led.setBounds(rightControlsX + 160, y, 20, 20);
    
    y += gap;

    // 3. Backing
    backingTrackSectionLabel.setBounds(10, y, w, 25);
    y += 30;
    mediaPlayerLabel.setBounds(10, y, 140, 25);
    mediaPlayerToggle.setBounds(160, y, 80, 25);
    mediaPlayerLed.setBounds(250, y, 20, 20);
    y += 35;

    int sliderW_est = (int)(w * 0.3f);
    if (sliderW_est < 100) sliderW_est = 100;
    backingGainHeaderLabel.setBounds(w - sliderW_est, y, sliderW_est, 20);
    y += 20;
    int inputsH = playbackInputPairs.size() * 45;
    inputsContainer.setBounds(10, y, w, inputsH);
    int inY = 0;
    for (auto* pair : playbackInputPairs) {
        int rowW = inputsContainer.getWidth();
        pair->label.setBounds(0, inY, 80, 30);
        
        int sliderW = (int)(rowW * 0.30f);
        if (sliderW < 100) sliderW = 100;
        pair->gainSlider->setBounds(rowW - sliderW, inY, sliderW, 30);
        int startX = 90;
        int endX = rowW - sliderW - 10;
        int comboAreaW = endX - startX;

        if (comboAreaW > 50) 
        {
            int singleBlockW = comboAreaW / 2;
            int labelW = 20;
            int ledW = 20;
            int comboW = singleBlockW - labelW - ledW - 5;
            if (comboW < 30) comboW = 30; 

            // Left Pair
            pair->leftLabel.setBounds(startX, inY, labelW, 30);
            pair->leftSelector.setBounds(startX + labelW, inY, comboW, 30);
            pair->leftLed.setBounds(startX + labelW + comboW + 2, inY + 5, ledW, 20);
            // Right Pair
            int rightBlockX = startX + singleBlockW;
            pair->rightLabel.setBounds(rightBlockX, inY, labelW, 30);
            pair->rightSelector.setBounds(rightBlockX + labelW, inY, comboW, 30);
            pair->rightLed.setBounds(rightBlockX + labelW + comboW + 2, inY + 5, ledW, 20);
        }

        inY += 45;
    }
    y += inputsH + 20;

    // 4. Settings
    vocalSettingsSectionLabel.setBounds(10, y, w, 25);
    y += 30;
    latencyLabel.setBounds(10, y, 160, 25);
    latencySlider->setBounds(180, y, w - 190, 25);
    y += 35;
    vocalBoostLabel.setBounds(10, y, 160, 25);
    vocalBoostSlider->setBounds(180, y, w - 190, 25);
    y += gap;

    // 5. MIDI
    // FIX: Layout with Refresh Button
    midiInputLabel.setBounds(10, y, 80, 25);
    int refreshBtnWidth = 60;
    int midiComboWidth = w - 110 - refreshBtnWidth - 10; 
    // 100 start + comboWidth + 10 gap + 60 btn = total w
    midiInputSelector.setBounds(100, y, midiComboWidth, 25);
    midiRefreshButton.setBounds(100 + midiComboWidth + 10, y, refreshBtnWidth, 25);
    
    y += gap;

    leftContainer.setBounds(0, 0, leftViewport.getWidth(), y + 50);
}

void IOPage::timerCallback()
{
    try {
        bool isPro = RegistrationManager::getInstance().isProMode();
        mic2InputSelector.setEnabled(isPro);
        mic2MuteToggle.setEnabled(isPro);
        mic2BypassToggle.setEnabled(isPro);
        
        for (auto* p : playbackInputPairs) {
            p->leftSelector.setEnabled(isPro);
            p->rightSelector.setEnabled(isPro);
            p->gainSlider->setEnabled(isPro);
        }

        const float threshold = 0.001f;
        mic1Led.setOn(audioEngine.getInputLevel(0) > threshold);
        mic2Led.setOn(isPro && (audioEngine.getInputLevel(1) > threshold));
        mediaPlayerLed.setOn(audioEngine.getBackingTrackLevel(0) > threshold);

        for (int i = 0; i < 4; ++i) {
            float engineGain = audioEngine.getBackingTrackPairGain(i);
            if (std::abs(playbackInputPairs[i]->gainSlider->getValue() - engineGain) > 0.01f) {
                if (!playbackInputPairs[i]->gainSlider->isMouseButtonDown()) {
                    playbackInputPairs[i]->gainSlider->setValue(engineGain, juce::dontSendNotification);
                }
            }

            if (isPro) {
                playbackInputPairs[i]->leftLed.setOn(audioEngine.getBackingTrackLevel(1 + i*2) > threshold);
                playbackInputPairs[i]->rightLed.setOn(audioEngine.getBackingTrackLevel(2 + i*2) > threshold);
            }
        }
        
        if (auto* d = audioEngine.getDeviceManager().getCurrentAudioDevice()) {
            deviceInfoLabel.setText(juce::String(d->getCurrentSampleRate()) + " Hz / " + 
                                    juce::String(d->getCurrentBufferSizeSamples()) + " samples", juce::dontSendNotification);
        }
        else {
            deviceInfoLabel.setText("No Audio Device", juce::dontSendNotification);
        }

        float masterL = audioEngine.getOutputLevel(0);
        float masterR = audioEngine.getOutputLevel(1);
        for (auto* item : outputItems) {
            if (item->toggleButton.getToggleState()) {
                item->ledL.setOn(masterL > threshold);
                item->ledR.setOn(masterR > threshold);
            } else {
                item->ledL.setOn(false);
                item->ledR.setOn(false);
            }
        }
    }
    catch (...) { stopTimer();
    }
}

void IOPage::onSpecificDriverChanged() { 
    // This method is called by User Action via ComboBox
    ioSettingsManager.saveDriverType("ASIO"); // Explicitly ASIO
    ioSettingsManager.saveSpecificDriver(specificDriverSelector.getText());
    
    // This will trigger AudioEngine to verify/load the driver
    audioEngine.setSpecificDriver("ASIO", specificDriverSelector.getText());
    
    updateInputDevices(); 
    updateOutputDevices();
    
    // After driver change, try to restore last used mic routing if valid
    validateAndSelectMic(0, mic1InputSelector, ioSettingsManager.getLastMicInput(0));
    validateAndSelectMic(1, mic2InputSelector, ioSettingsManager.getLastMicInput(1));
    for (int i = 0; i < 4; ++i) {
        auto state = ioSettingsManager.getBackingTrackInput(1 + i*2);
        validateAndSelectBacking(i, true, playbackInputPairs[i]->leftSelector, state.leftSelection);
        validateAndSelectBacking(i, false, playbackInputPairs[i]->rightSelector, state.rightSelection);
    }
}

void IOPage::updateInputDevices() {
    auto inputs = audioEngine.getAvailableInputDevices();
    inputs.insert(0, "OFF");
    auto updateCombo = [&](juce::ComboBox& box) {
        auto current = box.getText();
        box.clear();
        box.addItemList(inputs, 1);
        if (inputs.contains(current)) 
        {
            box.setSelectedId(inputs.indexOf(current) + 1, juce::dontSendNotification);
        }
        else 
        {
            if (inputs.size() > 1)
                box.setSelectedId(2, juce::dontSendNotification);
            else
                box.setSelectedId(1, juce::dontSendNotification);
        }
    };
    
    updateCombo(mic1InputSelector);
    updateCombo(mic2InputSelector);
    for (auto* p : playbackInputPairs) {
        updateCombo(p->leftSelector);
        updateCombo(p->rightSelector);
    }
}

void IOPage::updateOutputDevices() {
    outputItems.clear();
    outputCheckboxContainer.removeAllChildren();
    
    auto outputs = audioEngine.getAvailableOutputDevices();
    for (int i=0; i < outputs.size(); i += 2) {
        juce::String name = outputs[i];
        if (i+1 < outputs.size()) name += " / " + outputs[i+1];
        
        auto* item = new OutputItemComponent(name, i);
        bool enabled = audioEngine.isOutputChannelEnabled(i);
        item->toggleButton.setToggleState(enabled, juce::dontSendNotification);
        
        item->toggleButton.onClick = [this, i, item] {
            bool isActive = item->toggleButton.getToggleState();
            audioEngine.setOutputChannelEnabled(i, isActive);
            if (i+1 < audioEngine.getAvailableOutputDevices().size())
                audioEngine.setOutputChannelEnabled(i+1, isActive);
            juce::StringArray activeNames;
            for (auto* comp : outputCheckboxContainer.getChildren()) {
                if (auto* outItem = dynamic_cast<OutputItemComponent*>(comp)) {
                    if (outItem->toggleButton.getToggleState())
                        activeNames.add(outItem->toggleButton.getButtonText());
                }
            }
            ioSettingsManager.saveOutputs(activeNames);
        };
        
        outputItems.add(item);
        outputCheckboxContainer.addAndMakeVisible(item);
    }
    resized();
}

void IOPage::updateMidiDevices() {
    auto currentSelection = midiInputSelector.getText();
    auto list = audioEngine.getAvailableMidiInputs();
    list.insert(0, "OFF");
    
    midiInputSelector.clear();
    midiInputSelector.addItemList(list, 1);
    
    // If current selection is still available, keep it. Otherwise default to OFF.
    if (currentSelection.isNotEmpty() && list.contains(currentSelection))
    {
        midiInputSelector.setText(currentSelection, juce::dontSendNotification);
    }
    else
    {
        midiInputSelector.setText("OFF", juce::sendNotification);
    }
}

void IOPage::validateAndSelectMic(int micIndex, juce::ComboBox& selector, const juce::String& savedName)
{
    auto available = audioEngine.getAvailableInputDevices();
    juce::String nameToSelect = savedName;

    if (savedName != "OFF" && !available.contains(savedName))
    {
        if (available.size() > 0)
            nameToSelect = available[0];
        else
            nameToSelect = "OFF";
    }

    selector.setText(nameToSelect, juce::dontSendNotification);
    audioEngine.selectInputDevice(micIndex, nameToSelect);
    
    if (nameToSelect != savedName)
        ioSettingsManager.saveMicInput(micIndex, nameToSelect);
}

void IOPage::validateAndSelectBacking(int pairIndex, bool isLeft, juce::ComboBox& selector, const juce::String& savedName)
{
    auto available = audioEngine.getAvailableInputDevices();
    juce::String nameToSelect = savedName;

    if (savedName != "OFF" && !available.contains(savedName))
    {
        nameToSelect = "OFF";
    }

    selector.setText(nameToSelect, juce::dontSendNotification);
    
    int inputIdx = isLeft ? (1 + pairIndex*2) : (2 + pairIndex*2);
    int channelIndex = -1;
    if (nameToSelect != "OFF") channelIndex = available.indexOf(nameToSelect);
    
    audioEngine.setBackingTrackInputMapping(inputIdx, channelIndex);
    audioEngine.setBackingTrackInputEnabled(inputIdx, channelIndex >= 0);
    ioSettingsManager.saveBackingTrackInput(inputIdx, channelIndex >= 0, channelIndex, 
                                            isLeft ? nameToSelect : playbackInputPairs[pairIndex]->leftSelector.getText(),
                                            isLeft ? playbackInputPairs[pairIndex]->rightSelector.getText() 
                                            : nameToSelect,
                                            (float)playbackInputPairs[pairIndex]->gainSlider->getValue());
}

void IOPage::restoreSavedSettings() {
    LOG_INFO("IOPage: Restoring Saved Settings...");
    
    juce::String savedDriver = ioSettingsManager.getLastSpecificDriver();
    auto availableDrivers = audioEngine.getSpecificDrivers("ASIO");
    
    // STRICT VALIDATION LOGIC:
    bool shouldLoad = false;
    
    if (savedDriver.isNotEmpty() && savedDriver != "OFF")
    {
        // Must exist in the system's driver list to be loaded
        if (availableDrivers.contains(savedDriver))
        {
            shouldLoad = true;
        }
        else
        {
            LOG_WARNING("Saved ASIO driver '" + savedDriver + "' not found on system. Defaulting to OFF.");
        }
    }
    
    if (shouldLoad)
    {
        LOG_INFO("Restoring saved ASIO driver: " + savedDriver);
        specificDriverSelector.setText(savedDriver, juce::dontSendNotification);
        onSpecificDriverChanged(); // Trigger full load
    }
    else
    {
        LOG_INFO("No valid saved driver found. Staying OFF.");
        specificDriverSelector.setText("OFF", juce::dontSendNotification);
        // Ensure engine is explicitly OFF
        audioEngine.setSpecificDriver("ASIO", "OFF");
    }

    // Restore routing regardless (UI state only)
    updateInputDevices();
    updateOutputDevices();

    // Re-check outputs only if we are live
    if (shouldLoad) {
        auto savedOutputs = ioSettingsManager.getLastOutputs();
        for (auto* item : outputItems) {
            if (savedOutputs.contains(item->toggleButton.getButtonText())) {
                if (!item->toggleButton.getToggleState())
                {
                    item->toggleButton.setToggleState(true, juce::sendNotification);
                }
            }
        }
    }

    latencySlider->setValue(ioSettingsManager.getLastLatencyMs(), juce::sendNotification);
    vocalBoostSlider->setValue(ioSettingsManager.getLastVocalBoostDb(), juce::sendNotification);
    
    // FIX: RESTORE MIDI WITH VALIDATION
    juce::String savedMidi = ioSettingsManager.getLastMidiDevice();
    auto midiList = audioEngine.getAvailableMidiInputs();
    
    // Update the list first to ensure it's current
    midiInputSelector.clear();
    midiInputSelector.addItemList(midiList, 1);
    midiInputSelector.addItem("OFF", 1); // Ensure OFF is top if list empty, or bottom? Actually updateMidiDevices puts OFF at top.
    
    // Call standard update which puts OFF at top
    updateMidiDevices();
    
    if (savedMidi.isNotEmpty() && savedMidi != "OFF" && midiList.contains(savedMidi))
    {
        LOG_INFO("Restoring MIDI Device: " + savedMidi);
        midiInputSelector.setText(savedMidi, juce::sendNotification);
    }
    else
    {
        LOG_INFO("Saved MIDI device not available or OFF. Defaulting to OFF.");
        midiInputSelector.setText("OFF", juce::sendNotification);
        audioEngine.setMidiInput("OFF"); // Ensure explicitly OFF
    }
}

void IOPage::onPlaybackInputChanged(int pairIndex, bool isLeft) {
    auto* pair = playbackInputPairs[pairIndex];
    juce::String selection = isLeft ?
                             pair->leftSelector.getText() : pair->rightSelector.getText();
    int inputIdx = isLeft ? (1 + pairIndex*2) : (2 + pairIndex*2);
    int channelIndex = -1;
    if (selection != "OFF") {
        auto inputs = audioEngine.getAvailableInputDevices();
        channelIndex = inputs.indexOf(selection);
    }
    
    audioEngine.setBackingTrackInputMapping(inputIdx, channelIndex);
    audioEngine.setBackingTrackInputEnabled(inputIdx, channelIndex >= 0);
    ioSettingsManager.saveBackingTrackInput(inputIdx, channelIndex >= 0, channelIndex, 
                                            pair->leftSelector.getText(), pair->rightSelector.getText(),
                                            (float)pair->gainSlider->getValue());
}