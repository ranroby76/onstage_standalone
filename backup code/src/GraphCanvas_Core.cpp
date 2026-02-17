

// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Core.cpp
// CRITICAL FIX: Use MeteringProcessor::isInstrument() instead of getPluginDescription()
// getPluginDescription() freezes some plugins!
// FREEZE FIX: Cache plugin name to avoid calling getName() during paint()
// Some plugins (like SOLO by Taqs.im) freeze forever when getName() is called repeatedly!
// FIX: Stereo meter gets dedicated 50ms timer (20fps) for smooth animation
// FIX: MIDI Monitor only repaints when MIDI changes
// FIXED: Added RecorderProcessor support
// NEW: Added ManualSamplerProcessor, AutoSamplerProcessor, MidiPlayerProcessor support

#include "GraphCanvas.h"
#include "PluginEditor.h"
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
#include <set>
#include <sstream>
#include <thread>

// =============================================================================
// THREAD-SAFE DEBUG LOGGER (matches the one in PluginMenu) - DISABLED FOR RELEASE
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
        
        logFile.open(logPath.getFullPathName().toStdString(), std::ios::out | std::ios::app);
        
        if (logFile.is_open()) {
            log("=== GraphCanvas_Core debug logging started ===");
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
// GRAPHCANVAS IMPLEMENTATION WITH DUAL TIMERS
// =============================================================================

GraphCanvas::GraphCanvas(SubterraneumAudioProcessor& p) : processor(p)
{
    LOG("GraphCanvas constructor called");
    setOpaque(true);
    
    // Load persistent last browsed directory
    loadLastBrowsedDirectory();
    
    // DON'T attach OpenGL here - component isn't visible yet!
    // OpenGL will be initialized in parentHierarchyChanged() when component has a valid peer
    
    // FIX: Start both timers
    startTimer(MainTimerID, 200);           // Main timer: 200ms for general updates
    startTimer(StereoMeterTimerID, 50);     // Stereo meter: 50ms (20fps) for smooth animation
    LOG("GraphCanvas constructor complete - dual timers started (200ms main, 50ms meter)");
}

GraphCanvas::~GraphCanvas()
{
    LOG("GraphCanvas destructor called");
    if (openGLContext.isAttached()) openGLContext.detach();
    activePluginWindows.clear();
    stopTimer(MainTimerID);
    stopTimer(StereoMeterTimerID);
    LOG("GraphCanvas destructor complete");
}

void GraphCanvas::parentHierarchyChanged()
{
    // Only attempt OpenGL attachment once, when component becomes visible with a peer
    static bool openGLInitialized = false;
    
    // Component doesn't need to be directly on desktop - it can be in a TabbedComponent
    // Just need: showing (visible + all parents visible) + has peer (native window exists)
    if (!openGLInitialized && isShowing() && getPeer() != nullptr)
    {
        LOG("Initializing OpenGL...");
        openGLInitialized = true;
        initializeOpenGL();
        LOG("OpenGL initialized");
    }
}

void GraphCanvas::initializeOpenGL()
{
    // Configure OpenGL before attaching
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.setContinuousRepainting(false);
    
    // Set OpenGL pixel format for maximum compatibility
    juce::OpenGLPixelFormat pixelFormat;
    pixelFormat.redBits = 8;
    pixelFormat.greenBits = 8;
    pixelFormat.blueBits = 8;
    pixelFormat.alphaBits = 8;
    pixelFormat.depthBufferBits = 24;
    pixelFormat.stencilBufferBits = 8;
    pixelFormat.multisamplingLevel = 0;  // No MSAA for compatibility
    
    openGLContext.setPixelFormat(pixelFormat);
    
    // Attach to this component
    openGLContext.attachTo(*this);
}

void GraphCanvas::resized()
{
    needsRepaint = true;
}

void GraphCanvas::updateParentSelector()
{
    LOG("updateParentSelector() called");
    if (auto* editor = findParentComponentOfClass<SubterraneumAudioProcessorEditor>())
    {
        LOG("  Calling editor->updateInstrumentSelector()");
        editor->updateInstrumentSelector();
        LOG("  updateInstrumentSelector() complete");
    }
    else
    {
        LOG("  No parent editor found!");
    }
}

void GraphCanvas::rebuildNodeTypeCache()
{
    LOG(">>> rebuildNodeTypeCache() START");
    
    nodeTypeCache.clear();
    hasStereoMeter = false;
    hasRecorder = false;
    hasSampler = false;
    hasMidiPlayer = false;
    hasStepSeq = false;
    
    if (!processor.mainGraph) {
        LOG("  mainGraph is null, returning");
        cachedConnections.clear();
        return;
    }
    
    int nodeCount = 0;
    for (auto* node : processor.mainGraph->getNodes())
    {
        nodeCount++;
        LOG("  Processing node " + juce::String(nodeCount) + " ID: " + juce::String(node->nodeID.uid));
        
        NodeTypeCache cache;
        auto* proc = node->getProcessor();
        
        LOG("    Getting processor type...");
        cache.meteringProc = dynamic_cast<MeteringProcessor*>(proc);
        cache.simpleConnector = dynamic_cast<SimpleConnectorProcessor*>(proc);
        cache.stereoMeter = dynamic_cast<StereoMeterProcessor*>(proc);
        cache.midiMonitor = dynamic_cast<MidiMonitorProcessor*>(proc);
        cache.recorder = dynamic_cast<RecorderProcessor*>(proc);
        cache.manualSampler = dynamic_cast<ManualSamplerProcessor*>(proc);
        cache.autoSampler = dynamic_cast<AutoSamplerProcessor*>(proc);
        cache.midiPlayer = dynamic_cast<MidiPlayerProcessor*>(proc);
        cache.ccStepper = dynamic_cast<CCStepperProcessor*>(proc);
        cache.transientSplitter = dynamic_cast<TransientSplitterProcessor*>(proc);
        
        // FIX: Track if stereo meter exists
        if (cache.stereoMeter) {
            hasStereoMeter = true;
            LOG("    StereoMeter detected!");
        }
        
        // Track if recorder exists (for timer-based refresh)
        if (cache.recorder) {
            hasRecorder = true;
            LOG("    Recorder detected!");
        }
        
        // Track samplers (need 20fps refresh for waveform/meters)
        if (cache.manualSampler || cache.autoSampler) {
            hasSampler = true;
            LOG("    Sampler detected!");
        }
        
        // Track MIDI player (need 20fps refresh for position slider)
        if (cache.midiPlayer) {
            hasMidiPlayer = true;
            LOG("    MidiPlayer detected!");
        }
        
        if (cache.ccStepper) {
            hasStepSeq = true;
            LOG("    StepSeq detected!");
        }
        
        LOG("    Checking if I/O node...");
        cache.isAudioInput  = (node == processor.audioInputNode.get());
        cache.isAudioOutput = (node == processor.audioOutputNode.get());
        cache.isMidiInput   = (node == processor.midiInputNode.get());
        cache.isMidiOutput  = (node == processor.midiOutputNode.get());
        cache.isIO = cache.isAudioInput || cache.isAudioOutput || cache.isMidiInput || cache.isMidiOutput;
        
        // =========================================================================
        // FREEZE FIX: Cache plugin name HERE during cache rebuild
        // NEVER call getName() during paint() - some plugins freeze forever!
        // =========================================================================
        try {
            if (cache.isAudioInput) {
                cache.pluginName = "Audio Input";
            }
            else if (cache.isAudioOutput) {
                cache.pluginName = "Audio Output";
            }
            else if (cache.isMidiInput) {
                cache.pluginName = "MIDI Input";
            }
            else if (cache.isMidiOutput) {
                cache.pluginName = "MIDI Output";
            }
            else if (proc) {
                // Call getName() ONCE here, cache it for paint()
                cache.pluginName = proc->getName();
            }
            else {
                cache.pluginName = "Unknown";
            }
            
            // Truncate long names for display
            if (cache.pluginName.length() > 20) {
                cache.pluginName = cache.pluginName.substring(0, 18) + "..";
            }
            
            LOG("    Cached name: '" + cache.pluginName + "'");
        }
        catch (...) {
            LOG("    EXCEPTION while getting name! Using fallback.");
            cache.pluginName = "Plugin";
        }
        
        if (cache.meteringProc)
        {
            LOG("    MeteringProcessor found");
            
            // =========================================================================
            // CRITICAL FIX: Use cached isInstrument() instead of getPluginDescription()
            // getPluginDescription() freezes some plugins when called!
            // =========================================================================
            cache.isInstrument = cache.meteringProc->isInstrument();
            LOG("    isInstrument (cached): " + juce::String(cache.isInstrument ? "YES" : "NO"));
            
            LOG("    Checking sidechain...");
            cache.hasSidechain = cache.meteringProc->hasSidechain();
        }
        
        nodeTypeCache[node->nodeID] = cache;
        LOG("  Node cached successfully");
    }
    
    lastNodeCount = processor.mainGraph->getNumNodes();
    
    // FIX: Cache connections vector here (once per structure change)
    // instead of copying it every paint() call at 20fps
    cachedConnections = processor.mainGraph->getConnections();
    lastConnectionCount = cachedConnections.size();
    
    // FIX: Moved from paint() — only needs to run when graph structure changes
    verifyPositions();
    
    // =========================================================================
    // Mark nodes in AutoSampler recording chains
    // For each AutoSampler, follow MIDI out → VSTi → effects chain
    // =========================================================================
    for (auto& [nodeID, cache] : nodeTypeCache)
    {
        if (!cache.autoSampler) continue;
        
        // Find our MIDI output target (use cached connections)
        juce::AudioProcessorGraph::NodeID midiTargetID;
        for (auto& conn : cachedConnections)
        {
            if (conn.source.nodeID == nodeID &&
                conn.source.channelIndex == juce::AudioProcessorGraph::midiChannelIndex)
            {
                midiTargetID = conn.destination.nodeID;
                break;
            }
        }
        if (midiTargetID.uid == 0) continue;
        
        // Walk audio chain from MIDI target downstream
        auto currentID = midiTargetID;
        std::set<uint32> visited;
        
        while (currentID.uid != 0)
        {
            if (visited.count(currentID.uid)) break;
            visited.insert(currentID.uid);
            
            auto* chainNode = processor.mainGraph->getNodeForId(currentID);
            if (!chainNode) break;
            
            // Stop at Audio Output node
            if (chainNode == processor.audioOutputNode.get()) break;
            
            // Mark this node as in sampling chain
            auto cacheIt = nodeTypeCache.find(currentID);
            if (cacheIt != nodeTypeCache.end())
                cacheIt->second.inSamplingChain = true;
            
            // Follow first audio output connection (use cached connections)
            juce::AudioProcessorGraph::NodeID nextID;
            for (auto& conn : cachedConnections)
            {
                if (conn.source.nodeID == currentID && conn.source.channelIndex == 0)
                {
                    if (visited.count(conn.destination.nodeID.uid) == 0)
                    {
                        nextID = conn.destination.nodeID;
                        break;
                    }
                }
            }
            currentID = nextID;
        }
    }
    
    LOG("<<< rebuildNodeTypeCache() COMPLETE");
}

const GraphCanvas::NodeTypeCache* GraphCanvas::getCachedNodeType(juce::AudioProcessorGraph::NodeID nodeID)
{
    auto it = nodeTypeCache.find(nodeID);
    return (it != nodeTypeCache.end()) ? &it->second : nullptr;
}

void GraphCanvas::timerCallback(int timerID)
{
    // FIX: Separate timer callbacks for different refresh rates
    
    if (timerID == MainTimerID)
    {
        // =============================================================================
        // MAIN TIMER (200ms) - General UI updates, graph changes, window cleanup
        // =============================================================================
        static int callCount = 0;
        callCount++;
        
        bool shouldLog = (callCount <= 5) || (callCount % 10 == 0);
        
        if (shouldLog)
            LOG(">>> MainTimer callback #" + juce::String(callCount));
        
        // Clean up closed plugin windows
        for (auto it = activePluginWindows.begin(); it != activePluginWindows.end();)
        {
            if (it->second && it->second->getPeer() == nullptr)
                it = activePluginWindows.erase(it);
            else if (!it->second)
                it = activePluginWindows.erase(it);
            else
                ++it;
        }

        // Check for graph structure changes
        if (processor.mainGraph)
        {
            size_t cn = processor.mainGraph->getNumNodes();
            size_t cc = processor.mainGraph->getConnections().size();
            if (cn != lastNodeCount || cc != lastConnectionCount) {
                LOG("  Node/connection count changed - rebuilding cache...");
                rebuildNodeTypeCache();
                needsRepaint = true;
                LOG("  Rebuild complete, repaint scheduled");
            }
        }

        // Check for UI state changes
        if (highlightPin != lastHighlightPin) {
            lastHighlightPin = highlightPin;
            needsRepaint = true;
        }

        if (hoveredConnection.source.nodeID != lastHoveredConnection.source.nodeID ||
            hoveredConnection.destination.nodeID != lastHoveredConnection.destination.nodeID) {
            lastHoveredConnection = hoveredConnection;
            needsRepaint = true;
        }

        if (dragCable.active || draggingNodeID.uid != 0)
            needsRepaint = true;

        // Check for I/O node activity (every 3rd callback = ~600ms)
        static int mc = 0;
        if (++mc >= 3) {
            mc = 0;
            if (processor.mainInputRms[0].load() > 0.001f || processor.mainInputRms[1].load() > 0.001f ||
                processor.mainOutputRms[0].load() > 0.001f || processor.mainOutputRms[1].load() > 0.001f ||
                processor.mainMidiInFlash.load() || processor.mainMidiOutFlash.load())
                needsRepaint = true;
            
            // FIX: MIDI Monitor only repaints when MIDI has changed
            if (processor.mainGraph) {
                for (auto* node : processor.mainGraph->getNodes()) {
                    if (auto* midiMonitor = dynamic_cast<MidiMonitorProcessor*>(node->getProcessor())) {
                        if (midiMonitor->hasChanged()) {
                            midiMonitor->clearChanged();
                            needsRepaint = true;
                            break;
                        }
                    }
                }
            }
        }

        if (needsRepaint) {
            if (shouldLog)
                LOG("  Calling repaint()...");
            repaint();
            if (shouldLog)
                LOG("  repaint() returned");
            needsRepaint = false;
        }
        
        if (shouldLog)
            LOG("<<< MainTimer complete");
    }
    else if (timerID == StereoMeterTimerID)
    {
        // =============================================================================
        // STEREO METER TIMER (50ms = 20fps) - Smooth meter animation
        // =============================================================================
        // FIX: Only run if stereo meter, recorder, or sampler exists (efficiency)
        if (hasStereoMeter || hasRecorder || hasSampler || hasMidiPlayer || hasStepSeq)
        {
            // Always repaint when meter/recorder/sampler is present for smooth animation
            repaint();
        }
    }
    else if (timerID == MouseInteractionTimerID)
    {
        // =============================================================================
        // MOUSE INTERACTION TIMER (16ms = 60fps) - Smooth dragging
        // =============================================================================
        // Timer is started during mouse drag operations and stopped when drag ends
        // This ensures smooth 60fps visual feedback during node/knob dragging
        needsRepaint = true;
        repaint();
    }
}

bool GraphCanvas::isAsioActive() const
{
    if (auto* dm = SubterraneumAudioProcessor::standaloneDeviceManager)
        return dm->getCurrentAudioDevice() != nullptr;
    return false;
}

// FIX: All branches returned true — removed 8 dead dynamic_cast RTTI lookups
// that were firing ~12,800 times/sec (80 calls/repaint × 20fps stereo meter timer)
bool GraphCanvas::shouldShowNode(juce::AudioProcessorGraph::Node* node) const
{
    return (node != nullptr && node->getProcessor() != nullptr);
}

void GraphCanvas::scanPlugins() {}

void GraphCanvas::verifyPositions()
{
    if (!processor.mainGraph) return;
    for (auto* node : processor.mainGraph->getNodes()) {
        if (!node->properties.contains("x")) node->properties.set("x", 100.0);
        if (!node->properties.contains("y")) node->properties.set("y", 100.0);
    }
}




