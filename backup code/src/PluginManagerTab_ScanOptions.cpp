// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_ScanOptions.cpp
// Simplified scan options — no vendor blocking, no trust-all toggle, no timeout slider
// Just: Scan New, Rescan All, Remove Missing, Remove All
// Uses the new 3-phase out-of-process scanner for zero freezes

#include "PluginManagerTab.h"

ScanOptionsPanel::ScanOptionsPanel(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    formatInfoLabel.setFont(juce::Font(11.0f));
    formatInfoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    formatInfoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(formatInfoLabel);
    
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    // Safe scan info label
    safeInfoLabel.setText("Safe Scan: Plugins are scanned in a separate process — Colosseum cannot freeze or crash", 
                          juce::dontSendNotification);
    safeInfoLabel.setFont(juce::Font(11.0f));
    safeInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF50C878)); // Lime green
    safeInfoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(safeInfoLabel);
    
    // Buttons
    scanNewBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
    scanNewBtn.onClick = [this]() { scanForNewPlugins(false); };
    addAndMakeVisible(scanNewBtn);
    
    rescanAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4A3A20));
    rescanAllBtn.onClick = [this]() { scanForNewPlugins(true); };
    addAndMakeVisible(rescanAllBtn);
    
    removeMissingBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A3A3A));
    removeMissingBtn.onClick = [this]() { removeMissingPlugins(); };
    addAndMakeVisible(removeMissingBtn);
    
    removeAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF5A2020));
    removeAllBtn.onClick = [this]() { removeAllPlugins(); };
    addAndMakeVisible(removeAllBtn);
    
    cancelBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A3A3A));
    cancelBtn.onClick = [this]() {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->setVisible(false);
    };
    addAndMakeVisible(cancelBtn);
    
    updateFormatInfo();
    
    setSize(440, 340);
}

ScanOptionsPanel::~ScanOptionsPanel() {}

void ScanOptionsPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
}

void ScanOptionsPanel::resized() {
    auto area = getLocalBounds().reduced(20);
    
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);
    formatInfoLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(12);
    safeInfoLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(16);
    
    int btnHeight = 32;
    int btnSpacing = 8;
    
    auto btnArea = area.reduced(40, 0);
    scanNewBtn.setBounds(btnArea.removeFromTop(btnHeight));
    btnArea.removeFromTop(btnSpacing);
    rescanAllBtn.setBounds(btnArea.removeFromTop(btnHeight));
    btnArea.removeFromTop(btnSpacing);
    removeMissingBtn.setBounds(btnArea.removeFromTop(btnHeight));
    btnArea.removeFromTop(btnSpacing);
    removeAllBtn.setBounds(btnArea.removeFromTop(btnHeight));
    btnArea.removeFromTop(btnSpacing + 4);
    cancelBtn.setBounds(btnArea.removeFromTop(btnHeight));
}

void ScanOptionsPanel::updateFormatInfo() {
    juce::StringArray formats;
    
    #if JUCE_PLUGINHOST_VST3
    formats.add("VST3");
    #endif
    #if JUCE_PLUGINHOST_VST
    formats.add("VST2");
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formats.add("AU");
    #endif
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formats.add("LADSPA");
    #endif
    
    formatInfoLabel.setText("Formats: " + formats.joinIntoString(", "), juce::dontSendNotification);
    
    int numPlugins = processor.knownPluginList.getNumTypes();
    statusLabel.setText("Currently " + juce::String(numPlugins) + " plugins in list", juce::dontSendNotification);
}

void ScanOptionsPanel::scanForNewPlugins(bool clearList) {
    if (clearList) {
        // Confirm before clearing
        auto result = juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Clear Plugin List",
            "This will remove all plugins from the list and rescan everything.\n\nContinue?",
            "Yes", "No", nullptr, nullptr);
        
        if (!result) return; // User said No
        
        processor.knownPluginList.clear();
        
        if (auto* userSettings = processor.appProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml())
                userSettings->setValue("KnownPluginsV2", xml.get());
            userSettings->saveIfNeeded();
        }
    }
    
    launchScanner();
}

void ScanOptionsPanel::launchScanner() {
    // Close this dialog
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->setVisible(false);
    
    // Create the scanner dialog
    auto callback = onCompleteCallback;
    auto* scanner = new AutoPluginScanner(processor, [callback]() {
        if (callback) callback();
    });
    
    auto* dialog = new CloseableDialogWindow(
        "Safe Scanning Plugins...", 
        juce::Colour(0xFF1E1E1E),
        true
    );
    
    dialog->setContentOwned(scanner, true);
    dialog->setResizable(false, false);
    dialog->centreWithSize(500, 240);
    dialog->setVisible(true);
    
    scanner->startScan();
}

void ScanOptionsPanel::removeMissingPlugins() {
    int removedCount = 0;
    auto types = processor.knownPluginList.getTypes();
    
    for (const auto& plugin : types) {
        juce::File pluginFile(plugin.fileOrIdentifier);
        
        // Skip AU identifiers (they don't use regular file paths)
        if (plugin.pluginFormatName == "AudioUnit" && !pluginFile.exists()) {
            // AU identifiers start with AudioUnit: — don't check as files
            if (plugin.fileOrIdentifier.startsWith("AudioUnit:"))
                continue;
        }
        
        if (!pluginFile.exists()) {
            processor.knownPluginList.removeType(plugin);
            removedCount++;
        }
    }
    
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPluginsV2", xml.get());
        userSettings->saveIfNeeded();
    }
    
    processor.knownPluginList.sendChangeMessage();
    
    updateFormatInfo();
    
    juce::NativeMessageBox::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "Remove Missing",
        "Removed " + juce::String(removedCount) + " missing plugin(s).");
}

void ScanOptionsPanel::removeAllPlugins() {
    auto result = juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Remove All Plugins",
        "This will remove ALL plugins from the list.\n\nAre you sure?",
        "Yes", "No", nullptr, nullptr);
    
    if (!result) return;
    
    processor.knownPluginList.clear();
    
    // Also clear blacklist
    processor.knownPluginList.clearBlacklistedFiles();
    
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPluginsV2", xml.get());
        userSettings->saveIfNeeded();
    }
    
    processor.knownPluginList.sendChangeMessage();
    
    updateFormatInfo();
    
    juce::NativeMessageBox::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "Plugins Cleared",
        "All plugins have been removed from the list.\nUse 'Scan For New Plugins' to re-scan.");
}
