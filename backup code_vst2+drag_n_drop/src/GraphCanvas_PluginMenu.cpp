// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_PluginMenu.cpp
// Plugin menu implementation
// FIXED: Added Recorder system tool

#include "GraphCanvas.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
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
// PLUGIN MENU WITH ENHANCED DEBUG LOGGING
// =============================================================================
void GraphCanvas::showPluginMenu()
{
    LOG(">>> showPluginMenu() called");
    
    juce::PopupMenu m;

    m.addSectionHeader("Add Node");
    m.addSeparator();
    m.addItem(1, "Connector");
    m.addItem(2, "Stereo Meter");
    m.addItem(3, "MIDI Monitor");
    m.addItem(4, "Recorder");
    m.addSeparator();

    if (processor.knownPluginList.getNumTypes() == 0)
    {
        LOG("No plugins found in list");
        m.addItem(10, "No plugins found. Please scan via Plugin Manager tab.", true, false);
    }
    else
    {
        LOG("Building plugin menu - " + juce::String(processor.knownPluginList.getNumTypes()) + " plugins available");
        
        auto types = processor.knownPluginList.getTypes();
        const int idBase = 100;

        std::vector<juce::PluginDescription> instruments, effects;
        for (auto& t : types)
        {
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
                    if (types[(size_t)i].isInstrument)
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
                    if (types[(size_t)i].isInstrument)
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
                    if (!types[(size_t)i].isInstrument)
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
                    if (!types[(size_t)i].isInstrument)
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
        else if (result == 10)
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
                    
                    bool hasEditor = instance->hasEditor();
                    LOG("  - hasEditor(): " + juce::String(hasEditor ? "YES" : "NO"));
                    
                    bool acceptsMidi = instance->acceptsMidi();
                    LOG("  - acceptsMidi(): " + juce::String(acceptsMidi ? "YES" : "NO"));
                    
                    bool producesMidi = instance->producesMidi();
                    LOG("  - producesMidi(): " + juce::String(producesMidi ? "YES" : "NO"));
                    
                    int numIns = instance->getTotalNumInputChannels();
                    LOG("  - getTotalNumInputChannels(): " + juce::String(numIns));
                    
                    int numOuts = instance->getTotalNumOutputChannels();
                    LOG("  - getTotalNumOutputChannels(): " + juce::String(numOuts));
                    
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
