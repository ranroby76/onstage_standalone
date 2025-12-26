#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EQPanel.h"
#include "DynamicEQPanel.h"
#include "ExciterPanel.h"
#include "SculptPanel.h"
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

    std::unique_ptr<juce::TabbedComponent> tabbedComponent;

    // Tab Contents
    EQPanel* eqPanel1 = nullptr;
    EQPanel* eqPanel2 = nullptr;
    CompressorPanel* compPanel1 = nullptr;
    CompressorPanel* compPanel2 = nullptr;
    ExciterPanel* excPanel1 = nullptr;
    ExciterPanel* excPanel2 = nullptr;
    SculptPanel* sculptPanel1 = nullptr;
    SculptPanel* sculptPanel2 = nullptr;
    
    HarmonizerPanel* harmonizerPanel = nullptr;
    ReverbPanel* reverbPanel = nullptr;
    DelayPanel* delayPanel = nullptr;
    DynamicEQPanel* dynEqPanel = nullptr;

    void setupTabbedComponent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalsPage)
};