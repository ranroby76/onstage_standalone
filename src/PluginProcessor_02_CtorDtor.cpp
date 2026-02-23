

// #D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_02_CtorDtor.cpp
// Constructor / Destructor
// FIX: Destructor now stops recording and writer thread before clearing graph

#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
// Constructor / Destructor
// =============================================================================
SubterraneumAudioProcessor::SubterraneumAudioProcessor()
     : AudioProcessor(juce::AudioProcessor::BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    initializePluginFormats();
    
    options.applicationName = "Colosseum";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "Fanan";
    options.commonToAllUsers = false;
    options.ignoreCaseOfKeyNames = false;
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    appProperties.setStorageParameters(options);
    
    auto* userSettings = appProperties.getUserSettings();
    if (auto xml = userSettings->getXmlValue("KnownPluginsV2"))
        knownPluginList.recreateFromXml(*xml);

    loadAudioSettings();
    mainGraph = std::make_unique<juce::AudioProcessorGraph>();
    knownPluginList.addChangeListener(this);
    
    if (standaloneDeviceManager)
        standaloneDeviceManager->addChangeListener(this);

    for (int i = 0; i < maxMixerChannels; ++i) {
        inputGains[i].store(1.0f);
        outputGains[i].store(1.0f);
    }

    updateGraph();
    
    // Initialize workspace names to "1", "2", ..., "16"
    for (int i = 0; i < maxWorkspaces; ++i)
        workspaceNames[i] = juce::String(i + 1);
    workspaceEnabled[0] = true; // Only workspace 1 enabled on fresh start
    
    // FIX #1: Delay MIDI channel mask initialization until device manager is ready
    // This ensures the MIDI settings are properly applied on startup
    juce::MessageManager::callAsync([this]() {
        updateHardwareMidiChannelMasks();
    });
    
    // FIX #2: Auto-load default patch on startup (deferred so graph is fully ready)
    juce::MessageManager::callAsync([this]() {
        loadDefaultPatchOnStartup();
    });
}

SubterraneumAudioProcessor::~SubterraneumAudioProcessor() {
    // CRITICAL: Stop any active recording first
    stopRecording();
    backgroundWriter.reset();
    writerThread.stopThread(5000);
    
    knownPluginList.removeChangeListener(this);
    if (standaloneDeviceManager)
        standaloneDeviceManager->removeChangeListener(this);

    // Suspend processing before tearing down graph
    suspendProcessing(true);

    audioInputNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    midiOutputNode = nullptr;
    
    if (mainGraph) {
        mainGraph->clear();
        mainGraph->releaseResources();
    }

    auto* userSettings = appProperties.getUserSettings();
    if (auto xml = knownPluginList.createXml())
        userSettings->setValue("KnownPluginsV2", xml.get());
    userSettings->saveIfNeeded();
}

// =============================================================================
// Default Patch File Location
// =============================================================================
juce::File SubterraneumAudioProcessor::getDefaultPatchFile() const {
    // Store in the same Fanan settings folder used by appProperties
    auto settingsFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    
    #if JUCE_MAC
    settingsFolder = settingsFolder.getChildFile("Application Support");
    #endif
    
    return settingsFolder.getChildFile("Fanan").getChildFile("Colosseum_Default.subt");
}

// =============================================================================
// Auto-Load Default Patch on Startup
// =============================================================================
void SubterraneumAudioProcessor::loadDefaultPatchOnStartup() {
    auto defaultFile = getDefaultPatchFile();
    
    if (defaultFile.existsAsFile()) {
        loadUserPreset(defaultFile);
        
        // Notify editor to refresh UI if it exists
        if (auto* editor = dynamic_cast<SubterraneumAudioProcessorEditor*>(getActiveEditor())) {
            editor->repaint();
        }
    }
}









