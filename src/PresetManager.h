#pragma once

#include <juce_core/juce_core.h>
#include "AudioEngine.h" // FIXED: Removed leading ../
#include "dsp/DynamicEQProcessor.h" // FIXED: Added dsp/ path

class PresetManager
{
public:
    PresetManager(AudioEngine& engine);
    ~PresetManager() = default;

    void loadDefaultPreset();
    juce::String getCurrentPresetName() const;
    bool savePreset(const juce::File& file);
    bool loadPreset(const juce::File& file);

    // Helper Declarations - Updated for Dual Band
    juce::var eqParamsToVar(const EQProcessor::Params& params);
    CompressorProcessor::Params varToCompParams(const juce::var& v);
    juce::var compParamsToVar(const CompressorProcessor::Params& p);
    juce::var exciterParamsToVar(const ExciterProcessor::Params& p);
    ExciterProcessor::Params varToExciterParams(const juce::var& v);
    juce::var reverbParamsToVar(const ReverbProcessor::Params& p);
    ReverbProcessor::Params varToReverbParams(const juce::var& v);
    juce::var delayParamsToVar(const DelayProcessor::Params& p);
    DelayProcessor::Params varToDelayParams(const juce::var& v);
    juce::var harmonizerParamsToVar(const HarmonizerProcessor::Params& p);
    HarmonizerProcessor::Params varToHarmonizerParams(const juce::var& v);
    
    // Fixed Signatures for Dynamic EQ
    juce::var dynEqParamsToVar(DynamicEQProcessor& dynEq);
    void varToDynEqParams(const juce::var& v, DynamicEQProcessor& dynEq);

private:
    AudioEngine& audioEngine;
    juce::String currentPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};