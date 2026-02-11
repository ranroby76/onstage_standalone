#pragma once

#include <juce_core/juce_core.h>
#include "AudioEngine.h"
#include "dsp/EQProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/ExciterProcessor.h"
#include "dsp/SculptProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/DelayProcessor.h"
#include "dsp/HarmonizerProcessor.h"
#include "dsp/DynamicEQProcessor.h"
#include "dsp/PitchProcessor.h"
#include "dsp/MasterProcessor.h"
#include "dsp/DoublerProcessor.h"
#include "dsp/StudioReverbProcessor.h"

// Guitar processors
#include "guitar/OverdriveProcessor.h"
#include "guitar/DistortionProcessor.h"
#include "guitar/FuzzProcessor.h"
#include "guitar/GuitarChorusProcessor.h"
#include "guitar/GuitarFlangerProcessor.h"
#include "guitar/GuitarPhaserProcessor.h"
#include "guitar/GuitarTremoloProcessor.h"
#include "guitar/GuitarVibratoProcessor.h"
#include "guitar/GuitarToneProcessor.h"
#include "guitar/GuitarRotaryProcessor.h"
#include "guitar/GuitarWahProcessor.h"
#include "guitar/GuitarReverbProcessor.h"
#include "guitar/GuitarNoiseGateProcessor.h"
#include "guitar/ToneStackProcessor.h"
#include "guitar/CabSimProcessor.h"

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
    
    // Pitch Processor helpers
    juce::var pitchParamsToVar(const PitchProcessor::Params& p);
    PitchProcessor::Params varToPitchParams(const juce::var& v);

    // --- Guitar processor helpers ---
    juce::var overdriveParamsToVar(const OverdriveProcessor::Params& p);
    OverdriveProcessor::Params varToOverdriveParams(const juce::var& v);

    juce::var distortionParamsToVar(const DistortionProcessor::Params& p);
    DistortionProcessor::Params varToDistortionParams(const juce::var& v);

    juce::var fuzzParamsToVar(const FuzzProcessor::Params& p);
    FuzzProcessor::Params varToFuzzParams(const juce::var& v);

    juce::var guitarChorusParamsToVar(const GuitarChorusProcessor::Params& p);
    GuitarChorusProcessor::Params varToGuitarChorusParams(const juce::var& v);

    juce::var guitarFlangerParamsToVar(const GuitarFlangerProcessor::Params& p);
    GuitarFlangerProcessor::Params varToGuitarFlangerParams(const juce::var& v);

    juce::var guitarPhaserParamsToVar(const GuitarPhaserProcessor::Params& p);
    GuitarPhaserProcessor::Params varToGuitarPhaserParams(const juce::var& v);

    juce::var guitarTremoloParamsToVar(const GuitarTremoloProcessor::Params& p);
    GuitarTremoloProcessor::Params varToGuitarTremoloParams(const juce::var& v);

    juce::var guitarVibratoParamsToVar(const GuitarVibratoProcessor::Params& p);
    GuitarVibratoProcessor::Params varToGuitarVibratoParams(const juce::var& v);

    juce::var guitarToneParamsToVar(const GuitarToneProcessor::Params& p);
    GuitarToneProcessor::Params varToGuitarToneParams(const juce::var& v);

    juce::var guitarRotaryParamsToVar(const GuitarRotaryProcessor::Params& p);
    GuitarRotaryProcessor::Params varToGuitarRotaryParams(const juce::var& v);

    juce::var guitarWahParamsToVar(const GuitarWahProcessor::Params& p);
    GuitarWahProcessor::Params varToGuitarWahParams(const juce::var& v);

    juce::var guitarReverbParamsToVar(const GuitarReverbProcessor::Params& p);
    GuitarReverbProcessor::Params varToGuitarReverbParams(const juce::var& v);

    juce::var guitarNoiseGateParamsToVar(const GuitarNoiseGateProcessor::Params& p);
    GuitarNoiseGateProcessor::Params varToGuitarNoiseGateParams(const juce::var& v);

    juce::var toneStackParamsToVar(const ToneStackProcessor::Params& p);
    ToneStackProcessor::Params varToToneStackParams(const juce::var& v);

    juce::var cabSimParamsToVar(const CabSimProcessor::Params& p);
    CabSimProcessor::Params varToCabSimParams(const juce::var& v);

    juce::var masterParamsToVar(const MasterProcessor::Params& p);
    MasterProcessor::Params varToMasterParams(const juce::var& v);

    juce::var doublerParamsToVar(const DoublerProcessor::Params& p);
    DoublerProcessor::Params varToDoublerParams(const juce::var& v);

    juce::var studioReverbParamsToVar(const StudioReverbProcessor::Params& p, int modelIndex);
    StudioReverbProcessor::Params varToStudioReverbParams(const juce::var& v, int& modelIndex);

private:
    AudioEngine& audioEngine;
    juce::String currentPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
