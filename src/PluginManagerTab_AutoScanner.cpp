// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_AutoScanner.cpp
// PluginManagerTab_AutoScanner.cpp
// AutoPluginScanner — Single-pass scanner UI with continuous visual feedback
// UI timer reads atomics from background OutOfProcessScanner — NEVER blocks
// FIX: Sea blue gradient progress bar
// FIX: Quick scan now properly compares against known plugins and timestamps
// FIX: Shared container dedup — don't remove VSTi/FX variants from same bundle

#include "PluginManagerTab.h"
#include "OutOfProcessScanner.h"

// =============================================================================
// Sea Blue Gradient Progress Bar LookAndFeel (for AutoScanner)
// =============================================================================
class AutoScannerSeaBlueProgressBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawProgressBar(juce::Graphics& g, juce::ProgressBar& /*bar*/,
                         int width, int height, double progress,
                         const juce::String& textToShow) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
        
        // Background
        g.setColour(juce::Colour(30, 35, 45));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(juce::Colour(60, 80, 100));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
        
        // Progress fill with sea blue gradient
        if (progress > 0.0)
        {
            auto fillWidth = (float)(progress * width);
            auto fillBounds = bounds.withWidth(fillWidth).reduced(1.0f);
            
            // Sea blue gradient: dark teal to bright cyan
            juce::ColourGradient gradient(
                juce::Colour(20, 80, 120),    // Dark sea blue
                0.0f, 0.0f,
                juce::Colour(40, 180, 220),   // Bright cyan
                fillWidth, 0.0f,
                false
            );
            gradient.addColour(0.5, juce::Colour(30, 140, 180));  // Mid teal
            
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(fillBounds, 3.0f);
            
            // Subtle highlight on top
            auto highlight = fillBounds.removeFromTop(fillBounds.getHeight() * 0.4f);
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.fillRoundedRectangle(highlight, 3.0f);
        }
        
        // Text
        if (textToShow.isNotEmpty())
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f));
            g.drawText(textToShow, bounds, juce::Justification::centred);
        }
    }
};

// Static instance for AutoPluginScanner
static AutoScannerSeaBlueProgressBarLookAndFeel autoScannerSeaBlueProgressLF;

// =============================================================================
// Helper: Normalize path for comparison (lowercase, consistent separators)
// =============================================================================
static juce::String normalizePath(const juce::String& path)
{
    return path.toLowerCase().replace("/", "\\");
}

// =============================================================================
// AutoPluginScanner implementation
// =============================================================================
AutoPluginScanner::AutoPluginScanner(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    phaseLabel.setFont(juce::Font(12.0f));
    phaseLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
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
    statsLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    statsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statsLabel);
    
    // Apply sea blue gradient look and feel to progress bar
    progressBar.setLookAndFeel(&autoScannerSeaBlueProgressLF);
    addAndMakeVisible(progressBar);
    
    cancelBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    cancelBtn.onClick = [this]() { cancelScan(); };
    addAndMakeVisible(cancelBtn);
    
    setSize(500, 280);
}

AutoPluginScanner::~AutoPluginScanner() {
    stopTimer();
    progressBar.setLookAndFeel(nullptr);
    if (oopScanner) {
        oopScanner->cancelNow();
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
    
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);
    phaseLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
    pluginLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(15);
    progressBar.setBounds(area.removeFromTop(24).reduced(30, 0));
    area.removeFromTop(10);
    statsLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(15);
    cancelBtn.setBounds(area.removeFromTop(30).withSizeKeepingCentre(100, 28));
}

void AutoPluginScanner::startScan() {
    titleLabel.setText("Scanning All Plugins...", juce::dontSendNotification);
    phaseLabel.setText("", juce::dontSendNotification);
    statusLabel.setText("Collecting plugin files...", juce::dontSendNotification);
    pluginLabel.setText("", juce::dontSendNotification);
    statsLabel.setText("", juce::dontSendNotification);
    progress = 0.0;
    
    collectPluginFiles();
}

void AutoPluginScanner::startRescanExisting(const juce::Array<OutOfProcessScanner::PluginToScan>& plugins) {
    titleLabel.setText("Rescanning Existing Plugins...", juce::dontSendNotification);
    phaseLabel.setText("", juce::dontSendNotification);
    statusLabel.setText("Preparing...", juce::dontSendNotification);
    progress = 0.0;
    
    if (plugins.isEmpty()) {
        statusLabel.setText("No plugins to rescan", juce::dontSendNotification);
        juce::Timer::callAfterDelay(2000, [this]() {
            if (onCompleteCallback) onCompleteCallback();
        });
        return;
    }
    
    // Filter out 32-bit plugins
    juce::Array<OutOfProcessScanner::PluginToScan> filtered;
    for (const auto& plugin : plugins) {
        if (!OutOfProcessScanner::is32BitPlugin(plugin.filePath))
            filtered.add(plugin);
    }
    
    statusLabel.setText("Found " + juce::String(filtered.size()) + " plugins to rescan", juce::dontSendNotification);
    
    oopScanner = std::make_unique<OutOfProcessScanner>(processor);
    oopScanner->setPluginsToScan(filtered);
    oopScanner->startScanning();
    
    startTimer(50);
}

// =============================================================================
// Quick Scan - Only scan NEW or UPDATED plugins
// Compares disk files against knownPluginList + stored timestamps
// =============================================================================
void AutoPluginScanner::startQuickScan() {
    titleLabel.setText("Quick Startup Check", juce::dontSendNotification);
    phaseLabel.setText("Checking for new plugins...", juce::dontSendNotification);
    phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF50C878));
    statusLabel.setText("", juce::dontSendNotification);
    pluginLabel.setText("", juce::dontSendNotification);
    progress = 0.0;
    
    // Collect all plugin files from disk
    juce::Array<OutOfProcessScanner::PluginToScan> allFiles;
    
    auto addFromPath = [&](const juce::FileSearchPath& searchPath, const juce::String& extension, const juce::String& formatName) {
        for (int i = 0; i < searchPath.getNumPaths(); ++i) {
            juce::File folder = searchPath[i];
            if (!folder.exists()) continue;
            
            juce::Array<juce::File> found;
            folder.findChildFiles(found, juce::File::findFilesAndDirectories, true, extension);
            
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
                    if (OutOfProcessScanner::is32BitPlugin(f.getFullPathName()))
                        continue;
                    addedPaths.add(f.getFullPathName());
                    allFiles.add({ f.getFullPathName(), formatName });
                }
            }
        }
    };
    
    // VST3
    addFromPath(getSearchPathForFormat("VST3"), "*.vst3", "VST3");
    
    // VST (if enabled)
    #if JUCE_PLUGINHOST_VST
    addFromPath(getSearchPathForFormat("VST"), "*.dll", "VST");
    #endif
    
    // AU (macOS only)
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    addFromPath(getSearchPathForFormat("AudioUnit"), "*.component", "AudioUnit");
    #endif
    
    // Build set of known plugin paths (NORMALIZED for comparison)
    std::set<juce::String> knownPaths;
    auto existingTypes = processor.knownPluginList.getTypes();
    for (const auto& desc : existingTypes) {
        knownPaths.insert(normalizePath(desc.fileOrIdentifier));
    }
    
    // Load saved timestamps
    std::map<juce::String, juce::int64> savedModTimes;
    if (auto* settings = processor.pluginProperties.getUserSettings()) {
        juce::String timestamps = settings->getValue("PluginTimestamps", "");
        if (timestamps.isNotEmpty()) {
            juce::StringArray lines;
            lines.addLines(timestamps);
            for (const auto& line : lines) {
                int sepIndex = line.lastIndexOf("|");
                if (sepIndex > 0) {
                    juce::String path = line.substring(0, sepIndex);
                    juce::int64 modTime = line.substring(sepIndex + 1).getLargeIntValue();
                    savedModTimes[normalizePath(path)] = modTime;
                }
            }
        }
    }
    
    // Find NEW files (not in knownPluginList) and UPDATED files (mod time changed)
    juce::Array<OutOfProcessScanner::PluginToScan> toScan;
    
    for (const auto& plugin : allFiles) {
        juce::String pathNorm = normalizePath(plugin.filePath);
        
        if (knownPaths.find(pathNorm) == knownPaths.end()) {
            // New plugin — not in known list
            toScan.add(plugin);
        } else {
            // Known plugin — check if modified
            juce::File f(plugin.filePath);
            juce::int64 currentModTime = f.getLastModificationTime().toMilliseconds();
            auto it = savedModTimes.find(pathNorm);
            if (it != savedModTimes.end()) {
                if (currentModTime != it->second)
                    toScan.add(plugin);
            }
            // If no stored timestamp but it's known → skip (will be timestamped after scan)
        }
    }
    
    // Also remove plugins whose files no longer exist
    int removedCount = 0;
    for (const auto& desc : existingTypes) {
        if (desc.pluginFormatName == "AudioUnit") continue; // AU uses identifiers, not paths
        juce::File pluginFile(desc.fileOrIdentifier);
        if (!pluginFile.existsAsFile() && !pluginFile.isDirectory()) {
            processor.knownPluginList.removeType(desc);
            removedCount++;
        }
    }
    
    // Store allFiles for timestamp saving after scan completes
    quickScanAllFiles = allFiles;
    
    if (toScan.isEmpty()) {
        // Nothing new! Show brief status and finish
        phaseLabel.setText("All up to date!", juce::dontSendNotification);
        phaseLabel.setColour(juce::Label::textColourId, juce::Colours::lime);
        
        int total = processor.knownPluginList.getNumTypes();
        juce::String statusMsg = juce::String(total) + " plugins ready";
        if (removedCount > 0)
            statusMsg += " (" + juce::String(removedCount) + " removed — files missing)";
        statusLabel.setText(statusMsg, juce::dontSendNotification);
        pluginLabel.setText("No new or updated plugins found", juce::dontSendNotification);
        progress = 1.0;
        cancelBtn.setVisible(false);
        
        // Save cleaned list if we removed anything
        if (removedCount > 0) {
            if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
                if (auto xml = processor.knownPluginList.createXml())
                    userSettings->setValue("KnownPluginsV2", xml.get());
                userSettings->saveIfNeeded();
            }
            processor.knownPluginList.sendChangeMessage();
        }
        
        // Update timestamps for all current files
        savePluginTimestamps(allFiles);
        
        // Auto-close after 1.5 seconds
        juce::Timer::callAfterDelay(1500, [this]() {
            if (onCompleteCallback) {
                auto callback = onCompleteCallback;
                juce::MessageManager::callAsync([callback]() {
                    if (callback) callback();
                });
            }
        });
        return;
    }
    
    // Found new/updated plugins — scan them
    int newCount = toScan.size();
    phaseLabel.setText("Found " + juce::String(newCount) + " new/updated plugin" + 
                       (newCount != 1 ? "s" : "") + " — scanning...", juce::dontSendNotification);
    phaseLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    statusLabel.setText("This won't take long", juce::dontSendNotification);
    
    oopScanner = std::make_unique<OutOfProcessScanner>(processor);
    oopScanner->setPluginsToScan(toScan);
    oopScanner->startScanning();
    
    startTimer(50);
}

void AutoPluginScanner::cancelScan() {
    if (oopScanner) {
        oopScanner->cancelNow();
    }
    stopTimer();
    
    titleLabel.setText("Scan Cancelled", juce::dontSendNotification);
    statusLabel.setText("", juce::dontSendNotification);
    pluginLabel.setText("", juce::dontSendNotification);
    
    juce::Timer::callAfterDelay(1000, [this]() {
        if (onCompleteCallback) onCompleteCallback();
    });
}

void AutoPluginScanner::collectPluginFiles() {
    juce::Array<OutOfProcessScanner::PluginToScan> allFiles;
    
    // Collect from all format paths
    auto addFromPath = [&](const juce::FileSearchPath& searchPath, const juce::String& extension, const juce::String& formatName) {
        for (int i = 0; i < searchPath.getNumPaths(); ++i) {
            juce::File folder = searchPath[i];
            if (!folder.exists()) continue;
            
            juce::Array<juce::File> found;
            folder.findChildFiles(found, juce::File::findFilesAndDirectories, true, extension);
            
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
                    // Skip 32-bit plugins
                    if (OutOfProcessScanner::is32BitPlugin(f.getFullPathName()))
                        continue;
                        
                    addedPaths.add(f.getFullPathName());
                    allFiles.add({ f.getFullPathName(), formatName });
                }
            }
        }
    };
    
    // VST3
    addFromPath(getSearchPathForFormat("VST3"), "*.vst3", "VST3");
    
    // VST (if enabled)
    #if JUCE_PLUGINHOST_VST
    addFromPath(getSearchPathForFormat("VST"), "*.dll", "VST");
    #endif
    
    // AU (macOS only)
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    addFromPath(getSearchPathForFormat("AudioUnit"), "*.component", "AudioUnit");
    #endif
    
    // CLAP - disabled (app can't host CLAP plugins yet)
    // addFromPath(getSearchPathForFormat("CLAP"), "*.clap", "CLAP");
    
    if (allFiles.isEmpty()) {
        statusLabel.setText("No plugins found in search paths", juce::dontSendNotification);
        juce::Timer::callAfterDelay(2000, [this]() {
            if (onCompleteCallback) onCompleteCallback();
        });
        return;
    }
    
    statusLabel.setText("Found " + juce::String(allFiles.size()) + " plugin files", juce::dontSendNotification);
    
    // For full scan, also save all files for timestamp saving
    quickScanAllFiles = allFiles;
    
    oopScanner = std::make_unique<OutOfProcessScanner>(processor);
    oopScanner->setPluginsToScan(allFiles);
    oopScanner->startScanning();
    
    startTimer(50);
}

void AutoPluginScanner::timerCallback() {
    if (!oopScanner) {
        stopTimer();
        return;
    }
    
    // Read atomics (lock-free, instant)
    float scanProgress = oopScanner->progress.load();
    bool finished = oopScanner->scanFinished.load();
    bool cancelled = oopScanner->wasCancelled();
    
    progress = (double)scanProgress;
    
    // Update phase text
    juce::String phaseText = oopScanner->getCurrentPhaseText();
    if (phaseText.isNotEmpty())
        phaseLabel.setText(phaseText, juce::dontSendNotification);
    
    // Update current plugin name
    juce::String currentPlugin = oopScanner->getCurrentPlugin();
    if (currentPlugin.length() > 50)
        currentPlugin = currentPlugin.substring(0, 47) + "...";
    pluginLabel.setText(currentPlugin, juce::dontSendNotification);
    
    // Update stats
    int scanned = oopScanner->getScannedCount();
    int total = oopScanner->getTotalCount();
    int success = oopScanner->getResolvedByOOP();
    int failed = oopScanner->getFailedOOP();
    
    statusLabel.setText("Scanning " + juce::String(scanned) + " / " + juce::String(total), 
                        juce::dontSendNotification);
    statsLabel.setText("Success: " + juce::String(success) + " | Failed: " + juce::String(failed),
                       juce::dontSendNotification);
    
    if (finished || cancelled) {
        stopTimer();
        finishScan();
    }
}

void AutoPluginScanner::finishScan() {
    if (oopScanner) {
        oopScanner->stopScanning();
        oopScanner = nullptr;
    }
    
    removeDuplicatePlugins();
    
    int totalFound = processor.knownPluginList.getNumTypes();
    int instruments = 0, effects = 0;
    for (const auto& plugin : processor.knownPluginList.getTypes()) {
        if (plugin.isInstrument) instruments++;
        else effects++;
    }
    
    // Save the plugin list
    if (auto* settings = processor.pluginProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml()) {
            settings->setValue("KnownPluginsV2", xml.get());
            settings->saveIfNeeded();
        }
    }
    
    // Save timestamps for quick scan next time
    if (!quickScanAllFiles.isEmpty())
        savePluginTimestamps(quickScanAllFiles);
    
    processor.knownPluginList.sendChangeMessage();
    
    titleLabel.setText("Scan Complete!", juce::dontSendNotification);
    phaseLabel.setText("", juce::dontSendNotification);
    statusLabel.setText("Found " + juce::String(totalFound) + " plugins", juce::dontSendNotification);
    pluginLabel.setText(juce::String(instruments) + " instruments, " + juce::String(effects) + " effects",
                        juce::dontSendNotification);
    statsLabel.setText("", juce::dontSendNotification);
    progress = 1.0;
    
    juce::Timer::callAfterDelay(2000, [this]() {
        if (onCompleteCallback) onCompleteCallback();
    });
}

void AutoPluginScanner::removeDuplicatePlugins() {
    // Remove duplicate plugin entries (same file path + name + uniqueId)
    // NOTE: Shared container VST3 bundles (e.g. Serum 2) contain MULTIPLE
    // plugins in one .vst3 file — VSTi + FX versions share the same
    // fileOrIdentifier path. We must include name and uniqueId in the key
    // so they are treated as distinct plugins.
    auto types = processor.knownPluginList.getTypes();
    std::set<juce::String> seenPlugins;
    
    for (const auto& plugin : types) {
        juce::String key = normalizePath(plugin.fileOrIdentifier) + "|" 
                         + plugin.pluginFormatName + "|"
                         + plugin.name + "|"
                         + juce::String(plugin.uniqueId);
        if (seenPlugins.count(key) > 0) {
            processor.knownPluginList.removeType(plugin);
        } else {
            seenPlugins.insert(key);
        }
    }
}

void AutoPluginScanner::savePluginTimestamps(const juce::Array<OutOfProcessScanner::PluginToScan>& files) {
    // Save last modified timestamps for quick scan (using normalized paths)
    if (auto* settings = processor.pluginProperties.getUserSettings()) {
        juce::String timestampData;
        for (const auto& f : files) {
            juce::File file(f.filePath);
            if (file.exists()) {
                // Store normalized path for consistent comparison
                timestampData += normalizePath(f.filePath) + "|" + juce::String(file.getLastModificationTime().toMilliseconds()) + "\n";
            }
        }
        settings->setValue("PluginTimestamps", timestampData);
        settings->saveIfNeeded();
    }
}

juce::FileSearchPath AutoPluginScanner::getSearchPathForFormat(const juce::String& formatName) {
    juce::FileSearchPath searchPath;
    
    if (auto* settings = processor.appProperties.getUserSettings()) {
        juce::String key = formatName + "Folders";
        juce::String paths = settings->getValue(key, "");
        if (paths.isNotEmpty()) {
            juce::StringArray folders;
            folders.addTokens(paths, "|", "");
            for (const auto& folder : folders) {
                if (folder.isNotEmpty())
                    searchPath.add(juce::File(folder));
            }
        }
    }
    
    // Add defaults if empty
    if (searchPath.getNumPaths() == 0) {
        if (formatName == "VST3") {
            #if JUCE_WINDOWS
            searchPath.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
            searchPath.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));
            #elif JUCE_MAC
            searchPath.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
            searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("Library/Audio/Plug-Ins/VST3"));
            #elif JUCE_LINUX
            searchPath.add(juce::File("/usr/lib/vst3"));
            searchPath.add(juce::File("/usr/local/lib/vst3"));
            searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile(".vst3"));
            #endif
        }
        #if JUCE_PLUGINHOST_VST
        else if (formatName == "VST") {
            #if JUCE_WINDOWS
            searchPath.add(juce::File("C:\\Program Files\\VSTPlugins"));
            searchPath.add(juce::File("C:\\Program Files\\Steinberg\\VSTPlugins"));
            searchPath.add(juce::File("C:\\Program Files (x86)\\VSTPlugins"));
            searchPath.add(juce::File("C:\\Program Files (x86)\\Steinberg\\VSTPlugins"));
            #endif
        }
        #endif
        #if JUCE_PLUGINHOST_AU && JUCE_MAC
        else if (formatName == "AudioUnit") {
            searchPath.add(juce::File("/Library/Audio/Plug-Ins/Components"));
            searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("Library/Audio/Plug-Ins/Components"));
        }
        #endif
    }
    
    return searchPath;
}

// =============================================================================
// checkForCrashedScan — cleanup any stale temp files from previous crashes
// No more blacklist prompts, just silent cleanup
// =============================================================================
void PluginManagerTab::checkForCrashedScan() {
    // Clean up any stale temp files from previous scan sessions
    OutOfProcessScanner::cleanupTempFiles();
    
    // Check for dead man's pedal file (indicates a crash during scan)
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File deadMansPedal = tempDir.getChildFile("colosseum_scanning.tmp");
    
    if (deadMansPedal.existsAsFile()) {
        // Previous scan crashed - just clean up silently
        deadMansPedal.deleteFile();
        
        // No prompts, no blacklisting - the OOP scanner handles crashes gracefully
        // Just log it for debugging
        DBG("Previous scan session detected - cleaned up stale files");
    }
}