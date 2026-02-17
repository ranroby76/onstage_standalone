

// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_PluginMenu.cpp
// Plugin menu implementation
// FIXED: Added Recorder system tool
// NEW: Hidden plugins (eye toggle) are filtered out of the menu
// NEW: Added Manual Sampling and Auto Sampling system tools

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
#include "ManualSamplerProcessor.h"
#include "AutoSamplerProcessor.h"
#include "MidiPlayerProcessor.h"
#include "CCStepperProcessor.h"
#include "TransientSplitterProcessor.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>

// =============================================================================
// THREAD-SAFE DEBUG LOGGER CLASS - DISABLED FOR RELEASE
// =============================================================================
/*
class PluginLoadLogger {
public:
    static PluginLoadLogger& getInstance() {
        static PluginLoadLogger instance;
        return instance;
    }
    
    void log(const juce::String& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        if (!logFile.is_open()) return;
        
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        char timeBuffer[100];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", std::localtime(&time_t_now));
        
        // Get thread ID
        auto threadId = std::this_thread::get_id();
        std::stringstream ss;
        ss << threadId;
        
        logFile << "[" << timeBuffer << "." << std::setfill('0') << std::setw(3) << ms.count() 
                << "][Thread:" << ss.str().substr(ss.str().length() - 4) << "] " 
                << message.toStdString() << std::endl;
        logFile.flush();
    }
    
    ~PluginLoadLogger() {
        if (logFile.is_open()) {
            log("=== Logger shutdown ===");
            logFile.close();
        }
    }
    
private:
    PluginLoadLogger() {
        auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        auto logPath = exePath.getParentDirectory().getChildFile("plugin_load_debug.log");
        
        logFile.open(logPath.getFullPathName().toStdString(), std::ios::out | std::ios::trunc);
        
        if (logFile.is_open()) {
            log("=== Plugin Load Debug Log Started (ENHANCED) ===");
            log("Log file: " + logPath.getFullPathName());
        }
    }
    
    std::ofstream logFile;
    std::mutex logMutex;
};

#define LOG(msg) PluginLoadLogger::getInstance().log(msg)
*/

// Debug logging disabled - no-op macro
#define LOG(msg) ((void)0)

// =============================================================================
// PLUGIN MENU WITH HIDDEN PLUGIN FILTERING
// =============================================================================
void GraphCanvas::showPluginMenu()
{
    LOG(">>> showPluginMenu() called");
    
    juce::PopupMenu m;

    m.addSectionHeader("Add Node");
    m.addSeparator();
    
    // System Tools submenu
    juce::PopupMenu systemToolsMenu;
    systemToolsMenu.addItem(1, "Connector");
    systemToolsMenu.addItem(2, "Stereo Meter");
    systemToolsMenu.addItem(3, "MIDI Monitor");
    systemToolsMenu.addItem(4, "Recorder");
    systemToolsMenu.addItem(6, "Manual Sampling");
    systemToolsMenu.addItem(7, "Auto Sampling");
    systemToolsMenu.addItem(8, "MIDI Player");
    systemToolsMenu.addItem(9, "Step Seq");
    systemToolsMenu.addItem(10, "Transient Splitter");
    #if JUCE_PLUGINHOST_VST
    systemToolsMenu.addSeparator();
    systemToolsMenu.addItem(5, "VST2 Plugin...");
    #endif
    m.addSubMenu("System Tools", systemToolsMenu);
    m.addSeparator();

    if (processor.knownPluginList.getNumTypes() == 0)
    {
        LOG("No plugins found in list");
        m.addItem(11, "No plugins found. Please scan via Plugin Manager tab.", true, false);
    }
    else
    {
        LOG("Building plugin menu - " + juce::String(processor.knownPluginList.getNumTypes()) + " plugins available");
        
        auto types = processor.knownPluginList.getTypes();
        const int idBase = 100;

        // NEW: Helper lambda to check if a plugin is hidden via eye toggle
        auto* userSettings = processor.appProperties.getUserSettings();
        auto isHidden = [&](const juce::PluginDescription& desc) -> bool {
            if (!userSettings) return false;
            juce::String key = "PluginHidden_" + desc.fileOrIdentifier.replaceCharacters(" :/\\.", "_____")
                               + "_" + juce::String(desc.uniqueId);
            return userSettings->getBoolValue(key, false);
        };

        std::vector<juce::PluginDescription> instruments, effects;
        for (auto& t : types)
        {
            if (isHidden(t)) continue;  // NEW: Skip hidden plugins
            if (t.isInstrument) instruments.push_back(t);
            else effects.push_back(t);
        }

        if (!instruments.empty())
        {
            juce::PopupMenu instrMenu;

            if (processor.sortPluginsByVendor)
            {
                std::map<juce::String, juce::Array<int>> vendorMap;
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]))
                    {
                        juce::String vendor = types[(size_t)i].manufacturerName.isEmpty()
                                            ? "Unknown"
                                            : types[(size_t)i].manufacturerName;
                        vendorMap[vendor].add(i);
                    }
                }

                juce::StringArray vendors;
                for (auto& pair : vendorMap) vendors.add(pair.first);
                vendors.sort(true);

                for (const auto& vendor : vendors)
                {
                    juce::PopupMenu subMenu;
                    for (int idx : vendorMap[vendor])
                        subMenu.addItem(idBase + idx, types[(size_t)idx].name);

                    instrMenu.addSubMenu(vendor, subMenu);
                }
            }
            else
            {
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]))
                        instrMenu.addItem(idBase + i, types[(size_t)i].name);
                }
            }

            m.addSubMenu("INSTRUMENTS", instrMenu);
        }

        if (!effects.empty())
        {
            juce::PopupMenu fxMenu;

            if (processor.sortPluginsByVendor)
            {
                std::map<juce::String, juce::Array<int>> vendorMap;
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (!types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]))
                    {
                        juce::String vendor = types[(size_t)i].manufacturerName.isEmpty()
                                            ? "Unknown"
                                            : types[(size_t)i].manufacturerName;
                        vendorMap[vendor].add(i);
                    }
                }

                juce::StringArray vendors;
                for (auto& pair : vendorMap) vendors.add(pair.first);
                vendors.sort(true);

                for (const auto& vendor : vendors)
                {
                    juce::PopupMenu subMenu;
                    for (int idx : vendorMap[vendor])
                        subMenu.addItem(idBase + idx, types[(size_t)idx].name);

                    fxMenu.addSubMenu(vendor, subMenu);
                }
            }
            else
            {
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (!types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]))
                        fxMenu.addItem(idBase + i, types[(size_t)i].name);
                }
            }

            m.addSubMenu("EFFECTS", fxMenu);
        }
    }

    LOG("Showing menu async...");
    
    juce::Component::SafePointer<GraphCanvas> safeThis(this);

    m.showMenuAsync(juce::PopupMenu::Options(), [safeThis](int result)
    {
        LOG(">>> Menu callback - result: " + juce::String(result));
        
        if (safeThis == nullptr) {
            LOG("ERROR: GraphCanvas was deleted!");
            return;
        }
        
        auto types = safeThis->processor.knownPluginList.getTypes();

        if (result == 1)
        {
            LOG("Adding Connector node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<SimpleConnectorProcessor>());
            if (nodePtr)
            {
                // FIX #3: Place new node at mouse cursor position
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Connector added successfully");
            }
        }
        else if (result == 2)
        {
            LOG("Adding Stereo Meter node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<StereoMeterProcessor>());
            if (nodePtr)
            {
                // FIX #3: Place new node at mouse cursor position
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Stereo Meter added successfully");
            }
        }
        else if (result == 3)
        {
            LOG("Adding MIDI Monitor node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<MidiMonitorProcessor>());
            if (nodePtr)
            {
                // FIX #3: Place new node at mouse cursor position
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("MIDI Monitor added successfully");
            }
        }
        else if (result == 4)
        {
            LOG("Adding Recorder node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<RecorderProcessor>());
            if (nodePtr)
            {
                // Place new node at mouse cursor position
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Recorder added successfully");
            }
        }
        else if (result == 5)
        {
            LOG("VST2 Plugin loader selected");
            safeThis->loadVST2Plugin(safeThis->lastRightClickPos);
        }
        else if (result == 6)
        {
            LOG("Adding Manual Sampling node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<ManualSamplerProcessor>());
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Manual Sampling added successfully");
            }
        }
        else if (result == 7)
        {
            LOG("Adding Auto Sampling node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<AutoSamplerProcessor>(safeThis->processor.mainGraph.get(), &safeThis->processor));
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Auto Sampling added successfully");
            }
        }
        else if (result == 8)
        {
            LOG("Adding MIDI Player node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<MidiPlayerProcessor>());
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("MIDI Player added successfully");
            }
        }
        else if (result == 9)
        {
            LOG("Adding Step Seq node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<CCStepperProcessor>());
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Step Seq added successfully");
            }
        }
        else if (result == 10)
        {
            LOG("Adding Transient Splitter node");
            auto nodePtr = safeThis->processor.mainGraph->addNode(std::make_unique<TransientSplitterProcessor>());
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Transient Splitter added successfully");
            }
        }
        else if (result == 11)
        {
            LOG("No plugins menu item selected");
            return;
        }
        else if (result >= 100)
        {
            int index = result - 100;
            if (index >= 0 && index < (int)types.size())
            {
                auto description = types[(size_t)index];
                
                LOG("==========================================================");
                LOG("PLUGIN LOAD START");
                LOG("Plugin: " + description.name);
                LOG("Vendor: " + description.manufacturerName);
                LOG("Format: " + description.pluginFormatName);
                LOG("File: " + description.fileOrIdentifier);
                LOG("==========================================================");
                
                LOG("STEP 1: Calling createPluginInstance()...");
                auto startTime = juce::Time::getMillisecondCounterHiRes();
                
                juce::String error;
                auto instance = safeThis->processor.formatManager.createPluginInstance(
                    description,
                    safeThis->processor.getSampleRate(),
                    safeThis->processor.getBlockSize(),
                    error);
                
                auto endTime = juce::Time::getMillisecondCounterHiRes();
                LOG("STEP 1 COMPLETE: createPluginInstance() took " + juce::String((endTime - startTime) / 1000.0, 3) + " seconds");

                if (instance)
                {
                    LOG("SUCCESS: Plugin instance created");
                    
                    LOG("STEP 2: Querying plugin capabilities...");
                    startTime = juce::Time::getMillisecondCounterHiRes();
                    
                    juce::String pluginName = instance->getName();
                    LOG("  - getName(): " + pluginName);
                    
                    LOG("  - hasEditor(): " + juce::String(instance->hasEditor() ? "YES" : "NO"));
                    
                    LOG("  - acceptsMidi(): " + juce::String(instance->acceptsMidi() ? "YES" : "NO"));
                    
                    LOG("  - producesMidi(): " + juce::String(instance->producesMidi() ? "YES" : "NO"));
                    
                    LOG("  - getTotalNumInputChannels(): " + juce::String(instance->getTotalNumInputChannels()));
                    
                    LOG("  - getTotalNumOutputChannels(): " + juce::String(instance->getTotalNumOutputChannels()));
                    
                    endTime = juce::Time::getMillisecondCounterHiRes();
                    LOG("STEP 2 COMPLETE: Capability queries took " + juce::String((endTime - startTime) / 1000.0, 3) + " seconds");
                    
                    LOG("STEP 3: Creating MeteringProcessor wrapper...");
                    startTime = juce::Time::getMillisecondCounterHiRes();
                    
                    auto meteringProc = std::make_unique<MeteringProcessor>(std::move(instance));
                    
                    endTime = juce::Time::getMillisecondCounterHiRes();
                    LOG("STEP 3 COMPLETE: MeteringProcessor created in " + juce::String((endTime - startTime) / 1000.0, 3) + " seconds");
                    
                    LOG("STEP 4: Adding node to graph...");
                    startTime = juce::Time::getMillisecondCounterHiRes();
                    
                    // CRITICAL: This is where audio processing might start!
                    auto nodePtr = safeThis->processor.mainGraph->addNode(std::move(meteringProc));
                    
                    endTime = juce::Time::getMillisecondCounterHiRes();
                    LOG("STEP 4 COMPLETE: addNode() took " + juce::String((endTime - startTime) / 1000.0, 3) + " seconds");

                    if (nodePtr)
                    {
                        LOG("STEP 5: Setting node properties...");
                        // FIX #3: Place new node at mouse cursor position
                        nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                        nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                        
                        LOG("STEP 6: Updating UI...");
                        safeThis->updateParentSelector();
                        safeThis->markDirty();
                        
                        LOG("=== PLUGIN LOAD COMPLETE SUCCESS ===");
                    }
                    else
                    {
                        LOG("ERROR: addNode() returned nullptr!");
                    }
                }
                else
                {
                    LOG("FAILED: createPluginInstance() returned nullptr");
                    LOG("Error: " + (error.isEmpty() ? "No error message" : error));
                }
                
                LOG("==========================================================");
            }
        }
        
        LOG("<<< Menu callback complete");
    });
    
    LOG("<<< showPluginMenu() complete");
}






