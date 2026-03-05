// VST3 moduleinfo.json scanner - Zero crash risk plugin discovery
// Reads metadata from JSON files without loading plugin binaries
// NEW: scanAllPlugins for shared containers (e.g. Serum2.vst3 = synth + FX)

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
        juce::String cid;          // VST3 Class ID (32 hex chars from moduleinfo.json)
        juce::String filePath;
        bool isInstrument = false;
        bool isValid = false;
    };
    
    // Scan a single VST3 bundle and return first plugin info (legacy)
    static PluginInfo scanPlugin(const juce::File& vst3Bundle);
    
    // Scan ALL plugins in a VST3 bundle (handles shared containers like Serum2)
    static juce::Array<PluginInfo> scanAllPlugins(const juce::File& vst3Bundle);
    
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
    
    // Parse moduleinfo.json content (returns first Audio Module Class)
    static PluginInfo parseModuleInfo(const juce::File& jsonFile, const juce::File& vst3Bundle);
    
    // Parse ALL Audio Module Classes from moduleinfo.json
    static juce::Array<PluginInfo> parseAllModuleInfo(const juce::File& jsonFile, const juce::File& vst3Bundle);
    
    // Determine if plugin is instrument from subcategories
    static bool isInstrumentFromSubCategories(const juce::String& subCategories);
    
    // Convert CID hex string (32 chars) to JUCE-compatible uniqueId hash
    static int computeUniqueIdFromCID(const juce::String& cidHex);
    
    // Convert CID hex string to JUCE-compatible deprecatedUid hash (COM byte order)
    static int computeDeprecatedUidFromCID(const juce::String& cidHex);
    
    // Parse 32-char hex CID string into 16 raw bytes
    static bool parseCIDBytes(const juce::String& cidHex, juce::uint8* outBytes);
};
