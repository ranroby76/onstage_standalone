// PluginManagerTab_Scanner.cpp
// ScanProgressPanel — Simple VST3-only scan dialog 
// NOW uses OutOfProcessScanner instead of JUCE's PluginDirectoryScanner
// ZERO freezes, ZERO crashes — same 3-phase approach as the full scanner
// FIX: Sea blue gradient progress bar

#include "PluginManagerTab.h"
#include "OutOfProcessScanner.h"

// =============================================================================
// Sea Blue Gradient Progress Bar LookAndFeel
// =============================================================================
class SeaBlueProgressBarLookAndFeel : public juce::LookAndFeel_V4
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

// Static instance for ScanProgressPanel
static SeaBlueProgressBarLookAndFeel scannerSeaBlueProgressLF;

// =============================================================================
// ScanProgressPanel — kept for the simple "Scan VST3" entry point
// Now routes through OutOfProcessScanner for safety
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
    
    // Apply sea blue gradient look and feel to progress bar
    progressBar.setLookAndFeel(&scannerSeaBlueProgressLF);
    addAndMakeVisible(progressBar);
    
    setSize(400, 180);
}

ScanProgressPanel::~ScanProgressPanel() {
    stopTimer();
    progressBar.setLookAndFeel(nullptr);
    if (oopScanner) {
        oopScanner->stopScanning();
        oopScanner = nullptr;
    }
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
    titleLabel.setText("Scanning VST3 Plugins", juce::dontSendNotification);
    statusLabel.setText("Collecting VST3 files...", juce::dontSendNotification);
    progress = 0.0;
    scanning = true;
    
    // Get VST3 folders from settings
    juce::FileSearchPath searchPath;
    
    if (auto* settings = processor.appProperties.getUserSettings()) {
        juce::String vst3Paths = settings->getValue("VST3Folders", "");
        if (vst3Paths.isNotEmpty()) {
            juce::StringArray folders;
            folders.addTokens(vst3Paths, "|", "");
            for (const auto& folder : folders) {
                if (folder.isNotEmpty())
                    searchPath.add(juce::File(folder));
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
    
    // Collect VST3 files
    juce::Array<OutOfProcessScanner::PluginToScan> pluginFiles;
    
    for (int i = 0; i < searchPath.getNumPaths(); ++i) {
        juce::File folder = searchPath[i];
        if (!folder.exists()) continue;
        
        juce::Array<juce::File> found;
        folder.findChildFiles(found, juce::File::findFilesAndDirectories, true, "*.vst3");
        
        juce::StringArray addedPaths;
        for (const auto& f : found) {
            // Skip nested bundles (e.g. .vst3 inside .vst3)
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
                pluginFiles.add({ f.getFullPathName(), "VST3" });
            }
        }
    }
    
    if (pluginFiles.isEmpty()) {
        statusLabel.setText("No VST3 plugins found in search paths", juce::dontSendNotification);
        scanning = false;
        juce::Timer::callAfterDelay(2000, [this]() {
            if (onCompleteCallback) onCompleteCallback();
        });
        return;
    }
    
    statusLabel.setText("Found " + juce::String(pluginFiles.size()) + " VST3 files", juce::dontSendNotification);
    
    // Create OutOfProcessScanner for safe scanning
    oopScanner = std::make_unique<OutOfProcessScanner>(processor);
    oopScanner->setPluginsToScan(pluginFiles);
    oopScanner->startScanning();
    
    // Start UI timer — 50ms = never blocks
    startTimer(50);
}

void ScanProgressPanel::timerCallback() {
    if (!scanning || !oopScanner) {
        stopTimer();
        return;
    }
    
    // Read atomics (lock-free, instant)
    float scanProgress = oopScanner->progress.load();
    bool finished = oopScanner->scanFinished.load();
    
    progress = (double)scanProgress;
    
    // Update current plugin name
    juce::String currentPlugin = oopScanner->getCurrentPlugin();
    if (currentPlugin.length() > 40)
        currentPlugin = currentPlugin.substring(0, 37) + "...";
    pluginLabel.setText(currentPlugin, juce::dontSendNotification);
    
    int found = processor.knownPluginList.getNumTypes();
    statusLabel.setText("Found " + juce::String(found) + " plugins...", juce::dontSendNotification);
    
    if (finished) {
        stopTimer();
        scanning = false;
        finishScan();
    }
}

void ScanProgressPanel::finishScan() {
    if (oopScanner) {
        oopScanner->stopScanning();
        oopScanner = nullptr;
    }
    
    // Delete old dead man's pedal if it exists
    if (deadMansPedal.existsAsFile())
        deadMansPedal.deleteFile();
    
    int totalFound = processor.knownPluginList.getNumTypes();
    int instruments = 0, effects = 0;
    for (const auto& plugin : processor.knownPluginList.getTypes()) {
        if (plugin.isInstrument) instruments++;
        else effects++;
    }
    
    // Save the plugin list
    if (auto* settings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml()) {
            settings->setValue("KnownPluginsV2", xml.get());
            settings->saveIfNeeded();
        }
    }
    
    processor.knownPluginList.sendChangeMessage();
    
    titleLabel.setText("Scan Complete!", juce::dontSendNotification);
    statusLabel.setText("Found " + juce::String(totalFound) + " plugins", juce::dontSendNotification);
    pluginLabel.setText(juce::String(instruments) + " instruments, " + juce::String(effects) + " effects",
                        juce::dontSendNotification);
    progress = 1.0;
    
    juce::Timer::callAfterDelay(2000, [this]() {
        if (onCompleteCallback) onCompleteCallback();
    });
}
