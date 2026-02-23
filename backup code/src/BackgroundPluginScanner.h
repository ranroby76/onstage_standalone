// #D:\Workspace\Subterraneum_plugins_daw\src\BackgroundPluginScanner.h
// Background plugin metadata enrichment — JSON only, no plugin loading
// Used after scan to fill in missing metadata from moduleinfo.json
// Completely safe — reads files, never loads plugin binaries

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class BackgroundPluginScanner : public juce::Thread,
                                 public juce::ChangeBroadcaster {
public:
    BackgroundPluginScanner(SubterraneumAudioProcessor& processor);
    ~BackgroundPluginScanner() override;
    
    void startEnrichment();
    void stopEnrichment();
    
    std::atomic<int> enrichedCount { 0 };
    std::atomic<int> totalToEnrich { 0 };
    std::atomic<bool> finished { false };
    
private:
    void run() override;
    void enrichPluginMetadata(juce::PluginDescription& desc);
    
    SubterraneumAudioProcessor& processor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackgroundPluginScanner)
};
