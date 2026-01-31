#include "PluginManagerTab.h"
#include "UIComponents.h"

// =============================================================================
// PluginFolderSection Implementation
// =============================================================================
PluginFolderSection::PluginFolderSection(const juce::String& formatName,
                                         const juce::String& settingsKey,
                                         juce::PropertiesFile* settings,
                                         std::function<void()> onChanged)
    : formatName(formatName), settingsKey(settingsKey), settings(settings), onChangedCallback(onChanged)
{
    headerLabel.setText(formatName, juce::dontSendNotification);
    headerLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);
    
    expandBtn.setButtonText("-");
    expandBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::grey.darker());
    expandBtn.onClick = [this]() { setExpanded(!expanded); };
    addAndMakeVisible(expandBtn);
    
    addBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::green.darker());
    addBtn.onClick = [this]() { browseForFolder(); };
    addAndMakeVisible(addBtn);
    
    loadFolders();
}

PluginFolderSection::~PluginFolderSection() { folderRows.clear(); }

void PluginFolderSection::paint(juce::Graphics& g) {
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.fillRect(0, 0, getWidth(), 28);
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(getLocalBounds(), 1);
}

void PluginFolderSection::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(28).reduced(4, 2);
    expandBtn.setBounds(header.removeFromLeft(24));
    header.removeFromLeft(4);
    headerLabel.setBounds(header.removeFromLeft(200));
    addBtn.setBounds(header.removeFromRight(80));
    
    if (expanded) {
        area.removeFromTop(4);
        for (auto* row : folderRows) {
            row->setBounds(area.removeFromTop(24).reduced(28, 0));
            area.removeFromTop(2);
        }
    }
}

int PluginFolderSection::getPreferredHeight() const {
    if (!expanded) return 28;
    return 28 + 4 + (folderRows.size() * 26) + 4;
}

void PluginFolderSection::setExpanded(bool exp) {
    expanded = exp;
    expandBtn.setButtonText(expanded ? "-" : "+");
    for (auto* row : folderRows) row->setVisible(expanded);
    if (auto* parent = getParentComponent()) parent->resized();
}

void PluginFolderSection::addFolder(const juce::String& path) {
    for (auto* row : folderRows)
        if (row->getPath() == path) return;
    
    auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
    folderRows.add(row);
    addAndMakeVisible(row);
    row->setVisible(expanded);
    saveFolders();
    if (auto* parent = getParentComponent()) parent->resized();
    if (onChangedCallback) onChangedCallback();
}

void PluginFolderSection::removeFolder(const juce::String& path) {
    for (int i = 0; i < folderRows.size(); ++i) {
        if (folderRows[i]->getPath() == path) {
            folderRows.remove(i);
            break;
        }
    }
    saveFolders();
    if (auto* parent = getParentComponent()) parent->resized();
    if (onChangedCallback) onChangedCallback();
}

juce::StringArray PluginFolderSection::getFolders() const {
    juce::StringArray folders;
    for (auto* row : folderRows) folders.add(row->getPath());
    return folders;
}

void PluginFolderSection::setFolders(const juce::StringArray& folders) {
    folderRows.clear();
    for (const auto& path : folders) {
        if (path.isNotEmpty()) {
            auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
            folderRows.add(row);
            addAndMakeVisible(row);
            row->setVisible(expanded);
        }
    }
    saveFolders();
    resized();
}

void PluginFolderSection::rebuildRows() {
    auto folders = getFolders();
    folderRows.clear();
    for (const auto& path : folders) {
        auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
        folderRows.add(row);
        addAndMakeVisible(row);
        row->setVisible(expanded);
    }
    resized();
}

void PluginFolderSection::saveFolders() {
    if (!settings) return;
    juce::StringArray folders;
    for (auto* row : folderRows) folders.add(row->getPath());
    settings->setValue(settingsKey, folders.joinIntoString("|"));
    settings->saveIfNeeded();
}

void PluginFolderSection::loadFolders() {
    if (!settings) return;
    juce::String savedPaths = settings->getValue(settingsKey, "");
    if (savedPaths.isEmpty()) return;
    juce::StringArray folders = juce::StringArray::fromTokens(savedPaths, "|", "");
    for (const auto& path : folders) {
        if (path.isNotEmpty()) {
            auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
            folderRows.add(row);
            addAndMakeVisible(row);
        }
    }
}

void PluginFolderSection::browseForFolder() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select " + formatName + " Plugin Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.exists() && result.isDirectory())
                addFolder(result.getFullPathName());
        });
}

// =============================================================================
// PluginFoldersPanel Implementation
// =============================================================================
PluginFoldersPanel::PluginFoldersPanel(SubterraneumAudioProcessor& p) : processor(p)
{
    auto* settings = processor.appProperties.getUserSettings();
    auto onChanged = [this]() { updateLayout(); applyToFormatManager(); };
    
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    collapseAllBtn.onClick = [this]() {
        if (vst3Section) vst3Section->setExpanded(false);
        if (auSection) auSection->setExpanded(false);
        if (clapSection) clapSection->setExpanded(false);
        if (ladspaSection) ladspaSection->setExpanded(false);
        updateLayout();
    };
    addAndMakeVisible(collapseAllBtn);
    
    addDefaultsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::blue.darker());
    addDefaultsBtn.onClick = [this]() { addDefaultPaths(); };
    addAndMakeVisible(addDefaultsBtn);
    
    #if JUCE_PLUGINHOST_VST3
    vst3Section = std::make_unique<PluginFolderSection>("VST3 Plugins", "VST3Folders", settings, onChanged);
    contentComponent.addAndMakeVisible(vst3Section.get());
    #endif
    
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    auSection = std::make_unique<PluginFolderSection>("AudioUnit Plugins", "AUFolders", settings, onChanged);
    contentComponent.addAndMakeVisible(auSection.get());
    #endif
    
    #if SUBTERRANEUM_CLAP_HOSTING
    clapSection = std::make_unique<PluginFolderSection>("CLAP Plugins", "CLAPFolders", settings, onChanged);
    contentComponent.addAndMakeVisible(clapSection.get());
    #endif
    
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    ladspaSection = std::make_unique<PluginFolderSection>("LADSPA Plugins", "LADSPAFolders", settings, onChanged);
    contentComponent.addAndMakeVisible(ladspaSection.get());
    #endif
    
    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    
    bool hasAnyPaths = false;
    if (vst3Section && !vst3Section->getFolders().isEmpty()) hasAnyPaths = true;
    if (auSection && !auSection->getFolders().isEmpty()) hasAnyPaths = true;
    if (clapSection && !clapSection->getFolders().isEmpty()) hasAnyPaths = true;
    if (ladspaSection && !ladspaSection->getFolders().isEmpty()) hasAnyPaths = true;
    
    if (!hasAnyPaths) addDefaultPaths();
    applyToFormatManager();
}

PluginFoldersPanel::~PluginFoldersPanel() {}

void PluginFoldersPanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xFF1E1E1E)); }

void PluginFoldersPanel::resized() {
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(30);
    titleLabel.setBounds(header.removeFromLeft(200));
    addDefaultsBtn.setBounds(header.removeFromRight(100));
    header.removeFromRight(10);
    collapseAllBtn.setBounds(header.removeFromRight(90));
    area.removeFromTop(10);
    viewport.setBounds(area);
    updateLayout();
}

void PluginFoldersPanel::updateLayout() {
    int y = 0;
    int width = viewport.getWidth() - 20;
    if (vst3Section) { vst3Section->setBounds(0, y, width, vst3Section->getPreferredHeight()); y += vst3Section->getPreferredHeight() + 8; }
    if (auSection) { auSection->setBounds(0, y, width, auSection->getPreferredHeight()); y += auSection->getPreferredHeight() + 8; }
    if (clapSection) { clapSection->setBounds(0, y, width, clapSection->getPreferredHeight()); y += clapSection->getPreferredHeight() + 8; }
    if (ladspaSection) { ladspaSection->setBounds(0, y, width, ladspaSection->getPreferredHeight()); y += ladspaSection->getPreferredHeight() + 8; }
    contentComponent.setSize(width, y);
}

int PluginFoldersPanel::getPreferredHeight() const {
    int height = 50;
    if (vst3Section) height += vst3Section->getPreferredHeight() + 8;
    if (auSection) height += auSection->getPreferredHeight() + 8;
    if (clapSection) height += clapSection->getPreferredHeight() + 8;
    if (ladspaSection) height += ladspaSection->getPreferredHeight() + 8;
    return height + 20;
}

juce::StringArray PluginFoldersPanel::getAllFolders() const {
    juce::StringArray all;
    if (vst3Section) all.addArray(vst3Section->getFolders());
    if (auSection) all.addArray(auSection->getFolders());
    if (clapSection) all.addArray(clapSection->getFolders());
    if (ladspaSection) all.addArray(ladspaSection->getFolders());
    return all;
}

void PluginFoldersPanel::applyToFormatManager() {
    if (vst3Section) {
        for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
            auto* format = processor.formatManager.getFormat(i);
            if (format->getName() == "VST3") {
                auto paths = vst3Section->getFolders();
                juce::FileSearchPath searchPath;
                for (const auto& p : paths) searchPath.add(juce::File(p));
                format->searchPathsForPlugins(searchPath, true, false);
            }
        }
    }
}

void PluginFoldersPanel::addDefaultPaths() {
    #if JUCE_WINDOWS
    if (vst3Section) {
        vst3Section->addFolder("C:\\Program Files\\Common Files\\VST3");
        vst3Section->addFolder("C:\\Program Files (x86)\\Common Files\\VST3");
    }
    if (clapSection) {
        clapSection->addFolder("C:\\Program Files\\Common Files\\CLAP");
    }
    #elif JUCE_MAC
    if (vst3Section) {
        vst3Section->addFolder("/Library/Audio/Plug-Ins/VST3");
        vst3Section->addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile("Library/Audio/Plug-Ins/VST3").getFullPathName());
    }
    if (auSection) {
        auSection->addFolder("/Library/Audio/Plug-Ins/Components");
        auSection->addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile("Library/Audio/Plug-Ins/Components").getFullPathName());
    }
    if (clapSection) {
        clapSection->addFolder("/Library/Audio/Plug-Ins/CLAP");
        clapSection->addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile("Library/Audio/Plug-Ins/CLAP").getFullPathName());
    }
    #elif JUCE_LINUX
    if (vst3Section) {
        vst3Section->addFolder("/usr/lib/vst3");
        vst3Section->addFolder("/usr/local/lib/vst3");
        vst3Section->addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(".vst3").getFullPathName());
    }
    if (clapSection) {
        clapSection->addFolder("/usr/lib/clap");
        clapSection->addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(".clap").getFullPathName());
    }
    if (ladspaSection) {
        ladspaSection->addFolder("/usr/lib/ladspa");
        ladspaSection->addFolder("/usr/local/lib/ladspa");
    }
    #endif
    updateLayout();
}

// =============================================================================
// AutoPluginScanner Implementation - Automatic background scanning
// =============================================================================
AutoPluginScanner::AutoPluginScanner(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    formatLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    formatLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    formatLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(formatLabel);
    
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    pluginLabel.setFont(juce::Font(11.0f));
    pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    pluginLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pluginLabel);
    
    addAndMakeVisible(progressBar);
    
    // Setup dead mans pedal
    juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Colosseum");
    if (!dataDir.exists()) dataDir.createDirectory();
    deadMansPedal = dataDir.getChildFile("PluginScanDeadMan.txt");
    
    // Build format list based on platform
    #if JUCE_PLUGINHOST_VST3
    formatNames.add("VST3");
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatNames.add("AudioUnit");
    #endif
    #if SUBTERRANEUM_CLAP_HOSTING
    formatNames.add("CLAP");
    #endif
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formatNames.add("LADSPA");
    #endif
    
    setSize(450, 200);
}

AutoPluginScanner::~AutoPluginScanner() {
    stopTimer();
    scanner = nullptr;
}

void AutoPluginScanner::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(getLocalBounds(), 1);
}

void AutoPluginScanner::resized() {
    auto area = getLocalBounds().reduced(20);
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    formatLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
    pluginLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(15);
    progressBar.setBounds(area.removeFromTop(20).reduced(20, 0));
}

void AutoPluginScanner::startScan() {
    currentFormatIndex = 0;
    totalPluginsFound = 0;
    progress = 0.0;
    
    juce::String platformInfo;
    #if JUCE_WINDOWS
        #if defined(_M_ARM64) || defined(__aarch64__)
        platformInfo = "Windows ARM64";
        #else
        platformInfo = "Windows x64";
        #endif
    #elif JUCE_MAC
        #if defined(__arm64__) || defined(__aarch64__)
        platformInfo = "macOS Apple Silicon";
        #else
        platformInfo = "macOS Intel";
        #endif
    #elif JUCE_LINUX
    platformInfo = "Linux";
    #endif
    
    titleLabel.setText("Scanning Plugins (" + platformInfo + ")", juce::dontSendNotification);
    statusLabel.setText("Formats: " + formatNames.joinIntoString(", "), juce::dontSendNotification);
    
    scanNextFormat();
}

juce::FileSearchPath AutoPluginScanner::getSearchPathForFormat(const juce::String& formatName) {
    juce::FileSearchPath path;
    
    #if JUCE_WINDOWS
    if (formatName == "VST3") {
        path.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
        path.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));
    }
    else if (formatName == "CLAP") {
        path.add(juce::File("C:\\Program Files\\Common Files\\CLAP"));
    }
    #elif JUCE_MAC
    if (formatName == "VST3") {
        path.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
        path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/VST3"));
    }
    else if (formatName == "AudioUnit") {
        path.add(juce::File("/Library/Audio/Plug-Ins/Components"));
        path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/Components"));
    }
    else if (formatName == "CLAP") {
        path.add(juce::File("/Library/Audio/Plug-Ins/CLAP"));
        path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/CLAP"));
    }
    #elif JUCE_LINUX
    if (formatName == "VST3") {
        path.add(juce::File("/usr/lib/vst3"));
        path.add(juce::File("/usr/local/lib/vst3"));
        path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3"));
    }
    else if (formatName == "CLAP") {
        path.add(juce::File("/usr/lib/clap"));
        path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".clap"));
    }
    else if (formatName == "LADSPA") {
        path.add(juce::File("/usr/lib/ladspa"));
        path.add(juce::File("/usr/local/lib/ladspa"));
    }
    #endif
    
    return path;
}

void AutoPluginScanner::scanNextFormat() {
    scanner = nullptr;
    
    if (currentFormatIndex >= formatNames.size()) {
        finishScan();
        return;
    }
    
    juce::String formatName = formatNames[currentFormatIndex];
    formatLabel.setText("Scanning: " + formatName, juce::dontSendNotification);
    
    // Find the format
    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
        if (processor.formatManager.getFormat(i)->getName() == formatName) {
            format = processor.formatManager.getFormat(i);
            break;
        }
    }
    
    if (format == nullptr) {
        currentFormatIndex++;
        scanNextFormat();
        return;
    }
    
    juce::FileSearchPath searchPath = getSearchPathForFormat(formatName);
    
    scanner = std::make_unique<juce::PluginDirectoryScanner>(
        processor.knownPluginList,
        *format,
        searchPath,
        true,  // recursive
        deadMansPedal,
        false  // don't allow plugins requiring async instantiation
    );
    
    startTimer(1);  // Fast timer for responsive scanning
}

void AutoPluginScanner::timerCallback() {
    if (!scanner) {
        stopTimer();
        return;
    }
    
    juce::String pluginBeingScanned;
    
    if (scanner->scanNextFile(true, pluginBeingScanned)) {
        // Still scanning
        pluginLabel.setText(pluginBeingScanned, juce::dontSendNotification);
        
        float formatProgress = scanner->getProgress();
        float overallProgress = ((float)currentFormatIndex + formatProgress) / (float)formatNames.size();
        progress = overallProgress;
        
        statusLabel.setText("Found " + juce::String(processor.knownPluginList.getNumTypes()) + " plugins", 
                           juce::dontSendNotification);
    } else {
        // This format is done
        stopTimer();
        currentFormatIndex++;
        
        // Small delay before next format
        juce::Timer::callAfterDelay(100, [this]() {
            scanNextFormat();
        });
    }
}

void AutoPluginScanner::finishScan() {
    progress = 1.0;
    totalPluginsFound = processor.knownPluginList.getNumTypes();
    
    formatLabel.setText("Scan Complete!", juce::dontSendNotification);
    statusLabel.setText("Found " + juce::String(totalPluginsFound) + " plugins", juce::dontSendNotification);
    pluginLabel.setText("", juce::dontSendNotification);
    
    // Save the plugin list
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPlugins", xml.get());
        userSettings->saveIfNeeded();
    }
    
    // Callback after short delay to show completion
    juce::Timer::callAfterDelay(1000, [this]() {
        if (onCompleteCallback) onCompleteCallback();
    });
}

// =============================================================================
// ScanOptionsPanel Implementation
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
    
    cancelBtn.onClick = [this]() { if (onCompleteCallback) onCompleteCallback(); };
    addAndMakeVisible(cancelBtn);
    
    statusLabel.setFont(juce::Font(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    setSize(400, 320);
}

ScanOptionsPanel::~ScanOptionsPanel() {}

void ScanOptionsPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(10, 80, getWidth() - 20, 180, 1);
}

void ScanOptionsPanel::resized() {
    auto area = getLocalBounds().reduced(15);
    titleLabel.setBounds(area.removeFromTop(30));
    formatInfoLabel.setBounds(area.removeFromTop(25));
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
    
    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
    cancelBtn.setBounds(area.removeFromBottom(28).reduced(120, 0));
}

void ScanOptionsPanel::updateFormatInfo() {
    juce::StringArray formats;
    #if JUCE_PLUGINHOST_VST3
    formats.add("VST3");
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formats.add("AudioUnit");
    #endif
    #if SUBTERRANEUM_CLAP_HOSTING
    formats.add("CLAP");
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

void ScanOptionsPanel::scanForNewPlugins(bool clearList) {
    if (clearList) {
        int result = juce::NativeMessageBox::showYesNoBox(
            juce::MessageBoxIconType::QuestionIcon,
            "Rescan All Plugins",
            "This will clear the current plugin list and rescan.\nContinue?");
        if (result != 1) return;
        processor.knownPluginList.clear();
    }
    
    // Close this panel and open auto scanner
    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>()) {
        auto callback = onCompleteCallback;
        auto& proc = processor;
        
        parent->setVisible(false);
        
        juce::MessageManager::callAsync([&proc, callback]() {
            auto* scanner = new AutoPluginScanner(proc, callback);
            
            juce::DialogWindow::LaunchOptions opts;
            opts.dialogTitle = "Scanning Plugins";
            opts.dialogBackgroundColour = juce::Colour(0xFF1E1E1E);
            opts.content.setOwned(scanner);
            opts.escapeKeyTriggersCloseButton = false;
            opts.useNativeTitleBar = true;
            opts.resizable = false;
            
            auto* dialog = opts.launchAsync();
            scanner->startScan();
        });
    }
}

void ScanOptionsPanel::removeMissingPlugins() {
    int removed = 0;
    auto types = processor.knownPluginList.getTypes();
    
    for (int i = types.size() - 1; i >= 0; --i) {
        juce::File pluginFile(types[i].fileOrIdentifier);
        if (!pluginFile.exists()) {
            processor.knownPluginList.removeType(types[i]);
            removed++;
        }
    }
    
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPlugins", xml.get());
        userSettings->saveIfNeeded();
    }
    
    statusLabel.setText("Removed " + juce::String(removed) + " missing plugin(s)", juce::dontSendNotification);
}

void ScanOptionsPanel::removeAllPlugins() {
    int result = juce::NativeMessageBox::showYesNoBox(
        juce::MessageBoxIconType::WarningIcon,
        "Remove All Plugins",
        "This will remove all plugins from the list.\nContinue?");
    
    if (result == 1) {
        processor.knownPluginList.clear();
        if (auto* userSettings = processor.appProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml())
                userSettings->setValue("KnownPlugins", xml.get());
            userSettings->saveIfNeeded();
        }
        statusLabel.setText("All plugins removed", juce::dontSendNotification);
    }
}

// =============================================================================
// PluginManagerTab Implementation
// =============================================================================
PluginManagerTab::PluginManagerTab(SubterraneumAudioProcessor& p) : processor(p) {
    addAndMakeVisible(sortLabel);
    addAndMakeVisible(sortCombo);
    sortCombo.addItem("Type", 1);
    sortCombo.addItem("Vendor", 2);
    sortCombo.setSelectedId(processor.sortPluginsByVendor ? 2 : 1, juce::dontSendNotification);
    sortCombo.addListener(this);

    instBtn.setRadioGroupId(101);
    effectBtn.setRadioGroupId(101);
    allBtn.setRadioGroupId(101);
    allBtn.setToggleState(true, juce::dontSendNotification);
    instBtn.addListener(this);
    effectBtn.addListener(this);
    allBtn.addListener(this);
    addAndMakeVisible(instBtn);
    addAndMakeVisible(effectBtn);
    addAndMakeVisible(allBtn);
    
    expandAllBtn.setTooltip("Expand All");
    expandAllBtn.addListener(this);
    addAndMakeVisible(expandAllBtn);
    
    collapseAllBtn.setTooltip("Collapse All");
    collapseAllBtn.addListener(this);
    addAndMakeVisible(collapseAllBtn);

    pluginTree.setLookAndFeel(&treeLAF);
    pluginTree.setDefaultOpenness(false);
    pluginTree.setMultiSelectEnabled(false);
    pluginTree.setRootItemVisible(false);
    addAndMakeVisible(pluginTree);
    
    addAndMakeVisible(tableHeader);
    tableHeader.addColumn("Plugin Name", 1, 250);
    tableHeader.addColumn("Vendor", 2, 150);
    tableHeader.addColumn("Format", 3, 80);
    tableHeader.addColumn("Category", 4, 100);
    tableHeader.addColumn("Version", 5, 80);
    tableHeader.setVisible(!processor.sortPluginsByVendor);

    addAndMakeVisible(resetBlacklistBtn);
    resetBlacklistBtn.addListener(this);
    
    addAndMakeVisible(scanBtn); 
    scanBtn.addListener(this);
    scanBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::green.darker());
    
    addAndMakeVisible(foldersBtn);
    foldersBtn.addListener(this);
    foldersBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());

    processor.knownPluginList.addChangeListener(this);

    buildTree();
    checkForCrashedScan();
    
    // Auto-scan on first run if no plugins found
    if (processor.knownPluginList.getNumTypes() == 0) {
        startTimer(500);  // Small delay to let UI settle
    }
}

PluginManagerTab::~PluginManagerTab() {
    stopTimer();
    processor.knownPluginList.removeChangeListener(this);
    pluginTree.setLookAndFeel(nullptr);
    pluginTree.setRootItem(nullptr);
    scanDialog = nullptr;
    foldersDialog = nullptr;
}

void PluginManagerTab::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &processor.knownPluginList) {
        juce::MessageManager::callAsync([this]() { buildTree(); });
    }
}

void PluginManagerTab::resized() {
    auto area = getLocalBounds().reduced(5);
    auto header = area.removeFromTop(30);
    sortLabel.setBounds(header.removeFromLeft(60));
    sortCombo.setBounds(header.removeFromLeft(150)); 
    header.removeFromLeft(20); 
    instBtn.setBounds(header.removeFromLeft(100));
    effectBtn.setBounds(header.removeFromLeft(80));
    allBtn.setBounds(header.removeFromLeft(60));
    header.removeFromLeft(10);
    expandAllBtn.setBounds(header.removeFromLeft(28));
    header.removeFromLeft(4);
    collapseAllBtn.setBounds(header.removeFromLeft(28));
    
    area.removeFromTop(5);
    auto footer = area.removeFromBottom(30);
    scanBtn.setBounds(footer.removeFromLeft(110));
    footer.removeFromLeft(8);
    foldersBtn.setBounds(footer.removeFromLeft(110));
    footer.removeFromLeft(8);
    resetBlacklistBtn.setBounds(footer.removeFromLeft(110));
    
    area.removeFromBottom(5);
    if (tableHeader.isVisible()) tableHeader.setBounds(area.removeFromTop(24));
    pluginTree.setBounds(area);
}

void PluginManagerTab::paint(juce::Graphics& g) { g.fillAll(Style::colBackground); }

void PluginManagerTab::comboBoxChanged(juce::ComboBox* cb) {
    if (cb == &sortCombo) {
        processor.sortPluginsByVendor = (sortCombo.getSelectedId() == 2);
        tableHeader.setVisible(!processor.sortPluginsByVendor);
        resized();
        buildTree();
    }
}

void PluginManagerTab::buttonClicked(juce::Button* b) {
    if (b == &instBtn || b == &effectBtn || b == &allBtn) buildTree();
    else if (b == &resetBlacklistBtn) {
        processor.resetBlacklist();
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, 
            "Blacklist Reset", "Blacklisted plugins cleared. Please rescan.");
    }
    else if (b == &scanBtn) showScanDialog();
    else if (b == &foldersBtn) showFoldersDialog();
    else if (b == &expandAllBtn) expandAllItems();
    else if (b == &collapseAllBtn) collapseAllItems();
}

void PluginManagerTab::expandAllItems() {
    if (auto* root = pluginTree.getRootItem()) {
        std::function<void(juce::TreeViewItem*)> expandRecursive = [&](juce::TreeViewItem* item) {
            if (item) {
                item->setOpen(true);
                for (int i = 0; i < item->getNumSubItems(); ++i)
                    expandRecursive(item->getSubItem(i));
            }
        };
        expandRecursive(root);
    }
}

void PluginManagerTab::collapseAllItems() {
    if (auto* root = pluginTree.getRootItem()) {
        std::function<void(juce::TreeViewItem*)> collapseRecursive = [&](juce::TreeViewItem* item) {
            if (item) {
                for (int i = 0; i < item->getNumSubItems(); ++i)
                    collapseRecursive(item->getSubItem(i));
                item->setOpen(false);
            }
        };
        for (int i = 0; i < root->getNumSubItems(); ++i)
            collapseRecursive(root->getSubItem(i));
    }
}

void PluginManagerTab::timerCallback() {
    stopTimer();
    // No plugins found - start automatic scan
    if (processor.knownPluginList.getNumTypes() == 0) {
        startAutoScan();
    }
}

void PluginManagerTab::startAutoScan() {
    disableAudioForScan();
    
    auto* scanner = new AutoPluginScanner(processor, [this]() {
        restoreAudioAfterScan();
        juce::MessageManager::callAsync([this]() {
            scanDialog = nullptr;
            buildTree();
        });
    });
    
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Scanning Plugins";
    options.dialogBackgroundColour = juce::Colour(0xFF1E1E1E);
    options.content.setOwned(scanner);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = true;
    options.resizable = false;
    
    scanDialog.reset(options.launchAsync());
    scanner->startScan();
}

void PluginManagerTab::showFoldersDialog() {
    auto* panel = new PluginFoldersPanel(processor);
    panel->setSize(550, 400);
    
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Plugin Search Folders";
    options.dialogBackgroundColour = juce::Colour(0xFF1E1E1E);
    options.content.setOwned(panel);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    
    foldersDialog.reset(options.launchAsync());
}

void PluginManagerTab::showScanDialog() {
    disableAudioForScan();
    
    auto* panel = new ScanOptionsPanel(processor, [this]() {
        restoreAudioAfterScan();
        juce::MessageManager::callAsync([this]() {
            scanDialog = nullptr;
            buildTree();
        });
    });
    
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Plugin Scan";
    options.dialogBackgroundColour = juce::Colour(0xFF1E1E1E);
    options.content.setOwned(panel);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    
    scanDialog.reset(options.launchAsync());
}

void PluginManagerTab::buildTree() {
    pluginTree.setRootItem(nullptr);
    auto types = processor.knownPluginList.getTypes();
    
    if (types.isEmpty()) {
        auto root = std::make_unique<PluginTreeItem>("No plugins found - click 'Scan Plugins'", processor.sortPluginsByVendor);
        pluginTree.setRootItem(root.release());
        return;
    }

    bool showInstruments = instBtn.getToggleState();
    bool showEffects = effectBtn.getToggleState();
    bool showAll = allBtn.getToggleState();

    auto root = std::make_unique<PluginTreeItem>("Plugins", processor.sortPluginsByVendor);

    if (processor.sortPluginsByVendor) {
        std::map<juce::String, std::vector<juce::PluginDescription>> byVendor;
        for (auto& t : types) {
            if (showAll || (showInstruments && t.isInstrument) || (showEffects && !t.isInstrument))
                byVendor[t.manufacturerName].push_back(t);
        }
        for (auto& [vendor, plugins] : byVendor) {
            auto vendorNode = std::make_unique<PluginTreeItem>(
                vendor.isEmpty() ? "Unknown Vendor" : vendor, processor.sortPluginsByVendor, true);
            vendorNode->setOpen(false);
            for (auto& p : plugins)
                vendorNode->addSubItem(new PluginTreeItem(p.name, p, processor.sortPluginsByVendor));
            root->addSubItem(vendorNode.release());
        }
    } else {
        std::vector<juce::PluginDescription> instruments, effects;
        for (auto& t : types) {
            if (t.isInstrument) instruments.push_back(t);
            else effects.push_back(t);
        }
        
        if ((showAll || showInstruments) && !instruments.empty()) {
            auto instNode = std::make_unique<PluginTreeItem>("Instruments", processor.sortPluginsByVendor);
            for (auto& p : instruments)
                instNode->addSubItem(new PluginTreeItem(p.name, p, processor.sortPluginsByVendor));
            root->addSubItem(instNode.release());
        }
        if ((showAll || showEffects) && !effects.empty()) {
            auto fxNode = std::make_unique<PluginTreeItem>("Effects", processor.sortPluginsByVendor);
            for (auto& p : effects)
                fxNode->addSubItem(new PluginTreeItem(p.name, p, processor.sortPluginsByVendor));
            root->addSubItem(fxNode.release());
        }
    }

    pluginTree.setRootItem(root.release());
}

void PluginManagerTab::disableAudioForScan() {
    if (SubterraneumAudioProcessor::standaloneDeviceManager) {
        if (auto* device = SubterraneumAudioProcessor::standaloneDeviceManager->getCurrentAudioDevice()) {
            savedDeviceName = device->getName();
            wasDeviceOpen = true;
        } else {
            wasDeviceOpen = false;
        }
        SubterraneumAudioProcessor::standaloneDeviceManager->closeAudioDevice();
    }
}

void PluginManagerTab::restoreAudioAfterScan() {
    if (SubterraneumAudioProcessor::standaloneDeviceManager && wasDeviceOpen && savedDeviceName.isNotEmpty()) {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = savedDeviceName;
        setup.outputDeviceName = savedDeviceName;
        juce::BigInteger allChannels;
        allChannels.setRange(0, 256, true);
        setup.inputChannels = allChannels;
        setup.outputChannels = allChannels;
        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = false;
        SubterraneumAudioProcessor::standaloneDeviceManager->setAudioDeviceSetup(setup, true);
    }
    wasDeviceOpen = false;
    savedDeviceName = "";
}

void PluginManagerTab::checkForCrashedScan() {
    juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Colosseum");
    juce::File deadMansPedal = dataDir.getChildFile("PluginScanDeadMan.txt");
    
    if (deadMansPedal.existsAsFile()) {
        juce::String crashedPlugin = deadMansPedal.loadFileAsString().trim();
        if (crashedPlugin.isNotEmpty()) {
            juce::MessageManager::callAsync([this, crashedPlugin, deadMansPedal]() {
                int result = juce::NativeMessageBox::showYesNoCancelBox(
                    juce::MessageBoxIconType::WarningIcon,
                    "Plugin Scan Crash Detected",
                    "The app crashed while scanning:\n\n" + crashedPlugin + 
                    "\n\nBlacklist this plugin?\n\nYes = Blacklist\nNo = Try again\nCancel = Skip",
                    nullptr, nullptr);
                
                if (result == 1) {
                    processor.knownPluginList.addToBlacklist(crashedPlugin);
                    deadMansPedal.deleteFile();
                    if (auto* userSettings = processor.appProperties.getUserSettings()) {
                        if (auto xml = processor.knownPluginList.createXml())
                            userSettings->setValue("KnownPlugins", xml.get());
                        userSettings->saveIfNeeded();
                    }
                } else if (result == 2) {
                    deadMansPedal.deleteFile();
                }
            });
        }
    }
}

// =============================================================================
// PluginTreeItem Implementation
// =============================================================================
void PluginManagerTab::PluginTreeItem::paintItem(juce::Graphics& g, int w, int h) {
    if (!isPlugin) { 
        g.setColour(juce::Colours::grey);
        g.drawRect(0, 0, w, h, 1); 
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(name, 25, 0, w-25, h, juce::Justification::centredLeft);
    } else { 
        g.setColour(isSelected() ? juce::Colours::blue.withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillRect(0, 0, w, h);
        g.setColour(juce::Colours::lightgrey);
        g.setFont(14.0f);

        if (showColumns) {
            const int COL_NAME = 250, COL_VENDOR = 150, COL_FORMAT = 80, COL_CAT = 100, COL_VER = 80;
            int x = 0;
            g.drawText(name, x + 5, 0, COL_NAME - 5, h, juce::Justification::centredLeft);
            x += COL_NAME;
            g.drawText(description.manufacturerName, x + 5, 0, COL_VENDOR - 5, h, juce::Justification::centredLeft);
            x += COL_VENDOR;
            g.drawText(description.pluginFormatName, x + 5, 0, COL_FORMAT - 5, h, juce::Justification::centredLeft);
            x += COL_FORMAT;
            g.drawText(description.category, x + 5, 0, COL_CAT - 5, h, juce::Justification::centredLeft);
            x += COL_VER;
            g.drawText(description.version, x + 5, 0, COL_VER - 5, h, juce::Justification::centredLeft);
            
            g.setColour(juce::Colours::grey.withAlpha(0.3f));
            g.drawVerticalLine(COL_NAME, 0, (float)h);
            g.drawVerticalLine(COL_NAME + COL_VENDOR, 0, (float)h);
            g.drawVerticalLine(COL_NAME + COL_VENDOR + COL_FORMAT, 0, (float)h);
            g.drawVerticalLine(COL_NAME + COL_VENDOR + COL_FORMAT + COL_CAT, 0, (float)h);
        } else {
            g.drawText(name, 5, 0, w-5, h, juce::Justification::centredLeft);
        }
    }
}

void PluginManagerTab::PluginTreeItem::itemClicked(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    if (!isPlugin) setOpen(!isOpen());
}

juce::var PluginManagerTab::PluginTreeItem::getDragSourceDescription() {
    if (isPlugin) return description.fileOrIdentifier;
    return {};
}