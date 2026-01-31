#pragma once

#include <juce_core/juce_core.h>
#include "AudioEngine.h"

// FIX: Include DSP headers so we can access their ::Params structs
#include "dsp/EQProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/ExciterProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/DelayProcessor.h"
#include "dsp/HarmonizerProcessor.h"
#include "dsp/DynamicEQProcessor.h"

class PresetManager
{
public:
    PresetManager(AudioEngine& engine);
    ~PresetManager() = default;

    bool savePreset(const juce::File& file);
    bool loadPreset(const juce::File& file);
    void loadDefaultPreset();
    juce::String getCurrentPresetName() const;

private:
    AudioEngine& audioEngine;
    juce::String currentPresetName;

    // Helper converters
    juce::var eqParamsToVar(const EQProcessor::Params& params);
    
    juce::var compParamsToVar(const CompressorProcessor::Params& params);
    CompressorProcessor::Params varToCompParams(const juce::var& v);

    juce::var exciterParamsToVar(const ExciterProcessor::Params& params);
    ExciterProcessor::Params varToExciterParams(const juce::var& v);

    juce::var reverbParamsToVar(const ReverbProcessor::Params& params);
    ReverbProcessor::Params varToReverbParams(const juce::var& v);
    
    juce::var delayParamsToVar(const DelayProcessor::Params& params);
    DelayProcessor::Params varToDelayParams(const juce::var& v);

    juce::var harmonizerParamsToVar(const HarmonizerProcessor::Params& params);
    HarmonizerProcessor::Params varToHarmonizerParams(const juce::var& v);
    
    juce::var dynEqParamsToVar(const DynamicEQProcessor::Params& params);
    DynamicEQProcessor::Params varToDynEqParams(const juce::var& v);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};