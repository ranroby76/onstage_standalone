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
    juce::TextButton btnIntro { "Introduction" };
    juce::TextButton btnRack { "Rack View" };
    juce::TextButton btnMixer { "Mixer" };
    juce::TextButton btnStudio { "Studio" };
    juce::TextButton btnAudioMidi { "Audio/MIDI" };
    juce::TextButton btnPlugins { "Plugins" };
    juce::TextButton btnSystemTools { "System Tools" };
    juce::TextButton btnShortcuts { "Shortcuts" };
    juce::TextButton btnTroubleshooting { "Troubleshooting" };
    
    // Content display
    juce::TextEditor contentView;
    
    // Chapter content
    enum Chapter {
        Introduction,
        RackView,
        Mixer,
        Studio,
        AudioMidi,
        Plugins,
        SystemTools,
        Shortcuts,
        Troubleshooting
    };
    
    Chapter currentChapter = Introduction;
    
    void showChapter(Chapter chapter);
    juce::String getChapterContent(Chapter chapter);
    void highlightButton(Chapter chapter);
    
    std::vector<juce::TextButton*> chapterButtons;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ManualTab)
};
