

// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_PluginMenu.cpp
// Plugin menu implementation
// FIXED: Added Recorder system tool
// NEW: Hidden plugins (eye toggle) are filtered out of the menu
// NEW: Added Manual Sampling and Auto Sampling system tools
// FIX: Added MidiMultiFilter system tool
// NEW: Added Container system tool

#include "GraphCanvas.h"
#include <set>
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
#include "ManualSamplerProcessor.h"
#include "AutoSamplerProcessor.h"
#include "MidiPlayerProcessor.h"
#include "CCStepperProcessor.h"
#include "TransientSplitterProcessor.h"
#include "LatcherProcessor.h"
#include "MidiMultiFilterProcessor.h"
#include "ContainerProcessor.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

// =============================================================================
// THREAD-SAFE DEBUG LOGGER - DISABLED FOR RELEASE
// Creates plugin_load_debug.log next to the .exe
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
            log("=== Plugin Load Debug Log Started ===");
            log("Log file: " + logPath.getFullPathName());
        }
    }

    std::ofstream logFile;
    std::mutex logMutex;
};
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
    systemToolsMenu.addItem(7, "Auto Sampling");
    systemToolsMenu.addItem(1, "Connector");
    systemToolsMenu.addItem(14, "Container");
    systemToolsMenu.addItem(11, "Latcher");
    systemToolsMenu.addItem(6, "Manual Sampling");
    systemToolsMenu.addItem(3, "MIDI Monitor");
    systemToolsMenu.addItem(13, "MIDI Multi Filter");
    systemToolsMenu.addItem(8, "MIDI Player");
    systemToolsMenu.addItem(4, "Recorder");
    systemToolsMenu.addItem(9, "Step Seq");
    systemToolsMenu.addItem(2, "Stereo Meter");
    systemToolsMenu.addItem(10, "Transient Splitter");
    #if JUCE_PLUGINHOST_VST
    systemToolsMenu.addSeparator();
    systemToolsMenu.addItem(5, "VST2 Plugin...");
    #endif
    #if JUCE_PLUGINHOST_VST3
    systemToolsMenu.addItem(12, "VST3 Plugin...");
    #endif
    m.addSubMenu("System Tools", systemToolsMenu);
    m.addSeparator();

    if (processor.knownPluginList.getNumTypes() == 0)
    {
        LOG("No plugins found in list");
        m.addItem(99, "No plugins found. Please scan via Plugin Manager tab.", true, false);
    }
    else
    {
        LOG("Building plugin menu - " + juce::String(processor.knownPluginList.getNumTypes()) + " plugins available");

        auto types = processor.knownPluginList.getTypes();
        const int idBase = 100;

        // =====================================================================
        // FIX: Build a set of duplicate indices to skip in menu display.
        // VST3 plugins can register multiple components (audio processor +
        // controller). Keep only the first entry per name+format combination.
        // =====================================================================
        std::set<int> duplicateIndices;
        {
            std::set<juce::String> seenPlugins;
            for (int i = 0; i < (int)types.size(); ++i)
            {
                juce::String key = types[(size_t)i].name + "|" + types[(size_t)i].pluginFormatName;
                if (!seenPlugins.insert(key).second)
                    duplicateIndices.insert(i);  // This is a duplicate — skip in menu
            }
        }

        // NEW: Helper lambda to check if a plugin is hidden via eye toggle
        auto* userSettings = processor.appProperties.getUserSettings();
        auto isHidden = [&](const juce::PluginDescription& desc) -> bool {
            if (!userSettings) return false;
            juce::String key = "PluginHidden_" + desc.fileOrIdentifier.replaceCharacters(" :/\\.", "_____")
                               + "_" + juce::String(desc.uniqueId);
            return userSettings->getBoolValue(key, false);
        };

        std::vector<juce::PluginDescription> instruments, effects;
        for (int i = 0; i < (int)types.size(); ++i)
        {
            if (isHidden(types[(size_t)i])) continue;
            if (duplicateIndices.count(i)) continue;  // Skip duplicate VST3 components
            if (types[(size_t)i].isInstrument) instruments.push_back(types[(size_t)i]);
            else effects.push_back(types[(size_t)i]);
        }

        // Detect plugins with same name in multiple formats
        std::map<juce::String, int> nameCount;
        for (auto& t : types)
            if (!isHidden(t)) nameCount[t.name]++;

        auto displayName = [&](const juce::PluginDescription& d) -> juce::String {
            if (nameCount[d.name] > 1) {
                if (d.pluginFormatName.containsIgnoreCase("VST3")) return d.name + " [VST3]";
                if (d.pluginFormatName.containsIgnoreCase("VST"))  return d.name + " [VST2]";
                if (d.pluginFormatName.containsIgnoreCase("AU"))   return d.name + " [AU]";
                return d.name + " [" + d.pluginFormatName + "]";
            }
            return d.name;
        };

        if (!instruments.empty())
        {
            juce::PopupMenu instrMenu;

            if (processor.sortPluginsByVendor)
            {
                std::map<juce::String, juce::Array<int>> vendorMap;
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]) && !duplicateIndices.count(i))
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
                        subMenu.addItem(idBase + idx, displayName(types[(size_t)idx]));

                    instrMenu.addSubMenu(vendor, subMenu);
                }
            }
            else
            {
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]) && !duplicateIndices.count(i))
                        instrMenu.addItem(idBase + i, displayName(types[(size_t)i]));
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
                    if (!types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]) && !duplicateIndices.count(i))
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
                        subMenu.addItem(idBase + idx, displayName(types[(size_t)idx]));

                    fxMenu.addSubMenu(vendor, subMenu);
                }
            }
            else
            {
                for (int i = 0; i < (int)types.size(); ++i)
                {
                    if (!types[(size_t)i].isInstrument && !isHidden(types[(size_t)i]) && !duplicateIndices.count(i))
                        fxMenu.addItem(idBase + i, displayName(types[(size_t)i]));
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

        auto* activeGraph = safeThis->getActiveGraph();
        if (!activeGraph) return;

        auto types = safeThis->processor.knownPluginList.getTypes();

        if (result == 1)
        {
            LOG("Adding Connector node");
            auto nodePtr = activeGraph->addNode(std::make_unique<SimpleConnectorProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<StereoMeterProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<MidiMonitorProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<RecorderProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<ManualSamplerProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<AutoSamplerProcessor>(activeGraph, &safeThis->processor));
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
            auto nodePtr = activeGraph->addNode(std::make_unique<MidiPlayerProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<CCStepperProcessor>());
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
            auto nodePtr = activeGraph->addNode(std::make_unique<TransientSplitterProcessor>());
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
            LOG("Adding Latcher node");
            auto nodePtr = activeGraph->addNode(std::make_unique<LatcherProcessor>());
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Latcher added successfully");
            }
        }
        else if (result == 12)
        {
            LOG("VST3 Plugin loader selected");
            safeThis->loadVST3Plugin(safeThis->lastRightClickPos);
        }
        else if (result == 13)
        {
            LOG("Adding MIDI Multi Filter node");
            auto nodePtr = activeGraph->addNode(std::make_unique<MidiMultiFilterProcessor>());
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("MIDI Multi Filter added successfully");
            }
        }
        else if (result == 14)
        {
            if (safeThis->isInsideContainer())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Not Supported",
                    "Containers cannot be nested inside other containers.\n\nDive out to the main rack first.",
                    "OK");
                return;
            }
            LOG("Adding Container node");
            auto containerProc = std::make_unique<ContainerProcessor>();
            containerProc->setContainerName("Container " + juce::String(++safeThis->processor.containerCounter));  // FIX 3
            containerProc->setParentProcessor(&safeThis->processor);
            auto nodePtr = activeGraph->addNode(std::move(containerProc));
            if (nodePtr)
            {
                nodePtr->properties.set("x", (double)safeThis->lastRightClickPos.x);
                nodePtr->properties.set("y", (double)safeThis->lastRightClickPos.y);
                safeThis->markDirty();
                LOG("Container added successfully");
            }
        }
        else if (result == 99)
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
                LOG("UniqueId: " + juce::String(description.uniqueId));
                LOG("DeprecatedUid: " + juce::String(description.deprecatedUid));
                LOG("IsInstrument: " + juce::String(description.isInstrument ? "YES" : "NO"));
                LOG("NumIn: " + juce::String(description.numInputChannels) + " NumOut: " + juce::String(description.numOutputChannels));
                LOG("IdentifierString: " + description.createIdentifierString());
                LOG("==========================================================");

                LOG("STEP 1: Calling createPluginInstance()...");
                auto startTime = juce::Time::getMillisecondCounterHiRes();

                // DIAGNOSTIC: For VST3 bundles, try LoadLibrary manually to get Windows error
                if (description.pluginFormatName == "VST3")
                {
                    juce::File vstFile(description.fileOrIdentifier);
                    juce::File dllFile;

                    if (vstFile.isDirectory())
                    {
                        // Bundle format: Contents/x86_64-win/PluginName.vst3
                        dllFile = vstFile.getChildFile("Contents")
                                         .getChildFile("x86_64-win")
                                         .getChildFile(vstFile.getFileNameWithoutExtension() + ".vst3");
                    }
                    else
                    {
                        dllFile = vstFile;  // Direct .vst3 file
                    }

                    LOG("  VST3 bundle path: " + vstFile.getFullPathName());
                    LOG("  VST3 DLL path: " + dllFile.getFullPathName());
                    LOG("  DLL exists: " + juce::String(dllFile.existsAsFile() ? "YES" : "NO"));
                    LOG("  DLL size: " + juce::String(dllFile.getSize()) + " bytes");

                    if (dllFile.existsAsFile())
                    {
                    #if JUCE_WINDOWS
                        // Try loading the DLL directly to get Windows error code
                        HMODULE hMod = LoadLibraryExW(
                            dllFile.getFullPathName().toWideCharPointer(),
                            nullptr,
                            LOAD_LIBRARY_AS_DATAFILE);  // Safe: doesn't execute DllMain

                        if (hMod)
                        {
                            LOG("  LoadLibrary (data): SUCCESS — DLL is loadable");
                            FreeLibrary(hMod);

                            // Now try full load (executes DllMain)
                            hMod = LoadLibraryW(dllFile.getFullPathName().toWideCharPointer());
                            if (hMod)
                            {
                                LOG("  LoadLibrary (full): SUCCESS");

                                // Check for VST3 entry point
                                auto* getFactory = (void*)GetProcAddress(hMod, "GetPluginFactory");
                                LOG("  GetPluginFactory: " + juce::String(getFactory ? "FOUND" : "NOT FOUND"));

                                auto* initDll = (void*)GetProcAddress(hMod, "InitDll");
                                LOG("  InitDll: " + juce::String(initDll ? "FOUND" : "NOT FOUND"));

                                // Enumerate factory classes to compare with stored uniqueId
                                if (getFactory && initDll)
                                {
                                    typedef bool (*InitDllFunc)();
                                    typedef void* (*GetFactoryFunc)();

                                    auto initFunc = (InitDllFunc)GetProcAddress(hMod, "InitDll");
                                    if (initFunc) initFunc();

                                    auto factoryFunc = (GetFactoryFunc)GetProcAddress(hMod, "GetPluginFactory");
                                    if (factoryFunc)
                                    {
                                        // IPluginFactory interface
                                        struct FUnknown {
                                            virtual int queryInterface(const void*, void**) = 0;
                                            virtual unsigned int addRef() = 0;
                                            virtual unsigned int release() = 0;
                                        };
                                        struct PClassInfo {
                                            char cid[16];
                                            int cardinality;
                                            char category[32];
                                            char name[64];
                                        };
                                        struct IPluginFactory : FUnknown {
                                            virtual int getFactoryInfo(void*) = 0;
                                            virtual int countClasses() = 0;
                                            virtual int getClassInfo(int, PClassInfo*) = 0;
                                        };

                                        auto* factory = (IPluginFactory*)factoryFunc();
                                        if (factory)
                                        {
                                            int numClasses = factory->countClasses();
                                            LOG("  Factory has " + juce::String(numClasses) + " classes:");

                                            for (int ci = 0; ci < numClasses; ci++)
                                            {
                                                PClassInfo info;
                                                if (factory->getClassInfo(ci, &info) == 0)
                                                {
                                                    // Convert CID to hex string
                                                    juce::String cidHex;
                                                    for (int b = 0; b < 16; b++)
                                                        cidHex += juce::String::toHexString((unsigned char)info.cid[b]).paddedLeft('0', 2);

                                                    LOG("    [" + juce::String(ci) + "] name=\"" + juce::String(info.name)
                                                        + "\" category=\"" + juce::String(info.category)
                                                        + "\" CID=" + cidHex);
                                                }
                                            }
                                            factory->release();
                                        }
                                        else
                                        {
                                            LOG("  GetPluginFactory returned nullptr!");
                                        }
                                    }

                                    typedef bool (*ExitDllFunc)();
                                    auto exitFunc = (ExitDllFunc)GetProcAddress(hMod, "ExitDll");
                                    if (exitFunc) exitFunc();
                                }

                                FreeLibrary(hMod);
                            }
                            else
                            {
                                DWORD err = GetLastError();
                                LOG("  LoadLibrary (full): FAILED — Windows error " + juce::String((int)err));

                                // Format error message
                                LPWSTR msgBuf = nullptr;
                                FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                                    nullptr, err, 0, (LPWSTR)&msgBuf, 0, nullptr);
                                if (msgBuf) {
                                    LOG("  Error message: " + juce::String(msgBuf));
                                    LocalFree(msgBuf);
                                }
                            }
                        }
                        else
                        {
                            DWORD err = GetLastError();
                            LOG("  LoadLibrary (data): FAILED — Windows error " + juce::String((int)err));

                            LPWSTR msgBuf = nullptr;
                            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                                nullptr, err, 0, (LPWSTR)&msgBuf, 0, nullptr);
                            if (msgBuf) {
                                LOG("  Error message: " + juce::String(msgBuf));
                                LocalFree(msgBuf);
                            }
                        }
                    #else
                        LOG("  (LoadLibrary diagnostics are Windows-only, skipping on this platform)");
                    #endif
                    }
                }

                juce::String error;

                // SAFE DEFAULTS: avoid 0/0 when ASIO is off
                double sr = safeThis->processor.getSampleRate();
                int bs = safeThis->processor.getBlockSize();
                LOG("  Raw sampleRate=" + juce::String(sr) + " blockSize=" + juce::String(bs));
                if (sr <= 0.0) sr = 44100.0;
                if (bs <= 0) bs = 512;
                LOG("  Using sampleRate=" + juce::String(sr) + " blockSize=" + juce::String(bs));

                auto instance = safeThis->processor.formatManager.createPluginInstance(
                    description, sr, bs, error);

                // =============================================================
                // FIX: Fallback — if exact match fails, try finding by name+format
                // JUCE scanner sometimes stores UIDs that differ from what
                // createPluginInstance expects (especially VST3 multi-component)
                // =============================================================
                if (!instance)
                {
                    LOG("First attempt failed, trying fallback by name+format...");
                    for (auto& alt : safeThis->processor.knownPluginList.getTypes())
                    {
                        if (alt.name == description.name &&
                            alt.pluginFormatName == description.pluginFormatName &&
                            alt.uniqueId != description.uniqueId)
                        {
                            LOG("  Trying alternate UID: " + juce::String(alt.uniqueId));
                            juce::String altError;
                            instance = safeThis->processor.formatManager.createPluginInstance(
                                alt, sr, bs, altError);
                            if (instance)
                            {
                                LOG("  Fallback succeeded with alternate UID!");
                                error.clear();
                                break;
                            }
                        }
                    }
                }

                // =============================================================
                // NUCLEAR FALLBACK: Re-scan the .vst3 file in-process to get
                // fresh descriptions with correct UIDs from the factory.
                // This handles plugins stored with uid=0 or fake hashCode UIDs.
                // =============================================================
                if (!instance && description.pluginFormatName == "VST3")
                {
                    LOG("All UID fallbacks failed, trying in-process rescan...");
                    for (int fi = 0; fi < safeThis->processor.formatManager.getNumFormats(); ++fi)
                    {
                        auto* format = safeThis->processor.formatManager.getFormat(fi);
                        if (format->getName() != "VST3") continue;

                        juce::OwnedArray<juce::PluginDescription> freshDescs;
                        format->findAllTypesForFile(freshDescs, description.fileOrIdentifier);

                        for (auto* fresh : freshDescs)
                        {
                            if (fresh->name == description.name)
                            {
                                LOG("  Found fresh desc: uid=" + juce::String(fresh->uniqueId));
                                juce::String freshError;
                                instance = safeThis->processor.formatManager.createPluginInstance(
                                    *fresh, sr, bs, freshError);
                                if (instance)
                                {
                                    LOG("  In-process rescan succeeded!");
                                    // Update the known list with correct description
                                    safeThis->processor.knownPluginList.addType(*fresh);
                                    error.clear();
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }

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
                    auto nodePtr = activeGraph->addNode(std::move(meteringProc));

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

                    // CRITICAL FIX: Show error to user (was silently swallowed!)
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Plugin Load Failed",
                        "Could not load: " + description.name + "\n\n" +
                        (error.isEmpty() ? "Unknown error" : error) + "\n\n" +
                        "Format: " + description.pluginFormatName + "\n" +
                        "File: " + description.fileOrIdentifier,
                        "OK");
                }

                LOG("==========================================================");
            }
        }

        LOG("<<< Menu callback complete");
    });

    LOG("<<< showPluginMenu() complete");
}
