// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas.h
// FIX: Added dedicated 50ms timer for stereo meter (20fps refresh)
// NEW: Drag and drop support for Plugin Browser Panel
// NEW: addPluginAtPosition() method for adding plugins at specific coordinates
// FIXED: Added RecorderProcessor support
// NEW: Per-plugin transport override (T button)
// NEW: ManualSampler, AutoSampler, MidiPlayer system tools
// NEW: 4x canvas (19200x12800), Minimap + Scrollbars, extended zoom 0.10-2.0

#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "UIComponents.h"
#include "MidiSelectors.h"
#include "PluginProcessor.h"
#include "TransportOverrideComponent.h"

// Forward declarations
class SubterraneumAudioProcessorEditor;
class SimpleConnectorProcessor;
class StereoMeterProcessor;
class MidiMonitorProcessor;
class RecorderProcessor;
class ManualSamplerProcessor;
class AutoSamplerProcessor;
class MidiPlayerProcessor;
class CCStepperProcessor;
class TransientSplitterProcessor;
class LatcherProcessor;
class MidiMultiFilterProcessor;

// FIX: Use MultiTimer for separate timer rates
// NEW: Inherit from DragAndDropTarget for plugin browser support
class GraphCanvas : public juce::Component, 
                    public juce::MultiTimer,
                    public juce::DragAndDropTarget { 
public: 
    GraphCanvas(SubterraneumAudioProcessor& p);
    ~GraphCanvas() override; 
    void paint(juce::Graphics& g) override; 
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override; 
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override; 
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override; 
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void updateParentSelector(); 
    void timerCallback(int timerID) override;
    void parentHierarchyChanged() override;
    
    // =========================================================================
    // NEW: Drag and Drop support for Plugin Browser Panel
    // =========================================================================
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;
    
    // NEW: Add plugin at specific position (used by drag-drop and double-click)
    void addPluginAtPosition(const juce::PluginDescription& description, juce::Point<int> position);
    
    // VST2 plugin loader (file chooser) - public for PluginEditor drag-drop
    void loadVST2Plugin(juce::Point<float> position);
    void loadVST3Plugin(juce::Point<float> position);
    
    // Timer IDs for different refresh rates
    enum TimerIDs {
        MainTimerID = 1,
        StereoMeterTimerID = 2,
        MouseInteractionTimerID = 3
    };
    
    // FIX: Properly delete all editors before clearing windows
    void closeAllPluginWindows() 
    { 
        for (auto& [nodeID, window] : activePluginWindows)
        {
            if (window)
            {
                auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(window->getContentComponent());
                
                if (editor)
                {
                    if (auto* proc = editor->getAudioProcessor())
                    {
                        proc->editorBeingDeleted(editor);
                    }
                }
                
                window->clearContentComponent();
            }
        }
        
        activePluginWindows.clear(); 
    }
    
    void markDirty() { needsRepaint = true; }
    void refreshCache() { rebuildNodeTypeCache(); }
    
    // =========================================================================
    // Virtual canvas dimensions — 4x expanded (was 4800x3200)
    // =========================================================================
    static constexpr float virtualCanvasWidth  = 19200.0f;
    static constexpr float virtualCanvasHeight = 12800.0f;
    
    // Pan offset in virtual canvas coordinates
    float panOffsetX = 0.0f;
    float panOffsetY = 0.0f;
    
    void setZoomLevel(float zoom) 
    { 
        zoomLevel = juce::jlimit(0.10f, 2.0f, zoom);
        // NOTE: No setTransform() — zoom is applied in paint() only
        // This prevents the component from overflowing its parent bounds
        clampPanOffset();
        repaint();
        if (auto* parent = getParentComponent())
            parent->repaint();
    }
    float getZoomLevel() const { return zoomLevel; }
    
    // Convert component pixel position to virtual canvas position
    // Without setTransform, e.position is in raw pixels → divide by zoom then add pan
    juce::Point<float> toVirtual(juce::Point<float> localPos) const
    {
        return juce::Point<float>(localPos.x / zoomLevel + panOffsetX,
                                  localPos.y / zoomLevel + panOffsetY);
    }
    
    // Convert virtual canvas coords to screen coords for popup positioning
    juce::Point<int> virtualToScreen(float vx, float vy) const
    {
        auto sb = getScreenBounds();
        return { sb.getX() + (int)((vx - panOffsetX) * zoomLevel),
                 sb.getY() + (int)((vy - panOffsetY) * zoomLevel) };
    }
    
    // =========================================================================
    // FIX: clampPanOffset now accounts for zoom level
    // Visible area in virtual coords = component pixel size / zoomLevel
    // =========================================================================
    void clampPanOffset()
    {
        float visibleW = (float)getWidth()  / zoomLevel;
        float visibleH = (float)getHeight() / zoomLevel;
        panOffsetX = juce::jlimit(0.0f, juce::jmax(0.0f, virtualCanvasWidth  - visibleW), panOffsetX);
        panOffsetY = juce::jlimit(0.0f, juce::jmax(0.0f, virtualCanvasHeight - visibleH), panOffsetY);
    }
    
    // Helper: visible area dimensions in virtual coords
    float getVisibleWidth()  const { return (float)getWidth()  / zoomLevel; }
    float getVisibleHeight() const { return (float)getHeight() / zoomLevel; }
    
    std::map<juce::AudioProcessorGraph::NodeID, std::unique_ptr<juce::DocumentWindow>> activePluginWindows;
    
private: 
    SubterraneumAudioProcessor& processor; 
    
    float zoomLevel = 1.0f;
    
    // Drag-to-pan state
    bool isPanning = false;
    juce::Point<int> panMouseScreenStart;   // screen position at drag start
    float panStartOffsetX = 0.0f;           // panOffset at drag start
    float panStartOffsetY = 0.0f;
    juce::OpenGLContext openGLContext;
    
    struct PinID { 
        juce::AudioProcessorGraph::NodeID nodeID; 
        int pinIndex; 
        bool isInput; 
        bool isMidi;
        bool isValid() const { return nodeID.uid != 0; } 
        bool operator==(const PinID& other) const { 
            return nodeID == other.nodeID && pinIndex == other.pinIndex && 
                   isInput == other.isInput && isMidi == other.isMidi;
        } 
        bool operator!=(const PinID& other) const { return !(*this == other); } 
    }; 
    
    struct DraggingCable { 
        PinID sourcePin; 
        juce::Point<float> currentDragPos; 
        bool active = false; 
        juce::Colour dragColor; 
    };
    
    struct NodeTypeCache {
        MeteringProcessor* meteringProc = nullptr;
        SimpleConnectorProcessor* simpleConnector = nullptr;
        StereoMeterProcessor* stereoMeter = nullptr;
        MidiMonitorProcessor* midiMonitor = nullptr;
        RecorderProcessor* recorder = nullptr;
        ManualSamplerProcessor* manualSampler = nullptr;
        AutoSamplerProcessor* autoSampler = nullptr;
        MidiPlayerProcessor* midiPlayer = nullptr;
        CCStepperProcessor* ccStepper = nullptr;
        TransientSplitterProcessor* transientSplitter = nullptr;
        LatcherProcessor* latcher = nullptr;
        MidiMultiFilterProcessor* midiMultiFilter = nullptr;
        bool isAudioInput = false;
        bool isAudioOutput = false;
        bool isMidiInput = false;
        bool isMidiOutput = false;
        bool isIO = false;
        bool isInstrument = false;
        bool hasSidechain = false;
        bool inSamplingChain = false;
        juce::String pluginName;
    };
    
    std::map<juce::AudioProcessorGraph::NodeID, NodeTypeCache> nodeTypeCache;
    std::vector<juce::AudioProcessorGraph::Connection> cachedConnections;  // FIX: Cache connections to avoid vector copy in paint()
    size_t lastNodeCount = 0;
    size_t lastConnectionCount = 0;
    bool needsRepaint = true;
    
    bool hasStereoMeter = false;
    bool hasRecorder = false;
    bool hasSampler = false;
    bool hasMidiPlayer = false;
    bool hasStepSeq = false;
    bool hasLatcher = false;
    
    PinID lastHighlightPin;
    juce::AudioProcessorGraph::Connection lastHoveredConnection = { 
        {juce::AudioProcessorGraph::NodeID(), 0}, 
        {juce::AudioProcessorGraph::NodeID(), 0} 
    };
    
    DraggingCable dragCable; 
    PinID highlightPin; 
    juce::AudioProcessorGraph::Connection hoveredConnection = { 
        {juce::AudioProcessorGraph::NodeID(), 0}, 
        {juce::AudioProcessorGraph::NodeID(), 0} 
    };
    juce::AudioProcessorGraph::NodeID draggingNodeID; 
    juce::Point<float> nodeDragOffset; 
    
    // Magnetic snap: connected nodes attract when dragged nearby
    static constexpr float magneticSnapThreshold = 20.0f;  // pixels
    void applyMagneticSnap(juce::AudioProcessorGraph::Node* draggedNode, float& x, float& y);
    
    juce::AudioProcessorGraph::NodeID draggingKnobNodeID;
    float knobDragStartY = 0.0f;
    float knobDragStartValue = 0.0f;
    
    juce::Point<float> lastRightClickPos {300.0f, 300.0f};
    
    // NEW: Drag-drop hover indicator
    bool isDragHovering = false;
    juce::Point<int> dragHoverPosition;
    
    // NEW: VST2 manual loading file chooser (must stay alive for async callback)
    std::unique_ptr<juce::FileChooser> vst2FileChooser;
    
    // Shared file chooser for VST3 manual loading
    std::unique_ptr<juce::FileChooser> pluginFileChooser;
    
    // NEW: Sampler folder chooser
    std::unique_ptr<juce::FileChooser> samplerFolderChooser;
    
    // NEW: MIDI file chooser
    std::unique_ptr<juce::FileChooser> midiFileChooser;
    
    // Shared last-used directory for ALL file browsers (MIDI, presets, etc.)
    juce::File lastBrowsedDirectory;
    
    // Persist lastBrowsedDirectory across sessions
    void saveLastBrowsedDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Subterraneum");
        dir.createDirectory();
        auto file = dir.getChildFile("lastBrowsedDir.txt");
        file.replaceWithText(lastBrowsedDirectory.getFullPathName());
    }
    
    void loadLastBrowsedDirectory()
    {
        auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("Subterraneum")
                        .getChildFile("lastBrowsedDir.txt");
        if (file.existsAsFile())
        {
            auto path = file.loadFileAsString().trim();
            if (path.isNotEmpty())
            {
                juce::File dir(path);
                if (dir.isDirectory())
                    lastBrowsedDirectory = dir;
            }
        }
    }
    
    // MIDI Player slider drag state
    bool midiSliderDragging = false;
    MidiPlayerProcessor* midiSliderDragPlayer = nullptr;
    float midiSliderTrackX = 0.0f;
    float midiSliderTrackW = 1.0f;
    
    // MIDI Player BPM knob drag state
    bool midiBpmDragging = false;
    MidiPlayerProcessor* midiBpmDragPlayer = nullptr;
    float midiBpmDragStartY = 0.0f;
    double midiBpmDragStartValue = 120.0;
    
    // Step Seq BPM drag state
    bool stepSeqBpmDragging = false;
    CCStepperProcessor* stepSeqBpmDragProcessor = nullptr;
    float stepSeqBpmDragStartY = 0.0f;
    double stepSeqBpmDragStartValue = 120.0;
    
    void initializeOpenGL();
    void rebuildNodeTypeCache();
    const NodeTypeCache* getCachedNodeType(juce::AudioProcessorGraph::NodeID nodeID);
    
    struct PinInfo {
        juce::AudioProcessorGraph::NodeID nodeID;
        int channelIndex;
        bool isInput;
        bool isMidi;
    };
    
    juce::Rectangle<float> getPinBounds(const PinInfo& pin, juce::AudioProcessorGraph::Node* node) const;
    juce::Point<float> getPinCenterPos(const PinID& pinID) const;
    void drawWire(juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, juce::Colour color, float thickness);
    void drawNodePins(juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawNodeButtons(juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawAudioIOToggle(juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    
    void drawNode(juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawPin(juce::Graphics& g, juce::Point<float> pos, juce::Colour color, bool isHovered, bool isHighlighted); 
    juce::Rectangle<float> getNodeBounds(juce::AudioProcessorGraph::Node* node) const;
    juce::Point<float> getPinPos(juce::AudioProcessorGraph::Node* node, const PinID& pinId) const;
    juce::Colour getPinColor(const PinID& pinId, juce::AudioProcessorGraph::Node* node); 
    juce::Rectangle<float> getButtonRect(juce::Rectangle<float> nodeBounds, int index);
    juce::Rectangle<float> getIOButtonRect(juce::Rectangle<float> nodeBounds);
    void handleButtonClick(juce::AudioProcessorGraph::Node* node, int buttonIndex); 
    void handleIOButtonClick(juce::AudioProcessorGraph::Node* node);
    void openPluginWindow(juce::AudioProcessorGraph::Node* node);
    PinID findPinAt(juce::Point<float> pos); 
    juce::AudioProcessorGraph::Node* findNodeAt(juce::Point<float> pos); 
    juce::AudioProcessorGraph::Connection getConnectionAt(juce::Point<float> pos);
    bool canConnect(PinID start, PinID end); 
    void createConnection(PinID start, PinID end);
    void deleteConnectionAt(juce::Point<float> pos); 
    void showPinInfo(const PinID& pin, const juce::Point<float>& screenPos);
    void showWireMenu(const juce::AudioProcessorGraph::Connection& conn, const juce::Point<float>& screenPos); 
    void showPluginMenu();
    void showNodeContextMenu(juce::AudioProcessorGraph::Node* node, juce::Point<float> pos);
    void showMidiChannelFilter(juce::AudioProcessorGraph::Node* node);
    void showTransportOverride(juce::AudioProcessorGraph::Node* node);
    void reloadVST2Node(juce::AudioProcessorGraph::Node* node);
    juce::File getVST2DefaultFolder() const;
    juce::File getVST3DefaultFolder() const;
    
    // Auto Sampler editor popup
    void showAutoSamplerEditor(AutoSamplerProcessor* autoSampler);
    void showMidiPlayerChannelInfo(MidiPlayerProcessor* midiPlayer);
    void showStepSeqEditor(juce::AudioProcessorGraph::Node* node);
    void showTransientSplitterEditor(TransientSplitterProcessor* proc);
    void showLatcherEditor(juce::AudioProcessorGraph::Node* node);
    void showMidiMultiFilterEditor(juce::AudioProcessorGraph::Node* node);
    void disconnectNode(juce::AudioProcessorGraph::Node* node);
    void scanPlugins(); 
    void verifyPositions();
    bool isAsioActive() const;
    bool shouldShowNode(juce::AudioProcessorGraph::Node* node) const;
    
    // =========================================================================
    // NEW: Minimap + Scrollbar system
    // =========================================================================
    static constexpr float minimapWidth  = 220.0f;
    static constexpr float minimapHeight = 146.67f;  // 3:2 ratio matching canvas
    static constexpr float minimapMargin = 8.0f;
    static constexpr float scrollbarThickness = 14.0f;
    
    // Get overlay rectangles in component PIXEL coords (not virtual)
    // Overlays are drawn after undoing the zoom+pan transform
    juce::Rectangle<float> getMinimapRect() const
    {
        float w = (float)getWidth();
        float h = (float)getHeight();
        return juce::Rectangle<float>(
            w - minimapWidth - minimapMargin - scrollbarThickness,
            h - minimapHeight - minimapMargin - scrollbarThickness,
            minimapWidth, minimapHeight);
    }
    
    juce::Rectangle<float> getHScrollbarRect() const
    {
        float w = (float)getWidth();
        float h = (float)getHeight();
        return juce::Rectangle<float>(0.0f, h - scrollbarThickness, 
                                       w - scrollbarThickness, scrollbarThickness);
    }
    
    juce::Rectangle<float> getVScrollbarRect() const
    {
        float w = (float)getWidth();
        float h = (float)getHeight();
        return juce::Rectangle<float>(w - scrollbarThickness, 0.0f, 
                                       scrollbarThickness, h - scrollbarThickness);
    }
    
    // Minimap interaction state
    bool isDraggingMinimap = false;
    
    // Scrollbar interaction state
    bool isDraggingHScrollbar = false;
    bool isDraggingVScrollbar = false;
    float scrollbarDragStartOffset = 0.0f;
    float scrollbarDragStartPan = 0.0f;
    
    // Drawing helpers (implemented in GraphCanvas_Paint.cpp)
    void drawMinimap(juce::Graphics& g);
    void drawScrollbars(juce::Graphics& g);
    
    // Hit-test helpers
    bool isPointInMinimap(juce::Point<float> localPos) const { return getMinimapRect().contains(localPos); }
    bool isPointInHScrollbar(juce::Point<float> localPos) const { return getHScrollbarRect().contains(localPos); }
    bool isPointInVScrollbar(juce::Point<float> localPos) const { return getVScrollbarRect().contains(localPos); }
    
    // Convert minimap click to virtual canvas position
    void navigateMinimapTo(juce::Point<float> localClickPos);
};
