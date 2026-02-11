// ==============================================================================
//  WiringCanvas.h
//  OnStage — Visual node-graph canvas for audio routing
//
//  Adapted from Colosseum's GraphCanvas with key differences:
//    • Audio-only pins (no MIDI nodes/pins/wires in the graph)
//    • No external plugin loading — only our built-in effect nodes
//    • Sidechain pins rendered green (Compressor, DynamicEQ)
//    • Editor windows show our custom full-size panels (touch-friendly)
//    • MIDI control stays as an invisible internal layer
//    • PreAmp nodes have inline sliders (no popup editor)
//    • Recorder nodes have on-surface GUI (record/stop/waveform/meters)
//    • NEW: DragAndDropTarget for InternalPluginBrowser drag-drop
//
//  Split across multiple .cpp files for maintainability:
//    WiringCanvas_Core.cpp         — ctor, timer, cache, PreAmp sliders, drag-drop
//    WiringCanvas_Paint.cpp        — paint(), drawNode(), drawWire()
//    WiringCanvas_Mouse.cpp        — mouseDown/Drag/Up, button clicks
//    WiringCanvas_Connections.cpp  — canConnect(), createConnection()
//    WiringCanvas_Layout.cpp       — getNodeBounds(), getPinPos(), findPinAt()
//    WiringCanvas_Menu.cpp         — right-click "Add Effect" menu
//    WiringCanvas_NodeWindows.cpp  — editor window management
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WiringStyle.h"
#include "../graph/OnStageGraph.h"
#include "../graph/EffectNodes.h"
#include "../dsp/RecorderProcessor.h"
#include "../PresetManager.h"

class WiringCanvas : public juce::Component,
                     public juce::MultiTimer,
                     public juce::DragAndDropTarget
{
public:
    WiringCanvas (OnStageGraph& graph, PresetManager& presets);
    ~WiringCanvas() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown       (const juce::MouseEvent& e) override;
    void mouseDrag       (const juce::MouseEvent& e) override;
    void mouseUp         (const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove       (const juce::MouseEvent& e) override;

    void timerCallback (int timerID) override;

    void closeAllEditorWindows();
    void markDirty() { needsRepaint = true; }

    // Access the graph (needed by editor windows for size persistence)
    OnStageGraph& getStageGraph() { return stageGraph; }

    // Timer IDs
    enum TimerIDs { MainTimer = 1, MeterTimer = 2, DragTimer = 3 };

    // =========================================================================
    //  DragAndDropTarget — accept drags from InternalPluginBrowser
    // =========================================================================
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter  (const SourceDetails& dragSourceDetails) override;
    void itemDragMove   (const SourceDetails& dragSourceDetails) override;
    void itemDragExit   (const SourceDetails& dragSourceDetails) override;
    void itemDropped    (const SourceDetails& dragSourceDetails) override;

    // =========================================================================
    //  Node type cache (rebuilt when graph topology changes)
    //  PUBLIC: Needed by WiringCanvas_Layout.cpp helper function
    // =========================================================================
    struct NodeTypeCache
    {
        EffectProcessorNode* effectNode  = nullptr;
        PlaybackNode*        playback    = nullptr;
        RecorderProcessor*   recorder    = nullptr;   // Direct ptr to inner recorder
        bool isAudioInput   = false;
        bool isAudioOutput  = false;
        bool isPlayback     = false;
        bool isRecorder     = false;
        bool hasSidechain   = false;
        juce::String displayName;
    };

    // Editor window storage (indexed by NodeID)
    std::map<juce::AudioProcessorGraph::NodeID,
             std::unique_ptr<juce::DocumentWindow>> editorWindows;

private:
    OnStageGraph&  stageGraph;
    PresetManager& presetManager;

    // =========================================================================
    //  Pin identification
    // =========================================================================
    struct PinID
    {
        juce::AudioProcessorGraph::NodeID nodeID;
        int  pinIndex = 0;
        bool isInput  = false;

        bool isValid() const { return nodeID.uid != 0; }

        bool operator== (const PinID& o) const
        { return nodeID == o.nodeID && pinIndex == o.pinIndex && isInput == o.isInput; }

        bool operator!= (const PinID& o) const { return ! (*this == o); }
    };

    // =========================================================================
    //  Drag state
    // =========================================================================
    struct DraggingCable
    {
        PinID  sourcePin;
        juce::Point<float> currentPos;
        bool   active = false;
        juce::Colour color;
    };

    std::map<juce::AudioProcessorGraph::NodeID, NodeTypeCache> nodeCache;
    size_t lastNodeCount      = 0;
    size_t lastConnectionCount= 0;
    bool   needsRepaint       = true;

    // Track if any recorders exist (for 20fps timer)
    bool hasRecorder = false;

    // Interaction state
    DraggingCable dragCable;
    PinID highlightPin;
    PinID lastHighlightPin;

    juce::AudioProcessorGraph::Connection hoveredConnection {
        { juce::AudioProcessorGraph::NodeID(), 0 },
        { juce::AudioProcessorGraph::NodeID(), 0 }
    };
    juce::AudioProcessorGraph::Connection lastHoveredConnection {
        { juce::AudioProcessorGraph::NodeID(), 0 },
        { juce::AudioProcessorGraph::NodeID(), 0 }
    };

    juce::AudioProcessorGraph::NodeID draggingNodeID;
    juce::Point<float> nodeDragOffset;

    juce::Point<float> lastRightClickPos { 300.0f, 300.0f };

    // =========================================================================
    //  Drag-drop hover state (visual feedback for browser drags)
    // =========================================================================
    bool   dropTargetHovered = false;
    juce::Point<int> dropHoverPos;

    // =========================================================================
    //  PreAmp inline sliders     (real juce::Slider children on canvas)
    // =========================================================================
    std::map<juce::AudioProcessorGraph::NodeID,
             std::unique_ptr<juce::Slider>> preAmpSliders;
    void updatePreAmpSliders();

    // =========================================================================
    //  Recorder inline name editors (tracked to reposition with node)
    //  FIX: TextEditors are now tracked as members so they follow the node
    //       during 20Hz repaints and don't float away on the canvas.
    // =========================================================================
    struct RecorderNameEditorInfo
    {
        std::unique_ptr<juce::TextEditor> editor;
        RecorderProcessor* recorder = nullptr;
    };
    std::map<juce::AudioProcessorGraph::NodeID, RecorderNameEditorInfo> recorderNameEditors;

    void showRecorderNameEditor (juce::AudioProcessorGraph::NodeID nodeID,
                                 RecorderProcessor* recorder,
                                 juce::Rectangle<float> bounds);
    void updateRecorderNameEditors();
    void dismissRecorderNameEditor (juce::AudioProcessorGraph::NodeID nodeID);

    // =========================================================================
    //  Cache management          (WiringCanvas_Core.cpp)
    // =========================================================================
    void rebuildNodeCache();
    const NodeTypeCache* getCached (juce::AudioProcessorGraph::NodeID id);
    bool shouldShowNode (juce::AudioProcessorGraph::Node* node) const;

    // =========================================================================
    //  Layout / geometry         (WiringCanvas_Layout.cpp)
    // =========================================================================
    juce::Rectangle<float> getNodeBounds (juce::AudioProcessorGraph::Node* node) const;
    juce::Point<float>     getPinPos     (juce::AudioProcessorGraph::Node* node,
                                          const PinID& pin) const;
    juce::Point<float>     getPinCenter  (const PinID& pin) const;
    juce::Colour           getPinColor   (const PinID& pin,
                                          juce::AudioProcessorGraph::Node* node) const;
    PinID                  findPinAt     (juce::Point<float> pos);
    juce::AudioProcessorGraph::Node* findNodeAt (juce::Point<float> pos);

    // =========================================================================
    //  Drawing helpers           (WiringCanvas_Paint.cpp)
    // =========================================================================
    void drawWire  (juce::Graphics& g, juce::Point<float> a,
                    juce::Point<float> b, juce::Colour c, float thickness);
    void drawPin   (juce::Graphics& g, juce::Point<float> pos,
                    juce::Colour color, bool hovered, bool highlighted);
    void drawNode  (juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawNodePins    (juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawNodeButtons (juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawRecorderNode (juce::Graphics& g, juce::AudioProcessorGraph::Node* node,
                           juce::Rectangle<float> bounds);

    // =========================================================================
    //  Connections               (WiringCanvas_Connections.cpp)
    // =========================================================================
    bool canConnect (PinID a, PinID b);
    void createConnection (PinID a, PinID b);
    juce::AudioProcessorGraph::Connection getConnectionAt (juce::Point<float> pos);
    void deleteConnectionAt (juce::Point<float> pos);

    // =========================================================================
    //  Button hit-test           (WiringCanvas_Mouse.cpp)
    // =========================================================================
    juce::Rectangle<float> getButtonRect (juce::Rectangle<float> nodeBounds, int index);
    void handleButtonClick (juce::AudioProcessorGraph::Node* node, int buttonIndex);

    // =========================================================================
    //  Menus                     (WiringCanvas_Menu.cpp)
    // =========================================================================
    void showAddEffectMenu();
    void showNodeContextMenu (juce::AudioProcessorGraph::Node* node,
                              juce::Point<float> pos);
    void showWireMenu (const juce::AudioProcessorGraph::Connection& conn,
                       juce::Point<float> pos);

    // =========================================================================
    //  Pin tooltip               (WiringCanvas_Mouse.cpp)
    // =========================================================================
    void showPinTooltip (const PinID& pin, juce::Point<float> pos);

    // =========================================================================
    //  Editor windows            (WiringCanvas_NodeWindows.cpp)
    // =========================================================================
    void openEditorWindow  (juce::AudioProcessorGraph::Node* node);
    void closeEditorWindow (juce::AudioProcessorGraph::NodeID id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiringCanvas)
};
