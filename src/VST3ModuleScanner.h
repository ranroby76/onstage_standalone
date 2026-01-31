// VST3 moduleinfo.json scanner - Zero crash risk plugin discovery
// Reads metadata from JSON files without loading plugin binaries

#pragma once

#include <JuceHeader.h>

// =============================================================================
// VST3 Module Info Scanner
// Scans VST3 plugins by reading moduleinfo.json - no binary loading required
// =============================================================================
class VST3ModuleScanner {
public:
    struct PluginInfo {
        juce::String name;
        juce::String vendor;
        juce::String version;
        juce::String category;
        juce::String subCategories;
        juce::String filePath;
        bool isInstrument = false;
        bool isValid = false;
    };
    
    // Scan a single VST3 bundle and return plugin info
    static PluginInfo scanPlugin(const juce::File& vst3Bundle);
    
    // Scan all VST3 plugins in given folders
    static juce::Array<PluginInfo> scanFolders(const juce::StringArray& folders, 
                                                std::function<void(int current, int total, const juce::String& name)> progressCallback = nullptr);
    
    // Convert PluginInfo to JUCE PluginDescription for compatibility
    static juce::PluginDescription toPluginDescription(const PluginInfo& info);
    
    // Check if a VST3 bundle has moduleinfo.json
    static bool hasModuleInfo(const juce::File& vst3Bundle);
    
private:
    // Find moduleinfo.json inside VST3 bundle (handles platform differences)
    static juce::File findModuleInfoJson(const juce::File& vst3Bundle);
    
    // Parse moduleinfo.json content
    static PluginInfo parseModuleInfo(const juce::File& jsonFile, const juce::File& vst3Bundle);
    
    // Determine if plugin is instrument from subcategories
    static bool isInstrumentFromSubCategories(const juce::String& subCategories);
};