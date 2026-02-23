// #D:\Workspace\Subterraneum_plugins_daw\src\PluginScanWorker.cpp
// Scan worker — runs inside child process, loads ONE plugin, writes metadata.
// If this crashes, only the child process dies — main Colosseum is unaffected.
// Watchdog thread self-terminates after 15 seconds to prevent infinite hangs.

#include "PluginScanWorker.h"

void PluginScanWorker::runScan(const juce::String& pluginPath,
                                const juce::String& formatName,
                                const juce::String& outputFile)
{
    // =========================================================================
    // Start watchdog — if we hang, it kills this process after 15 seconds
    // =========================================================================
    WatchdogThread watchdog(15000);
    watchdog.startThread();
    
    // =========================================================================
    // Create the format manager — manually register formats
    // (addDefaultFormats() may not be available in all build configurations)
    // =========================================================================
    juce::AudioPluginFormatManager formatManager;
    
    #if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
    #endif
    #if JUCE_PLUGINHOST_VST
    formatManager.addFormat(new juce::VSTPluginFormat());
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatManager.addFormat(new juce::AudioUnitPluginFormat());
    #endif
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formatManager.addFormat(new juce::LADSPAPluginFormat());
    #endif
    
    // Find the requested format
    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i) {
        if (formatManager.getFormat(i)->getName() == formatName) {
            format = formatManager.getFormat(i);
            break;
        }
    }
    
    if (format == nullptr) {
        // Write error result
        juce::File(outputFile).replaceWithText(
            "{\"success\":false,\"error\":\"Format not found: " + formatName + "\"}");
        watchdog.signalThreadShouldExit();
        return;
    }
    
    // =========================================================================
    // Ask the format to find all plugin types in this file
    // THIS IS THE CALL THAT CAN CRASH — that's why we're in a child process
    // =========================================================================
    juce::OwnedArray<juce::PluginDescription> results;
    format->findAllTypesForFile(results, pluginPath);
    
    // =========================================================================
    // Build JSON result
    // =========================================================================
    juce::String json = "{\n  \"success\":true,\n  \"plugins\":[\n";
    
    for (int i = 0; i < results.size(); ++i) {
        auto* desc = results[i];
        
        if (i > 0) json += ",\n";
        
        // Escape strings for JSON
        auto escape = [](const juce::String& s) -> juce::String {
            return s.replace("\\", "\\\\")
                    .replace("\"", "\\\"")
                    .replace("\n", "\\n")
                    .replace("\r", "\\r")
                    .replace("\t", "\\t");
        };
        
        json += "    {\n";
        json += "      \"name\":\"" + escape(desc->name) + "\",\n";
        json += "      \"vendor\":\"" + escape(desc->manufacturerName) + "\",\n";
        json += "      \"category\":\"" + escape(desc->category) + "\",\n";
        json += "      \"version\":\"" + escape(desc->version) + "\",\n";
        json += "      \"fileOrIdentifier\":\"" + escape(desc->fileOrIdentifier) + "\",\n";
        json += "      \"pluginFormatName\":\"" + escape(desc->pluginFormatName) + "\",\n";
        json += "      \"isInstrument\":" + juce::String(desc->isInstrument ? "true" : "false") + ",\n";
        json += "      \"numInputChannels\":" + juce::String(desc->numInputChannels) + ",\n";
        json += "      \"numOutputChannels\":" + juce::String(desc->numOutputChannels) + ",\n";
        json += "      \"uniqueId\":" + juce::String(desc->uniqueId) + ",\n";
        json += "      \"deprecatedUid\":" + juce::String(desc->deprecatedUid) + ",\n";
        json += "      \"hasSharedContainer\":" + juce::String(desc->hasSharedContainer ? "true" : "false") + "\n";
        json += "    }";
    }
    
    json += "\n  ]\n}\n";
    
    // Write result file
    juce::File(outputFile).replaceWithText(json);
    
    // Stop watchdog
    watchdog.signalThreadShouldExit();
    watchdog.waitForThreadToExit(1000);
}
