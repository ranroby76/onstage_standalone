#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "RegistrationManager.h"

// Forward declaration
class SubterraneumAudioProcessor;

// =============================================================================
// RegistrationTab - Product registration and license management
// =============================================================================
class RegistrationTab : public juce::Component,
                        public juce::Button::Listener,
                        private juce::Timer {
public:
    RegistrationTab(SubterraneumAudioProcessor& p);
    ~RegistrationTab() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    void timerCallback() override;
    
    // Check if registered (for LED indicator)
    bool isRegistered() const { return RegistrationManager::getInstance().isRegistered(); }
    
private:
    SubterraneumAudioProcessor& processor;
    
    // Status display
    juce::Label statusLabel { "status", "Checking registration..." };
    juce::Label machineIdLabel { "machineId", "Machine ID:" };
    juce::Label machineIdValue { "machineIdVal", "" };
    
    // Serial input
    juce::Label serialLabel { "serial", "Enter Serial Number:" };
    juce::TextEditor serialInput;
    juce::TextButton registerBtn { "Register" };
    
    // Info labels
    juce::Label infoLabel { "info", "" };
    juce::Label demoInfoLabel { "demoInfo", "" };
    
    // Copy machine ID button
    juce::TextButton copyIdBtn { "Copy ID" };
    
    void updateStatus();
    void attemptRegistration();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegistrationTab)
};

// =============================================================================
// Registration LED component for tab bar
// =============================================================================
class RegistrationLED : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(2);
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto ledBounds = juce::Rectangle<float>(
            bounds.getCentreX() - size/2,
            bounds.getCentreY() - size/2,
            size, size
        );
        
        g.setColour(isActive ? juce::Colours::lime : juce::Colours::red);
        g.fillEllipse(ledBounds);
        
        // Highlight
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillEllipse(ledBounds.reduced(size * 0.3f).translated(-1, -1));
    }
    
    void setActive(bool active) { 
        isActive = active; 
        repaint(); 
    }
    
    bool getActive() const { return isActive; }
    
private:
    bool isActive = false;
};
