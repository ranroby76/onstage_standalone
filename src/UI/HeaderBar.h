#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <functional>
#include "../AudioEngine.h"

class HeaderBar : public juce::Component, private juce::Timer
{
public:
    HeaderBar(AudioEngine& engine);
    ~HeaderBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void()> onSavePreset;
    std::function<void()> onLoadPreset;

    void setPresetName(const juce::String& name);
    juce::String getPresetName() const { return currentPresetName; }

private:
    void timerCallback() override;

    AudioEngine& audioEngine;

    juce::Image fananLogo;
    juce::Image onStageLogo;
    
    // Manual Button
    juce::TextButton manualButton;

    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;
    juce::Label presetNameLabel;
    juce::TextButton registerButton;
    juce::Label modeLabel;
    
    juce::String currentPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};
