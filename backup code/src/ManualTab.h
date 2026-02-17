#pragma once

#include <JuceHeader.h>
#include "Style.h"

// Forward declaration
class SubterraneumAudioProcessor;

// =============================================================================
// ManualTab - Complete user manual with chapter navigation
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
    
    // Chapter navigation buttons
    juce::TextButton btnIntro { "Welcome" };
    juce::TextButton btnRack { "Rack" };
    juce::TextButton btnWorkspaces { "Workspaces" };
    juce::TextButton btnMixer { "Mixer" };
    juce::TextButton btnSettings { "Settings" };
    juce::TextButton btnPlugins { "Plugins" };
    juce::TextButton btnSystemTools { "System Tools" };
    juce::TextButton btnSampling { "Sampling" };
    juce::TextButton btnShortcuts { "Shortcuts" };
    juce::TextButton btnTroubleshooting { "Troubleshoot" };
    juce::TextButton btnDemo { "Demo / Register" };
    
    // Content display
    juce::TextEditor contentView;
    
    // Chapter content
    enum Chapter {
        Introduction,
        RackView,
        Workspaces,
        Mixer,
        Settings,
        Plugins,
        SystemTools,
        Sampling,
        Shortcuts,
        Troubleshooting,
        DemoLimitations
    };
    
    Chapter currentChapter = Introduction;
    
    void showChapter(Chapter chapter);
    juce::String getChapterContent(Chapter chapter);
    void highlightButton(Chapter chapter);
    
    std::vector<juce::TextButton*> chapterButtons;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ManualTab)
};
