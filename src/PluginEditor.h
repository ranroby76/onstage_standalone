
// FIX: Plugin Browser Panel is now a fixed panel (288px) to the left of yellow menu
// FIX: Removed Studio tab - tempo/metronome moved to AudioSettingsTab
// FIX: Added MIDI Panic button under Keys button

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
#include "ManualTab.h"
#include "RegistrationTab.h"
#include "PluginBrowserPanel.h"

// =============================================================================
// Main Editor - Plugin Browser Panel as fixed side panel
// FIX: Removed StudioTab - tempo/metronome now in AudioSettingsTab
// FIX: Added MIDI Panic button
// =============================================================================
class SubterraneumAudioProcessorEditor : public juce::AudioProcessorEditor, 
                                          public juce::Button::Listener,
                                          public juce::DragAndDropContainer,
                                          private juce::Timer { 
public: 
    SubterraneumAudioProcessorEditor(SubterraneumAudioProcessor&);
    ~SubterraneumAudioProcessorEditor() override;
    void paint(juce::Graphics&) override; 
    void resized() override; 
    void buttonClicked(juce::Button*) override;
    void timerCallback() override;
    void updateInstrumentSelector();
    void updateTabButtonColors();
    
    // Keyboard shortcut support
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    
    // Public access for GraphCanvas to request browser panel state
    PluginBrowserPanel* getPluginBrowserPanel() { return pluginBrowserPanel.get(); }
    
private: 
    SubterraneumAudioProcessor& audioProcessor;
    
    // Logo images
    juce::Image fananLogo;
    juce::Image colosseumLogo;
    
    juce::TextButton loadButton { "Load Patch" };
    juce::TextButton saveButton { "Save Patch" }; 
    juce::TextButton resetButton { "Reset" };
    juce::TextButton keysButton { "Keys" };
    juce::TextButton panicButton { "PANIC" };  // NEW: MIDI Panic button
    
    // Left green menu tab buttons - FIX: Only 6 buttons (removed studioButton)
    juce::TextButton rackButton { "Rack" };
    juce::TextButton mixerButton { "Mixer" };
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton pluginsButton { "Plugins" };
    juce::TextButton manualButton { "Manual" };
    juce::TextButton registerButton { "Register" };
    
    // Header Status LEDs with labels
    AsioStatusLed asioLed;
    juce::Label asioLabel { "asioLbl", "ASIO" };
    
    RegistrationLED registrationLED;
    juce::Label regLabel { "regLbl", "REG" };
    
    // CPU and RAM meters in header
    juce::Label cpuLabel { "cpuLbl", "CPU: 0%" };
    juce::Label ramLabel { "ramLbl", "RAM: 0MB" };
    
    // Zoom slider for Rack tab (right-click resets to 100%)
    class ZoomSlider : public juce::Slider {
    public:
        ZoomSlider() { setMouseClickGrabsKeyboardFocus(false); }
        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isPopupMenu()) {
                setValue(1.0, juce::sendNotificationSync);
                return;
            }
            juce::Slider::mouseDown(e);
        }
    };
    ZoomSlider zoomSlider;
    juce::Label zoomLabel { "zoomLbl", "100%" };
    
    GraphCanvas graphCanvas; 
    MixerView mixerView;
    AudioSettingsTab audioSettingsTab;  // Now includes tempo/metronome
    PluginManagerTab pluginManagerTab;
    // FIX: Removed StudioTab studioTab;
    ManualTab manualTab;
    RegistrationTab registrationTab;
    InstrumentSelector instrumentSelector; 
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // =========================================================================
    // Plugin Browser Panel - Fixed 288px (3x yellow menu) panel to left of yellow menu
    // Only visible on Rack tab, no toggle button needed
    // =========================================================================
    std::unique_ptr<PluginBrowserPanel> pluginBrowserPanel;
    static constexpr int pluginBrowserWidth = 288;  // 3x yellow menu width (96)
    
    // Helper to show/hide browser panel based on current tab
    void updatePluginBrowserVisibility();
    
    // NEW: Send MIDI panic to all instruments
    void sendMidiPanic();
    
    // =========================================================================
    // Workspace Selector Bar — 16 switchable sessions
    // =========================================================================
    static constexpr int workspaceBarHeight = 28;
    juce::TextButton workspaceButtons[16];
    juce::Label workspacesLabel { "wsLabel", "WORKSPACES" };
    void updateWorkspaceButtonColors();
    void showWorkspaceContextMenu(int workspaceIndex);
    
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






