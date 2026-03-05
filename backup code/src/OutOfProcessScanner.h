#pragma once

// Out-of-process plugin scanner - spawns child processes to safely scan plugins
// FIX: Parallel scanning with 8 simultaneous child processes
// FIX: 15 second timeout for better coverage
// FIX: Aggressive cancel - instantly kills all child processes
// FIX: Skips 32-bit plugins automatically

#include <JuceHeader.h>
#include "PluginProcessor.h"

class OutOfProcessScanner : public juce::Thread,
                            public juce::ChangeBroadcaster
{
public:
    // Nested struct for backward compatibility
    struct PluginToScan {
        juce::String filePath;
        juce::String formatName;
    };

    // ScanJob defined in header so std::vector works
    struct ScanJob {
        PluginToScan plugin;
        juce::File resultFile;
        std::unique_ptr<juce::ChildProcess> process;
        int64_t startTimeMs = 0;
        bool completed = false;
        bool success = false;
    };

    OutOfProcessScanner(SubterraneumAudioProcessor& processor);
    ~OutOfProcessScanner() override;

    void setPluginsToScan(const juce::Array<PluginToScan>& plugins);
    void startScanning();
    void stopScanning();
    
    void cancelNow();
    void requestCancel() { cancelNow(); }

    float getProgress() const { return progress.load(); }
    bool isFinished() const { return scanFinished.load(); }
    bool wasCancelled() const { return cancelRequested.load(); }
    juce::String getCurrentPlugin() const;
    juce::String getCurrentPhaseText() const;
    
    int getTotalCount() const { return totalCount.load(); }
    int getScannedCount() const { return scannedCount.load(); }
    int getPhase() const { return phase.load(); }
    int getSkippedKnown() const { return skippedKnown.load(); }
    int getResolvedByJson() const { return resolvedByJson.load(); }
    int getResolvedByOOP() const { return resolvedByOOP.load(); }
    int getFailedOOP() const { return failedOOP.load(); }

    static void cleanupTempFiles();
    static bool is32BitPlugin(const juce::String& filePath);

    std::atomic<float> progress { 0.0f };
    std::atomic<bool> scanFinished { false };
    std::atomic<bool> cancelRequested { false };
    std::atomic<int> totalCount { 0 };
    std::atomic<int> scannedCount { 0 };
    std::atomic<int> phase { 0 };
    std::atomic<int> skippedKnown { 0 };
    std::atomic<int> resolvedByJson { 0 };
    std::atomic<int> resolvedByOOP { 0 };
    std::atomic<int> failedOOP { 0 };

private:
    void run() override;
    
    static constexpr int maxParallelScans = 8;
    static constexpr int childTimeoutMs = 15000;  // 15 seconds
    static constexpr int pollIntervalMs = 50;
    
    bool launchScanJob(const PluginToScan& plugin, ScanJob& job);
    bool checkJobComplete(ScanJob& job);
    void processCompletedJob(ScanJob& job);
    bool parseChildResult(const juce::File& resultFile, const PluginToScan& plugin);
    void addPluginWithFallbackInfo(const PluginToScan& plugin);
    void killAllJobs();
    
    juce::String getExecutablePath() const;
    static juce::String extractVendorFromPath(const juce::String& filePath);
    static bool looksLikeCategory(const juce::String& name);

    SubterraneumAudioProcessor& processor;
    juce::Array<PluginToScan> allPlugins;
    std::vector<ScanJob> activeJobs;
    juce::CriticalSection jobsLock;
    juce::String currentPluginName;
    juce::String currentPhaseText;
    mutable juce::CriticalSection currentPluginLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutOfProcessScanner)
};
