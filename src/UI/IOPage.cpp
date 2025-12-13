#include "IOPage.h"
#include "../RegistrationManager.h" 
#include "../AppLogger.h"

IOPage::IOPage(AudioEngine& engine, IOSettingsManager& settings)
    : audioEngine(engine), ioSettingsManager(settings)
{
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    setLookAndFeel(goldenLookAndFeel.get());

    // ==============================================================================
    // COLUMN 1: AUDIO DEVICE (25%)
    // ==============================================================================
    
    // 1. ASIO
    addAndMakeVisible(asioLabel);
    asioLabel.setText("AUDIO DEVICE", juce::dontSendNotification);
    asioLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    asioLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));

    addAndMakeVisible(driverLabel); driverLabel.setText("Driver:", juce::dontSendNotification);
    addAndMakeVisible(specificDriverSelector);
    auto drivers = audioEngine.getSpecificDrivers("ASIO");
    drivers.insert(0, "OFF"); 
    specificDriverSelector.addItemList(drivers, 1);
    specificDriverSelector.onChange = [this] { onSpecificDriverChanged(); };

    addAndMakeVisible(controlPanelButton);
    controlPanelButton.setButtonText("CP");
    controlPanelButton.setTooltip("Open Control Panel");
    controlPanelButton.onClick = [this] { audioEngine.openDriverControlPanel(); };

    addAndMakeVisible(deviceInfoLabel);
    deviceInfoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    deviceInfoLabel.setFont(juce::Font(13.0f));

    // 2. Logic Controls (Mic Status)
    addAndMakeVisible(logicLabel);
    logicLabel.setText("MIC STATUS", juce::dontSendNotification);
    logicLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    logicLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));

    auto setupBtn = [&](MidiTooltipToggleButton& b, const juce::String& t, const juce::String& midi) {
        b.setButtonText(t); 
        b.setMidiInfo(midi); 
        addAndMakeVisible(b);
    };
    setupBtn(mic1Mute, "Mic 1 Mute", "MIDI: Note 10"); 
    mic1Mute.onClick = [this] {
        audioEngine.setMicMute(0, mic1Mute.getToggleState());
        ioSettingsManager.saveMicMute(0, mic1Mute.getToggleState());
    };
    setupBtn(mic1Bypass, "Mic 1 FX Bypass", "MIDI: Note 11"); 
    mic1Bypass.onClick = [this] {
        audioEngine.setFxBypass(0, mic1Bypass.getToggleState());
        ioSettingsManager.saveMicBypass(0, mic1Bypass.getToggleState());
    };
    setupBtn(mic2Mute, "Mic 2 Mute", "MIDI: Note 12"); 
    mic2Mute.onClick = [this] {
        audioEngine.setMicMute(1, mic2Mute.getToggleState());
        ioSettingsManager.saveMicMute(1, mic2Mute.getToggleState());
    };
    setupBtn(mic2Bypass, "Mic 2 FX Bypass", "MIDI: Note 13"); 
    mic2Bypass.onClick = [this] {
        audioEngine.setFxBypass(1, mic2Bypass.getToggleState());
        ioSettingsManager.saveMicBypass(1, mic2Bypass.getToggleState());
    };

    // 3. Settings
    addAndMakeVisible(settingsLabel);
    settingsLabel.setText("SETTINGS", juce::dontSendNotification);
    settingsLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    settingsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));

    addAndMakeVisible(latencyLabel); latencyLabel.setText("Latency (ms)", juce::dontSendNotification);
    latencySlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    latencySlider->setRange(0.0, 500.0, 1.0);
    latencySlider->setMidiInfo("MIDI: CC 24"); 
    latencySlider->onValueChange = [this] { 
        audioEngine.setLatencyCorrectionMs((float)latencySlider->getValue());
        ioSettingsManager.saveVocalSettings((float)latencySlider->getValue(), (float)vocalBoostSlider->getValue());
    };
    addAndMakeVisible(latencySlider.get());

    addAndMakeVisible(vocalBoostLabel); vocalBoostLabel.setText("Rec Boost (dB)", juce::dontSendNotification);
    vocalBoostSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    vocalBoostSlider->setRange(0.0, 24.0, 0.1);
    vocalBoostSlider->setMidiInfo("MIDI: CC 25");
    vocalBoostSlider->onValueChange = [this] {
        audioEngine.setVocalBoostDb((float)vocalBoostSlider->getValue());
        ioSettingsManager.saveVocalSettings((float)latencySlider->getValue(), (float)vocalBoostSlider->getValue());
    };
    addAndMakeVisible(vocalBoostSlider.get());

    // 4. MIDI
    addAndMakeVisible(midiLabel); midiLabel.setText("MIDI INPUT", juce::dontSendNotification);
    midiLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    midiLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));

    addAndMakeVisible(midiInputSelector);
    midiInputSelector.onChange = [this] {
        audioEngine.setMidiInput(midiInputSelector.getText());
        ioSettingsManager.saveMidiDevice(midiInputSelector.getText());
    };
    addAndMakeVisible(midiRefreshButton);
    midiRefreshButton.setButtonText("Refresh");
    midiRefreshButton.onClick = [this] { updateMidiDevices(); };

    // ==============================================================================
    // COLUMN 2: INPUTS MATRIX (50%)
    // ==============================================================================
    addAndMakeVisible(inputsLabel);
    inputsLabel.setText("INPUTS MATRIX", juce::dontSendNotification);
    inputsLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    inputsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));

    // Matrix Headers
    auto setupHead = [&](juce::Label& l, const juce::String& t) {
        l.setText(t, juce::dontSendNotification);
        l.setFont(juce::Font(12.0f, juce::Font::bold));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(l);
    };
    setupHead(headMic1, "Mic 1");
    setupHead(headMic2, "Mic 2");
    setupHead(headPbL, "Playback L");
    setupHead(headPbR, "Playback R");
    setupHead(headGain, "Gain");

    addAndMakeVisible(inputsViewport);
    inputsViewport.setViewedComponent(&inputsContainer, false);
    inputsViewport.setScrollBarsShown(true, false);

    // ==============================================================================
    // COLUMN 3: OUTPUTS ROUTING (25%)
    // ==============================================================================
    addAndMakeVisible(outputsLabel);
    outputsLabel.setText("OUTPUTS ROUTING", juce::dontSendNotification);
    outputsLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    outputsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));

    addAndMakeVisible(outputsViewport);
    outputsViewport.setViewedComponent(&outputsContainer, false);
    outputsViewport.setScrollBarsShown(true, false);

    updateInputList();
    updateOutputList();
    updateMidiDevices();
    
    startTimerHz(20);
}

IOPage::~IOPage() { stopTimer(); setLookAndFeel(nullptr); }

void IOPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF202020));
    
    int w = getWidth();
    int col1 = (int)(w * 0.25f);
    int col2 = (int)(w * 0.75f); 
    
    g.setColour(juce::Colour(0xFF404040));
    g.fillRect(col1, 10, 2, getHeight() - 20);
    g.fillRect(col2, 10, 2, getHeight() - 20);
}

void IOPage::resized()
{
    auto area = getLocalBounds().reduced(10);
    int w = area.getWidth();
    
    int col1W = (int)(w * 0.25f);
    int col2W = (int)(w * 0.50f);
    
    auto col1Area = area.removeFromLeft(col1W).reduced(10, 0);
    auto col2Area = area.removeFromLeft(col2W).reduced(10, 0);
    auto col3Area = area.reduced(10, 0);

    // --- COLUMN 1: AUDIO DEVICE ---
    int y = 0;
    
    asioLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 25); y += 30;
    
    auto driverRow = col1Area.removeFromTop(25); driverRow.setY(y);
    driverLabel.setBounds(driverRow.removeFromLeft(50));
    controlPanelButton.setBounds(driverRow.removeFromRight(40));
    specificDriverSelector.setBounds(driverRow.reduced(5, 0));
    y += 30;
    
    deviceInfoLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 35;

    logicLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 25); y += 30;
    mic1Mute.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 22;
    mic1Bypass.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 25;
    mic2Mute.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 22;
    mic2Bypass.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 35;

    settingsLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 25); y += 30;
    latencyLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 20;
    latencySlider->setBounds(col1Area.getX(), y, col1Area.getWidth(), 25); y += 30;
    vocalBoostLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 20); y += 20;
    vocalBoostSlider->setBounds(col1Area.getX(), y, col1Area.getWidth(), 25); y += 35;

    midiLabel.setBounds(col1Area.getX(), y, col1Area.getWidth(), 25); y += 30;
    auto midiRow = col1Area.removeFromTop(25); midiRow.setY(y);
    midiRefreshButton.setBounds(midiRow.removeFromRight(60));
    midiInputSelector.setBounds(midiRow.reduced(5, 0));

    // --- COLUMN 2: INPUTS MATRIX ---
    inputsLabel.setBounds(col2Area.removeFromTop(30));
    
    // Position Matrix Headers (Above Viewport)
    auto headerRow = col2Area.removeFromTop(20);
    
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

    inputsViewport.setBounds(col2Area);
    inputsContainer.setBounds(0, 0, inputsViewport.getWidth(), inputItems.size() * 40);
    
    for (int i=0; i<inputItems.size(); ++i) {
        inputItems[i]->setBounds(0, i * 40, inputsContainer.getWidth(), 40);
    }

    // --- COLUMN 3: OUTPUTS ROUTING ---
    outputsLabel.setBounds(col3Area.removeFromTop(30));
    outputsViewport.setBounds(col3Area);
    outputsContainer.setBounds(0, 0, outputsViewport.getWidth(), outputItems.size() * 35);
    
    for (int i=0; i<outputItems.size(); ++i) {
        outputItems[i]->setBounds(0, i * 35, outputsContainer.getWidth(), 35);
    }
}

void IOPage::timerCallback() {
    if (auto* d = audioEngine.getDeviceManager().getCurrentAudioDevice()) {
        deviceInfoLabel.setText(juce::String(d->getCurrentSampleRate()) + "Hz / " + 
                                juce::String(d->getCurrentBufferSizeSamples()) + "smp", juce::dontSendNotification);
    } else { deviceInfoLabel.setText("No Device", juce::dontSendNotification); }

    const float thres = 0.001f;
    for (auto* item : inputItems) {
        float lvl = audioEngine.getInputLevel(item->itemIndex);
        item->signalLed.setOn(lvl > thres);
    }
    
    float masterL = audioEngine.getOutputLevel(0);
    float masterR = audioEngine.getOutputLevel(1);
    for (auto* item : outputItems) {
        int mask = audioEngine.getOutputRoute(item->itemIndex);
        bool sig = (mask & 1 && masterL > thres) || (mask & 2 && masterR > thres);
        item->signalLed.setOn(sig);
    }
}

void IOPage::onSpecificDriverChanged() {
    ioSettingsManager.saveDriverType("ASIO");
    ioSettingsManager.saveSpecificDriver(specificDriverSelector.getText());
    audioEngine.setSpecificDriver("ASIO", specificDriverSelector.getText());
    
    updateInputList();
    updateOutputList();
    
    auto savedIn = ioSettingsManager.getInputRouting();
    auto inputs = audioEngine.getAvailableInputDevices();
    for (int i=0; i<inputs.size(); ++i) {
        if (savedIn.count(inputs[i])) {
            audioEngine.setInputRoute(i, savedIn[inputs[i]].first);
            audioEngine.setInputGain(i, savedIn[inputs[i]].second);
        }
    }
    updateInputList();

    auto savedOut = ioSettingsManager.getOutputRouting();
    auto outputs = audioEngine.getAvailableOutputDevices();
    for (int i=0; i<outputs.size(); ++i) {
        if (savedOut.count(outputs[i])) audioEngine.setOutputRoute(i, savedOut[outputs[i]]);
    }
    updateOutputList();
}

void IOPage::updateInputList() {
    inputItems.clear();
    inputsContainer.removeAllChildren();
    
    auto inputs = audioEngine.getAvailableInputDevices();
    for (int i=0; i<inputs.size(); ++i) {
        auto* item = new InputItemComponent(inputs[i], i);
        int mask = audioEngine.getInputRoute(i);
        float gain = audioEngine.getInputGain(i);
        
        item->checkV1.setToggleState(mask & 1, juce::dontSendNotification);
        item->checkV2.setToggleState(mask & 2, juce::dontSendNotification);
        item->checkPBL.setToggleState(mask & 4, juce::dontSendNotification);
        item->checkPBR.setToggleState(mask & 8, juce::dontSendNotification);
        item->gainSlider->setValue(gain, juce::dontSendNotification);
        
        auto update = [this, i, item, name=inputs[i]] {
            int newMask = 0;
            if (item->checkV1.getToggleState()) newMask |= 1;
            if (item->checkV2.getToggleState()) newMask |= 2;
            if (item->checkPBL.getToggleState()) newMask |= 4;
            if (item->checkPBR.getToggleState()) newMask |= 8;
            float newGain = (float)item->gainSlider->getValue();
            
            audioEngine.setInputRoute(i, newMask);
            audioEngine.setInputGain(i, newGain);
            
            auto map = ioSettingsManager.getInputRouting();
            map[name] = { newMask, newGain };
            ioSettingsManager.saveInputRouting(map);
        };
        
        item->checkV1.onClick = update;
        item->checkV2.onClick = update;
        item->checkPBL.onClick = update;
        item->checkPBR.onClick = update;
        item->gainSlider->onValueChange = update;
        
        inputItems.add(item);
        inputsContainer.addAndMakeVisible(item);
    }
    resized();
}

void IOPage::updateOutputList() {
    outputItems.clear();
    outputsContainer.removeAllChildren();
    
    auto outputs = audioEngine.getAvailableOutputDevices();
    for (int i=0; i<outputs.size(); ++i) {
        auto* item = new OutputItemComponent(outputs[i], i);
        int mask = audioEngine.getOutputRoute(i);
        
        item->checkL.setToggleState(mask & 1, juce::dontSendNotification);
        item->checkR.setToggleState(mask & 2, juce::dontSendNotification);
        
        auto update = [this, i, item, name=outputs[i]] {
            int newMask = 0;
            if (item->checkL.getToggleState()) newMask |= 1;
            if (item->checkR.getToggleState()) newMask |= 2;
            audioEngine.setOutputRoute(i, newMask);
            
            auto map = ioSettingsManager.getOutputRouting();
            map[name] = newMask;
            ioSettingsManager.saveOutputRouting(map);
        };
        
        item->checkL.onClick = update;
        item->checkR.onClick = update;
        outputItems.add(item);
        outputsContainer.addAndMakeVisible(item);
    }
    resized();
}

void IOPage::updateMidiDevices() {
    auto current = midiInputSelector.getText();
    auto list = audioEngine.getAvailableMidiInputs();
    list.insert(0, "OFF");
    midiInputSelector.clear();
    midiInputSelector.addItemList(list, 1);
    if (list.contains(current)) midiInputSelector.setText(current, juce::dontSendNotification);
    else midiInputSelector.setText("OFF", juce::sendNotification);
}

void IOPage::restoreSavedSettings() {
    juce::String savedDriver = ioSettingsManager.getLastSpecificDriver();
    auto available = audioEngine.getSpecificDrivers("ASIO");
    
    if (savedDriver.isNotEmpty() && savedDriver != "OFF" && available.contains(savedDriver)) {
        specificDriverSelector.setText(savedDriver, juce::dontSendNotification);
        onSpecificDriverChanged();
    } else {
        specificDriverSelector.setText("OFF", juce::dontSendNotification);
    }
    
    auto s0 = ioSettingsManager.getMicSettings(0);
    mic1Mute.setToggleState(s0.isMuted, juce::dontSendNotification);
    mic1Bypass.setToggleState(s0.isBypassed, juce::dontSendNotification);
    audioEngine.setMicMute(0, s0.isMuted);
    audioEngine.setFxBypass(0, s0.isBypassed);
    
    auto s1 = ioSettingsManager.getMicSettings(1);
    mic2Mute.setToggleState(s1.isMuted, juce::dontSendNotification);
    mic2Bypass.setToggleState(s1.isBypassed, juce::dontSendNotification);
    audioEngine.setMicMute(1, s1.isMuted);
    audioEngine.setFxBypass(1, s1.isBypassed);

    latencySlider->setValue(ioSettingsManager.getLastLatencyMs(), juce::sendNotification);
    vocalBoostSlider->setValue(ioSettingsManager.getLastVocalBoostDb(), juce::sendNotification);
    
    updateMidiDevices();
    auto savedMidi = ioSettingsManager.getLastMidiDevice();
    if (savedMidi.isNotEmpty() && savedMidi != "OFF") midiInputSelector.setText(savedMidi, juce::sendNotification);
}