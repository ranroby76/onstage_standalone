// #D:\Workspace\onstage_colosseum_upgrade\src\ManualTab.h
#pragma once

#include <JuceHeader.h>
#include "Style.h"

// Forward declaration
class SubterraneumAudioProcessor;

// =============================================================================
// ManualTab - Complete user manual with chapter navigation
// OnStage Edition - Mini Karaoke DAW / Live Performance FX Host
// =============================================================================
class ManualTab : public juce::Component,
                  public juce::Button::Listener {
public:
    ManualTab(SubterraneumAudioProcessor& p);
    ~ManualTab() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    
private:
    SubterraneumAudioProcessor& processor;
    
    // Chapter navigation buttons (9 chapters for OnStage)
    juce::TextButton btnWelcome { "Welcome" };
    juce::TextButton btnRack { "Rack" };
    juce::TextButton btnMedia { "Media" };
    juce::TextButton btnSettings { "Settings" };
    juce::TextButton btnPlugins { "Plugins" };
    juce::TextButton btnSystemTools { "System Tools" };
    juce::TextButton btnShortcuts { "Shortcuts" };
    juce::TextButton btnTroubleshooting { "Troubleshoot" };
    juce::TextButton btnDemo { "Demo / Register" };
    
    // Content display
    juce::TextEditor contentView;
    
    // Chapter content
    enum Chapter {
        Welcome,
        RackView,
        Media,
        Settings,
        Plugins,
        SystemTools,
        Shortcuts,
        Troubleshooting,
        DemoLimitations
    };
    
    Chapter currentChapter = Welcome;
    
    void showChapter(Chapter chapter);
    juce::String getChapterContent(Chapter chapter);
    void highlightButton(Chapter chapter);
    
    std::vector<juce::TextButton*> chapterButtons;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ManualTab)
};
