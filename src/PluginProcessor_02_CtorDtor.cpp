// Constructor / Destructor

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
    
    options.applicationName = "Subterraneum";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "Subterraneum";
    options.commonToAllUsers = false;
    options.ignoreCaseOfKeyNames = false;
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    appProperties.setStorageParameters(options);
    
    auto* userSettings = appProperties.getUserSettings();
    if (auto xml = userSettings->getXmlValue("KnownPlugins"))
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
    
    // FIX #1: Delay MIDI channel mask initialization until device manager is ready
    // This ensures the MIDI settings are properly applied on startup
    juce::MessageManager::callAsync([this]() {
        updateHardwareMidiChannelMasks();
    });
}

SubterraneumAudioProcessor::~SubterraneumAudioProcessor() {
    knownPluginList.removeChangeListener(this);
    if (standaloneDeviceManager)
        standaloneDeviceManager->removeChangeListener(this);

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
        userSettings->setValue("KnownPlugins", xml.get());
    userSettings->saveIfNeeded();
}
