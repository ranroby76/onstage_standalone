// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_AutoScanner.cpp
// FIXED: Always use proper JUCE PluginDirectoryScanner for correct metadata
// Trust All mode now just skips problematic vendor checks instead of broken enumeration

#include "PluginManagerTab.h"
#include "BackgroundPluginScanner.h"

// =============================================================================
// AutoPluginScanner Implementation
// =============================================================================
AutoPluginScanner::AutoPluginScanner(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    formatLabel.setFont(juce::Font(13.0f));
    formatLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
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
    #if JUCE_PLUGINHOST_VST
    formatNames.add("VST");
    #endif
    #if JUCE_PLUGINHOST_VST3
    formatNames.add("VST3");
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatNames.add("AudioUnit");
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
    
    // Clear dead man's pedal at start
    if (deadMansPedal.existsAsFile()) {
        deadMansPedal.deleteFile();
    }
    
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
    
    // FIXED: Always use proper JUCE scanning - Trust All just skips vendor checks
    titleLabel.setText("Scanning Plugins (" + platformInfo + ")", juce::dontSendNotification);
    statusLabel.setText("Formats: " + formatNames.joinIntoString(", "), juce::dontSendNotification);
    
    if (!trustAllMode) {
        loadProblematicVendorsList();
    }
    
    scanNextFormat();
}

// =============================================================================
// Scan next format using JUCE's proper scanner
// =============================================================================
void AutoPluginScanner::scanNextFormat() {
    scanner = nullptr;
    
    if (currentFormatIndex >= formatNames.size()) {
        finishScan();
        return;
    }
    
    juce::String formatName = formatNames[currentFormatIndex];
    
    if (formatName == "VST") {
        formatLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF4A90D9));
        formatLabel.setText("Scanning: VST2", juce::dontSendNotification);
    } else if (formatName == "VST3") {
        formatLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF50C878));
        formatLabel.setText("Scanning: VST3", juce::dontSendNotification);
    } else {
        formatLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
        formatLabel.setText("Scanning: " + formatName, juce::dontSendNotification);
    }
    
    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
        if (processor.formatManager.getFormat(i)->getName() == formatName) {
            format = processor.formatManager.getFormat(i);
            break;
        }
    }
    
    if (format == nullptr) {
        currentFormatIndex++;
        juce::Component::SafePointer<AutoPluginScanner> safeThis(this);
        juce::MessageManager::callAsync([safeThis]() {
            if (safeThis != nullptr) safeThis->scanNextFormat();
        });
        return;
    }
    
    juce::FileSearchPath searchPath = getSearchPathForFormat(formatName);
    
    scanner = std::make_unique<juce::PluginDirectoryScanner>(
        processor.knownPluginList,
        *format,
        searchPath,
        true,
        deadMansPedal,
        false
    );
    
    startTimer(16);
}

// =============================================================================
// Timer callback - scan plugins one by one
// =============================================================================
void AutoPluginScanner::timerCallback() {
    if (!scanner) {
        stopTimer();
        return;
    }
    
    juce::String pluginBeingScanned;
    
    // Check for plugins to skip BEFORE scanning (only in non-Trust-All mode)
    juce::String nextPlugin = scanner->getNextPluginFileThatWillBeScanned();
    bool shouldSkip = false;
    juce::String skipReason;
    
    if (!trustAllMode) {
        // Check for VST2 shell plugins
        #if JUCE_PLUGINHOST_VST
        if (skipVST2Shells && formatNames[currentFormatIndex] == "VST") {
            if (isShellPlugin(nextPlugin)) {
                shouldSkip = true;
                skipReason = "shell plugin";
            }
        }
        #endif
        
        // Check for problematic plugins
        if (!shouldSkip && skipProblematicPlugins) {
            if (isProblematicPlugin(nextPlugin)) {
                shouldSkip = true;
                skipReason = "problematic vendor";
            }
        }
    }
    
    if (shouldSkip) {
        juce::String filename = juce::File(nextPlugin).getFileName();
        pluginLabel.setText("Skipping (" + skipReason + "): " + filename, juce::dontSendNotification);
        scanner->skipNextFile();
        
        float formatProgress = scanner->getProgress();
        float overallProgress = ((float)currentFormatIndex + formatProgress) / (float)formatNames.size();
        progress = overallProgress;
        return;
    }
    
    // Normal scanning - JUCE properly extracts all plugin metadata
    if (scanner->scanNextFile(true, pluginBeingScanned)) {
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
        
        juce::Component::SafePointer<AutoPluginScanner> safeThis(this);
        juce::Timer::callAfterDelay(50, [safeThis]() {
            if (safeThis != nullptr) {
                safeThis->scanNextFormat();
            }
        });
    }
}

// =============================================================================
// Get Search Path For Format
// =============================================================================
juce::FileSearchPath AutoPluginScanner::getSearchPathForFormat(const juce::String& formatName) {
    juce::FileSearchPath path;
    
    auto* settings = processor.appProperties.getUserSettings();
    
    #if JUCE_WINDOWS
    if (formatName == "VST") {
        if (settings) {
            juce::String savedPaths = settings->getValue("VST2Folders", "");
            if (savedPaths.isNotEmpty()) {
                auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
                for (const auto& folder : folders) {
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
                }
            }
        }
        
        if (path.getNumPaths() == 0) {
            path.add(juce::File("C:\\Program Files\\VSTPlugins"));
            path.add(juce::File("C:\\Program Files\\Steinberg\\VSTPlugins"));
            path.add(juce::File("C:\\Program Files\\Common Files\\VST2"));
            path.add(juce::File("C:\\Program Files (x86)\\VSTPlugins"));
            path.add(juce::File("C:\\Program Files (x86)\\Steinberg\\VSTPlugins"));
            path.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST2"));
            path.add(juce::File("C:\\Program Files\\Waves\\Plug-Ins V12"));
            path.add(juce::File("C:\\Program Files\\Waves\\Plug-Ins V13"));
            path.add(juce::File("C:\\Program Files\\Waves\\Plug-Ins V14"));
            path.add(juce::File("C:\\Program Files\\Waves\\Plug-Ins V15"));
        }
    }
    else if (formatName == "VST3") {
        if (settings) {
            juce::String savedPaths = settings->getValue("VST3Folders", "");
            if (savedPaths.isNotEmpty()) {
                auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
                for (const auto& folder : folders) {
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
                }
            }
        }
        
        if (path.getNumPaths() == 0) {
            path.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
            path.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));
        }
    }
    
    #elif JUCE_MAC
    if (formatName == "VST") {
        if (settings) {
            juce::String savedPaths = settings->getValue("VST2Folders", "");
            if (savedPaths.isNotEmpty()) {
                auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
                for (const auto& folder : folders) {
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
                }
            }
        }
        
        if (path.getNumPaths() == 0) {
            path.add(juce::File("/Library/Audio/Plug-Ins/VST"));
            path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("Library/Audio/Plug-Ins/VST"));
        }
    }
    else if (formatName == "VST3") {
        if (settings) {
            juce::String savedPaths = settings->getValue("VST3Folders", "");
            if (savedPaths.isNotEmpty()) {
                auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
                for (const auto& folder : folders) {
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
                }
            }
        }
        
        if (path.getNumPaths() == 0) {
            path.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
            path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("Library/Audio/Plug-Ins/VST3"));
        }
    }
    else if (formatName == "AudioUnit") {
        path.add(juce::File("/Library/Audio/Plug-Ins/Components"));
        path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile("Library/Audio/Plug-Ins/Components"));
    }
    
    #elif JUCE_LINUX
    if (formatName == "VST") {
        if (settings) {
            juce::String savedPaths = settings->getValue("VST2Folders", "");
            if (savedPaths.isNotEmpty()) {
                auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
                for (const auto& folder : folders) {
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
                }
            }
        }
        
        if (path.getNumPaths() == 0) {
            path.add(juce::File("/usr/lib/vst"));
            path.add(juce::File("/usr/local/lib/vst"));
            path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst"));
        }
    }
    else if (formatName == "VST3") {
        if (settings) {
            juce::String savedPaths = settings->getValue("VST3Folders", "");
            if (savedPaths.isNotEmpty()) {
                auto folders = juce::StringArray::fromTokens(savedPaths, "|", "");
                for (const auto& folder : folders) {
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
                }
            }
        }
        
        if (path.getNumPaths() == 0) {
            path.add(juce::File("/usr/lib/vst3"));
            path.add(juce::File("/usr/local/lib/vst3"));
            path.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3"));
        }
    }
    else if (formatName == "LADSPA") {
        path.add(juce::File("/usr/lib/ladspa"));
        path.add(juce::File("/usr/local/lib/ladspa"));
    }
    #endif
    
    return path;
}

// =============================================================================
// Shell plugin detection
// =============================================================================
bool AutoPluginScanner::isShellPlugin(const juce::String& pluginPath) {
    juce::String pathLower = pluginPath.toLowerCase();
    
    if (pathLower.contains("waveshell") ||
        (pathLower.contains("waves") && pathLower.contains("shell")) ||
        pathLower.contains("uad-2") ||
        pathLower.contains("uad shell")) {
        return true;
    }
    
    return false;
}

// =============================================================================
// Problematic vendor detection
// =============================================================================
void AutoPluginScanner::loadProblematicVendorsList() {
    problematicVendors.clear();
    
    auto* settings = processor.appProperties.getUserSettings();
    if (!settings) {
        problematicVendors.add("ujam");
        problematicVendors.add("arturia");
        problematicVendors.add("east west");
        problematicVendors.add("best service");
        problematicVendors.add("output");
        problematicVendors.add("spitfire audio");
        return;
    }
    
    juce::String defaultList = "UJAM\nArturia\nEast West\nBest Service\nOutput\nSpitfire Audio";
    juce::String vendorList = settings->getValue("ProblematicVendorsList", defaultList);
    
    juce::StringArray lines = juce::StringArray::fromLines(vendorList);
    for (const auto& line : lines) {
        juce::String trimmed = line.trim().toLowerCase();
        if (trimmed.isNotEmpty()) {
            problematicVendors.add(trimmed);
        }
    }
}

bool AutoPluginScanner::isProblematicPlugin(const juce::String& pluginPath) {
    juce::String pathLower = pluginPath.toLowerCase();
    
    // Check approved list
    auto* settings = processor.appProperties.getUserSettings();
    if (settings) {
        juce::String approvedPlugins = settings->getValue("ApprovedPlugins", "");
        if (approvedPlugins.isNotEmpty()) {
            juce::StringArray approved = juce::StringArray::fromTokens(approvedPlugins, "|", "");
            for (const auto& approvedPath : approved) {
                if (approvedPath.equalsIgnoreCase(pluginPath)) {
                    return false;
                }
            }
        }
    }
    
    for (const auto& vendor : problematicVendors) {
        if (pathLower.contains(vendor)) {
            return true;
        }
    }
    
    return false;
}

// =============================================================================
// Finish scan
// =============================================================================
void AutoPluginScanner::finishScan() {
    progress = 1.0;
    
    removeDuplicatePlugins();
    
    totalPluginsFound = processor.knownPluginList.getNumTypes();
    
    // Save the plugin list
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPlugins", xml.get());
        userSettings->saveIfNeeded();
    }
    
    // Delete dead man's pedal
    if (deadMansPedal.existsAsFile()) {
        deadMansPedal.deleteFile();
    }
    
    processor.knownPluginList.sendChangeMessage();
    
    formatLabel.setText("Scan Complete!", juce::dontSendNotification);
    statusLabel.setText("Found " + juce::String(totalPluginsFound) + " plugins", juce::dontSendNotification);
    pluginLabel.setText("", juce::dontSendNotification);
    
    if (onCompleteCallback) {
        auto callback = onCompleteCallback;
        juce::MessageManager::callAsync([callback]() {
            if (callback) callback();
        });
    }
}

// =============================================================================
// Handle background scanner progress updates
// =============================================================================
void AutoPluginScanner::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == backgroundScanner.get()) {
        float bgProgress = backgroundScanner->getProgress();
        int scanned = backgroundScanner->getScannedCount();
        int total = backgroundScanner->getTotalCount();
        juce::String current = backgroundScanner->getCurrentPlugin();
        
        progress = bgProgress;
        
        if (bgProgress >= 1.0f || (scanned >= total && total > 0)) {
            formatLabel.setText("Scan Complete!", juce::dontSendNotification);
            statusLabel.setText("Found " + juce::String(total) + " plugins with metadata", juce::dontSendNotification);
            pluginLabel.setText("", juce::dontSendNotification);
            progress = 1.0;
            
            if (auto* userSettings = processor.appProperties.getUserSettings()) {
                if (auto xml = processor.knownPluginList.createXml())
                    userSettings->setValue("KnownPlugins", xml.get());
                userSettings->saveIfNeeded();
            }
            
            processor.knownPluginList.sendChangeMessage();
            
            if (onCompleteCallback) {
                auto callback = onCompleteCallback;
                juce::MessageManager::callAsync([callback]() {
                    if (callback) callback();
                });
            }
        } else {
            formatLabel.setText("Gathering Metadata: " + juce::String((int)(bgProgress * 100)) + "%", juce::dontSendNotification);
            statusLabel.setText(juce::String(scanned) + " / " + juce::String(total), juce::dontSendNotification);
            
            if (current.isNotEmpty()) {
                if (current.length() > 40)
                    current = current.substring(0, 37) + "...";
                pluginLabel.setText(current, juce::dontSendNotification);
            }
        }
        
        repaint();
    }
}

void AutoPluginScanner::removeDuplicatePlugins() {
    auto types = processor.knownPluginList.getTypes();
    
    std::set<juce::String> seenPaths;
    std::vector<juce::PluginDescription> toRemove;
    
    for (const auto& plugin : types) {
        juce::String pathKey = plugin.fileOrIdentifier.toLowerCase();
        
        if (seenPaths.count(pathKey) > 0) {
            toRemove.push_back(plugin);
        } else {
            seenPaths.insert(pathKey);
        }
    }
    
    for (const auto& plugin : toRemove) {
        processor.knownPluginList.removeType(plugin);
    }
}

// =============================================================================
// checkForCrashedScan - Called from PluginManagerTab
// =============================================================================
void PluginManagerTab::checkForCrashedScan() {
    juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Colosseum");
    juce::File deadMansPedal = dataDir.getChildFile("PluginScanDeadMan.txt");
    
    if (deadMansPedal.existsAsFile()) {
        juce::String crashedPlugin = deadMansPedal.loadFileAsString().trim();
        if (crashedPlugin.isNotEmpty()) {
            juce::Component::SafePointer<PluginManagerTab> safeThis(this);
            
            juce::MessageManager::callAsync([safeThis, crashedPlugin, deadMansPedal]() {
                if (safeThis == nullptr) return;
                
                auto* alertWindow = new juce::AlertWindow(
                    "Plugin Scan Issue Detected",
                    "A previous scan had trouble with this plugin:\n\n" + crashedPlugin + 
                    "\n\nWhat would you like to do?",
                    juce::MessageBoxIconType::WarningIcon);
                
                alertWindow->addButton("Blacklist", 1);
                alertWindow->addButton("Allow Anyway", 2);
                alertWindow->addButton("Try Again", 3);
                alertWindow->addButton("Ignore", 0);
                
                alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
                    [safeThis, crashedPlugin, deadMansPedal, alertWindow](int result) {
                        if (safeThis == nullptr) {
                            delete alertWindow;
                            return;
                        }
                        
                        switch (result) {
                            case 1:  // Blacklist
                            {
                                safeThis->processor.knownPluginList.addToBlacklist(crashedPlugin);
                                deadMansPedal.deleteFile();
                                
                                if (auto* userSettings = safeThis->processor.appProperties.getUserSettings()) {
                                    if (auto xml = safeThis->processor.knownPluginList.createXml())
                                        userSettings->setValue("KnownPlugins", xml.get());
                                    userSettings->saveIfNeeded();
                                }
                                
                                juce::NativeMessageBox::showMessageBoxAsync(
                                    juce::MessageBoxIconType::InfoIcon,
                                    "Plugin Blacklisted",
                                    "The plugin has been blacklisted and will be skipped in future scans.");
                                break;
                            }
                            
                            case 2:  // Allow Anyway
                            {
                                deadMansPedal.deleteFile();
                                
                                auto* settings = safeThis->processor.appProperties.getUserSettings();
                                if (settings) {
                                    juce::String approved = settings->getValue("ApprovedPlugins", "");
                                    if (approved.isNotEmpty()) approved += "|";
                                    approved += crashedPlugin;
                                    settings->setValue("ApprovedPlugins", approved);
                                    settings->saveIfNeeded();
                                }
                                
                                juce::NativeMessageBox::showMessageBoxAsync(
                                    juce::MessageBoxIconType::InfoIcon,
                                    "Plugin Approved",
                                    "The plugin has been added to the approved list.");
                                break;
                            }
                            
                            case 3:  // Try Again
                                deadMansPedal.deleteFile();
                                break;
                            
                            case 0:  // Ignore
                            default:
                                break;
                        }
                        
                        delete alertWindow;
                    }), true);
            });
        } else {
            deadMansPedal.deleteFile();
        }
    }
}