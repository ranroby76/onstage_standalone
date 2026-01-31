// VST3 moduleinfo.json scanner implementation
// FIXED: Improved instrument detection from SubCategories field

#include "VST3ModuleScanner.h"

// =============================================================================
// Find moduleinfo.json inside VST3 bundle
// =============================================================================
juce::File VST3ModuleScanner::findModuleInfoJson(const juce::File& vst3Bundle) {
    if (!vst3Bundle.exists()) return {};
    
    // VST3 bundle structure varies by platform:
    // Windows: plugin.vst3/Contents/moduleinfo.json
    //      or: plugin.vst3/Contents/x86_64-win/moduleinfo.json
    // macOS:   plugin.vst3/Contents/moduleinfo.json
    // Linux:   plugin.vst3/Contents/moduleinfo.json
    //      or: plugin.vst3/Contents/x86_64-linux/moduleinfo.json
    
    juce::File contentsDir = vst3Bundle.getChildFile("Contents");
    if (!contentsDir.exists()) {
        // Some plugins don't have Contents folder (flat structure)
        contentsDir = vst3Bundle;
    }
    
    // Try direct location first
    juce::File moduleInfo = contentsDir.getChildFile("moduleinfo.json");
    if (moduleInfo.existsAsFile()) return moduleInfo;
    
    // Try platform-specific subfolders
    #if JUCE_WINDOWS
        #if defined(_M_ARM64) || defined(__aarch64__)
        moduleInfo = contentsDir.getChildFile("arm64-win").getChildFile("moduleinfo.json");
        #else
        moduleInfo = contentsDir.getChildFile("x86_64-win").getChildFile("moduleinfo.json");
        if (!moduleInfo.existsAsFile())
            moduleInfo = contentsDir.getChildFile("x86-win").getChildFile("moduleinfo.json");
        #endif
    #elif JUCE_MAC
        #if defined(__arm64__) || defined(__aarch64__)
        moduleInfo = contentsDir.getChildFile("arm64-macos").getChildFile("moduleinfo.json");
        #else
        moduleInfo = contentsDir.getChildFile("x86_64-macos").getChildFile("moduleinfo.json");
        #endif
    #elif JUCE_LINUX
        moduleInfo = contentsDir.getChildFile("x86_64-linux").getChildFile("moduleinfo.json");
    #endif
    
    if (moduleInfo.existsAsFile()) return moduleInfo;
    
    // Try Resources folder (some plugins put it there)
    moduleInfo = contentsDir.getChildFile("Resources").getChildFile("moduleinfo.json");
    if (moduleInfo.existsAsFile()) return moduleInfo;
    
    return {};
}

// =============================================================================
// Check if VST3 has moduleinfo.json
// =============================================================================
bool VST3ModuleScanner::hasModuleInfo(const juce::File& vst3Bundle) {
    return findModuleInfoJson(vst3Bundle).existsAsFile();
}

// =============================================================================
// Determine if instrument from subcategories
// FIXED: More comprehensive instrument detection
// =============================================================================
bool VST3ModuleScanner::isInstrumentFromSubCategories(const juce::String& subCategories) {
    juce::String lower = subCategories.toLowerCase();
    
    // VST3 standard subcategory indicators for instruments
    // Format is typically "Fx|Delay" or "Instrument|Synth"
    if (lower.contains("instrument")) return true;
    if (lower.contains("synth")) return true;
    if (lower.contains("sampler")) return true;
    if (lower.contains("drum")) return true;
    if (lower.contains("piano")) return true;
    if (lower.contains("organ")) return true;
    if (lower.contains("generator")) return true;
    if (lower.contains("tone")) return true;
    
    // Check for pipe-delimited format
    juce::StringArray parts = juce::StringArray::fromTokens(lower, "|", "");
    for (const auto& part : parts) {
        juce::String trimmed = part.trim();
        if (trimmed == "instrument") return true;
        if (trimmed == "synth") return true;
        if (trimmed == "sampler") return true;
    }
    
    // "Fx" prefix typically means effect
    // No instrument indicator = effect
    return false;
}

// =============================================================================
// Parse moduleinfo.json
// =============================================================================
VST3ModuleScanner::PluginInfo VST3ModuleScanner::parseModuleInfo(const juce::File& jsonFile, const juce::File& vst3Bundle) {
    PluginInfo info;
    info.filePath = vst3Bundle.getFullPathName();
    
    juce::String jsonContent = jsonFile.loadFileAsString();
    if (jsonContent.isEmpty()) return info;
    
    juce::var parsed = juce::JSON::parse(jsonContent);
    if (!parsed.isObject()) return info;
    
    auto* root = parsed.getDynamicObject();
    if (!root) return info;
    
    // Get top-level info
    info.name = root->getProperty("Name").toString();
    info.vendor = root->getProperty("Vendor").toString();
    info.version = root->getProperty("Version").toString();
    
    // Parse Classes array for more detailed info
    auto* classes = root->getProperty("Classes").getArray();
    if (classes && classes->size() > 0) {
        // Find the Audio Module Class (the main plugin)
        for (const auto& classEntry : *classes) {
            if (auto* classObj = classEntry.getDynamicObject()) {
                juce::String category = classObj->getProperty("Category").toString();
                
                // "Audio Module Class" is the main plugin entry
                if (category.containsIgnoreCase("Audio Module Class")) {
                    // Override with class-specific info if available
                    juce::String className = classObj->getProperty("Name").toString();
                    if (className.isNotEmpty()) info.name = className;
                    
                    juce::String classVendor = classObj->getProperty("Vendor").toString();
                    if (classVendor.isNotEmpty()) info.vendor = classVendor;
                    
                    juce::String classVersion = classObj->getProperty("Version").toString();
                    if (classVersion.isNotEmpty()) info.version = classVersion;
                    
                    // FIXED: Get SubCategories and determine if instrument
                    info.subCategories = classObj->getProperty("SubCategories").toString();
                    info.isInstrument = isInstrumentFromSubCategories(info.subCategories);
                    info.isValid = true;
                    break;
                }
            }
        }
        
        // FIXED: If no "Audio Module Class" found, check for any class with SubCategories
        if (!info.isValid) {
            for (const auto& classEntry : *classes) {
                if (auto* classObj = classEntry.getDynamicObject()) {
                    juce::String subCat = classObj->getProperty("SubCategories").toString();
                    if (subCat.isNotEmpty()) {
                        juce::String className = classObj->getProperty("Name").toString();
                        if (className.isNotEmpty()) info.name = className;
                        
                        juce::String classVendor = classObj->getProperty("Vendor").toString();
                        if (classVendor.isNotEmpty()) info.vendor = classVendor;
                        
                        info.subCategories = subCat;
                        info.isInstrument = isInstrumentFromSubCategories(info.subCategories);
                        info.isValid = true;
                        break;
                    }
                }
            }
        }
    }
    
    // Fallback: if no Audio Module Class found but we have a name, consider it valid
    if (!info.isValid && info.name.isNotEmpty()) {
        info.isValid = true;
    }
    
    // If still no name, use filename
    if (info.name.isEmpty()) {
        info.name = vst3Bundle.getFileNameWithoutExtension();
        info.isValid = true;
    }
    
    // Build category string
    if (info.isInstrument) {
        info.category = "Instrument";
    } else {
        info.category = "Effect";
    }
    
    return info;
}

// =============================================================================
// Scan a single VST3 plugin
// =============================================================================
VST3ModuleScanner::PluginInfo VST3ModuleScanner::scanPlugin(const juce::File& vst3Bundle) {
    PluginInfo info;
    info.filePath = vst3Bundle.getFullPathName();
    
    juce::File moduleInfoJson = findModuleInfoJson(vst3Bundle);
    
    if (moduleInfoJson.existsAsFile()) {
        // Has moduleinfo.json - parse it
        info = parseModuleInfo(moduleInfoJson, vst3Bundle);
    } else {
        // No moduleinfo.json - use filename as fallback
        // This is less ideal but still safe (no binary loading)
        info.name = vst3Bundle.getFileNameWithoutExtension();
        info.vendor = "Unknown";
        info.version = "Unknown";
        info.category = "Unknown";
        info.isInstrument = false;  // Assume effect when unknown
        info.isValid = true;  // Still valid, just limited info
    }
    
    return info;
}

// =============================================================================
// Scan all VST3 plugins in folders
// =============================================================================
juce::Array<VST3ModuleScanner::PluginInfo> VST3ModuleScanner::scanFolders(
    const juce::StringArray& folders,
    std::function<void(int, int, const juce::String&)> progressCallback) 
{
    juce::Array<PluginInfo> results;
    juce::Array<juce::File> vst3Files;
    
    // Collect all .vst3 bundles
    for (const auto& folderPath : folders) {
        juce::File folder(folderPath);
        if (!folder.exists()) continue;
        
        // Find all .vst3 files/folders
        juce::Array<juce::File> found;
        folder.findChildFiles(found, juce::File::findFilesAndDirectories, true, "*.vst3");
        
        for (const auto& f : found) {
            // Avoid duplicates and nested .vst3 bundles
            bool isDuplicate = false;
            for (const auto& existing : vst3Files) {
                if (existing.getFullPathName() == f.getFullPathName() ||
                    f.isAChildOf(existing)) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                vst3Files.add(f);
            }
        }
    }
    
    // Scan each plugin
    int total = vst3Files.size();
    for (int i = 0; i < total; ++i) {
        const auto& vst3File = vst3Files[i];
        
        if (progressCallback) {
            progressCallback(i + 1, total, vst3File.getFileNameWithoutExtension());
        }
        
        PluginInfo info = scanPlugin(vst3File);
        if (info.isValid) {
            results.add(info);
        }
    }
    
    return results;
}

// =============================================================================
// Convert to JUCE PluginDescription
// =============================================================================
juce::PluginDescription VST3ModuleScanner::toPluginDescription(const PluginInfo& info) {
    juce::PluginDescription desc;
    
    desc.name = info.name;
    desc.manufacturerName = info.vendor;
    desc.version = info.version;
    desc.category = info.category;
    desc.fileOrIdentifier = info.filePath;
    desc.pluginFormatName = "VST3";
    desc.isInstrument = info.isInstrument;
    
    // Generate a unique ID from the file path
    desc.uniqueId = info.filePath.hashCode();
    desc.deprecatedUid = desc.uniqueId;
    
    // These would normally come from loading the plugin, but we set defaults
    desc.numInputChannels = info.isInstrument ? 0 : 2;
    desc.numOutputChannels = 2;
    desc.hasSharedContainer = false;
    
    return desc;
}