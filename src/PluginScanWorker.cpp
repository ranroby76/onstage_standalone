// #D:\Workspace\Subterraneum_plugins_daw\src\PluginScanWorker.cpp
// Scan worker — runs inside child process, loads ONE plugin, writes metadata.
// If this crashes, only the child process dies — main Colosseum is unaffected.
// Watchdog thread self-terminates after 20 seconds to prevent infinite hangs.
// FIX: Writes JSON IMMEDIATELY after getting metadata, before any cleanup
// FIX: Flushes log after each write to ensure visibility on crash

#include "PluginScanWorker.h"

// =============================================================================
// Debug logging disabled - was writing colosseum_scan_debug.txt to Desktop
// =============================================================================
static void logDebug(const juce::String& /*message*/)
{
    // No-op for release
}

// =============================================================================
// Helper: Check if a PluginDescription has incomplete/missing metadata
// =============================================================================
static bool hasIncompleteMetadata(const juce::PluginDescription& desc)
{
    if (desc.uniqueId == 0)
        return true;
    
    if (desc.manufacturerName.isEmpty() || 
        desc.manufacturerName.equalsIgnoreCase("Unknown"))
        return true;
    
    if (desc.category.isEmpty() || 
        desc.category.equalsIgnoreCase("Unknown"))
        return true;
    
    return false;
}

// =============================================================================
// Helper: Write JSON result file
// =============================================================================
static void writeJsonResult(const juce::String& outputFile, 
                            const juce::OwnedArray<juce::PluginDescription>& results)
{
    logDebug("Building JSON with " + juce::String(results.size()) + " plugins");
    
    juce::String json = "{\n  \"success\":true,\n  \"plugins\":[\n";
    
    for (int i = 0; i < results.size(); ++i) {
        auto* desc = results[i];
        
        if (i > 0) json += ",\n";
        
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
    
    juce::File(outputFile).replaceWithText(json);
    logDebug("Wrote result to: " + outputFile);
}

void PluginScanWorker::runScan(const juce::String& pluginPath,
                                const juce::String& formatName,
                                const juce::String& outputFile)
{
    logDebug("=== SCAN START: " + pluginPath + " ===");
    logDebug("Format: " + formatName);
    
    // =========================================================================
    // Start watchdog — if we hang, it kills this process after 20 seconds
    // =========================================================================
    WatchdogThread watchdog(20000);
    watchdog.startThread();
    
    // =========================================================================
    // Create the format manager — manually register formats
    // =========================================================================
    juce::AudioPluginFormatManager formatManager;
    
    #if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
    logDebug("Added VST3 format");
    #endif
    #if JUCE_PLUGINHOST_VST
    formatManager.addFormat(new juce::VSTPluginFormat());
    logDebug("Added VST format");
    #endif
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatManager.addFormat(new juce::AudioUnitPluginFormat());
    logDebug("Added AU format");
    #endif
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formatManager.addFormat(new juce::LADSPAPluginFormat());
    logDebug("Added LADSPA format");
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
        logDebug("ERROR: Format not found: " + formatName);
        juce::File(outputFile).replaceWithText(
            "{\"success\":false,\"error\":\"Format not found: " + formatName + "\"}");
        watchdog.signalThreadShouldExit();
        return;
    }
    
    logDebug("Found format: " + format->getName());
    
    // =========================================================================
    // Phase 1: Ask the format to find all plugin types in this file
    // =========================================================================
    juce::OwnedArray<juce::PluginDescription> results;
    format->findAllTypesForFile(results, pluginPath);
    
    logDebug("Phase 1 - findAllTypesForFile returned " + juce::String(results.size()) + " results");
    
    for (int i = 0; i < results.size(); ++i)
    {
        auto* desc = results[i];
        logDebug("  [" + juce::String(i) + "] name=" + desc->name 
                 + ", vendor=" + desc->manufacturerName 
                 + ", category=" + desc->category
                 + ", uniqueId=" + juce::String(desc->uniqueId)
                 + ", isInstrument=" + juce::String(desc->isInstrument ? "true" : "false"));
    }
    
    // =========================================================================
    // FIX: If Phase 1 got results with good metadata, WRITE JSON IMMEDIATELY
    // Some plugins create background threads that block exit, so we write
    // the result BEFORE any further processing or cleanup
    // =========================================================================
    bool allMetadataOk = true;
    for (int i = 0; i < results.size(); ++i)
    {
        if (hasIncompleteMetadata(*results[i]))
        {
            allMetadataOk = false;
            break;
        }
    }
    
    if (!results.isEmpty() && allMetadataOk)
    {
        logDebug("Phase 1 metadata complete - writing JSON immediately");
        writeJsonResult(outputFile, results);
        logDebug("=== SCAN END (fast path) ===\n");
        watchdog.signalThreadShouldExit();
        return;  // Exit immediately, don't wait for cleanup
    }
    
    // =========================================================================
    // Phase 2: For plugins with incomplete metadata, INSTANTIATE to get real data
    // =========================================================================
    logDebug("Phase 2 - checking for incomplete metadata...");
    
    for (int i = 0; i < results.size(); ++i)
    {
        auto* desc = results[i];
        
        if (hasIncompleteMetadata(*desc))
        {
            logDebug("Phase 2 - Incomplete metadata for: " + desc->name + ", attempting instantiation...");
            
            juce::String errorMessage;
            std::unique_ptr<juce::AudioPluginInstance> instance;
            
            instance = formatManager.createPluginInstance(*desc, 44100.0, 512, errorMessage);
            
            if (instance != nullptr)
            {
                logDebug("  SUCCESS: Got instance!");
                
                auto realDesc = instance->getPluginDescription();
                
                logDebug("  Real data: name=" + realDesc.name 
                         + ", vendor=" + realDesc.manufacturerName 
                         + ", category=" + realDesc.category
                         + ", uniqueId=" + juce::String(realDesc.uniqueId)
                         + ", isInstrument=" + juce::String(realDesc.isInstrument ? "true" : "false"));
                
                desc->name = realDesc.name;
                desc->manufacturerName = realDesc.manufacturerName;
                desc->category = realDesc.category;
                desc->version = realDesc.version;
                desc->isInstrument = realDesc.isInstrument;
                desc->numInputChannels = realDesc.numInputChannels;
                desc->numOutputChannels = realDesc.numOutputChannels;
                desc->uniqueId = realDesc.uniqueId;
                desc->deprecatedUid = realDesc.deprecatedUid;
                desc->hasSharedContainer = realDesc.hasSharedContainer;
                
                instance->releaseResources();
                instance.reset();
            }
            else
            {
                logDebug("  FAILED: " + errorMessage);
            }
        }
        else
        {
            logDebug("Phase 2 - Metadata OK for: " + desc->name + ", skipping instantiation");
        }
    }
    
    // =========================================================================
    // Phase 3: If no results from findAllTypesForFile, try direct instantiation
    // =========================================================================
    if (results.isEmpty())
    {
        logDebug("Phase 3 - No results, trying direct instantiation...");
        
        juce::PluginDescription tryDesc;
        tryDesc.fileOrIdentifier = pluginPath;
        tryDesc.pluginFormatName = formatName;
        
        juce::String errorMessage;
        std::unique_ptr<juce::AudioPluginInstance> instance;
        
        instance = formatManager.createPluginInstance(tryDesc, 44100.0, 512, errorMessage);
        
        if (instance != nullptr)
        {
            logDebug("  SUCCESS: Direct instantiation worked!");
            auto* realDesc = new juce::PluginDescription(instance->getPluginDescription());
            
            logDebug("  Real data: name=" + realDesc->name 
                     + ", vendor=" + realDesc->manufacturerName 
                     + ", category=" + realDesc->category
                     + ", uniqueId=" + juce::String(realDesc->uniqueId)
                     + ", isInstrument=" + juce::String(realDesc->isInstrument ? "true" : "false"));
            
            results.add(realDesc);
            
            instance->releaseResources();
            instance.reset();
        }
        else
        {
            logDebug("  FAILED: " + errorMessage);
        }
    }
    
    // =========================================================================
    // Write JSON result (slow path - only if we didn't write earlier)
    // =========================================================================
    writeJsonResult(outputFile, results);
    logDebug("=== SCAN END ===\n");
    
    // Stop watchdog
    watchdog.signalThreadShouldExit();
    watchdog.waitForThreadToExit(1000);
}
