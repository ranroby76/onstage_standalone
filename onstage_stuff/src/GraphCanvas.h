#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "UIComponents.h"
#include "MidiSelectors.h"
#include "PluginProcessor.h"

// Forward declaration
class SubterraneumAudioProcessorEditor;

class GraphCanvas : public juce::Component, public juce::Timer { 
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
    void timerCallback() override;
    
    // Close all open plugin windows (call before reset)
    void closeAllPluginWindows() { activePluginWindows.clear(); }
    
private: 
    SubterraneumAudioProcessor& processor; 
    
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
    
    DraggingCable dragCable; 
    PinID highlightPin; 
    juce::AudioProcessorGraph::Connection hoveredConnection = { 
        {juce::AudioProcessorGraph::NodeID(), 0}, 
        {juce::AudioProcessorGraph::NodeID(), 0} 
    };
    juce::AudioProcessorGraph::NodeID draggingNodeID; 
    juce::Point<float> nodeDragOffset; 
    std::map<juce::AudioProcessorGraph::NodeID, std::unique_ptr<PluginWindow>> activePluginWindows;
    
    // Simple Connector knob dragging state
    juce::AudioProcessorGraph::NodeID draggingKnobNodeID;
    float knobDragStartY = 0.0f;
    float knobDragStartValue = 0.0f;
    
    void drawNode(juce::Graphics& g, juce::AudioProcessorGraph::Node* node);
    void drawPin(juce::Graphics& g, juce::Point<float> pos, juce::Colour color, bool isHovered, bool isHighlighted); 
    juce::Rectangle<float> getNodeBounds(juce::AudioProcessorGraph::Node* node);
    juce::Point<float> getPinPos(juce::AudioProcessorGraph::Node* node, const PinID& pinId);
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
    void scanPlugins(); 
    void verifyPositions();
    bool isAsioActive() const;
    bool shouldShowNode(juce::AudioProcessorGraph::Node* node) const;
};
