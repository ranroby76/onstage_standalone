// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas.h
// FIX: Added dedicated 50ms timer for stereo meter (20fps refresh)
// NEW: Drag and drop support for Plugin Browser Panel
// NEW: addPluginAtPosition() method for adding plugins at specific coordinates
// FIXED: Added RecorderProcessor support
// NEW: Per-plugin transport override (T button)
// NEW: ManualSampler, AutoSampler, MidiPlayer system tools

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
    
    // VST2 plugin loader (file chooser) - public for PluginEditor drag-drop
    void loadVST2Plugin(juce::Point<float> position);
    
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
    void refreshCache() { rebuildNodeTypeCache(); }
    
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
        ManualSamplerProcessor* manualSampler = nullptr;
        AutoSamplerProcessor* autoSampler = nullptr;
        MidiPlayerProcessor* midiPlayer = nullptr;
        CCStepperProcessor* ccStepper = nullptr;
        TransientSplitterProcessor* transientSplitter = nullptr;
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
    size_t lastNodeCount = 0;
    size_t lastConnectionCount = 0;
    bool needsRepaint = true;
    
    bool hasStereoMeter = false;
    bool hasRecorder = false;
    bool hasSampler = false;
    bool hasMidiPlayer = false;
    bool hasStepSeq = false;
    
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
    
    // NEW: VST2 manual loading file chooser (must stay alive for async callback)
    std::unique_ptr<juce::FileChooser> vst2FileChooser;
    
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
    
    // Auto Sampler editor popup
    void showAutoSamplerEditor(AutoSamplerProcessor* autoSampler);
    void showMidiPlayerChannelInfo(MidiPlayerProcessor* midiPlayer);
    void showStepSeqEditor(juce::AudioProcessorGraph::Node* node);
    void showTransientSplitterEditor(TransientSplitterProcessor* proc);
    void disconnectNode(juce::AudioProcessorGraph::Node* node);
    void scanPlugins(); 
    void verifyPositions();
    bool isAsioActive() const;
    bool shouldShowNode(juce::AudioProcessorGraph::Node* node) const;
};