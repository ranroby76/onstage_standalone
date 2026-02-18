// #D:\Workspace\Subterraneum_plugins_daw\src\OutOfProcessScanner.cpp
// Out-of-process plugin scanner — ZERO FREEZES, continuous visual feedback
//
// Phase 1: Skip plugins already known with good metadata
// Phase 2: Read moduleinfo.json for VST3 plugins (instant, no loading)
// Phase 3: Spawn child process per remaining plugin (crash-safe)
//
// ALL work runs on background thread. UI reads atomics via timer.
// If child crashes → main app is fine, plugin passes with filename info.

#include "OutOfProcessScanner.h"
#include "VST3ModuleScanner.h"

OutOfProcessScanner::OutOfProcessScanner(SubterraneumAudioProcessor& p)
    : Thread("OutOfProcessScanner"), processor(p)
{
}

OutOfProcessScanner::~OutOfProcessScanner() {
    stopScanning();
}

void OutOfProcessScanner::setPluginsToScan(const juce::Array<PluginToScan>& plugins) {
    allPlugins = plugins;
    totalCount.store(plugins.size());
}

void OutOfProcessScanner::startScanning() {
    if (isThreadRunning()) return;
    
    scannedCount.store(0);
    progress.store(0.0f);
    phase.store(0);
    skippedKnown.store(0);
    resolvedByJson.store(0);
    resolvedByOOP.store(0);
    failedOOP.store(0);
    scanFinished.store(false);
    
    startThread();
}

void OutOfProcessScanner::stopScanning() {
    signalThreadShouldExit();
    waitForThreadToExit(5000);
}

juce::String OutOfProcessScanner::getCurrentPlugin() const {
    juce::ScopedLock lock(currentPluginLock);
    return currentPluginName;
}

juce::String OutOfProcessScanner::getCurrentPhaseText() const {
    juce::ScopedLock lock(currentPluginLock);
    return currentPhaseText;
}

juce::String OutOfProcessScanner::getExecutablePath() const {
    return juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getFullPathName();
}

// =============================================================================
// Main scan thread — runs all 3 phases
// =============================================================================
void OutOfProcessScanner::run() {
    runPhase1_SkipKnown();
    if (threadShouldExit()) { scanFinished.store(true); sendChangeMessage(); return; }
    
    runPhase2_JsonMetadata();
    if (threadShouldExit()) { scanFinished.store(true); sendChangeMessage(); return; }
    
    runPhase3_OutOfProcess();
    
    scanFinished.store(true);
    progress.store(1.0f);
    sendChangeMessage();
}

// =============================================================================
// Phase 1: Skip plugins that already have good metadata
// =============================================================================
void OutOfProcessScanner::runPhase1_SkipKnown() {
    phase.store(1);
    {
        juce::ScopedLock lock(currentPluginLock);
        currentPhaseText = "Phase 1: Checking known plugins...";
    }
    
    needsJsonScan.clear();
    
    for (int i = 0; i < allPlugins.size(); ++i) {
        if (threadShouldExit()) return;
        
        const auto& plugin = allPlugins[i];
        
        {
            juce::ScopedLock lock(currentPluginLock);
            juce::File f(plugin.filePath);
            currentPluginName = f.getFileNameWithoutExtension();
        }
        
        if (hasGoodMetadata(plugin.filePath, plugin.formatName)) {
            skippedKnown.store(skippedKnown.load() + 1);
        } else {
            needsJsonScan.add(plugin);
        }
        
        scannedCount.store(i + 1);
        progress.store((float)(i + 1) / (float)allPlugins.size() * 0.1f); // Phase 1 = 0-10%
        sendChangeMessage();
    }
}

// =============================================================================
// Phase 2: Try moduleinfo.json for VST3 plugins
// =============================================================================
void OutOfProcessScanner::runPhase2_JsonMetadata() {
    phase.store(2);
    {
        juce::ScopedLock lock(currentPluginLock);
        currentPhaseText = "Phase 2: Reading plugin metadata...";
    }
    
    needsOOPScan.clear();
    
    for (int i = 0; i < needsJsonScan.size(); ++i) {
        if (threadShouldExit()) return;
        
        const auto& plugin = needsJsonScan[i];
        
        {
            juce::ScopedLock lock(currentPluginLock);
            juce::File f(plugin.filePath);
            currentPluginName = f.getFileNameWithoutExtension();
        }
        
        bool resolved = false;
        
        // VST3: Try moduleinfo.json
        if (plugin.formatName == "VST3") {
            juce::File vst3File(plugin.filePath);
            
            if (VST3ModuleScanner::hasModuleInfo(vst3File)) {
                auto info = VST3ModuleScanner::scanPlugin(vst3File);
                
                if (info.isValid && info.vendor.isNotEmpty() && info.vendor != "Unknown") {
                    // Good metadata from JSON — add to list
                    auto desc = VST3ModuleScanner::toPluginDescription(info);
                    
                    // Remove any existing entry with same path
                    for (const auto& existing : processor.knownPluginList.getTypes()) {
                        if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath)) {
                            processor.knownPluginList.removeType(existing);
                            break;
                        }
                    }
                    
                    processor.knownPluginList.addType(desc);
                    resolvedByJson.store(resolvedByJson.load() + 1);
                    resolved = true;
                }
            }
        }
        
        if (!resolved) {
            needsOOPScan.add(plugin);
        }
        
        float phaseProgress = (float)(i + 1) / (float)juce::jmax(1, needsJsonScan.size());
        progress.store(0.1f + phaseProgress * 0.1f); // Phase 2 = 10-20%
        sendChangeMessage();
    }
}

// =============================================================================
// Phase 3: Out-of-process scanning for remaining plugins
// =============================================================================
void OutOfProcessScanner::runPhase3_OutOfProcess() {
    phase.store(3);
    {
        juce::ScopedLock lock(currentPluginLock);
        currentPhaseText = "Phase 3: Deep scanning plugins...";
    }
    
    for (int i = 0; i < needsOOPScan.size(); ++i) {
        if (threadShouldExit()) return;
        
        const auto& plugin = needsOOPScan[i];
        
        {
            juce::ScopedLock lock(currentPluginLock);
            juce::File f(plugin.filePath);
            currentPluginName = f.getFileNameWithoutExtension();
        }
        
        bool success = scanPluginViaChildProcess(plugin);
        
        if (!success) {
            // Child crashed or timed out — add with filename-based info anyway
            addPluginWithFallbackInfo(plugin);
            failedOOP.store(failedOOP.load() + 1);
        } else {
            resolvedByOOP.store(resolvedByOOP.load() + 1);
        }
        
        float phaseProgress = (float)(i + 1) / (float)juce::jmax(1, needsOOPScan.size());
        progress.store(0.2f + phaseProgress * 0.8f); // Phase 3 = 20-100%
        sendChangeMessage();
        
        // Small breathing room between child processes
        Thread::sleep(50);
    }
}

// =============================================================================
// Spawn a child process to scan one plugin
// =============================================================================
bool OutOfProcessScanner::scanPluginViaChildProcess(const PluginToScan& plugin) {
    // Create temp file for result
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File resultFile = tempDir.getChildFile("colosseum_scan_" 
        + juce::String(juce::Random::getSystemRandom().nextInt64()) + ".json");
    
    // Build command line
    juce::String exePath = getExecutablePath();
    
    // Quote paths that may contain spaces
    juce::String cmdLine = "\"" + exePath + "\" --scan-plugin "
        + "\"" + plugin.filePath + "\" "
        + "\"" + plugin.formatName + "\" "
        + "\"" + resultFile.getFullPathName() + "\"";
    
    // Spawn child process
    juce::ChildProcess child;
    bool started = child.start(cmdLine);
    
    if (!started) {
        resultFile.deleteFile();
        return false;
    }
    
    // Wait for child to finish with timeout
    bool finished = child.waitForProcessToFinish(childTimeoutMs);
    
    if (!finished) {
        // Child is hanging — kill it
        child.kill();
        resultFile.deleteFile();
        return false;
    }
    
    // Read and parse result
    if (resultFile.existsAsFile()) {
        bool success = parseChildResult(resultFile, plugin);
        resultFile.deleteFile();
        return success;
    }
    
    // No result file = child crashed before writing
    return false;
}

// =============================================================================
// Parse JSON result from child process
// =============================================================================
bool OutOfProcessScanner::parseChildResult(const juce::File& resultFile, const PluginToScan& plugin) {
    juce::String content = resultFile.loadFileAsString();
    if (content.isEmpty()) return false;
    
    juce::var parsed = juce::JSON::parse(content);
    if (!parsed.isObject()) return false;
    
    auto* root = parsed.getDynamicObject();
    if (!root) return false;
    
    bool success = root->getProperty("success");
    if (!success) return false;
    
    auto* pluginsArray = root->getProperty("plugins").getArray();
    if (!pluginsArray || pluginsArray->isEmpty()) return false;
    
    // Remove any existing entry with same path
    for (const auto& existing : processor.knownPluginList.getTypes()) {
        if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath) &&
            existing.pluginFormatName == plugin.formatName) {
            processor.knownPluginList.removeType(existing);
            break;
        }
    }
    
    // Add all plugin descriptions from the result
    for (const auto& entry : *pluginsArray) {
        if (auto* obj = entry.getDynamicObject()) {
            juce::PluginDescription desc;
            desc.name              = obj->getProperty("name").toString();
            desc.manufacturerName  = obj->getProperty("vendor").toString();
            desc.category          = obj->getProperty("category").toString();
            desc.version           = obj->getProperty("version").toString();
            desc.fileOrIdentifier  = obj->getProperty("fileOrIdentifier").toString();
            desc.pluginFormatName  = obj->getProperty("pluginFormatName").toString();
            desc.isInstrument      = (bool)obj->getProperty("isInstrument");
            desc.numInputChannels  = (int)obj->getProperty("numInputChannels");
            desc.numOutputChannels = (int)obj->getProperty("numOutputChannels");
            desc.uniqueId          = (int)obj->getProperty("uniqueId");
            desc.deprecatedUid     = (int)obj->getProperty("deprecatedUid");
            desc.hasSharedContainer = (bool)obj->getProperty("hasSharedContainer");
            
            processor.knownPluginList.addType(desc);
        }
    }
    
    return true;
}

// =============================================================================
// Add plugin with fallback info (child crashed or timed out)
// Plugin still passes! Just with limited metadata until user loads it.
// =============================================================================
void OutOfProcessScanner::addPluginWithFallbackInfo(const PluginToScan& plugin) {
    // Check if already in list
    for (const auto& existing : processor.knownPluginList.getTypes()) {
        if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath) &&
            existing.pluginFormatName == plugin.formatName) {
            return; // Already there
        }
    }
    
    juce::File pluginFile(plugin.filePath);
    
    juce::PluginDescription desc;
    desc.fileOrIdentifier = plugin.filePath;
    desc.pluginFormatName = plugin.formatName;
    desc.name = pluginFile.getFileNameWithoutExtension();
    desc.manufacturerName = "Unknown";
    desc.version = "";
    desc.category = "Unknown";
    desc.isInstrument = false;
    desc.numInputChannels = 2;
    desc.numOutputChannels = 2;
    desc.uniqueId = plugin.filePath.hashCode();
    desc.deprecatedUid = desc.uniqueId;
    
    processor.knownPluginList.addType(desc);
}

// =============================================================================
// Check if plugin already has good metadata in the known list
// =============================================================================
bool OutOfProcessScanner::hasGoodMetadata(const juce::String& filePath, const juce::String& formatName) {
    for (const auto& existing : processor.knownPluginList.getTypes()) {
        if (existing.fileOrIdentifier.equalsIgnoreCase(filePath) &&
            existing.pluginFormatName == formatName) {
            // Has good metadata if vendor is known and not "Unknown"
            if (existing.manufacturerName.isNotEmpty() && 
                existing.manufacturerName != "Unknown") {
                return true;
            }
        }
    }
    return false;
}
