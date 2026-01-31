#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Forward declaration
class SubterraneumAudioProcessorEditor;

class InstrumentSelector : public juce::Component, public juce::Button::Listener, public juce::Timer { 
public: 
    InstrumentSelector(SubterraneumAudioProcessor& p); 
    ~InstrumentSelector() override;
    void paint(juce::Graphics& g) override;
    void resized() override; 
    void mouseDown(const juce::MouseEvent& e) override; 
    void mouseDrag(const juce::MouseEvent& e) override;
    void buttonClicked(juce::Button* b) override; 
    void timerCallback() override;
    void updateList(); 
    
private: 
    SubterraneumAudioProcessor& processor;
    juce::Label titleLabel { "Selector", "INSTRUMENTS" }; 
    juce::ToggleButton multiModeBtn { "Multi" }; 
    juce::OwnedArray<juce::TextButton> instButtons;
    std::vector<juce::AudioProcessorGraph::NodeID> nodeIDs; 
    void handleInstrumentClick(int index); 
};
