// #D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_01_Formats.cpp
// Plugin Format Initialization - VST3 + VST2 (manual load) + AU + LADSPA

#include "PluginProcessor.h"

juce::AudioDeviceManager* SubterraneumAudioProcessor::standaloneDeviceManager = nullptr;

// FIX 3: Container counter initialization
std::atomic<int> SubterraneumAudioProcessor::containerCounter{0};

// =============================================================================
// Plugin Format Initialization
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
    // VST2 Format - Manual loading only (no background scanning)
    // Users load VST2 plugins via the "VST2 Plugin..." menu or L button
    // =========================================================================
    #if JUCE_PLUGINHOST_VST
    formatManager.addFormat(new juce::VSTPluginFormat());
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
    if (fullFormatName.equalsIgnoreCase("VST"))
        return "VST2";
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
    if (fullFormatName.equalsIgnoreCase("VST"))
        return juce::Colour(0xFF4DA6FF);  // Blue for VST2
    if (fullFormatName.containsIgnoreCase("AudioUnit") || fullFormatName.containsIgnoreCase("AU"))
        return juce::Colour(0xFFFF6B6B);  // Red
    if (fullFormatName.containsIgnoreCase("LADSPA"))
        return juce::Colour(0xFF9B59B6);  // Purple
    return juce::Colours::grey;
}
