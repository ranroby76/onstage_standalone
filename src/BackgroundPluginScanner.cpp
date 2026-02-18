// #D:\Workspace\Subterraneum_plugins_daw\src\BackgroundPluginScanner.cpp
// Background metadata enrichment — reads moduleinfo.json only, NEVER loads plugins
// Runs after the main scan to fill in any "Unknown" vendor/category fields
// for VST3 plugins that have moduleinfo.json but weren't caught in Phase 2

#include "BackgroundPluginScanner.h"
#include "VST3ModuleScanner.h"

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

void BackgroundPluginScanner::run() {
    // Collect plugins that need enrichment
    auto types = processor.knownPluginList.getTypes();
    
    juce::Array<juce::PluginDescription> needsEnrichment;
    for (const auto& desc : types) {
        if (desc.pluginFormatName == "VST3" && 
            (desc.manufacturerName.isEmpty() || desc.manufacturerName == "Unknown")) {
            needsEnrichment.add(desc);
        }
    }
    
    totalToEnrich.store(needsEnrichment.size());
    
    for (int i = 0; i < needsEnrichment.size(); ++i) {
        if (threadShouldExit()) break;
        
        auto desc = needsEnrichment[i];
        enrichPluginMetadata(desc);
        enrichedCount.store(i + 1);
    }
    
    // Save if anything was enriched
    if (enrichedCount.load() > 0) {
        if (auto* userSettings = processor.appProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml())
                userSettings->setValue("KnownPluginsV2", xml.get());
            userSettings->saveIfNeeded();
        }
    }
    
    finished.store(true);
    sendChangeMessage();
}

void BackgroundPluginScanner::enrichPluginMetadata(juce::PluginDescription& desc) {
    juce::File vst3File(desc.fileOrIdentifier);
    
    if (!vst3File.exists()) return;
    if (!VST3ModuleScanner::hasModuleInfo(vst3File)) return;
    
    auto info = VST3ModuleScanner::scanPlugin(vst3File);
    
    if (info.isValid && info.vendor.isNotEmpty() && info.vendor != "Unknown") {
        // Update the description with enriched metadata
        auto enrichedDesc = VST3ModuleScanner::toPluginDescription(info);
        
        // Preserve the original fileOrIdentifier and format
        enrichedDesc.fileOrIdentifier = desc.fileOrIdentifier;
        enrichedDesc.pluginFormatName = desc.pluginFormatName;
        
        // Remove old, add enriched
        processor.knownPluginList.removeType(desc);
        processor.knownPluginList.addType(enrichedDesc);
    }
}
