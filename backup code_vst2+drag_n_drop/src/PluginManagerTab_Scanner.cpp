// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_Scanner.cpp
// FIXED: Uses JUCE's PluginDirectoryScanner with async instantiation support
// This properly handles plugins that need message-thread instantiation (like taq.sim solo)

#include "PluginManagerTab.h"

// =============================================================================
// ScanProgressPanel Implementation - USING JUCE SCANNER WITH ASYNC SUPPORT
// =============================================================================
ScanProgressPanel::ScanProgressPanel(SubterraneumAudioProcessor& p, std::function<void()> onComplete)
    : processor(p), onCompleteCallback(onComplete)
{
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    pluginLabel.setFont(juce::Font(11.0f));
    pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    pluginLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pluginLabel);
    
    addAndMakeVisible(progressBar);
    
    // Setup dead man's pedal for crash recovery
    juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Colosseum");
    if (!dataDir.exists()) dataDir.createDirectory();
    deadMansPedal = dataDir.getChildFile("PluginScanDeadMan.txt");
    
    setSize(400, 180);
}

ScanProgressPanel::~ScanProgressPanel() {
    stopTimer();
    scanner = nullptr;
}

void ScanProgressPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(getLocalBounds(), 1);
}

void ScanProgressPanel::resized() {
    auto area = getLocalBounds().reduced(20);
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
    pluginLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(15);
    progressBar.setBounds(area.removeFromTop(20).reduced(20, 0));
}

void ScanProgressPanel::startScan() {
    // Get VST3 folders from settings
    juce::FileSearchPath searchPath;
    
    if (auto* settings = processor.appProperties.getUserSettings()) {
        juce::String vst3Paths = settings->getValue("VST3Folders", "");
        if (vst3Paths.isNotEmpty()) {
            juce::StringArray folders;
            folders.addTokens(vst3Paths, "|", "");
            for (const auto& folder : folders) {
                if (folder.isNotEmpty()) {
                    searchPath.add(juce::File(folder));
                }
            }
        }
    }
    
    // Add defaults if no folders configured
    if (searchPath.getNumPaths() == 0) {
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
    
    // Find VST3 format
    juce::AudioPluginFormat* vst3Format = nullptr;
    for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
        if (processor.formatManager.getFormat(i)->getName() == "VST3") {
            vst3Format = processor.formatManager.getFormat(i);
            break;
        }
    }
    
    if (vst3Format == nullptr) {
        statusLabel.setText("VST3 format not available!", juce::dontSendNotification);
        juce::Timer::callAfterDelay(2000, [this]() {
            if (onCompleteCallback) onCompleteCallback();
        });
        return;
    }
    
    titleLabel.setText("Scanning VST3 Plugins", juce::dontSendNotification);
    statusLabel.setText("Initializing scanner...", juce::dontSendNotification);
    progress = 0.0;
    scanning = true;
    
    // Create JUCE's proper plugin scanner
    // FIXED: Last parameter is TRUE to allow async instantiation
    // This is critical for plugins like taq.sim solo that need message-thread instantiation
    scanner = std::make_unique<juce::PluginDirectoryScanner>(
        processor.knownPluginList,
        *vst3Format,
        searchPath,
        true,           // recursive
        deadMansPedal,  // crash recovery file
        true            // FIXED: allow plugins requiring async instantiation
    );
    
    // Start timer for incremental scanning (16ms = ~60fps, matching working version)
    startTimer(16);
}

void ScanProgressPanel::timerCallback() {
    if (!scanning || !scanner) {
        stopTimer();
        return;
    }
    
    juce::String pluginBeingScanned;
    
    // Scan next plugin - JUCE loads it properly to get all metadata
    if (scanner->scanNextFile(true, pluginBeingScanned)) {
        // Still scanning
        progress = scanner->getProgress();
        
        // Update display
        if (pluginBeingScanned.isNotEmpty()) {
            // Truncate long paths
            juce::String displayName = juce::File(pluginBeingScanned).getFileNameWithoutExtension();
            if (displayName.length() > 40) {
                displayName = displayName.substring(0, 37) + "...";
            }
            pluginLabel.setText(displayName, juce::dontSendNotification);
        }
        
        int found = processor.knownPluginList.getNumTypes();
        statusLabel.setText("Found " + juce::String(found) + " plugins...", juce::dontSendNotification);
    } else {
        // Scan complete
        stopTimer();
        scanning = false;
        finishScan();
    }
}

void ScanProgressPanel::finishScan() {
    scanner = nullptr;
    
    // Delete dead man's pedal on successful completion
    if (deadMansPedal.existsAsFile()) {
        deadMansPedal.deleteFile();
    }
    
    int totalFound = processor.knownPluginList.getNumTypes();
    
    // Count instruments vs effects
    int instruments = 0;
    int effects = 0;
    for (const auto& plugin : processor.knownPluginList.getTypes()) {
        if (plugin.isInstrument)
            instruments++;
        else
            effects++;
    }
    
    // Save the plugin list
    if (auto* settings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml()) {
            settings->setValue("KnownPlugins", xml.get());
            settings->saveIfNeeded();
        }
    }
    
    // Trigger change notification
    processor.knownPluginList.sendChangeMessage();
    
    titleLabel.setText("Scan Complete!", juce::dontSendNotification);
    statusLabel.setText("Found " + juce::String(totalFound) + " plugins", juce::dontSendNotification);
    pluginLabel.setText(juce::String(instruments) + " instruments, " + juce::String(effects) + " effects", 
                        juce::dontSendNotification);
    progress = 1.0;
    
    // CRITICAL FIX: Defer callback to prevent crash
    // Use MessageManager::callAsync to ensure finishScan() returns before object is deleted
    if (onCompleteCallback) {
        auto callback = onCompleteCallback;
        juce::MessageManager::callAsync([callback]() {
            if (callback) {
                callback();
            }
        });
    }
}
