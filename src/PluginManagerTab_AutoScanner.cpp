// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_AutoScanner.cpp
// 3-PHASE OUT-OF-PROCESS SCANNER — ZERO FREEZES, CONTINUOUS FEEDBACK
//
// UI runs on message thread with 50ms timer reading atomics from background thread.
// Background thread (OutOfProcessScanner) does all heavy lifting:
//   Phase 1: Skip known plugins (instant)
//   Phase 2: Read moduleinfo.json (instant)
//   Phase 3: Spawn child process per plugin (crash-safe)
//
// If child crashes → main app is fine, plugin passes with filename info.
// NO blacklisting, NO vendor blocking, EVERY plugin passes.

#include "PluginManagerTab.h"
#include "OutOfProcessScanner.h"

// =============================================================================
// AutoPluginScanner — UI Component with timer-based visual feedback
// =============================================================================
AutoPluginScanner::AutoPluginScanner(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    phaseLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    phaseLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    phaseLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(phaseLabel);
    
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    pluginLabel.setFont(juce::Font(11.0f));
    pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    pluginLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pluginLabel);
    
    statsLabel.setFont(juce::Font(10.0f));
    statsLabel.setColour(juce::Label::textColourId, juce::Colours::grey.darker());
    statsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statsLabel);
    
    addAndMakeVisible(progressBar);
    
    setSize(500, 240);
}

AutoPluginScanner::~AutoPluginScanner() {
    stopTimer();
    if (oopScanner) {
        oopScanner->stopScanning();
        oopScanner = nullptr;
    }
}

void AutoPluginScanner::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(getLocalBounds(), 1);
}

void AutoPluginScanner::resized() {
    auto area = getLocalBounds().reduced(20);
    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);
    phaseLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(5);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(3);
    pluginLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(12);
    progressBar.setBounds(area.removeFromTop(20).reduced(20, 0));
    area.removeFromTop(8);
    statsLabel.setBounds(area.removeFromTop(16));
}

// =============================================================================
// Start the scan — collect files, launch background scanner, start UI timer
// =============================================================================
void AutoPluginScanner::startScan() {
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
    
    titleLabel.setText("Safe Scan (" + platformInfo + ")", juce::dontSendNotification);
    phaseLabel.setText("Collecting plugin files...", juce::dontSendNotification);
    statusLabel.setText("", juce::dontSendNotification);
    pluginLabel.setText("", juce::dontSendNotification);
    progress = 0.0;
    
    // Collect all plugin files from all formats
    collectPluginFiles();
}

// =============================================================================
// Collect plugin files from disk, then start background scanner
// =============================================================================
void AutoPluginScanner::collectPluginFiles() {
    juce::Array<OutOfProcessScanner::PluginToScan> allPlugins;
    
    // Build format list
    juce::StringArray formatNames;
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
    
    for (const auto& formatName : formatNames) {
        juce::FileSearchPath searchPath = getSearchPathForFormat(formatName);
        
        if (formatName == "VST3") {
            for (int i = 0; i < searchPath.getNumPaths(); ++i) {
                juce::File folder = searchPath[i];
                if (!folder.exists()) continue;
                
                juce::Array<juce::File> found;
                folder.findChildFiles(found, juce::File::findFilesAndDirectories, true, "*.vst3");
                
                juce::StringArray addedPaths;
                for (const auto& f : found) {
                    bool isNested = false;
                    for (const auto& existing : addedPaths) {
                        if (f.isAChildOf(juce::File(existing))) {
                            isNested = true;
                            break;
                        }
                    }
                    if (!isNested && !addedPaths.contains(f.getFullPathName())) {
                        addedPaths.add(f.getFullPathName());
                        allPlugins.add({ f.getFullPathName(), "VST3" });
                    }
                }
            }
        }
        else if (formatName == "VST") {
            for (int i = 0; i < searchPath.getNumPaths(); ++i) {
                juce::File folder = searchPath[i];
                if (!folder.exists()) continue;
                
                juce::Array<juce::File> found;
                #if JUCE_WINDOWS
                folder.findChildFiles(found, juce::File::findFiles, true, "*.dll");
                #elif JUCE_MAC
                folder.findChildFiles(found, juce::File::findFilesAndDirectories, true, "*.vst");
                #elif JUCE_LINUX
                folder.findChildFiles(found, juce::File::findFiles, true, "*.so");
                #endif
                
                for (const auto& f : found) {
                    allPlugins.add({ f.getFullPathName(), "VST" });
                }
            }
        }
        else if (formatName == "AudioUnit") {
            juce::AudioPluginFormat* format = nullptr;
            for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
                if (processor.formatManager.getFormat(i)->getName() == "AudioUnit") {
                    format = processor.formatManager.getFormat(i);
                    break;
                }
            }
            if (format != nullptr) {
                auto auPaths = format->searchPathsForPlugins(searchPath, true);
                for (const auto& path : auPaths) {
                    allPlugins.add({ path, "AudioUnit" });
                }
            }
        }
        else if (formatName == "LADSPA") {
            for (int i = 0; i < searchPath.getNumPaths(); ++i) {
                juce::File folder = searchPath[i];
                if (!folder.exists()) continue;
                
                juce::Array<juce::File> found;
                folder.findChildFiles(found, juce::File::findFiles, true, "*.so");
                
                for (const auto& f : found) {
                    allPlugins.add({ f.getFullPathName(), "LADSPA" });
                }
            }
        }
    }
    
    statusLabel.setText("Found " + juce::String(allPlugins.size()) + " plugin files", juce::dontSendNotification);
    
    // Create and start the background scanner
    oopScanner = std::make_unique<OutOfProcessScanner>(processor);
    oopScanner->setPluginsToScan(allPlugins);
    oopScanner->startScanning();
    
    // Start UI timer — 50ms = 20fps visual feedback, NEVER blocks
    startTimer(50);
}

// =============================================================================
// Timer callback — reads atomics from background thread, updates UI labels
// This runs on the MESSAGE THREAD and NEVER does anything blocking
// =============================================================================
void AutoPluginScanner::timerCallback() {
    if (!oopScanner) return;
    
    // Read all atomics (lock-free, instant)
    float scanProgress = oopScanner->progress.load();
    int currentPhase = oopScanner->phase.load();
    int skipped = oopScanner->skippedKnown.load();
    int byJson = oopScanner->resolvedByJson.load();
    int byOOP = oopScanner->resolvedByOOP.load();
    int failed = oopScanner->failedOOP.load();
    bool finished = oopScanner->scanFinished.load();
    
    // Update progress bar
    progress = (double)scanProgress;
    
    // Update phase label with color
    juce::String phaseText = oopScanner->getCurrentPhaseText();
    switch (currentPhase) {
        case 1:
            phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF50C878));
            break;
        case 2:
            phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF4A90D9));
            break;
        case 3:
            phaseLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
            break;
    }
    phaseLabel.setText(phaseText, juce::dontSendNotification);
    
    // Update current plugin name
    juce::String currentPlugin = oopScanner->getCurrentPlugin();
    if (currentPlugin.length() > 50)
        currentPlugin = currentPlugin.substring(0, 47) + "...";
    pluginLabel.setText(currentPlugin, juce::dontSendNotification);
    
    // Update status
    int totalFound = processor.knownPluginList.getNumTypes();
    statusLabel.setText("Found " + juce::String(totalFound) + " plugins", juce::dontSendNotification);
    
    // Update stats line
    juce::String stats;
    if (skipped > 0) stats += "Known: " + juce::String(skipped);
    if (byJson > 0) stats += (stats.isEmpty() ? "" : " | ") + juce::String("JSON: ") + juce::String(byJson);
    if (byOOP > 0) stats += (stats.isEmpty() ? "" : " | ") + juce::String("Deep: ") + juce::String(byOOP);
    if (failed > 0) stats += (stats.isEmpty() ? "" : " | ") + juce::String("Timeout: ") + juce::String(failed);
    statsLabel.setText(stats, juce::dontSendNotification);
    
    // Check if scan is complete
    if (finished) {
        stopTimer();
        finishScan();
    }
}

// =============================================================================
// Scan complete — save, clean up, notify
// =============================================================================
void AutoPluginScanner::finishScan() {
    if (oopScanner) {
        oopScanner->stopScanning();
    }
    
    removeDuplicatePlugins();
    
    int totalFound = processor.knownPluginList.getNumTypes();
    int failed = oopScanner ? (int)oopScanner->failedOOP.load() : 0;
    
    // Save the plugin list
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPluginsV2", xml.get());
        userSettings->saveIfNeeded();
    }
    
    processor.knownPluginList.sendChangeMessage();
    
    progress = 1.0;
    phaseLabel.setText("Scan Complete!", juce::dontSendNotification);
    phaseLabel.setColour(juce::Label::textColourId, juce::Colours::lime);
    statusLabel.setText("Found " + juce::String(totalFound) + " plugins", juce::dontSendNotification);
    
    juce::String completionMsg = "All plugins accepted";
    if (failed > 0)
        completionMsg += " (" + juce::String(failed) + " needed filename-based info)";
    pluginLabel.setText(completionMsg, juce::dontSendNotification);
    
    oopScanner = nullptr;
    
    if (onCompleteCallback) {
        auto callback = onCompleteCallback;
        juce::MessageManager::callAsync([callback]() {
            if (callback) callback();
        });
    }
}

void AutoPluginScanner::removeDuplicatePlugins() {
    auto types = processor.knownPluginList.getTypes();
    
    std::set<juce::String> seenPaths;
    std::vector<juce::PluginDescription> toRemove;
    
    for (const auto& plugin : types) {
        juce::String pathKey = plugin.fileOrIdentifier.toLowerCase() + "|" + plugin.pluginFormatName;
        
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
                for (const auto& folder : folders)
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
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
                for (const auto& folder : folders)
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
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
                for (const auto& folder : folders)
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
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
                for (const auto& folder : folders)
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
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
                for (const auto& folder : folders)
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
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
                for (const auto& folder : folders)
                    if (folder.isNotEmpty()) path.add(juce::File(folder));
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
// checkForCrashedScan — Clean up old dead man's pedal, NO blacklisting
// =============================================================================
void PluginManagerTab::checkForCrashedScan() {
    juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Colosseum");
    
    // Clean up any leftover files from old scan system
    juce::File deadMansPedal = dataDir.getChildFile("PluginScanDeadMan.txt");
    juce::File rescanDeadMan = dataDir.getChildFile("RescanDeadMan.txt");
    
    if (deadMansPedal.existsAsFile()) {
        juce::String crashedPlugin = deadMansPedal.loadFileAsString().trim();
        deadMansPedal.deleteFile();
        
        if (crashedPlugin.isNotEmpty()) {
            juce::MessageManager::callAsync([crashedPlugin]() {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Previous Scan Note",
                    "A previous scan was interrupted while processing:\n\n" + crashedPlugin +
                    "\n\nThe new scanner uses out-of-process scanning, "
                    "so this plugin cannot crash Colosseum.\n\n"
                    "The plugin has NOT been blacklisted and is available for use.");
            });
        }
    }
    
    if (rescanDeadMan.existsAsFile())
        rescanDeadMan.deleteFile();
}
