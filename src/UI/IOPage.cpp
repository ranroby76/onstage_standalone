// D:\Workspace\ONSTAGE_WIRED\src\UI\IOPage.cpp
// ==============================================================================
//  IOPage.cpp
//  OnStage — ASIO-only device settings page
// ==============================================================================

#include "IOPage.h"
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"
#include "../AppLogger.h"
#include "../dsp/RecorderProcessor.h"  // NEW: For RecorderProcessor::setGlobalDefaultFolder

// ==============================================================================
//  Constructor
// ==============================================================================

IOPage::IOPage (AudioEngine& engine, IOSettingsManager& settings)
    : audioEngine (engine),
      ioSettingsManager (settings)
{
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    setLookAndFeel (goldenLookAndFeel.get());

    // --- Force ASIO device type ----------------------------------------------
    auto& dm = audioEngine.getDeviceManager();
    dm.setCurrentAudioDeviceType ("ASIO", true);
    dm.addChangeListener (this);

    // --- Section: AUDIO DEVICE -----------------------------------------------
    addAndMakeVisible (sectionAudioLabel);
    sectionAudioLabel.setText ("AUDIO DEVICE (ASIO)", juce::dontSendNotification);
    sectionAudioLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    sectionAudioLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));

    addAndMakeVisible (driverLabel);
    driverLabel.setText ("ASIO Driver:", juce::dontSendNotification);
    driverLabel.setFont (juce::Font (13.0f));
    driverLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible (driverSelector);
    populateDriverList();
    driverSelector.onChange = [this] { onDriverChanged(); };

    addAndMakeVisible (controlPanelButton);
    controlPanelButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF333333));
    controlPanelButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFD4AF37));
    controlPanelButton.onClick = [this]
    {
        auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
        if (device != nullptr)
            device->showControlPanel();
    };

    // --- Device info labels --------------------------------------------------
    auto setupInfoLabel = [&] (juce::Label& lbl, const juce::String& initial)
    {
        addAndMakeVisible (lbl);
        lbl.setText (initial, juce::dontSendNotification);
        lbl.setFont (juce::Font (13.0f));
        lbl.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    };

    setupInfoLabel (sampleRateLabel,  "Sample Rate: N/A");
    setupInfoLabel (bufferSizeLabel,  "Buffer Size: N/A");
    setupInfoLabel (latencyLabel,     "Latency: N/A");
    setupInfoLabel (inputCountLabel,  "Inputs: N/A");
    setupInfoLabel (outputCountLabel, "Outputs: N/A");

    // --- Section: MIDI INPUT -------------------------------------------------
    addAndMakeVisible (sectionMidiLabel);
    sectionMidiLabel.setText ("MIDI INPUT", juce::dontSendNotification);
    sectionMidiLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    sectionMidiLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));

    addAndMakeVisible (midiSelectButton);
    midiSelectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF333333));
    midiSelectButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    midiSelectButton.onClick = [this] { openMidiPopup(); };

    addAndMakeVisible (midiSummaryLabel);
    midiSummaryLabel.setFont (juce::Font (11.0f));
    midiSummaryLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    midiSummaryLabel.setJustificationType (juce::Justification::topLeft);

    // --- Section: RECORDING FOLDER (NEW) -------------------------------------
    addAndMakeVisible (sectionRecordingLabel);
    sectionRecordingLabel.setText ("RECORDING FOLDER", juce::dontSendNotification);
    sectionRecordingLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    sectionRecordingLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));

    addAndMakeVisible (recordingFolderButton);
    recordingFolderButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF333333));
    recordingFolderButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    recordingFolderButton.onClick = [this] { chooseRecordingFolder(); };

    addAndMakeVisible (recordingFolderPathLabel);
    recordingFolderPathLabel.setFont (juce::Font (11.0f));
    recordingFolderPathLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    recordingFolderPathLabel.setJustificationType (juce::Justification::topLeft);

    // --- Section: INPUTS -----------------------------------------------------
    addAndMakeVisible (sectionInputsLabel);
    sectionInputsLabel.setText ("INPUT CHANNELS", juce::dontSendNotification);
    sectionInputsLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    sectionInputsLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));

    addAndMakeVisible (inputChannelList);
    inputChannelList.setMultiLine (true);
    inputChannelList.setReadOnly (true);
    inputChannelList.setScrollbarsShown (true);
    inputChannelList.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1A1A1A));
    inputChannelList.setColour (juce::TextEditor::textColourId,       juce::Colours::lightgrey);
    inputChannelList.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xFF333333));
    inputChannelList.setFont (juce::Font (24.0f));  // Font size * 2

    // --- Section: OUTPUTS ----------------------------------------------------
    addAndMakeVisible (sectionOutputsLabel);
    sectionOutputsLabel.setText ("OUTPUT CHANNELS", juce::dontSendNotification);
    sectionOutputsLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    sectionOutputsLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));

    addAndMakeVisible (outputChannelList);
    outputChannelList.setMultiLine (true);
    outputChannelList.setReadOnly (true);
    outputChannelList.setScrollbarsShown (true);
    outputChannelList.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1A1A1A));
    outputChannelList.setColour (juce::TextEditor::textColourId,       juce::Colours::lightgrey);
    outputChannelList.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xFF333333));
    outputChannelList.setFont (juce::Font (24.0f));  // Font size * 2

    // --- Routing notice ------------------------------------------------------
    addAndMakeVisible (routingNotice);
    routingNotice.setText ("All channels are always active.\nUse the Studio tab to route audio between nodes.",
                           juce::dontSendNotification);
    routingNotice.setFont (juce::Font (12.0f, juce::Font::italic));
    routingNotice.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));
    routingNotice.setJustificationType (juce::Justification::centredLeft);

    // --- Restore saved settings and kick off ---------------------------------
    restoreSavedSettings();
    updateDeviceInfo();
    updateMidiSummary();
    updateRecordingFolderDisplay();  // NEW: Initialize recording folder display
    startTimerHz (4);
}

IOPage::~IOPage()
{
    closeMidiPopup();
    audioEngine.getDeviceManager().removeChangeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

// ==============================================================================
//  Restore saved settings on startup
// ==============================================================================

void IOPage::restoreSavedSettings()
{
    // --- Restore ASIO driver -------------------------------------------------
    juce::String savedDriver = ioSettingsManager.getLastSpecificDriver();
    
    if (savedDriver.isNotEmpty())
    {
        // Check if the saved driver is available
        auto& dm = audioEngine.getDeviceManager();
        auto* asioType = dm.getCurrentDeviceTypeObject();
        
        if (asioType != nullptr)
        {
            asioType->scanForDevices();
            auto availableDrivers = asioType->getDeviceNames();
            
            if (availableDrivers.contains (savedDriver))
            {
                LOG_INFO ("IOPage: Restoring saved ASIO driver: " + savedDriver);
                driverSelector.setText (savedDriver, juce::dontSendNotification);
                openDeviceWithAllChannels (savedDriver);
            }
            else
            {
                LOG_INFO ("IOPage: Saved ASIO driver not available: " + savedDriver);
                driverSelector.setSelectedId (1, juce::dontSendNotification);  // "None"
            }
        }
    }
    else
    {
        // No saved driver - default to "None"
        driverSelector.setSelectedId (1, juce::dontSendNotification);
    }
    
    // --- Restore MIDI devices ------------------------------------------------
    auto savedMidiDevices = ioSettingsManager.getLastMidiDevices();
    auto& dm = audioEngine.getDeviceManager();
    auto availableMidi = juce::MidiInput::getAvailableDevices();
    
    for (const auto& savedId : savedMidiDevices)
    {
        // Check if device is still connected
        bool found = false;
        for (const auto& device : availableMidi)
        {
            if (device.identifier == savedId)
            {
                dm.setMidiInputDeviceEnabled (savedId, true);
                LOG_INFO ("IOPage: Restored MIDI device: " + device.name);
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            LOG_INFO ("IOPage: Saved MIDI device not connected: " + savedId);
        }
    }
    
    // --- Restore recording folder (NEW) --------------------------------------
    juce::String savedRecordingFolder = ioSettingsManager.getRecordingFolder();
    
    if (savedRecordingFolder.isNotEmpty())
    {
        juce::File folder (savedRecordingFolder);
        if (folder.exists())
        {
            RecorderProcessor::setGlobalDefaultFolder (folder);
            LOG_INFO ("IOPage: Restored recording folder: " + savedRecordingFolder);
        }
        else
        {
            LOG_INFO ("IOPage: Saved recording folder does not exist: " + savedRecordingFolder);
        }
    }
}

// ==============================================================================
//  Save MIDI settings
// ==============================================================================

void IOPage::saveMidiSettings()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    auto& dm = audioEngine.getDeviceManager();
    
    juce::StringArray enabledIds;
    for (const auto& d : devices)
    {
        if (dm.isMidiInputDeviceEnabled (d.identifier))
            enabledIds.add (d.identifier);
    }
    
    ioSettingsManager.saveMidiDevices (enabledIds);
}

// ==============================================================================
//  Driver list
// ==============================================================================

void IOPage::populateDriverList()
{
    driverSelector.clear (juce::dontSendNotification);
    driverSelector.addItem ("None", 1);

    auto& dm = audioEngine.getDeviceManager();
    auto* asioType = dm.getCurrentDeviceTypeObject();

    if (asioType != nullptr)
    {
        asioType->scanForDevices();
        auto names = asioType->getDeviceNames();

        for (int i = 0; i < names.size(); ++i)
            driverSelector.addItem (names[i], i + 2);
    }

    auto* currentDevice = dm.getCurrentAudioDevice();
    if (currentDevice != nullptr)
        driverSelector.setText (currentDevice->getName(), juce::dontSendNotification);
    else
        driverSelector.setSelectedId (1, juce::dontSendNotification);
}

// ==============================================================================
//  Driver changed
// ==============================================================================

void IOPage::onDriverChanged()
{
    juce::String selected = driverSelector.getText();

    if (selected == "None" || selected.isEmpty())
    {
        audioEngine.getDeviceManager().closeAudioDevice();
        LOG_INFO ("IOPage: Audio device closed (None selected)");
        
        // Save empty driver selection
        ioSettingsManager.saveSpecificDriver ("");
        
        updateDeviceInfo();
        return;
    }

    openDeviceWithAllChannels (selected);
    
    // Save the selected driver
    ioSettingsManager.saveSpecificDriver (selected);
}

// ==============================================================================
//  Open device with ALL channels enabled
// ==============================================================================

void IOPage::openDeviceWithAllChannels (const juce::String& deviceName)
{
    auto& dm = audioEngine.getDeviceManager();

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup (setup);

    setup.outputDeviceName = deviceName;
    setup.inputDeviceName  = deviceName;

    juce::BigInteger allChannels;
    allChannels.setRange (0, 128, true);

    setup.inputChannels  = allChannels;
    setup.outputChannels = allChannels;
    setup.useDefaultInputChannels  = false;
    setup.useDefaultOutputChannels = false;

    juce::String err = dm.setAudioDeviceSetup (setup, true);

    if (err.isNotEmpty())
    {
        LOG_ERROR ("IOPage: Failed to open ASIO device: " + err);
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "ASIO Error",
            "Could not open device:\n" + err);
    }
    else
    {
        LOG_INFO ("IOPage: Opened ASIO device: " + deviceName);
    }

    updateDeviceInfo();
}

// ==============================================================================
//  Update device info display
// ==============================================================================

void IOPage::updateDeviceInfo()
{
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();

    if (device == nullptr)
    {
        sampleRateLabel.setText  ("Sample Rate: N/A", juce::dontSendNotification);
        bufferSizeLabel.setText  ("Buffer Size: N/A", juce::dontSendNotification);
        latencyLabel.setText     ("Latency: N/A",     juce::dontSendNotification);
        inputCountLabel.setText  ("Inputs: N/A",      juce::dontSendNotification);
        outputCountLabel.setText ("Outputs: N/A",     juce::dontSendNotification);
        inputChannelList.setText ("");
        outputChannelList.setText ("");
        return;
    }

    double sr  = device->getCurrentSampleRate();
    int    buf = device->getCurrentBufferSizeSamples();
    double latMs = (sr > 0) ? (buf / sr) * 1000.0 : 0.0;

    auto activeIns  = device->getActiveInputChannels();
    auto activeOuts = device->getActiveOutputChannels();
    int numIns  = activeIns.countNumberOfSetBits();
    int numOuts = activeOuts.countNumberOfSetBits();

    sampleRateLabel.setText  ("Sample Rate: " + juce::String ((int) sr) + " Hz", juce::dontSendNotification);
    bufferSizeLabel.setText  ("Buffer Size: " + juce::String (buf) + " samples",  juce::dontSendNotification);
    latencyLabel.setText     ("Latency: " + juce::String (latMs, 1) + " ms",      juce::dontSendNotification);
    inputCountLabel.setText  ("Inputs: " + juce::String (numIns),                  juce::dontSendNotification);
    outputCountLabel.setText ("Outputs: " + juce::String (numOuts),                juce::dontSendNotification);

    auto inNames  = device->getInputChannelNames();
    auto outNames = device->getOutputChannelNames();

    juce::String inText;
    int ch = 1;
    for (int i = 0; i < inNames.size(); ++i)
    {
        if (activeIns[i])
            inText += juce::String (ch++) + ".  " + inNames[i] + "\n";
    }
    inputChannelList.setText (inText.trimEnd());

    juce::String outText;
    ch = 1;
    for (int i = 0; i < outNames.size(); ++i)
    {
        if (activeOuts[i])
            outText += juce::String (ch++) + ".  " + outNames[i] + "\n";
    }
    outputChannelList.setText (outText.trimEnd());
}

// ==============================================================================
//  MIDI multi-select popup
// ==============================================================================

void IOPage::openMidiPopup()
{
    // If already open, just bring to front
    if (midiPopup != nullptr)
    {
        midiPopup->toFront (true);
        return;
    }

    auto& dm = audioEngine.getDeviceManager();

    auto* content = new MidiSelectorContent (dm);
    content->onSelectionChanged = [this] { 
        updateMidiSummary(); 
        saveMidiSettings();  // Save whenever selection changes
    };

    midiPopup = std::make_unique<MidiPopupWindow> (
        "MIDI Input Devices",
        juce::Colour (0xFF202020),
        juce::DocumentWindow::closeButton);

    midiPopup->setUsingNativeTitleBar (false);
    midiPopup->setContentOwned (content, true);
    midiPopup->setResizable (false, false);
    midiPopup->setAlwaysOnTop (true);

    // Position near the button
    auto screenPos = midiSelectButton.localPointToGlobal (juce::Point<int> (0, midiSelectButton.getHeight()));
    midiPopup->setTopLeftPosition (screenPos.x, screenPos.y + 4);

    midiPopup->setVisible (true);

    // Override close button to call our cleanup
    midiPopup->onClose = [this] { closeMidiPopup(); };
}

void IOPage::closeMidiPopup()
{
    midiPopup.reset();
}

void IOPage::updateMidiSummary()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    auto& dm = audioEngine.getDeviceManager();

    juce::StringArray enabled;
    for (auto& d : devices)
    {
        if (dm.isMidiInputDeviceEnabled (d.identifier))
            enabled.add (d.name);
    }

    if (enabled.isEmpty())
        midiSummaryLabel.setText ("No MIDI devices active", juce::dontSendNotification);
    else
        midiSummaryLabel.setText (enabled.joinIntoString (", "), juce::dontSendNotification);
}

// ==============================================================================
//  Recording Folder Management (NEW)
// ==============================================================================

void IOPage::chooseRecordingFolder()
{
    recordingFolderChooser = std::make_shared<juce::FileChooser> (
        "Select Default Recording Folder",
        RecorderProcessor::getEffectiveDefaultFolder(),
        "",
        true);
    
    recordingFolderChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser = recordingFolderChooser] (const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.isEmpty())
                return;
            
            auto folder = results.getFirst();
            
            // Save to settings
            ioSettingsManager.saveRecordingFolder (folder.getFullPathName());
            
            // Update RecorderProcessor global default
            RecorderProcessor::setGlobalDefaultFolder (folder);
            
            // Update display
            updateRecordingFolderDisplay();
            
            LOG_INFO ("IOPage: Recording folder set to: " + folder.getFullPathName());
        });
}

void IOPage::updateRecordingFolderDisplay()
{
    juce::String folderPath = ioSettingsManager.getRecordingFolder();
    
    if (folderPath.isEmpty())
    {
        // Show default
        auto defaultFolder = RecorderProcessor::getEffectiveDefaultFolder();
        recordingFolderPathLabel.setText ("Default: " + defaultFolder.getFullPathName(),
                                         juce::dontSendNotification);
    }
    else
    {
        recordingFolderPathLabel.setText ("Current: " + folderPath,
                                         juce::dontSendNotification);
    }
}

// ==============================================================================
//  ChangeListener
// ==============================================================================

void IOPage::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateDeviceInfo();
}

// ==============================================================================
//  Timer
// ==============================================================================

void IOPage::timerCallback()
{
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
    if (device != nullptr)
    {
        double sr  = device->getCurrentSampleRate();
        int    buf = device->getCurrentBufferSizeSamples();
        double latMs = (buf / sr) * 1000.0;
        sampleRateLabel.setText ("Sample Rate: " + juce::String ((int) sr) + " Hz", juce::dontSendNotification);
        bufferSizeLabel.setText ("Buffer Size: " + juce::String (buf) + " samples",  juce::dontSendNotification);
        latencyLabel.setText    ("Latency: " + juce::String (latMs, 1) + " ms",      juce::dontSendNotification);
    }
}

// ==============================================================================
//  Paint
// ==============================================================================

void IOPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF202020));

    // Left canvas width * 1.25 = 43.75%
    int divX = (int) (getWidth() * 0.4375f);
    g.setColour (juce::Colour (0xFF404040));
    g.drawVerticalLine (divX, 10.0f, (float) getHeight() - 10.0f);
}

// ==============================================================================
//  Layout
// ==============================================================================

void IOPage::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Left canvas width * 1.25 = 43.75%
    int divX = (int) (area.getWidth() * 0.4375f);
    auto left  = area.removeFromLeft (divX).reduced (0, 4);
    area.removeFromLeft (16);

    auto right = area;

    // ---- Left column (centered content) -------------------------------------
    // Calculate content width and center it
    int contentWidth = juce::jmin (left.getWidth() - 20, 280);
    int leftPadding = (left.getWidth() - contentWidth) / 2;
    auto leftContent = left.reduced (leftPadding, 0);

    sectionAudioLabel.setBounds (leftContent.removeFromTop (28));
    leftContent.removeFromTop (6);

    driverLabel.setBounds (leftContent.removeFromTop (20));
    leftContent.removeFromTop (2);

    auto driverRow = leftContent.removeFromTop (28);
    controlPanelButton.setBounds (driverRow.removeFromRight (110));
    driverRow.removeFromRight (6);
    driverSelector.setBounds (driverRow);
    leftContent.removeFromTop (12);

    sampleRateLabel.setBounds  (leftContent.removeFromTop (20));
    bufferSizeLabel.setBounds  (leftContent.removeFromTop (20));
    latencyLabel.setBounds     (leftContent.removeFromTop (20));
    leftContent.removeFromTop (4);
    inputCountLabel.setBounds  (leftContent.removeFromTop (20));
    outputCountLabel.setBounds (leftContent.removeFromTop (20));
    leftContent.removeFromTop (20);

    // MIDI section
    sectionMidiLabel.setBounds (leftContent.removeFromTop (28));
    leftContent.removeFromTop (4);
    midiSelectButton.setBounds (leftContent.removeFromTop (28));
    leftContent.removeFromTop (4);
    midiSummaryLabel.setBounds (leftContent.removeFromTop (36));
    leftContent.removeFromTop (20);

    // Recording Folder section (NEW)
    sectionRecordingLabel.setBounds (leftContent.removeFromTop (28));
    leftContent.removeFromTop (4);
    recordingFolderButton.setBounds (leftContent.removeFromTop (28));
    leftContent.removeFromTop (4);
    recordingFolderPathLabel.setBounds (leftContent.removeFromTop (36));
    leftContent.removeFromTop (20);

    // Routing notice
    routingNotice.setBounds (leftContent.removeFromTop (40));

    // ---- Right area: Inputs (top half) / Outputs (bottom half) --------------
    int halfH = right.getHeight() / 2 - 8;

    auto inputArea  = right.removeFromTop (halfH);
    right.removeFromTop (16);
    auto outputArea = right;

    sectionInputsLabel.setBounds (inputArea.removeFromTop (24));
    inputArea.removeFromTop (4);
    inputChannelList.setBounds (inputArea);

    sectionOutputsLabel.setBounds (outputArea.removeFromTop (24));
    outputArea.removeFromTop (4);
    outputChannelList.setBounds (outputArea);
}
