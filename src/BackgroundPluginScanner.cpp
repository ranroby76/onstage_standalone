// #D:\Workspace\Subterraneum_plugins_daw\src\BackgroundPluginScanner.cpp
// Background metadata scanner - loads plugins one-by-one with timeout
// Updates vendor, type, format info without blocking UI
// FIXED: Improved instrument detection from VST3 subcategories

#include "BackgroundPluginScanner.h"
#include "VST3ModuleScanner.h"

BackgroundPluginScanner::BackgroundPluginScanner(SubterraneumAudioProcessor& p)
    : Thread("BackgroundPluginScanner"), processor(p)
{
}

BackgroundPluginScanner::~BackgroundPluginScanner() {
    stopScanning();
}

void BackgroundPluginScanner::startScanning() {
    if (isThreadRunning()) return;
    
    scannedCount.store(0);
    totalCount.store(processor.knownPluginList.getNumTypes());
    progress.store(0.0f);
    
    startThread();
}

void BackgroundPluginScanner::stopScanning() {
    signalThreadShouldExit();
    waitForThreadToExit(3000);
}

juce::String BackgroundPluginScanner::getCurrentPlugin() const {
    juce::ScopedLock lock(currentPluginLock);
    return currentPluginName;
}

void BackgroundPluginScanner::run() {
    auto types = processor.knownPluginList.getTypes();
    totalCount.store(types.size());
    
    for (int i = 0; i < types.size(); ++i) {
        if (threadShouldExit()) break;
        
        const auto& desc = types[i];
        
        {
            juce::ScopedLock lock(currentPluginLock);
            currentPluginName = desc.name;
        }
        
        // Try to get metadata for this plugin
        scanPluginWithTimeout(desc);
        
        scannedCount.store(i + 1);
        progress.store((float)(i + 1) / (float)types.size());
        
        // Notify listeners of progress
        sendChangeMessage();
        
        // Small delay to prevent CPU overload
        Thread::sleep(10);
    }
    
    progress.store(1.0f);
    sendChangeMessage();
}

// =============================================================================
// Helper: Check if string indicates an instrument
// =============================================================================
static bool isInstrumentCategory(const juce::String& category, const juce::String& subCategories) {
    juce::String catLower = category.toLowerCase();
    juce::String subLower = subCategories.toLowerCase();
    
    // Check standard VST3 instrument indicators
    if (catLower.contains("instrument")) return true;
    if (catLower.contains("synth")) return true;
    if (catLower.contains("vsti")) return true;
    
    // Check subcategories (VST3 standard)
    if (subLower.contains("instrument")) return true;
    if (subLower.contains("synth")) return true;
    if (subLower.contains("sampler")) return true;
    if (subLower.contains("drum")) return true;
    if (subLower.contains("piano")) return true;
    if (subLower.contains("organ")) return true;
    if (subLower.contains("generator")) return true;
    
    // Also check for "|Instrument" tag format used by VST3
    if (subLower.contains("|instrument")) return true;
    
    return false;
}

bool BackgroundPluginScanner::scanPluginWithTimeout(const juce::PluginDescription& desc) {
    // =========================================================================
    // First try moduleinfo.json (fast, no loading)
    // =========================================================================
    if (desc.pluginFormatName == "VST3") {
        juce::File vst3File(desc.fileOrIdentifier);
        if (VST3ModuleScanner::hasModuleInfo(vst3File)) {
            auto info = VST3ModuleScanner::scanPlugin(vst3File);
            
            if (info.isValid) {
                // Update the plugin description with real metadata
                juce::PluginDescription updatedDesc = desc;
                
                if (info.name.isNotEmpty()) 
                    updatedDesc.name = info.name;
                if (info.vendor.isNotEmpty()) 
                    updatedDesc.manufacturerName = info.vendor;
                if (info.version.isNotEmpty()) 
                    updatedDesc.version = info.version;
                
                updatedDesc.isInstrument = info.isInstrument;
                updatedDesc.category = info.isInstrument ? "Instrument" : "Effect";
                
                // Update in list
                processor.knownPluginList.removeType(desc);
                processor.knownPluginList.addType(updatedDesc);
                return true;
            }
        }
    }
    
    // =========================================================================
    // Try to find plugin format
    // =========================================================================
    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
        if (processor.formatManager.getFormat(i)->getName() == desc.pluginFormatName) {
            format = processor.formatManager.getFormat(i);
            break;
        }
    }
    
    if (!format) return false;
    
    // =========================================================================
    // Try parsing moduleinfo.json directly (for VST3 without proper bundle detection)
    // =========================================================================
    if (desc.pluginFormatName == "VST3") {
        juce::File pluginFile(desc.fileOrIdentifier);
        juce::File contentsDir = pluginFile.getChildFile("Contents");
        juce::File moduleInfo;
        
        // Try various locations
        if (contentsDir.exists()) {
            moduleInfo = contentsDir.getChildFile("moduleinfo.json");
            if (!moduleInfo.existsAsFile()) {
                #if JUCE_WINDOWS
                moduleInfo = contentsDir.getChildFile("x86_64-win").getChildFile("moduleinfo.json");
                #elif JUCE_MAC
                moduleInfo = contentsDir.getChildFile("MacOS").getChildFile("moduleinfo.json");
                #endif
            }
        }
        
        if (moduleInfo.existsAsFile()) {
            juce::String jsonContent = moduleInfo.loadFileAsString();
            juce::var parsed = juce::JSON::parse(jsonContent);
            
            if (parsed.isObject()) {
                auto* root = parsed.getDynamicObject();
                if (root) {
                    auto* classes = root->getProperty("Classes").getArray();
                    if (classes) {
                        for (const auto& cls : *classes) {
                            if (auto* classObj = cls.getDynamicObject()) {
                                juce::String category = classObj->getProperty("Category").toString();
                                juce::String subCategories = classObj->getProperty("SubCategories").toString();
                                juce::String name = classObj->getProperty("Name").toString();
                                juce::String vendor = classObj->getProperty("Vendor").toString();
                                
                                // Look for Audio Module Class
                                if (category.containsIgnoreCase("Audio Module Class") || 
                                    name.isNotEmpty() || vendor.isNotEmpty()) {
                                    
                                    juce::PluginDescription updatedDesc = desc;
                                    
                                    if (name.isNotEmpty()) 
                                        updatedDesc.name = name;
                                    if (vendor.isNotEmpty()) 
                                        updatedDesc.manufacturerName = vendor;
                                    
                                    // FIXED: Check both category and subcategories for instrument
                                    updatedDesc.isInstrument = isInstrumentCategory(category, subCategories);
                                    updatedDesc.category = updatedDesc.isInstrument ? "Instrument" : "Effect";
                                    
                                    processor.knownPluginList.removeType(desc);
                                    processor.knownPluginList.addType(updatedDesc);
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // =========================================================================
    // Fall back to loading the plugin (with timeout protection)
    // =========================================================================
    
    std::atomic<bool> completed { false };
    juce::OwnedArray<juce::PluginDescription> results;
    
    // Use findAllTypesForFile which is safer than full instantiation
    juce::Thread::launch([&]() {
        format->findAllTypesForFile(results, desc.fileOrIdentifier);
        completed = true;
    });
    
    // Wait with timeout
    int elapsed = 0;
    while (!completed && elapsed < timeout) {
        if (threadShouldExit()) return false;
        Thread::sleep(50);
        elapsed += 50;
    }
    
    if (!completed) {
        // Timeout - plugin took too long
        juce::ScopedLock lock(failedPluginsLock);
        failedPlugins.add(desc.fileOrIdentifier);
        return false;
    }
    
    // Process results
    if (results.size() > 0) {
        for (auto* result : results) {
            // Find matching plugin by path or name
            if (result->fileOrIdentifier == desc.fileOrIdentifier ||
                result->name == desc.name) {
                
                // Create updated description with all the real metadata
                juce::PluginDescription updatedDesc = desc;
                updatedDesc.name = result->name;
                updatedDesc.manufacturerName = result->manufacturerName;
                updatedDesc.category = result->category;
                updatedDesc.isInstrument = result->isInstrument;
                updatedDesc.numInputChannels = result->numInputChannels;
                updatedDesc.numOutputChannels = result->numOutputChannels;
                updatedDesc.uniqueId = result->uniqueId;
                updatedDesc.deprecatedUid = result->deprecatedUid;
                updatedDesc.version = result->version;
                
                // Update in list
                processor.knownPluginList.removeType(desc);
                processor.knownPluginList.addType(updatedDesc);
                
                return true;
            }
        }
        
        // If we have results but no exact match, use the first one
        juce::PluginDescription updatedDesc = desc;
        updatedDesc.name = results[0]->name;
        updatedDesc.manufacturerName = results[0]->manufacturerName;
        updatedDesc.category = results[0]->category;
        updatedDesc.isInstrument = results[0]->isInstrument;
        updatedDesc.numInputChannels = results[0]->numInputChannels;
        updatedDesc.numOutputChannels = results[0]->numOutputChannels;
        updatedDesc.uniqueId = results[0]->uniqueId;
        updatedDesc.deprecatedUid = results[0]->deprecatedUid;
        
        processor.knownPluginList.removeType(desc);
        processor.knownPluginList.addType(updatedDesc);
        
        return true;
    }
    
    return false;
}