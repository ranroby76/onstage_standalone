// #D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_01_Formats.cpp
// Plugin Format Initialization - VST3 Only (VST2 removed for stability)

#include "PluginProcessor.h"

juce::AudioDeviceManager* SubterraneumAudioProcessor::standaloneDeviceManager = nullptr;

// =============================================================================
// Plugin Format Initialization - VST3 ONLY
// =============================================================================
void SubterraneumAudioProcessor::initializePluginFormats()
{
    // =========================================================================
    // VST3 Format - Primary format, uses moduleinfo.json for safe scanning
    // =========================================================================
    #if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
    #endif
    
    // =========================================================================
    // Audio Units (macOS only)
    // =========================================================================
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatManager.addFormat(new juce::AudioUnitPluginFormat());
    #endif
    
    // =========================================================================
    // LADSPA (Linux only)
    // =========================================================================
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formatManager.addFormat(new juce::LADSPAPluginFormat());
    #endif
    
    // NOTE: VST2 support has been removed for stability
    // VST2 requires loading the plugin binary to get metadata, which can crash
    // VST3 provides moduleinfo.json for crash-free metadata extraction
}

juce::StringArray SubterraneumAudioProcessor::getSupportedFormatNames() const
{
    juce::StringArray names;
    for (int i = 0; i < formatManager.getNumFormats(); ++i) {
        names.add(formatManager.getFormat(i)->getName());
    }
    return names;
}

// =============================================================================
// Helper: Get short format name for display
// =============================================================================
juce::String SubterraneumAudioProcessor::getShortFormatName(const juce::String& fullFormatName)
{
    if (fullFormatName.containsIgnoreCase("VST3"))
        return "VST3";
    if (fullFormatName.containsIgnoreCase("AudioUnit") || fullFormatName.containsIgnoreCase("AU"))
        return "AU";
    if (fullFormatName.containsIgnoreCase("LADSPA"))
        return "LADSPA";
    return fullFormatName.substring(0, 4).toUpperCase();
}

// =============================================================================
// Helper: Get format badge color
// =============================================================================
juce::Colour SubterraneumAudioProcessor::getFormatColor(const juce::String& fullFormatName)
{
    if (fullFormatName.containsIgnoreCase("VST3"))
        return juce::Colour(0xFF50C878);  // Green
    if (fullFormatName.containsIgnoreCase("AudioUnit") || fullFormatName.containsIgnoreCase("AU"))
        return juce::Colour(0xFFFF6B6B);  // Red
    if (fullFormatName.containsIgnoreCase("LADSPA"))
        return juce::Colour(0xFF9B59B6);  // Purple
    return juce::Colours::grey;
}