// #D:\Workspace\Subterraneum_plugins_daw\src\OutOfProcessScanner.h
// Out-of-process plugin scanner — spawns child processes to load plugins safely
// Main app NEVER freezes — all work on background thread, UI reads atomics
//
// 3-Phase scan:
//   Phase 1: Skip already-known plugins (vendor != "Unknown")
//   Phase 2: Read moduleinfo.json for VST3 (instant, safe)
//   Phase 3: Out-of-process scan for the rest (child process per plugin)

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class OutOfProcessScanner : public juce::Thread,
                             public juce::ChangeBroadcaster {
public:
    OutOfProcessScanner(SubterraneumAudioProcessor& processor);
    ~OutOfProcessScanner() override;
    
    // Set the list of plugins to scan (file paths + format names)
    struct PluginToScan {
        juce::String filePath;
        juce::String formatName;
    };
    
    void setPluginsToScan(const juce::Array<PluginToScan>& plugins);
    
    // Start/stop scanning
    void startScanning();
    void stopScanning();
    
    // =========================================================================
    // Atomic state — read by UI timer, written by scan thread. NEVER blocks UI.
    // =========================================================================
    std::atomic<float> progress { 0.0f };
    std::atomic<int> scannedCount { 0 };
    std::atomic<int> totalCount { 0 };
    std::atomic<int> phase { 0 };          // 1=known, 2=json, 3=oop
    std::atomic<int> skippedKnown { 0 };
    std::atomic<int> resolvedByJson { 0 };
    std::atomic<int> resolvedByOOP { 0 };
    std::atomic<int> failedOOP { 0 };       // child crashed/timed out
    std::atomic<bool> scanFinished { false };
    
    // Current plugin name (lock-protected, read by UI)
    juce::String getCurrentPlugin() const;
    juce::String getCurrentPhaseText() const;
    
    // Child process timeout in ms
    void setChildTimeout(int ms) { childTimeoutMs = ms; }
    
private:
    void run() override;
    
    // Phase implementations
    void runPhase1_SkipKnown();
    void runPhase2_JsonMetadata();
    void runPhase3_OutOfProcess();
    
    // Spawn child process to scan one plugin
    bool scanPluginViaChildProcess(const PluginToScan& plugin);
    
    // Parse the JSON result file written by child process
    bool parseChildResult(const juce::File& resultFile, const PluginToScan& plugin);
    
    // Add plugin with filename-based fallback info
    void addPluginWithFallbackInfo(const PluginToScan& plugin);
    
    // Check if plugin already has good metadata in knownPluginList
    bool hasGoodMetadata(const juce::String& filePath, const juce::String& formatName);
    
    // Get path to this executable (for spawning child processes)
    juce::String getExecutablePath() const;
    
    SubterraneumAudioProcessor& processor;
    
    juce::Array<PluginToScan> allPlugins;
    juce::Array<PluginToScan> needsJsonScan;     // After phase 1
    juce::Array<PluginToScan> needsOOPScan;      // After phase 2
    
    juce::CriticalSection currentPluginLock;
    juce::String currentPluginName;
    juce::String currentPhaseText;
    
    int childTimeoutMs = 15000;  // 15 seconds per plugin
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutOfProcessScanner)
};
