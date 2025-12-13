// **Changes:** Changed `mic1GainSlider` / `mic2GainSlider` type from `VerticalSlider` to `StyledSlider` (Task 3).

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EQPanel.h"
#include "DynamicEQPanel.h"
#include "ExciterPanel.h"
#include "CompressorPanel.h"
#include "ReverbPanel.h"
#include "HarmonizerPanel.h"
#include "DelayPanel.h"
#include "../AudioEngine.h"
#include "../PresetManager.h"

class VocalsPage : public juce::Component, private juce::Timer
{
public:
    explicit VocalsPage(AudioEngine& engineRef, PresetManager& presetMgr);
    ~VocalsPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void updateAllControlsFromEngine();

private:
    void timerCallback() override;

    AudioEngine& audioEngine;
    PresetManager& presetManager;

    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;

    juce::Label mic1GainLabel;
    // CHANGED: Use StyledSlider for Horizontal support
    std::unique_ptr<StyledSlider> mic1GainSlider;
    
    juce::Label mic2GainLabel;
    std::unique_ptr<StyledSlider> mic2GainSlider;

    std::unique_ptr<juce::TabbedComponent> tabbedComponent;

    // Tab Contents
    EQPanel* eqPanel1 = nullptr;
    EQPanel* eqPanel2 = nullptr;
    CompressorPanel* compPanel1 = nullptr;
    CompressorPanel* compPanel2 = nullptr;
    ExciterPanel* excPanel1 = nullptr;
    ExciterPanel* excPanel2 = nullptr;
    
    HarmonizerPanel* harmonizerPanel = nullptr;
    ReverbPanel* reverbPanel = nullptr;
    DelayPanel* delayPanel = nullptr;
    DynamicEQPanel* dynEqPanel = nullptr;

    void setupPreampGains();
    void setupTabbedComponent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalsPage)
};