// Out-of-process plugin scanner implementation
// FIX: Parallel scanning with 8 simultaneous child processes
// FIX: 15 second timeout for better coverage
// FIX: Aggressive cancel - instantly kills all child processes
// FIX: Skips 32-bit plugins automatically

#include "OutOfProcessScanner.h"
#include "VST3ModuleScanner.h"

// Debug logging disabled - was writing colosseum_parent_log.txt to Desktop
static void logParent(const juce::String& /*message*/)
{
    // No-op for release
}

// =============================================================================
// Check if a plugin is 32-bit by examining the PE header (Windows)
// =============================================================================
bool OutOfProcessScanner::is32BitPlugin(const juce::String& filePath)
{
    juce::File file(filePath);
    
    // For VST3 bundles, check the actual DLL inside
    if (file.isDirectory() && filePath.endsWithIgnoreCase(".vst3"))
    {
        // VST3 bundle structure: plugin.vst3/Contents/x86_64-win/plugin.vst3
        juce::File x64Dll = file.getChildFile("Contents").getChildFile("x86_64-win");
        if (x64Dll.isDirectory())
            return false;  // Has 64-bit version
        
        juce::File x86Dll = file.getChildFile("Contents").getChildFile("x86-win");
        if (x86Dll.isDirectory())
            return true;   // Only has 32-bit version
        
        return false;  // Unknown structure, try anyway
    }
    
    // Check "Program Files (x86)" path - strong indicator of 32-bit
    if (filePath.containsIgnoreCase("Program Files (x86)"))
        return true;
    
    // For DLL files, check PE header
    if (filePath.endsWithIgnoreCase(".dll"))
    {
        juce::FileInputStream stream(file);
        if (!stream.openedOk() || stream.getTotalLength() < 512)
            return false;
        
        // Read DOS header
        char dosHeader[64];
        stream.read(dosHeader, 64);
        
        // Check DOS signature "MZ"
        if (dosHeader[0] != 'M' || dosHeader[1] != 'Z')
            return false;
        
        // Get PE header offset (at offset 0x3C)
        int peOffset = *reinterpret_cast<int*>(&dosHeader[0x3C]);
        if (peOffset < 0 || peOffset > 1024)
            return false;
        
        // Seek to PE header
        stream.setPosition(peOffset);
        
        // Read PE signature and machine type
        char peHeader[6];
        stream.read(peHeader, 6);
        
        // Check PE signature "PE\0\0"
        if (peHeader[0] != 'P' || peHeader[1] != 'E' || peHeader[2] != 0 || peHeader[3] != 0)
            return false;
        
        // Machine type is at offset 4-5 after PE signature
        unsigned short machineType = *reinterpret_cast<unsigned short*>(&peHeader[4]);
        
        // 0x014c = i386 (32-bit)
        // 0x8664 = AMD64 (64-bit)
        return (machineType == 0x014c);
    }
    
    return false;  // Unknown, try anyway
}

OutOfProcessScanner::OutOfProcessScanner(SubterraneumAudioProcessor& p)
    : Thread("OOPScanner"), processor(p)
{
    activeJobs.resize(maxParallelScans);
}

OutOfProcessScanner::~OutOfProcessScanner()
{
    cancelNow();
}

void OutOfProcessScanner::setPluginsToScan(const juce::Array<PluginToScan>& plugins)
{
    allPlugins = plugins;
    totalCount.store(plugins.size());
    scannedCount.store(0);
    progress.store(0.0f);
    scanFinished.store(false);
    cancelRequested.store(false);
    skippedKnown.store(0);
    resolvedByJson.store(0);
    resolvedByOOP.store(0);
    failedOOP.store(0);
}

void OutOfProcessScanner::startScanning()
{
    scanFinished.store(false);
    cancelRequested.store(false);
    processor.knownPluginList.clearBlacklistedFiles();
    startThread();
}

void OutOfProcessScanner::stopScanning()
{
    cancelNow();
}

void OutOfProcessScanner::cancelNow()
{
    logParent("!!! CANCEL NOW - killing all processes !!!");
    
    cancelRequested.store(true);
    signalThreadShouldExit();
    
    killAllJobs();
    
    scanFinished.store(true);
    
    waitForThreadToExit(500);
    
    if (isThreadRunning())
    {
        logParent("Thread still running - forcing exit");
        waitForThreadToExit(100);
    }
    
    sendChangeMessage();
    logParent("Cancel complete");
}

void OutOfProcessScanner::killAllJobs()
{
    juce::ScopedLock lock(jobsLock);
    
    for (auto& job : activeJobs)
    {
        if (job.process != nullptr)
        {
            logParent("Killing child: " + job.plugin.filePath);
            job.process->kill();
            job.process.reset();
        }
        
        if (job.resultFile.existsAsFile())
            job.resultFile.deleteFile();
        
        job.completed = true;
    }
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

bool OutOfProcessScanner::looksLikeCategory(const juce::String& name) {
    auto lower = name.toLowerCase();
    
    if (lower.length() <= 3 && lower.containsChar(':')) return true;
    
    static const char* generics[] = {
        "vst3", "vst", "vst2", "au", "components", "ladspa", "clap",
        "common files", "program files", "program files (x86)",
        "vstplugins", "audio", "plug-ins", "plugins",
        "effects", "instruments", "synths", "synthesizers",
        "dynamics", "eq", "reverb", "reverbs", "delay", "delays",
        "compressors", "limiters", "analyzers", "generators",
        "modulators", "distortion", "filter", "filters",
        "mastering", "mixing", "utilities", "utility", "tools",
        "steinberg", "library", "lib", "usr", "local",
        "home", "root", "applications", "opt",
        // MeldaProduction and common vendor subfolder category names
        "time", "stereo", "modulation", "pitch", "space",
        "imaging", "loudness", "analysis", "creative",
        "saturation", "chorus", "flanger", "phaser",
        "x86_64-win", "x86_64-linux", "arm64-macos", "x86_64-macos",
        "contents", "resources", "macos", "windows", "linux",
        nullptr
    };
    
    for (int i = 0; generics[i] != nullptr; ++i) {
        if (lower == generics[i]) return true;
    }
    
    if (lower.startsWith("plug-ins v")) return true;
    
    return false;
}

juce::String OutOfProcessScanner::extractVendorFromPath(const juce::String& filePath) {
    juce::File pluginFile(filePath);
    juce::File dir = pluginFile.getParentDirectory();
    
    for (int depth = 0; depth < 6 && dir.exists(); ++depth) {
        juce::String dirName = dir.getFileName();
        if (dirName.isNotEmpty() && !looksLikeCategory(dirName)) {
            return dirName;
        }
        dir = dir.getParentDirectory();
    }
    
    return "Unknown";
}

// =============================================================================
// Main scan thread - PARALLEL scanning with 32-bit skip
// =============================================================================
void OutOfProcessScanner::run() {
    logParent("=== PARALLEL SCAN STARTED ===");
    logParent("Max parallel: " + juce::String(maxParallelScans));
    logParent("Timeout: " + juce::String(childTimeoutMs) + "ms");
    logParent("Total plugins: " + juce::String(allPlugins.size()));
    
    phase.store(3);
    {
        juce::ScopedLock lock(currentPluginLock);
        currentPhaseText = "Scanning plugins (8 parallel)...";
    }
    
    {
        juce::ScopedLock lock(jobsLock);
        for (auto& job : activeJobs) {
            job.completed = true;
            job.process.reset();
        }
    }
    
    int nextPluginIndex = 0;
    int completedCount = 0;
    int skipped32Bit = 0;
    
    while (completedCount < allPlugins.size())
    {
        if (threadShouldExit() || cancelRequested.load())
        {
            logParent("Cancel detected in main loop");
            killAllJobs();
            break;
        }
        
        {
            juce::ScopedLock lock(jobsLock);
            
            for (auto& job : activeJobs)
            {
                if (cancelRequested.load()) break;
                
                if (job.completed && nextPluginIndex < allPlugins.size())
                {
                    const auto& plugin = allPlugins[nextPluginIndex];
                    
                    {
                        juce::ScopedLock plock(currentPluginLock);
                        juce::File f(plugin.filePath);
                        currentPluginName = f.getFileNameWithoutExtension() + 
                            " [" + juce::String(nextPluginIndex + 1) + "/" + juce::String(allPlugins.size()) + "]";
                    }
                    
                    // ==========================================================
                    // SKIP 32-BIT PLUGINS
                    // ==========================================================
                    if (is32BitPlugin(plugin.filePath))
                    {
                        logParent("SKIPPED 32-bit [" + juce::String(nextPluginIndex) + "]: " + plugin.filePath);
                        skipped32Bit++;
                        job.completed = true;
                        completedCount++;
                        scannedCount.store(completedCount);
                        nextPluginIndex++;
                        continue;
                    }
                    
                    if (launchScanJob(plugin, job))
                    {
                        logParent("Launched [" + juce::String(nextPluginIndex) + "]: " + plugin.filePath);
                    }
                    else
                    {
                        logParent("Launch FAILED [" + juce::String(nextPluginIndex) + "]: " + plugin.filePath);
                        addPluginWithFallbackInfo(plugin);
                        failedOOP.store(failedOOP.load() + 1);
                        job.completed = true;
                        completedCount++;
                        scannedCount.store(completedCount);
                    }
                    
                    nextPluginIndex++;
                }
            }
        }
        
        if (cancelRequested.load())
        {
            logParent("Cancel detected after launching");
            killAllJobs();
            break;
        }
        
        {
            juce::ScopedLock lock(jobsLock);
            
            for (auto& job : activeJobs)
            {
                if (cancelRequested.load()) break;
                
                if (!job.completed)
                {
                    if (checkJobComplete(job))
                    {
                        processCompletedJob(job);
                        completedCount++;
                        scannedCount.store(completedCount);
                        progress.store((float)completedCount / (float)juce::jmax(1, allPlugins.size()));
                        sendChangeMessage();
                    }
                }
            }
        }
        
        if (cancelRequested.load())
        {
            logParent("Cancel detected after checking jobs");
            killAllJobs();
            break;
        }
        
        Thread::sleep(pollIntervalMs);
    }
    
    killAllJobs();
    
    // Shared container components are now handled per-plugin in parseChildResult()
    // and addPluginWithFallbackInfo() with proper CID-based UIDs from moduleinfo.json.
    // No final sweep needed.
    
    logParent("=== PARALLEL SCAN FINISHED ===");
    logParent("Success: " + juce::String(resolvedByOOP.load()) + 
              ", Failed: " + juce::String(failedOOP.load()) +
              ", Skipped 32-bit: " + juce::String(skipped32Bit));
    
    scanFinished.store(true);
    progress.store(1.0f);
    sendChangeMessage();
}

bool OutOfProcessScanner::launchScanJob(const PluginToScan& plugin, ScanJob& job) {
    if (cancelRequested.load()) return false;
    
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    job.resultFile = tempDir.getChildFile("colosseum_scan_" 
        + juce::String(juce::Random::getSystemRandom().nextInt64()) + ".json");
    
    juce::String exePath = getExecutablePath();
    juce::String cmdLine = "\"" + exePath + "\" --scan-plugin "
        + "\"" + plugin.filePath + "\" "
        + "\"" + plugin.formatName + "\" "
        + "\"" + job.resultFile.getFullPathName() + "\"";
    
    job.plugin = plugin;
    job.process = std::make_unique<juce::ChildProcess>();
    job.startTimeMs = juce::Time::currentTimeMillis();
    job.completed = false;
    job.success = false;
    
    if (!job.process->start(cmdLine))
    {
        job.process.reset();
        job.resultFile.deleteFile();
        job.completed = true;
        return false;
    }
    
    return true;
}

bool OutOfProcessScanner::checkJobComplete(ScanJob& job) {
    if (job.completed)
        return false;
    
    if (cancelRequested.load())
    {
        job.completed = true;
        if (job.process) job.process->kill();
        return true;
    }
    
    int64_t elapsed = juce::Time::currentTimeMillis() - job.startTimeMs;
    
    if (job.resultFile.existsAsFile() && job.resultFile.getSize() > 10)
    {
        job.success = true;
        job.completed = true;
        logParent("  Completed in " + juce::String(elapsed) + "ms: " + job.plugin.filePath);
        return true;
    }
    
    if (elapsed >= childTimeoutMs)
    {
        job.success = false;
        job.completed = true;
        logParent("  TIMEOUT after " + juce::String(elapsed) + "ms: " + job.plugin.filePath);
        return true;
    }
    
    return false;
}

void OutOfProcessScanner::processCompletedJob(ScanJob& job) {
    if (cancelRequested.load())
    {
        if (job.process) { job.process->kill(); job.process.reset(); }
        job.resultFile.deleteFile();
        return;
    }
    
    if (job.process != nullptr)
    {
        job.process->kill();
        job.process.reset();
    }
    
    if (job.success)
    {
        Thread::sleep(30);
        
        if (parseChildResult(job.resultFile, job.plugin))
        {
            resolvedByOOP.store(resolvedByOOP.load() + 1);
        }
        else
        {
            logParent("  Parse FAILED: " + job.plugin.filePath);
            addPluginWithFallbackInfo(job.plugin);
            failedOOP.store(failedOOP.load() + 1);
        }
    }
    else
    {
        addPluginWithFallbackInfo(job.plugin);
        failedOOP.store(failedOOP.load() + 1);
    }
    
    job.resultFile.deleteFile();
}

bool OutOfProcessScanner::parseChildResult(const juce::File& resultFile, const PluginToScan& plugin) {
    if (cancelRequested.load()) return false;
    
    juce::String content = resultFile.loadFileAsString();
    if (content.isEmpty())
        return false;
    
    juce::var parsed = juce::JSON::parse(content);
    if (!parsed.isObject())
        return false;
    
    auto* root = parsed.getDynamicObject();
    if (!root)
        return false;
    
    bool success = root->getProperty("success");
    if (!success)
        return false;
    
    auto* pluginsArray = root->getProperty("plugins").getArray();
    if (!pluginsArray || pluginsArray->isEmpty())
        return false;
    
    const juce::MessageManagerLock mml(this);
    if (!mml.lockWasGained())
        return false;
    
    auto existingTypes = processor.knownPluginList.getTypes();
    
    // Build a set of names from the worker result so we only remove entries
    // that the worker is about to replace — NOT other components from the same bundle
    juce::StringArray workerPluginNames;
    for (const auto& entry : *pluginsArray) {
        if (auto* obj = entry.getDynamicObject())
            workerPluginNames.add(obj->getProperty("name").toString());
    }
    
    for (const auto& existing : existingTypes) {
        if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath) &&
            existing.pluginFormatName == plugin.formatName &&
            workerPluginNames.contains(existing.name, true)) {
            processor.knownPluginList.removeType(existing);
        }
    }
    
    processor.knownPluginList.clearBlacklistedFiles();
    
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
            
            if (desc.manufacturerName.isEmpty() || 
                desc.manufacturerName.equalsIgnoreCase("Unknown") ||
                looksLikeCategory(desc.manufacturerName)) {
                juce::String pathVendor = extractVendorFromPath(desc.fileOrIdentifier);
                if (pathVendor != "Unknown") {
                    desc.manufacturerName = pathVendor;
                }
            }
            
            processor.knownPluginList.addType(desc);
        }
    }
    
    // =========================================================================
    // Shared container check: if this is a VST3 with moduleinfo.json,
    // verify all Audio Module Classes were found. Add missing ones.
    // e.g. Serum2.vst3 = synth + FX, worker may only return one.
    // =========================================================================
    if (plugin.formatName == "VST3")
    {
        juce::File vst3File(plugin.filePath);
        if (VST3ModuleScanner::hasModuleInfo(vst3File))
        {
            auto allModulePlugins = VST3ModuleScanner::scanAllPlugins(vst3File);
            
            for (const auto& modulePlugin : allModulePlugins)
            {
                if (!modulePlugin.isValid) continue;
                
                // Check if this component was already added
                bool found = false;
                for (const auto& existing : processor.knownPluginList.getTypes()) {
                    if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath) &&
                        existing.name.equalsIgnoreCase(modulePlugin.name)) {
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    auto missingDesc = VST3ModuleScanner::toPluginDescription(modulePlugin);
                    processor.knownPluginList.addType(missingDesc);
                }
            }
        }
    }
    
    return true;
}

void OutOfProcessScanner::addPluginWithFallbackInfo(const PluginToScan& plugin) {
    if (cancelRequested.load()) return;
    
    const juce::MessageManagerLock mml(this);
    if (!mml.lockWasGained())
        return;
    
    juce::File pluginFile(plugin.filePath);
    
    // =========================================================================
    // VST3: Try moduleinfo.json FIRST — gets proper name, vendor, category
    // without loading the plugin binary. Handles shared containers too.
    // Now with CID parsing for correct uniqueId computation.
    // =========================================================================
    if (plugin.formatName == "VST3" && VST3ModuleScanner::hasModuleInfo(pluginFile))
    {
        auto allInfos = VST3ModuleScanner::scanAllPlugins(pluginFile);
        
        bool anyAdded = false;
        for (const auto& info : allInfos)
        {
            if (!info.isValid) continue;
            
            // Check this specific component doesn't already exist
            bool exists = false;
            for (const auto& existing : processor.knownPluginList.getTypes()) {
                if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath) &&
                    existing.name.equalsIgnoreCase(info.name)) {
                    exists = true;
                    break;
                }
            }
            if (exists) continue;
            
            auto desc = VST3ModuleScanner::toPluginDescription(info);
            processor.knownPluginList.addType(desc);
            anyAdded = true;
        }
        
        if (anyAdded)
            return;  // Successfully added from moduleinfo.json
    }
    
    // =========================================================================
    // Fallback: filename + path-based vendor (original behavior)
    // Only add if this exact name doesn't already exist for this file
    // =========================================================================
    juce::String fallbackName = pluginFile.getFileNameWithoutExtension();
    for (const auto& existing : processor.knownPluginList.getTypes()) {
        if (existing.fileOrIdentifier.equalsIgnoreCase(plugin.filePath) &&
            existing.name.equalsIgnoreCase(fallbackName)) {
            return;  // Already have this one
        }
    }
    
    juce::PluginDescription desc;
    desc.fileOrIdentifier = plugin.filePath;
    desc.pluginFormatName = plugin.formatName;
    desc.name = pluginFile.getFileNameWithoutExtension();
    desc.manufacturerName = extractVendorFromPath(plugin.filePath);
    desc.version = "";
    desc.category = "Unknown";
    desc.isInstrument = false;
    desc.numInputChannels = 2;
    desc.numOutputChannels = 2;
    desc.uniqueId = 0;
    desc.deprecatedUid = 0;
    
    processor.knownPluginList.addType(desc);
}

void OutOfProcessScanner::cleanupTempFiles() {
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::Array<juce::File> staleFiles;
    tempDir.findChildFiles(staleFiles, juce::File::findFiles, false, "colosseum_scan_*.json");
    for (const auto& f : staleFiles)
        f.deleteFile();
}