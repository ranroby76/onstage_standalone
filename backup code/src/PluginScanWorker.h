// #D:\Workspace\Subterraneum_plugins_daw\src\PluginScanWorker.h
// Scan worker — runs inside the child process (headless mode)
// Loads ONE plugin, writes metadata JSON to output file, exits.
// Has a watchdog thread that self-terminates after timeout.

#pragma once

#include <JuceHeader.h>

class PluginScanWorker {
public:
    // Run a scan of a single plugin and write results to outputFile
    // Called from StandaloneMain when --scan-plugin flag is detected
    // This runs in the CHILD process — if it crashes, only the child dies
    static void runScan(const juce::String& pluginPath,
                        const juce::String& formatName,
                        const juce::String& outputFile);
    
private:
    // Watchdog thread that kills the process after timeout
    class WatchdogThread : public juce::Thread {
    public:
        WatchdogThread(int timeoutMs) : Thread("ScanWatchdog"), timeout(timeoutMs) {}
        void run() override {
            auto start = juce::Time::getMillisecondCounter();
            while (!threadShouldExit()) {
                if ((int)(juce::Time::getMillisecondCounter() - start) > timeout) {
                    // Force kill — the plugin is hanging
                    juce::Process::terminate();
                    return;
                }
                sleep(100);
            }
        }
    private:
        int timeout;
    };
};
