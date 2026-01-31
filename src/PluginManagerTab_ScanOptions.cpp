// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_ScanOptions.cpp
// FIX: Trust All mode - fast scan without loading plugins
// When Trust All is ON, we just enumerate plugin files without validation
// This prevents ALL crashes during scanning

#include "PluginManagerTab.h"

// =============================================================================
// ScanOptionsPanel Implementation - TRUST ALL MODE
// =============================================================================
ScanOptionsPanel::ScanOptionsPanel(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    formatInfoLabel.setFont(juce::Font(12.0f));
    formatInfoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    formatInfoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(formatInfoLabel);
    updateFormatInfo();
    
    // =========================================================================
    // Trust All Plugins toggle - ON by default!
    // Fast scan = just enumerate files, no loading = no crashes
    // =========================================================================
    trustAllToggle.setToggleState(true, juce::dontSendNotification);
    trustAllToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lime);
    trustAllToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::lime);
    trustAllToggle.setTooltip("RECOMMENDED: Fast scan that finds all plugin files.\n"
                               "Does NOT load plugins during scan = NO CRASHES.\n"
                               "Plugins are validated when you actually use them.\n\n"
                               "Disable only if you need full plugin metadata during scan.");
    trustAllToggle.onClick = [this]() { updateSafetyOptionsVisibility(); };
    addAndMakeVisible(trustAllToggle);
    
    scanNewBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::green.darker());
    scanNewBtn.onClick = [this]() { scanForNewPlugins(false); };
    addAndMakeVisible(scanNewBtn);
    
    rescanAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::blue.darker());
    rescanAllBtn.onClick = [this]() { scanForNewPlugins(true); };
    addAndMakeVisible(rescanAllBtn);
    
    removeMissingBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
    removeMissingBtn.onClick = [this]() { removeMissingPlugins(); };
    addAndMakeVisible(removeMissingBtn);
    
    removeAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::red.darker());
    removeAllBtn.onClick = [this]() { removeAllPlugins(); };
    addAndMakeVisible(removeAllBtn);
    
    // VST2 options (hidden when Trust All is ON)
    #if JUCE_PLUGINHOST_VST
    skipShellsToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::yellow);
    skipShellsToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::yellow);
    addAndMakeVisible(skipShellsToggle);
    
    timeoutLabel.setFont(juce::Font(12.0f));
    timeoutLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(timeoutLabel);
    
    timeoutSlider.setRange(5.0, 120.0, 5.0);
    timeoutSlider.setValue(30.0);
    timeoutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timeoutSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible(timeoutSlider);
    #endif
    
    // Problematic plugins option (hidden when Trust All is ON)
    skipProblematicToggle.setToggleState(false, juce::dontSendNotification);
    skipProblematicToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::orange);
    skipProblematicToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);
    addAndMakeVisible(skipProblematicToggle);
    
    editSkipListBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::grey.darker());
    editSkipListBtn.onClick = [this]() { showSkipListEditor(); };
    addAndMakeVisible(editSkipListBtn);
    
    cancelBtn.onClick = [this]() { 
        saveSettings();
        if (onCompleteCallback) onCompleteCallback(); 
    };
    addAndMakeVisible(cancelBtn);
    
    statusLabel.setFont(juce::Font(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    loadSettings();
    updateSafetyOptionsVisibility();
    
    #if JUCE_PLUGINHOST_VST
    setSize(400, 520);
    #else
    setSize(400, 420);
    #endif
}

ScanOptionsPanel::~ScanOptionsPanel() {
    saveSettings();
}

void ScanOptionsPanel::updateSafetyOptionsVisibility() {
    bool trustAll = trustAllToggle.getToggleState();
    
    #if JUCE_PLUGINHOST_VST
    skipShellsToggle.setVisible(!trustAll);
    timeoutLabel.setVisible(!trustAll);
    timeoutSlider.setVisible(!trustAll);
    #endif
    
    skipProblematicToggle.setVisible(!trustAll);
    editSkipListBtn.setVisible(!trustAll);
    
    resized();
    repaint();
}

void ScanOptionsPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
    
    // Trust All box (green)
    g.setColour(juce::Colour(0xFF00FF00).withAlpha(0.2f));
    g.drawRect(10, 55, getWidth() - 20, 35, 1);
    
    // Buttons box
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(10, 100, getWidth() - 20, 180, 1);
    
    bool trustAll = trustAllToggle.getToggleState();
    
    if (!trustAll) {
        #if JUCE_PLUGINHOST_VST
        g.setColour(juce::Colour(0xFF4A90D9).withAlpha(0.3f));
        g.drawRect(10, 290, getWidth() - 20, 90, 1);
        
        g.setColour(juce::Colour(0xFF4A90D9));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("VST2 Options (validated scan only)", 15, 292, 220, 16, juce::Justification::left);
        
        int probBoxY = 390;
        #else
        int probBoxY = 290;
        #endif
        
        g.setColour(juce::Colour(0xFFFF8C00).withAlpha(0.3f));
        g.drawRect(10, probBoxY, getWidth() - 20, 60, 1);
        
        g.setColour(juce::Colour(0xFFFF8C00));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("Problematic Plugins (validated scan only)", 15, probBoxY + 2, 250, 16, juce::Justification::left);
    }
}

void ScanOptionsPanel::resized() {
    auto area = getLocalBounds().reduced(15);
    titleLabel.setBounds(area.removeFromTop(30));
    formatInfoLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
    
    trustAllToggle.setBounds(area.removeFromTop(30).reduced(20, 0));
    area.removeFromTop(10);
    
    auto buttonsArea = area.removeFromTop(170);
    int btnHeight = 32;
    int gap = 8;
    
    buttonsArea.removeFromTop(10);
    scanNewBtn.setBounds(buttonsArea.removeFromTop(btnHeight).reduced(40, 0));
    buttonsArea.removeFromTop(gap);
    rescanAllBtn.setBounds(buttonsArea.removeFromTop(btnHeight).reduced(40, 0));
    buttonsArea.removeFromTop(gap);
    removeMissingBtn.setBounds(buttonsArea.removeFromTop(btnHeight).reduced(40, 0));
    buttonsArea.removeFromTop(gap);
    removeAllBtn.setBounds(buttonsArea.removeFromTop(btnHeight).reduced(40, 0));
    
    bool trustAll = trustAllToggle.getToggleState();
    
    if (!trustAll) {
        #if JUCE_PLUGINHOST_VST
        area.removeFromTop(15);
        auto vst2Area = area.removeFromTop(80);
        vst2Area.removeFromTop(20);
        skipShellsToggle.setBounds(vst2Area.removeFromTop(24).reduced(20, 0));
        vst2Area.removeFromTop(8);
        auto timeoutRow = vst2Area.removeFromTop(24).reduced(20, 0);
        timeoutLabel.setBounds(timeoutRow.removeFromLeft(120));
        timeoutSlider.setBounds(timeoutRow);
        #endif
        
        area.removeFromTop(15);
        auto probArea = area.removeFromTop(50);
        probArea.removeFromTop(18);
        auto probRow = probArea.removeFromTop(24).reduced(20, 0);
        skipProblematicToggle.setBounds(probRow.removeFromLeft(280));
        probRow.removeFromLeft(10);
        editSkipListBtn.setBounds(probRow);
    }
    
    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
    cancelBtn.setBounds(area.removeFromBottom(28).reduced(120, 0));
}

void ScanOptionsPanel::updateFormatInfo() {
    juce::StringArray formats;
    
    #if JUCE_PLUGINHOST_VST
    formats.add("VST2");
    #endif
    #if JUCE_PLUGINHOST_VST3
    formats.add("VST3");
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formats.add("AudioUnit");
    #endif
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formats.add("LADSPA");
    #endif
    
    juce::String info = "Formats: " + formats.joinIntoString(", ");
    
    #if JUCE_WINDOWS
        #if defined(_M_ARM64) || defined(__aarch64__)
        info += " | Windows ARM64";
        #else
        info += " | Windows x64";
        #endif
    #elif JUCE_MAC
        #if defined(__arm64__) || defined(__aarch64__)
        info += " | macOS Apple Silicon";
        #else
        info += " | macOS Intel";
        #endif
    #elif JUCE_LINUX
    info += " | Linux";
    #endif
    
    formatInfoLabel.setText(info, juce::dontSendNotification);
}

void ScanOptionsPanel::saveSettings() {
    auto* settings = processor.appProperties.getUserSettings();
    if (!settings) return;
    
    settings->setValue("TrustAllPlugins", trustAllToggle.getToggleState());
    
    #if JUCE_PLUGINHOST_VST
    settings->setValue("SkipVST2ShellPlugins", skipShellsToggle.getToggleState());
    settings->setValue("VST2ScanTimeout", (int)timeoutSlider.getValue());
    #endif
    
    settings->setValue("SkipProblematicPlugins", skipProblematicToggle.getToggleState());
    
    settings->saveIfNeeded();
}

void ScanOptionsPanel::loadSettings() {
    auto* settings = processor.appProperties.getUserSettings();
    if (!settings) return;
    
    // Default Trust All to ON
    trustAllToggle.setToggleState(settings->getBoolValue("TrustAllPlugins", true), juce::dontSendNotification);
    
    #if JUCE_PLUGINHOST_VST
    skipShellsToggle.setToggleState(settings->getBoolValue("SkipVST2ShellPlugins", false), juce::dontSendNotification);
    timeoutSlider.setValue(settings->getIntValue("VST2ScanTimeout", 30), juce::dontSendNotification);
    #endif
    
    skipProblematicToggle.setToggleState(settings->getBoolValue("SkipProblematicPlugins", false), juce::dontSendNotification);
}

void ScanOptionsPanel::scanForNewPlugins(bool clearList) {
    saveSettings();
    
    juce::Component::SafePointer<ScanOptionsPanel> safeThis(this);
    
    if (clearList) {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "Rescan All Plugins",
            "This will clear the plugin list and rescan everything.\nContinue?",
            "Yes",
            "No",
            this,
            juce::ModalCallbackFunction::create([safeThis](int result) {
                if (safeThis == nullptr || result != 1) return;
                
                safeThis->processor.knownPluginList.clear();
                safeThis->processor.knownPluginList.clearBlacklistedFiles();
                
                if (auto* userSettings = safeThis->processor.appProperties.getUserSettings()) {
                    if (auto xml = safeThis->processor.knownPluginList.createXml())
                        userSettings->setValue("KnownPlugins", xml.get());
                    userSettings->saveIfNeeded();
                }
                
                safeThis->processor.knownPluginList.sendChangeMessage();
                safeThis->launchScanner();
            })
        );
    } else {
        launchScanner();
    }
}

// =============================================================================
// Launch scanner with Trust All mode
// =============================================================================
void ScanOptionsPanel::launchScanner() {
    auto* parent = findParentComponentOfClass<juce::DialogWindow>();
    if (!parent) return;
    
    SubterraneumAudioProcessor* procPtr = &processor;
    auto originalCallback = onCompleteCallback;
    
    // Get Trust All setting
    bool trustAll = trustAllToggle.getToggleState();
    
    // Get other settings (only used if Trust All is OFF)
    #if JUCE_PLUGINHOST_VST
    bool skipShells = skipShellsToggle.getToggleState();
    int timeoutMs = (int)timeoutSlider.getValue() * 1000;
    #else
    bool skipShells = false;
    int timeoutMs = 30000;
    #endif
    
    bool skipProblematic = skipProblematicToggle.getToggleState();
    
    parent->removeFromDesktop();
    
    juce::MessageManager::callAsync([procPtr, originalCallback, trustAll, skipShells, timeoutMs, skipProblematic]() {
        if (!procPtr) return;
        
        auto scannerDialogPtr = std::make_shared<juce::DialogWindow*>(nullptr);
        
        auto wrapperCallback = [originalCallback, scannerDialogPtr]() {
            if (*scannerDialogPtr != nullptr) {
                (*scannerDialogPtr)->removeFromDesktop();
                delete *scannerDialogPtr;
                *scannerDialogPtr = nullptr;
            }
            
            if (originalCallback) originalCallback();
        };
        
        auto* scanner = new AutoPluginScanner(*procPtr, wrapperCallback);
        
        // Set Trust All mode - THIS IS THE KEY!
        scanner->setTrustAllMode(trustAll);
        
        // Only relevant if Trust All is OFF
        scanner->setSkipVST2ShellPlugins(skipShells);
        scanner->setVST2Timeout(timeoutMs);
        scanner->setSkipProblematicPlugins(skipProblematic);
        
        scanner->setSize(450, 200);
        
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = trustAll ? "Fast Scan (Trust All)" : "Scanning Plugins...";
        options.dialogBackgroundColour = juce::Colour(0xFF1E1E1E);
        options.content.setOwned(scanner);
        options.escapeKeyTriggersCloseButton = false;
        options.useNativeTitleBar = true;
        options.resizable = false;
        
        *scannerDialogPtr = options.launchAsync();
        
        scanner->startScan();
    });
}

void ScanOptionsPanel::removeMissingPlugins() {
    juce::Component::SafePointer<ScanOptionsPanel> safeThis(this);
    
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon,
        "Remove Missing Plugins",
        "Remove plugins that no longer exist on disk?",
        "Yes",
        "No",
        this,
        juce::ModalCallbackFunction::create([safeThis](int result) {
            if (safeThis == nullptr || result != 1) return;
            
            auto types = safeThis->processor.knownPluginList.getTypes();
            int removedCount = 0;
            
            for (int i = types.size() - 1; i >= 0; --i) {
                juce::File pluginFile(types[i].fileOrIdentifier);
                if (!pluginFile.existsAsFile() && !pluginFile.isDirectory()) {
                    safeThis->processor.knownPluginList.removeType(types[i]);
                    removedCount++;
                }
            }
            
            if (auto* userSettings = safeThis->processor.appProperties.getUserSettings()) {
                if (auto xml = safeThis->processor.knownPluginList.createXml())
                    userSettings->setValue("KnownPlugins", xml.get());
                userSettings->saveIfNeeded();
            }
            
            safeThis->statusLabel.setText("Removed " + juce::String(removedCount) + " missing plugins", 
                                          juce::dontSendNotification);
        })
    );
}

void ScanOptionsPanel::removeAllPlugins() {
    juce::Component::SafePointer<ScanOptionsPanel> safeThis(this);
    
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Remove All Plugins",
        "This will remove ALL plugins from the list!\nContinue?",
        "Yes, Remove All",
        "Cancel",
        this,
        juce::ModalCallbackFunction::create([safeThis](int result) {
            if (safeThis == nullptr || result != 1) return;
            
            safeThis->processor.knownPluginList.clear();
            
            if (auto* userSettings = safeThis->processor.appProperties.getUserSettings()) {
                if (auto xml = safeThis->processor.knownPluginList.createXml())
                    userSettings->setValue("KnownPlugins", xml.get());
                userSettings->saveIfNeeded();
            }
            
            safeThis->statusLabel.setText("All plugins removed", juce::dontSendNotification);
        })
    );
}

void ScanOptionsPanel::showSkipListEditor() {
    auto* settings = processor.appProperties.getUserSettings();
    
    juce::String defaultList = "UJAM\nArturia\nEast West\nBest Service\nOutput\nSpitfire Audio";
    juce::String currentList = settings ? settings->getValue("ProblematicVendorsList", defaultList) : defaultList;
    
    auto* editor = new juce::AlertWindow("Edit Problematic Vendors List",
        "Enter vendor names to skip (one per line):",
        juce::MessageBoxIconType::NoIcon);
    
    editor->addTextEditor("vendors", currentList, "");
    
    if (auto* textEditor = editor->getTextEditor("vendors")) {
        textEditor->setMultiLine(true, true);
        textEditor->setReturnKeyStartsNewLine(true);
    }
    
    editor->addButton("Save", 1);
    editor->addButton("Reset", 2);
    editor->addButton("Cancel", 0);
    
    juce::Component::SafePointer<ScanOptionsPanel> safeThis(this);
    
    editor->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, editor, defaultList](int result) {
        if (safeThis == nullptr) {
            delete editor;
            return;
        }
        
        auto* settings = safeThis->processor.appProperties.getUserSettings();
        
        if (result == 1) {
            juce::String newList = editor->getTextEditorContents("vendors");
            if (settings) {
                settings->setValue("ProblematicVendorsList", newList);
                settings->saveIfNeeded();
            }
            safeThis->statusLabel.setText("Skip list saved", juce::dontSendNotification);
        }
        else if (result == 2) {
            if (settings) {
                settings->setValue("ProblematicVendorsList", defaultList);
                settings->saveIfNeeded();
            }
            safeThis->statusLabel.setText("Reset to defaults", juce::dontSendNotification);
        }
        
        delete editor;
    }), true);
}
