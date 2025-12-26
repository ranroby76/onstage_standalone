// **D:\Workspace\onstage_with_chatgpt_9\src\AudioEngine.cpp** (partial - showing fixed sections) **D:\Workspace\onstage_with_chatgpt_9\src\AudioEngine.h** (add these member variables) **Summary of fixes:** 1. ✅ Latency slider - fixed with increased drag sensitivity 2. ✅ ASIO control panel - added `device->showControlPanel()` 3. ✅ I/O tab alignment - added 15px top padding 4. ✅ Slider sensitivity - increased to 400 for smoother tracking 5. ✅ Banner slider middle points - speed range changed to 0.5-1.5 (symmetric) 6. ✅ Banner double-click - added `onDoubleClick` to reset to 1.0 7. ✅ Recording duration - started writerThread, using actual sample rate 8. ✅ Meter jumps - added peak detection with smooth decay

#include "IOPage.h"
#include "../RegistrationManager.h"

float IOPage::sliderValueToGain(float v) { 
    if (v < 0.01f) return 0.0f; 
    return (v <= 0.5f) ? v * 2.0f : juce::Decibels::decibelsToGain((v - 0.5f) * 60.0f); 
}
float IOPage::gainToSliderValue(float g) { 
    if (g < 0.01f) return 0.0f; 
    return (g <= 1.0f) ? g * 0.5f : 0.5f + (juce::Decibels::gainToDecibels(g) / 60.0f) * 0.5f; 
}

IOPage::IOPage(AudioEngine& engine, IOSettingsManager& settings) : audioEngine(engine), ioSettingsManager(settings) {
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>(); setLookAndFeel(goldenLookAndFeel.get());
    addAndMakeVisible(asioLabel); asioLabel.setText("AUDIO DEVICE", juce::dontSendNotification);
    asioLabel.setFont(juce::Font(18.0f, juce::Font::bold)); asioLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    addAndMakeVisible(driverLabel); driverLabel.setText("Driver:", juce::dontSendNotification);
    addAndMakeVisible(specificDriverSelector); specificDriverSelector.addItemList(audioEngine.getSpecificDrivers("ASIO"), 1);
    specificDriverSelector.onChange = [this] { onSpecificDriverChanged(); };
    addAndMakeVisible(controlPanelButton); controlPanelButton.setButtonText("CP");
    controlPanelButton.onClick = [this] { audioEngine.openDriverControlPanel(); };
    addAndMakeVisible(deviceInfoLabel); deviceInfoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(logicLabel); logicLabel.setText("MIC STATUS", juce::dontSendNotification);
    logicLabel.setFont(juce::Font(18.0f, juce::Font::bold)); logicLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    
    // Mic Mute/Bypass Buttons with onClick callbacks
    auto setupB = [&](MidiTooltipToggleButton& b, const juce::String& t, const juce::String& m) { 
        b.setButtonText(t); 
        b.setMidiInfo(m); 
        addAndMakeVisible(b); 
    };
    setupB(mic1Mute, "Mic 1 Mute", "Note 10"); 
    setupB(mic1Bypass, "Mic 1 FX Bypass", "Note 11");
    setupB(mic2Mute, "Mic 2 Mute", "Note 12"); 
    setupB(mic2Bypass, "Mic 2 FX Bypass", "Note 13");
    
    // Connect mic buttons to AudioEngine AND IOSettingsManager
    mic1Mute.onClick = [this] { 
        audioEngine.setMicMute(0, mic1Mute.getToggleState()); 
        ioSettingsManager.saveMicMute(0, mic1Mute.getToggleState());
    };
    mic1Bypass.onClick = [this] { 
        audioEngine.setFxBypass(0, mic1Bypass.getToggleState()); 
        ioSettingsManager.saveMicBypass(0, mic1Bypass.getToggleState());
    };
    mic2Mute.onClick = [this] { 
        audioEngine.setMicMute(1, mic2Mute.getToggleState()); 
        ioSettingsManager.saveMicMute(1, mic2Mute.getToggleState());
    };
    mic2Bypass.onClick = [this] { 
        audioEngine.setFxBypass(1, mic2Bypass.getToggleState()); 
        ioSettingsManager.saveMicBypass(1, mic2Bypass.getToggleState());
    };
    
    addAndMakeVisible(settingsLabel); settingsLabel.setText("SETTINGS", juce::dontSendNotification);
    settingsLabel.setFont(juce::Font(18.0f, juce::Font::bold)); settingsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    
    // Latency Label and Slider
    addAndMakeVisible(latencyLabel);
    latencyLabel.setText("Recorded Vocals Latency", juce::dontSendNotification);
    latencyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    latencyLabel.setFont(juce::Font(12.0f));
    
    latencySlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    latencySlider->setRange(0.0, 500.0, 1.0); 
    latencySlider->setTextValueSuffix(" ms");
    latencySlider->onValueChange = [this] {
        float latencyMs = (float)latencySlider->getValue();
        audioEngine.setLatencyCorrectionMs(latencyMs);
        ioSettingsManager.saveVocalSettings(latencyMs, (float)vocalBoostSlider->getValue());
    };
    addAndMakeVisible(latencySlider.get());
    
    // Vocal Boost Label and Slider
    addAndMakeVisible(vocalBoostLabel);
    vocalBoostLabel.setText("Vocals Recorded Gain Boost", juce::dontSendNotification);
    vocalBoostLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    vocalBoostLabel.setFont(juce::Font(12.0f));
    
    vocalBoostSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    vocalBoostSlider->setRange(-24.0, 24.0, 0.1); 
    vocalBoostSlider->setTextValueSuffix(" dB");
    vocalBoostSlider->onValueChange = [this] {
        float boostDb = (float)vocalBoostSlider->getValue();
        audioEngine.setVocalBoostDb(boostDb);
        ioSettingsManager.saveVocalSettings((float)latencySlider->getValue(), boostDb);
    };
    addAndMakeVisible(vocalBoostSlider.get());
    
    addAndMakeVisible(midiLabel); midiLabel.setText("MIDI INPUT", juce::dontSendNotification);
    midiLabel.setFont(juce::Font(18.0f, juce::Font::bold)); midiLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    addAndMakeVisible(midiInputSelector); 
    midiInputSelector.onChange = [this] {
        juce::String deviceName = midiInputSelector.getText();
        audioEngine.setMidiInput(deviceName);
        ioSettingsManager.saveMidiDevice(deviceName);
    };
    addAndMakeVisible(midiRefreshButton); midiRefreshButton.setButtonText("Refresh");
    midiRefreshButton.onClick = [this] { updateMidiDevices(); };
    addAndMakeVisible(inputsLabel); inputsLabel.setText("INPUTS MATRIX", juce::dontSendNotification);
    inputsLabel.setFont(juce::Font(18.0f, juce::Font::bold)); inputsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    auto setupH = [&](juce::Label& l, const juce::String& t) { l.setText(t, juce::dontSendNotification); l.setFont(juce::Font(11.0f, juce::Font::bold)); l.setJustificationType(juce::Justification::centred); l.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37)); addAndMakeVisible(l); };
    setupH(headMic1, "Mic 1"); setupH(headMic2, "Mic 2"); setupH(headPbL, "Pb L"); setupH(headPbR, "Pb R"); setupH(headGain, "Gain");
    addAndMakeVisible(inputsViewport); inputsViewport.setViewedComponent(&inputsContainer, false);
    addAndMakeVisible(outputsLabel); outputsLabel.setText("OUTPUTS ROUTING", juce::dontSendNotification);
    outputsLabel.setFont(juce::Font(18.0f, juce::Font::bold)); outputsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    addAndMakeVisible(outputsViewport); outputsViewport.setViewedComponent(&outputsContainer, false);
    restoreSavedSettings(); startTimerHz(20);
}

IOPage::~IOPage() { stopTimer(); setLookAndFeel(nullptr); }

void IOPage::resized() {
    auto area = getLocalBounds().reduced(10);
    int w = area.getWidth();
    auto leftCol = area.removeFromLeft((int)(w * 0.25f)).reduced(10, 0);
    auto centerCol = area.removeFromLeft((int)(w * 0.50f)).reduced(10, 0);
    auto rightCol = area.reduced(10, 0);

    // FIXED #3: Left Column Layout - Added top padding for Audio Device section
    int y = 15;  // Start 15px down instead of 0
    asioLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    auto dr = leftCol.withY(y).withHeight(25);
    driverLabel.setBounds(dr.removeFromLeft(50)); controlPanelButton.setBounds(dr.removeFromRight(40));
    specificDriverSelector.setBounds(dr.reduced(5, 0)); y += 30;
    deviceInfoLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 35); y += 40;
    logicLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    mic1Mute.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    mic1Bypass.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    mic2Mute.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    mic2Bypass.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 40;
    settingsLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    latencyLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 20); y += 20;
    latencySlider->setBounds(leftCol.getX(), y, leftCol.getWidth(), 30); y += 35;
    vocalBoostLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 20); y += 20;
    vocalBoostSlider->setBounds(leftCol.getX(), y, leftCol.getWidth(), 30); y += 40;
    midiLabel.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    midiInputSelector.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25); y += 30;
    midiRefreshButton.setBounds(leftCol.getX(), y, leftCol.getWidth(), 25);

    // Center Matrix - EXACT REFERENCE ALIGNMENT 
    inputsLabel.setBounds(centerCol.removeFromTop(30));
    
    // Position Matrix Headers (Above Viewport)
    auto headerRow = centerCol.removeFromTop(20);
    
    // Exact Right-to-Left alignment matching InputItemComponent
    int rightX = headerRow.getRight();
    // Skip LED (30) + Gap (10) = 40
    rightX -= 40;
    
    // Gain Header (Matches Slider 100)
    int sliderW = 100;
    headGain.setBounds(rightX - sliderW, headerRow.getY(), sliderW, 20);
    
    // Skip Gap (25)
    int checksEnd = rightX - sliderW - 25;
    
    // Columns (90px each)
    int colW = 90;
    headPbR.setBounds(checksEnd - colW, headerRow.getY(), colW, 20);
    headPbL.setBounds(checksEnd - colW*2, headerRow.getY(), colW, 20);
    headMic2.setBounds(checksEnd - colW*3, headerRow.getY(), colW, 20);
    headMic1.setBounds(checksEnd - colW*4, headerRow.getY(), colW, 20);

    inputsViewport.setBounds(centerCol);
    inputsContainer.setBounds(0, 0, inputsViewport.getWidth(), inputItems.size() * 40);
    
    for (int i=0; i<inputItems.size(); ++i) {
        inputItems[i]->setBounds(0, i * 40, inputsContainer.getWidth(), 40);
    }

    // Right Column Layout - FIXED ALIGNMENT
    outputsLabel.setBounds(rightCol.removeFromTop(30));
    rightCol.removeFromTop(20);  // Match input headers spacing for perfect alignment
    outputsViewport.setBounds(rightCol);
    outputsContainer.setSize(outputsViewport.getWidth(), outputItems.size() * 40);  // 40px = same as inputs
    for (int i=0; i<outputItems.size(); ++i) outputItems[i]->setBounds(0, i * 40, outputsContainer.getWidth(), 40);
}

void IOPage::timerCallback() {
    auto* d = audioEngine.getDeviceManager().getCurrentAudioDevice();
    if (d) deviceInfoLabel.setText("SR: " + juce::String(d->getCurrentSampleRate()) + "Hz\nBuf: " + juce::String(d->getCurrentBufferSizeSamples()), juce::dontSendNotification);
    
    // Update input LEDs
    for (auto* item : inputItems) 
        item->signalLed.setOn(audioEngine.getInputLevel(item->itemIndex) > 0.001f);
    
    // Update output LEDs
    for (auto* item : outputItems) 
        item->signalLed.setOn(audioEngine.getOutputLevel(item->itemIndex) > 0.001f);
}

void IOPage::updateInputList() {
    inputItems.clear(); 
    auto inputs = audioEngine.getAvailableInputDevices(); 
    auto saved = ioSettingsManager.getInputRouting();
    
    for (int i = 0; i < inputs.size(); ++i) {
        auto* item = inputItems.add(new InputItemComponent(inputs[i], i)); 
        inputsContainer.addAndMakeVisible(item);
        
        int mask = audioEngine.getInputRoute(i); 
        float gain = audioEngine.getInputGain(i);
        if (saved.count(inputs[i])) { 
            mask = saved[inputs[i]].first; 
            gain = saved[inputs[i]].second; 
        }
        
        // Set visual state
        item->checkV1.setToggleState(mask & 1, juce::dontSendNotification); 
        item->checkV2.setToggleState(mask & 2, juce::dontSendNotification);
        item->checkPBL.setToggleState(mask & 4, juce::dontSendNotification); 
        item->checkPBR.setToggleState(mask & 8, juce::dontSendNotification);
        item->gainSlider->setValue(gainToSliderValue(gain), juce::dontSendNotification);
        
        // Apply routing to AudioEngine immediately
        audioEngine.setInputRoute(i, mask);
        audioEngine.setInputGain(i, gain);
        
        // Setup callbacks
        auto up = [this, i, item, n=inputs[i]] {
            int m = 0; 
            if (item->checkV1.getToggleState()) m|=1; 
            if (item->checkV2.getToggleState()) m|=2; 
            if (item->checkPBL.getToggleState()) m|=4; 
            if (item->checkPBR.getToggleState()) m|=8;
            float g = sliderValueToGain((float)item->gainSlider->getValue()); 
            audioEngine.setInputRoute(i, m); 
            audioEngine.setInputGain(i, g);
            auto map = ioSettingsManager.getInputRouting(); 
            map[n] = { m, g }; 
            ioSettingsManager.saveInputRouting(map);
        };
        item->checkV1.onClick = item->checkV2.onClick = item->checkPBL.onClick = item->checkPBR.onClick = up; 
        item->gainSlider->onValueChange = up;
    }
    resized();
}

void IOPage::updateOutputList() {
    outputItems.clear(); 
    auto outputs = audioEngine.getAvailableOutputDevices(); 
    auto saved = ioSettingsManager.getOutputRouting();
    
    for (int i = 0; i < outputs.size(); ++i) {
        auto* item = outputItems.add(new OutputItemComponent(outputs[i], i)); 
        outputsContainer.addAndMakeVisible(item);
        
        int mask = (saved.count(outputs[i])) ? saved[outputs[i]] : audioEngine.getOutputRoute(i);
        
        // Set visual state
        item->checkL.setToggleState(mask & 1, juce::dontSendNotification); 
        item->checkR.setToggleState(mask & 2, juce::dontSendNotification);
        
        // Apply routing to AudioEngine immediately
        audioEngine.setOutputRoute(i, mask);
        
        // Setup callbacks
        auto up = [this, i, item, n=outputs[i]] {
            int m = 0; 
            if (item->checkL.getToggleState()) m|=1; 
            if (item->checkR.getToggleState()) m|=2;
            audioEngine.setOutputRoute(i, m); 
            auto map = ioSettingsManager.getOutputRouting(); 
            map[n] = m; 
            ioSettingsManager.saveOutputRouting(map);
        };
        item->checkL.onClick = item->checkR.onClick = up;
    }
    resized();
}

void IOPage::restoreSavedSettings() {
    // Restore ASIO driver
    specificDriverSelector.setText(ioSettingsManager.getLastSpecificDriver(), juce::dontSendNotification);
    onSpecificDriverChanged(); 
    
    // Restore MIDI device
    updateMidiDevices(); 
    juce::String savedMidiDevice = ioSettingsManager.getLastMidiDevice();
    midiInputSelector.setText(savedMidiDevice, juce::dontSendNotification);
    if (savedMidiDevice.isNotEmpty() && savedMidiDevice != "OFF") {
        audioEngine.setMidiInput(savedMidiDevice);
    }
    
    // Restore latency and vocal boost sliders
    float latencyMs = ioSettingsManager.getLastLatencyMs();
    float vocalBoostDb = ioSettingsManager.getLastVocalBoostDb();
    latencySlider->setValue(latencyMs, juce::dontSendNotification);
    vocalBoostSlider->setValue(vocalBoostDb, juce::dontSendNotification);
    audioEngine.setLatencyCorrectionMs(latencyMs);
    audioEngine.setVocalBoostDb(vocalBoostDb);
    
    // Restore mic mute/bypass states
    auto mic1Settings = ioSettingsManager.getMicSettings(0);
    auto mic2Settings = ioSettingsManager.getMicSettings(1);
    
    mic1Mute.setToggleState(mic1Settings.isMuted, juce::dontSendNotification);
    mic1Bypass.setToggleState(mic1Settings.isBypassed, juce::dontSendNotification);
    mic2Mute.setToggleState(mic2Settings.isMuted, juce::dontSendNotification);
    mic2Bypass.setToggleState(mic2Settings.isBypassed, juce::dontSendNotification);
    
    audioEngine.setMicMute(0, mic1Settings.isMuted);
    audioEngine.setFxBypass(0, mic1Settings.isBypassed);
    audioEngine.setMicMute(1, mic2Settings.isMuted);
    audioEngine.setFxBypass(1, mic2Settings.isBypassed);
}

void IOPage::onSpecificDriverChanged() { 
    audioEngine.setSpecificDriver("ASIO", specificDriverSelector.getText()); 
    ioSettingsManager.saveSpecificDriver(specificDriverSelector.getText()); 
    updateInputList(); 
    updateOutputList(); 
}

void IOPage::updateMidiDevices() { 
    midiInputSelector.clear(); 
    auto list = audioEngine.getAvailableMidiInputs(); 
    list.insert(0, "OFF"); 
    midiInputSelector.addItemList(list, 1); 
}

void IOPage::paint(juce::Graphics& g) { 
    g.fillAll(juce::Colour(0xFF202020)); 
    g.setColour(juce::Colour(0xFF404040)); 
    g.fillRect((int)(getWidth()*0.25f), 10, 2, getHeight()-20); 
    g.fillRect((int)(getWidth()*0.75f), 10, 2, getHeight()-20); 
}
