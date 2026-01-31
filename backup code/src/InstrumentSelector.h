#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Forward declaration
class SubterraneumAudioProcessorEditor;

// Custom LookAndFeel for table-style buttons (no rounding, connected)
class TableButtonLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, 
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted, 
                            bool shouldDrawButtonAsDown) override {
        auto bounds = button.getLocalBounds().toFloat();
        
        // Draw flat rectangular button (no rounding)
        g.setColour(backgroundColour);
        g.fillRect(bounds);
        
        // Draw subtle border between buttons
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawRect(bounds, 1.0f);
    }
};

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
    
    // Custom LookAndFeel for table-style buttons
    TableButtonLookAndFeel tableLookAndFeel;
    
    // FIX #2: TooltipWindow to display button tooltips
    juce::TooltipWindow tooltipWindow { this, 700 };  // 700ms delay
};
