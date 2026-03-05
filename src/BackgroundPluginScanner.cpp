// #D:\Workspace\Subterraneum_plugins_daw\src\BackgroundPluginScanner.cpp
// Background metadata enrichment — runs after scan to fix incomplete plugin entries
// Phase 1: VST3 moduleinfo.json enrichment (safe, no loading)
// Phase 2: VST2 path-based vendor/category heuristics
// Phase 3: Windows DLL version info extraction for VST2 (no loading)
// Catches: Unknown vendor, Unknown category, uid=0, missing isInstrument

#include "BackgroundPluginScanner.h"
#include "VST3ModuleScanner.h"
#include <map>

BackgroundPluginScanner::BackgroundPluginScanner(SubterraneumAudioProcessor& p)
    : Thread("BackgroundEnrichment"), processor(p)
{
}

BackgroundPluginScanner::~BackgroundPluginScanner() {
    stopEnrichment();
}

void BackgroundPluginScanner::startEnrichment() {
    if (isThreadRunning()) return;
    enrichedCount.store(0);
    finished.store(false);
    startThread(juce::Thread::Priority::low);
}

void BackgroundPluginScanner::stopEnrichment() {
    signalThreadShouldExit();
    waitForThreadToExit(3000);
}

// =============================================================================
// Check if a plugin needs enrichment
// =============================================================================
static bool needsEnrichment(const juce::PluginDescription& desc)
{
    if (desc.uniqueId == 0)
        return true;
    if (desc.manufacturerName.isEmpty() || desc.manufacturerName == "Unknown")
        return true;
    if (desc.category.isEmpty() || desc.category == "Unknown")
        return true;
    return false;
}

// =============================================================================
// VST2: Extract vendor from file path heuristics
// Common patterns:
//   C:\Program Files\VSTPlugins\FabFilter\FabFilter Pro-Q 3.dll -> FabFilter
//   C:\Program Files\VSTPlugins\UJAMsilk\VG-SILK.dll -> UJAM
//   C:\Program Files\VSTPlugins\SoundeviceDigital\MasterMind.dll -> SoundeviceDigital
// =============================================================================
static juce::String guessVST2VendorFromPath(const juce::String& filePath)
{
    juce::File file(filePath);
    juce::File parent = file.getParentDirectory();
    juce::String parentName = parent.getFileName();
    
    // If parent is a generic folder, no vendor info
    juce::StringArray genericFolders = { 
        "VSTPlugins", "VST", "VST2", "Steinberg", "Common Files", 
        "Program Files", "Program Files (x86)", "Plug-Ins" 
    };
    
    if (genericFolders.contains(parentName, true))
        return {};
    
    // If parent folder name differs from plugin name, it's likely a vendor folder
    juce::String pluginName = file.getFileNameWithoutExtension();
    
    if (!parentName.equalsIgnoreCase(pluginName) && 
        !pluginName.containsIgnoreCase(parentName))
    {
        return parentName;
    }
    
    // Try grandparent
    juce::File grandParent = parent.getParentDirectory();
    juce::String grandParentName = grandParent.getFileName();
    if (!genericFolders.contains(grandParentName, true) &&
        grandParentName != parentName)
    {
        return grandParentName;
    }
    
    return {};
}

// =============================================================================
// VST2: Guess category from plugin name patterns
// =============================================================================
static juce::String guessVST2Category(const juce::String& name, const juce::String& vendor)
{
    juce::String lower = name.toLowerCase();
    
    // Known instrument patterns
    if (lower.contains("synth") || lower.contains("piano") || lower.contains("organ") ||
        lower.contains("drum") || lower.contains("sampler") || lower.contains("strings") ||
        lower.contains("guitar") || lower.contains("bass") || lower.contains("keys") ||
        lower.contains("brass") || lower.contains("woodwind") || lower.contains("choir") ||
        lower.contains("pluck") || lower.contains("pad"))
        return "Synth";
    
    // Known effect patterns
    if (lower.contains("eq") || lower.contains("compressor") || lower.contains("limiter") ||
        lower.contains("reverb") || lower.contains("delay") || lower.contains("chorus") ||
        lower.contains("phaser") || lower.contains("flanger") || lower.contains("filter") ||
        lower.contains("distortion") || lower.contains("saturator") || lower.contains("analyzer") ||
        lower.contains("meter") || lower.contains("dynamics") || lower.contains("gate") ||
        lower.contains("deesser") || lower.contains("de-esser") || lower.contains("pitch") ||
        lower.contains("maximizer") || lower.contains("stereo") || lower.contains("pan") ||
        lower.contains("tremolo") || lower.contains("vibrato") || lower.contains("rotary") ||
        lower.contains("cabinet") || lower.contains("amp") || lower.contains("waveshaper") ||
        lower.contains("bitcrush") || lower.contains("transient") || lower.contains("vocoder"))
        return "Effect";
    
    // MeldaProduction — all start with M, most are effects
    if (vendor == "MeldaProduction")
    {
        if (lower.contains("soundfactory") || lower.contains("drummer") || 
            lower.contains("oscillator"))
            return "Synth";
        return "Effect";
    }
    
    return {};
}

#if JUCE_WINDOWS
// =============================================================================
// VST2 Windows: Extract vendor from DLL version info resource (no loading!)
// Uses GetFileVersionInfo — reads PE resources without executing DllMain
// =============================================================================
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "version.lib")
#endif

static juce::String extractDLLVendor(const juce::String& dllPath)
{
    DWORD dummy = 0;
    DWORD versionSize = GetFileVersionInfoSizeW(dllPath.toWideCharPointer(), &dummy);
    if (versionSize == 0) return {};
    
    std::vector<BYTE> versionData(versionSize);
    if (!GetFileVersionInfoW(dllPath.toWideCharPointer(), 0, versionSize, versionData.data()))
        return {};
    
    struct LangCodepage { WORD lang; WORD codepage; };
    LangCodepage* translations = nullptr;
    UINT translationSize = 0;
    
    VerQueryValueW(versionData.data(), L"\\VarFileInfo\\Translation",
                   (LPVOID*)&translations, &translationSize);
    
    auto tryQuery = [&](WORD lang, WORD cp) -> juce::String {
        wchar_t subBlock[256];
        swprintf(subBlock, 256, L"\\StringFileInfo\\%04x%04x\\CompanyName", lang, cp);
        
        LPVOID value = nullptr;
        UINT valueLen = 0;
        if (VerQueryValueW(versionData.data(), subBlock, &value, &valueLen) && valueLen > 0)
            return juce::String((const wchar_t*)value);
        return {};
    };
    
    // Try translations from the DLL
    int numTranslations = translationSize / sizeof(LangCodepage);
    for (int i = 0; i < numTranslations; i++)
    {
        auto result = tryQuery(translations[i].lang, translations[i].codepage);
        if (result.isNotEmpty()) return result;
    }
    
    // Try common fallbacks
    auto result = tryQuery(0x0409, 0x04E4);  // English, Windows Multilingual
    if (result.isNotEmpty()) return result;
    result = tryQuery(0x0409, 0x04B0);        // English, Unicode
    if (result.isNotEmpty()) return result;
    result = tryQuery(0x0000, 0x04E4);        // Neutral, Windows Multilingual
    if (result.isNotEmpty()) return result;
    
    return {};
}

static juce::String extractDLLVersion(const juce::String& dllPath)
{
    DWORD dummy = 0;
    DWORD versionSize = GetFileVersionInfoSizeW(dllPath.toWideCharPointer(), &dummy);
    if (versionSize == 0) return {};
    
    std::vector<BYTE> versionData(versionSize);
    if (!GetFileVersionInfoW(dllPath.toWideCharPointer(), 0, versionSize, versionData.data()))
        return {};
    
    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoSize = 0;
    if (VerQueryValueW(versionData.data(), L"\\", (LPVOID*)&fileInfo, &fileInfoSize) && fileInfo)
    {
        return juce::String((int)HIWORD(fileInfo->dwProductVersionMS)) + "." +
               juce::String((int)LOWORD(fileInfo->dwProductVersionMS)) + "." +
               juce::String((int)HIWORD(fileInfo->dwProductVersionLS)) + "." +
               juce::String((int)LOWORD(fileInfo->dwProductVersionLS));
    }
    
    return {};
}
#endif

// =============================================================================
// Main enrichment thread
// =============================================================================
void BackgroundPluginScanner::run() {
    auto types = processor.knownPluginList.getTypes();
    
    juce::Array<juce::PluginDescription> needsWork;
    for (const auto& desc : types) {
        if (needsEnrichment(desc))
            needsWork.add(desc);
    }
    
    totalToEnrich.store(needsWork.size());
    int actuallyEnriched = 0;
    
    for (int i = 0; i < needsWork.size(); ++i) {
        if (threadShouldExit()) break;
        
        auto desc = needsWork[i];
        bool changed = false;
        
        // =================================================================
        // VST3: Try moduleinfo.json enrichment
        // =================================================================
        if (desc.pluginFormatName == "VST3")
        {
            juce::File vst3File(desc.fileOrIdentifier);
            if (vst3File.exists() && VST3ModuleScanner::hasModuleInfo(vst3File))
            {
                auto info = VST3ModuleScanner::scanPlugin(vst3File);
                if (info.isValid)
                {
                    if ((desc.manufacturerName.isEmpty() || desc.manufacturerName == "Unknown") && 
                        info.vendor.isNotEmpty() && info.vendor != "Unknown") {
                        desc.manufacturerName = info.vendor;
                        changed = true;
                    }
                    if ((desc.category.isEmpty() || desc.category == "Unknown") && 
                        info.category.isNotEmpty() && info.category != "Unknown") {
                        desc.category = info.category;
                        changed = true;
                    }
                    if (desc.version.isEmpty() && info.version.isNotEmpty()) {
                        desc.version = info.version;
                        changed = true;
                    }
                    if (!desc.isInstrument && info.isInstrument) {
                        desc.isInstrument = true;
                        desc.category = "Synth";
                        changed = true;
                    }
                }
            }
        }
        
        // =================================================================
        // VST2: Try DLL version info + path heuristics
        // =================================================================
        if (desc.pluginFormatName == "VST" || desc.pluginFormatName == "VST2")
        {
            #if JUCE_WINDOWS
            // Try Windows DLL version info first (most reliable, no loading)
            if (desc.manufacturerName.isEmpty() || desc.manufacturerName == "Unknown")
            {
                juce::String dllVendor = extractDLLVendor(desc.fileOrIdentifier);
                if (dllVendor.isNotEmpty()) {
                    desc.manufacturerName = dllVendor;
                    changed = true;
                }
            }
            
            if (desc.version.isEmpty() || desc.version == "Unknown")
            {
                juce::String dllVersion = extractDLLVersion(desc.fileOrIdentifier);
                if (dllVersion.isNotEmpty() && dllVersion != "0.0.0.0") {
                    desc.version = dllVersion;
                    changed = true;
                }
            }
            #endif
            
            // Fallback: path-based vendor detection (all platforms)
            if (desc.manufacturerName.isEmpty() || desc.manufacturerName == "Unknown")
            {
                juce::String pathVendor = guessVST2VendorFromPath(desc.fileOrIdentifier);
                if (pathVendor.isNotEmpty()) {
                    desc.manufacturerName = pathVendor;
                    changed = true;
                }
            }
            
            // Category guessing from name patterns
            if (desc.category.isEmpty() || desc.category == "Unknown")
            {
                juce::String guessedCategory = guessVST2Category(desc.name, desc.manufacturerName);
                if (guessedCategory.isNotEmpty()) {
                    desc.category = guessedCategory;
                    changed = true;
                }
            }
        }
        
        // =================================================================
        // Apply changes
        // =================================================================
        if (changed)
        {
            processor.knownPluginList.removeType(needsWork[i]);
            processor.knownPluginList.addType(desc);
            actuallyEnriched++;
        }
        
        enrichedCount.store(i + 1);
    }
    
    // =================================================================
    // Phase 2: Detect missing shared container components
    // e.g. Serum2.vst3 has both synth and FX — if only one was scanned,
    // add the missing one from moduleinfo.json
    // =================================================================
    if (!threadShouldExit())
    {
        auto allTypes = processor.knownPluginList.getTypes();
        
        // Collect unique VST3 bundle paths already in the list
        std::map<juce::String, juce::StringArray> vst3BundlePlugins;  // path -> [names]
        for (const auto& desc : allTypes) {
            if (desc.pluginFormatName == "VST3") {
                vst3BundlePlugins[desc.fileOrIdentifier].add(desc.name);
            }
        }
        
        // For each VST3 bundle, check moduleinfo.json for missing components
        for (const auto& pair : vst3BundlePlugins) {
            if (threadShouldExit()) break;
            
            juce::File vst3File(pair.first);
            if (!vst3File.exists() || !VST3ModuleScanner::hasModuleInfo(vst3File))
                continue;
            
            auto allModulePlugins = VST3ModuleScanner::scanAllPlugins(vst3File);
            
            for (const auto& modulePlugin : allModulePlugins) {
                // Check if this component is already in the known list
                bool found = false;
                for (const auto& existingName : pair.second) {
                    if (existingName.equalsIgnoreCase(modulePlugin.name)) {
                        found = true;
                        break;
                    }
                }
                
                if (!found && modulePlugin.isValid) {
                    // Missing component — add it from moduleinfo.json
                    auto newDesc = VST3ModuleScanner::toPluginDescription(modulePlugin);
                    processor.knownPluginList.addType(newDesc);
                    actuallyEnriched++;
                }
            }
        }
    }
    
    // Save if anything was enriched
    if (actuallyEnriched > 0) {
        if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml())
                userSettings->setValue("KnownPluginsV2", xml.get());
            userSettings->saveIfNeeded();
        }
    }
    
    finished.store(true);
    sendChangeMessage();
}

// Legacy method — kept for compatibility
void BackgroundPluginScanner::enrichPluginMetadata(juce::PluginDescription& desc) {
    juce::File vst3File(desc.fileOrIdentifier);
    if (!vst3File.exists()) return;
    if (!VST3ModuleScanner::hasModuleInfo(vst3File)) return;
    
    auto info = VST3ModuleScanner::scanPlugin(vst3File);
    if (info.isValid && info.vendor.isNotEmpty() && info.vendor != "Unknown") {
        auto enrichedDesc = VST3ModuleScanner::toPluginDescription(info);
        enrichedDesc.fileOrIdentifier = desc.fileOrIdentifier;
        enrichedDesc.pluginFormatName = desc.pluginFormatName;
        processor.knownPluginList.removeType(desc);
        processor.knownPluginList.addType(enrichedDesc);
    }
}
