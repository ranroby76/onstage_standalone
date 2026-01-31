// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas.h
// FIX: Added dedicated 50ms timer for stereo meter (20fps refresh)
// NEW: Drag and drop support for Plugin Browser Panel
// NEW: addPluginAtPosition() method for adding plugins at specific coordinates
// FIXED: Added RecorderProcessor support

#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "UIComponents.h"
#include "MidiSelectors.h"
#include "PluginProcessor.h"

// Forward declarations
class SubterraneumAudioProcessorEditor;
class SimpleConnectorProcessor;
class StereoMeterProcessor;
class MidiMonitorProcessor;
class RecorderProcessor;

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
                    if (auto* processor = editor->getAudioProcessor())
                    {
                        processor->editorBeingDeleted(editor);
                    }
                }
                
                window->clearContentComponent();
            }
        }
        
        activePluginWindows.clear(); 
    }
    
    void markDirty() { needsRepaint = true; }
    
    std::map<juce::AudioProcessorGraph::NodeID, std::unique_ptr<juce::DocumentWindow>> activePluginWindows;
    
private: 
    SubterraneumAudioProcessor& processor; 
    
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
        bool isAudioInput = false;
        bool isAudioOutput = false;
        bool isMidiInput = false;
        bool isMidiOutput = false;
        bool isIO = false;
        bool isInstrument = false;
        bool hasSidechain = false;
        juce::String pluginName;
    };
    
    std::map<juce::AudioProcessorGraph::NodeID, NodeTypeCache> nodeTypeCache;
    size_t lastNodeCount = 0;
    size_t lastConnectionCount = 0;
    bool needsRepaint = true;
    
    bool hasStereoMeter = false;
    bool hasRecorder = false;
    
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
    
    juce::AudioProcessorGraph::NodeID draggingKnobNodeID;
    float knobDragStartY = 0.0f;
    float knobDragStartValue = 0.0f;
    
    juce::Point<float> lastRightClickPos {300.0f, 300.0f};
    
    // NEW: Drag-drop hover indicator
    bool isDragHovering = false;
    juce::Point<int> dragHoverPosition;
    
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
    void disconnectNode(juce::AudioProcessorGraph::Node* node);
    void scanPlugins(); 
    void verifyPositions();
    bool isAsioActive() const;
    bool shouldShowNode(juce::AudioProcessorGraph::Node* node) const;
};