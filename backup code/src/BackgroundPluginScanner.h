// #D:\Workspace\Subterraneum_plugins_daw\src\BackgroundPluginScanner.h
// Background metadata scanner - loads plugins one-by-one with timeout
// Updates vendor, type, format info without blocking UI

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// =============================================================================
// Background Plugin Scanner - Collects metadata without blocking UI
// =============================================================================
class BackgroundPluginScanner : public juce::Thread,
                                 public juce::ChangeBroadcaster {
public:
    BackgroundPluginScanner(SubterraneumAudioProcessor& processor);
    ~BackgroundPluginScanner() override;
    
    // Start scanning in background
    void startScanning();
    
    // Stop scanning
    void stopScanning();
    
    // Get progress (0.0 - 1.0)
    float getProgress() const { return progress.load(); }
    
    // Get current plugin being scanned
    juce::String getCurrentPlugin() const;
    
    // Check if scanning
    bool isScanning() const { return isThreadRunning(); }
    
    // Get count of scanned plugins
    int getScannedCount() const { return scannedCount.load(); }
    int getTotalCount() const { return totalCount.load(); }
    
    // Timeout in milliseconds (default 5 seconds)
    void setTimeout(int timeoutMs) { timeout = timeoutMs; }
    
private:
    void run() override;
    
    // Scan a single plugin with timeout
    bool scanPluginWithTimeout(const juce::PluginDescription& desc);
    
    SubterraneumAudioProcessor& processor;
    
    std::atomic<float> progress { 0.0f };
    std::atomic<int> scannedCount { 0 };
    std::atomic<int> totalCount { 0 };
    
    juce::CriticalSection currentPluginLock;
    juce::String currentPluginName;
    
    int timeout = 5000;  // 5 seconds default
    
    // List of plugins that timed out or failed
    juce::StringArray failedPlugins;
    juce::CriticalSection failedPluginsLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackgroundPluginScanner)
};
