#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Style.h"
#include "UIComponents.h"
#include "GraphCanvas.h"
#include "MixerView.h"
#include "AudioSettingsTab.h"
#include "PluginManagerTab.h"
#include "InstrumentSelector.h"
#include "StudioTab.h"
#include "ManualTab.h"
#include "RegistrationTab.h"

// =============================================================================
// Main Editor
// =============================================================================
class SubterraneumAudioProcessorEditor : public juce::AudioProcessorEditor, 
                                          public juce::Button::Listener,
                                          private juce::Timer { 
public: 
    SubterraneumAudioProcessorEditor(SubterraneumAudioProcessor&);
    ~SubterraneumAudioProcessorEditor() override;
    void paint(juce::Graphics&) override; 
    void resized() override; 
    void buttonClicked(juce::Button*) override;
    void timerCallback() override;
    void updateInstrumentSelector(); 
    
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    
private: 
    SubterraneumAudioProcessor& audioProcessor;
    
    // Logo images
    juce::Image subcoreLogo;
    juce::Image colosseumLogo;
    
    juce::TextButton loadButton { "Load Patch" };
    juce::TextButton saveButton { "Save Patch" }; 
    juce::TextButton resetButton { "Reset" };
    juce::TextButton keysButton { "Keys" }; 
    
    // Header Status LEDs with labels
    AsioStatusLed asioLed;
    juce::Label asioLabel { "asioLbl", "ASIO" };
    
    RegistrationLED registrationLED;
    juce::Label regLabel { "regLbl", "REG" };
    
    GraphCanvas graphCanvas; 
    MixerView mixerView;
    AudioSettingsTab audioSettingsTab; 
    PluginManagerTab pluginManagerTab;
    StudioTab studioTab;
    ManualTab manualTab;
    RegistrationTab registrationTab;
    InstrumentSelector instrumentSelector; 
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    class VirtualKeyboardWindow : public juce::DocumentWindow { 
    public: 
        VirtualKeyboardWindow(SubterraneumAudioProcessor& p);
        ~VirtualKeyboardWindow() override; 
        void closeButtonPressed() override; 
    private: 
        juce::MidiKeyboardComponent keyboardComp; 
    }; 
    
    std::unique_ptr<VirtualKeyboardWindow> keyboardWindow;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubterraneumAudioProcessorEditor) 
};
