#include "PresetManager.h"
#include <juce_core/juce_core.h>

PresetManager::PresetManager(AudioEngine& engine)
    : audioEngine(engine), currentPresetName("Default")
{
    loadDefaultPreset();
}

void PresetManager::loadDefaultPreset()
{
    // ==============================================================================
    // Channel 1 settings (Based on UPDATED Preset "a12")
    // ==============================================================================
    audioEngine.setMicPreampGain(0, -1.9f);
    audioEngine.setMicMute(0, false);       
    audioEngine.setFxBypass(0, false);

    // --- EQ 1 ---
    auto& eq1 = audioEngine.getEQProcessor(0);
    eq1.setLowFrequency(638.05f);
    eq1.setMidFrequency(1000.0f);
    eq1.setHighFrequency(2713.07f);
    
    eq1.setLowGain(-4.92f);
    eq1.setMidGain(0.0f);
    eq1.setHighGain(0.0f);
    
    eq1.setLowQ(2.38f);
    eq1.setMidQ(6.49f);
    eq1.setHighQ(5.69f);
    
    eq1.setBypassed(false);
    
    // --- Compressor 1 ---
    auto& comp1 = audioEngine.getCompressorProcessor(0);
    CompressorProcessor::Params params1;
    params1.thresholdDb = -18.0f;
    params1.ratio = 2.33f;
    params1.attackMs = 0.1f;
    params1.releaseMs = 54.55f;
    params1.makeupDb = 3.96f;
    comp1.setParams(params1);
    comp1.setBypassed(false);

    // --- Exciter 1 ---
    auto& exc1 = audioEngine.getExciterProcessor(0);
    ExciterProcessor::Params exParams1;
    exParams1.frequency = 1990.0f;
    exParams1.amount = 6.96f;
    exParams1.mix = 0.58f;
    exc1.setParams(exParams1);
    exc1.setBypassed(false);
    
    // ==============================================================================
    // Channel 2 settings
    // ==============================================================================
    audioEngine.setMicPreampGain(1, 0.0f);
    audioEngine.setMicMute(1, false);
    audioEngine.setFxBypass(1, false);

    // --- EQ 2 ---
    auto& eq2 = audioEngine.getEQProcessor(1);
    eq2.setLowFrequency(648.96f); 
    eq2.setMidFrequency(1000.0f);
    eq2.setHighFrequency(2731.96f); 
    
    eq2.setLowGain(0.0f);
    eq2.setMidGain(0.0f);
    eq2.setHighGain(0.0f);
    
    eq2.setLowQ(0.707f);
    eq2.setMidQ(0.707f);
    eq2.setHighQ(0.707f);
    
    eq2.setBypassed(false);
    
    // --- Compressor 2 ---
    auto& comp2 = audioEngine.getCompressorProcessor(1);
    CompressorProcessor::Params params2;
    params2.thresholdDb = -18.0f;
    params2.ratio = 3.0f;
    params2.attackMs = 8.0f;
    params2.releaseMs = 120.0f;
    params2.makeupDb = 0.0f;
    comp2.setParams(params2);
    comp2.setBypassed(false);

    // --- Exciter 2 ---
    auto& exc2 = audioEngine.getExciterProcessor(1);
    ExciterProcessor::Params exParams2;
    exParams2.frequency = 2350.0f; 
    exParams2.amount = 1.92f;      
    exParams2.mix = 0.11f;         
    exc2.setParams(exParams2);
    exc2.setBypassed(false);

    // ==============================================================================
    // Global Effects
    // ==============================================================================

    // --- Harmonizer - 4 voices ---
    auto& harmonizer = audioEngine.getHarmonizerProcessor();
    HarmonizerProcessor::Params harmParams;
    harmParams.enabled = true;
    harmParams.wetDb = -3.12f;
    harmParams.glideMs = 50.0f;

    // Voice 1: Minor 3rd, slight left
    harmParams.voices[0].enabled = true;
    harmParams.voices[0].semitones = 3.0f;
    harmParams.voices[0].pan = -0.3f;
    harmParams.voices[0].gainDb = -6.0f;
    harmParams.voices[0].delayMs = 0.0f;

    // Voice 2: Perfect 5th, slight right
    harmParams.voices[1].enabled = true;
    harmParams.voices[1].semitones = 7.0f;
    harmParams.voices[1].pan = 0.3f;
    harmParams.voices[1].gainDb = -6.0f;
    harmParams.voices[1].delayMs = 0.0f;

    // Voice 3: Major 3rd down, more left (disabled by default)
    harmParams.voices[2].enabled = false;
    harmParams.voices[2].semitones = -4.0f;
    harmParams.voices[2].pan = -0.6f;
    harmParams.voices[2].gainDb = -9.0f;
    harmParams.voices[2].delayMs = 15.0f;

    // Voice 4: Octave up, center (disabled by default)
    harmParams.voices[3].enabled = false;
    harmParams.voices[3].semitones = 12.0f;
    harmParams.voices[3].pan = 0.0f;
    harmParams.voices[3].gainDb = -9.0f;
    harmParams.voices[3].delayMs = 0.0f;

    harmonizer.setParams(harmParams);
    harmonizer.setBypassed(true); 

    // --- Reverb ---
    auto& reverb = audioEngine.getReverbProcessor();
    ReverbProcessor::Params reverbParams;
    reverbParams.wetGain = 1.9f; 
    reverbParams.lowCutHz = 470.8f;
    reverbParams.highCutHz = 9360.0f;
    reverbParams.irFilePath = ""; 
    reverb.setParams(reverbParams);
    reverb.setBypassed(false);

    // --- Delay ---
    auto& delay = audioEngine.getDelayProcessor();
    DelayProcessor::Params delayParams;
    delayParams.delayMs = 350.0f;
    delayParams.ratio = 0.3f;   
    delayParams.stage = 0.25f; 
    delayParams.mix = 1.0f;
    delayParams.stereoWidth = 1.0f;
    delayParams.lowCutHz = 200.0f;
    delayParams.highCutHz = 8000.0f;
    delay.setParams(delayParams);
    delay.setBypassed(true);

    // --- Dynamic EQ (Sidechain) - Dual Band Support ---
    auto& dynEQ = audioEngine.getDynamicEQProcessor();
    
    // Band 1
    DynamicEQProcessor::BandParams dynEQParams1;
    dynEQParams1.duckBandHz = 1838.0f;
    dynEQParams1.q = 7.33f;
    dynEQParams1.shape = 0.5f;
    dynEQParams1.threshold = -14.4f;
    dynEQParams1.ratio = 2.52f;
    dynEQParams1.attack = 6.09f;
    dynEQParams1.release = 128.8f;
    dynEQ.setParams(0, dynEQParams1);

    // Band 2 (Defaults)
    DynamicEQProcessor::BandParams dynEQParams2;
    dynEQParams2.duckBandHz = 2500.0f;
    dynEQParams2.q = 4.0f;
    dynEQParams2.shape = 0.5f;
    dynEQParams2.threshold = -20.0f;
    dynEQParams2.ratio = 2.0f;
    dynEQParams2.attack = 10.0f;
    dynEQParams2.release = 150.0f;
    dynEQ.setParams(1, dynEQParams2);

    dynEQ.setBypassed(false);
    
    currentPresetName = "a12";
}

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName;
}

// ==============================================================================
// SAVE
// ==============================================================================
bool PresetManager::savePreset(const juce::File& file)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("presetName", file.getFileNameWithoutExtension());
    root->setProperty("version", "1.0");

    // --- Mics ---
    juce::Array<juce::var> mics;
    for (int i = 0; i < 2; ++i)
    {
        juce::DynamicObject::Ptr micObj = new juce::DynamicObject();
        micObj->setProperty("preampGain", audioEngine.getMicPreampGain(i));
        micObj->setProperty("mute", audioEngine.isMicMuted(i));
        micObj->setProperty("fxBypass", audioEngine.isFxBypassed(i));

        auto& eq = audioEngine.getEQProcessor(i);
        juce::DynamicObject::Ptr eqObj = new juce::DynamicObject();
        eqObj->setProperty("lowFreq", eq.getLowFrequency());
        eqObj->setProperty("midFreq", eq.getMidFrequency());
        eqObj->setProperty("highFreq", eq.getHighFrequency());
        eqObj->setProperty("lowGain", eq.getLowGain());
        eqObj->setProperty("midGain", eq.getMidGain());
        eqObj->setProperty("highGain", eq.getHighGain());
        eqObj->setProperty("lowQ", eq.getLowQ());
        eqObj->setProperty("midQ", eq.getMidQ());
        eqObj->setProperty("highQ", eq.getHighQ());
        micObj->setProperty("eq", eqObj.get());
        micObj->setProperty("eqBypass", eq.isBypassed());

        auto& comp = audioEngine.getCompressorProcessor(i);
        micObj->setProperty("compressor", compParamsToVar(comp.getParams()));
        micObj->setProperty("compBypass", comp.isBypassed());

        auto& exc = audioEngine.getExciterProcessor(i);
        micObj->setProperty("exciter", exciterParamsToVar(exc.getParams()));
        micObj->setProperty("excBypass", exc.isBypassed());

        mics.add(micObj.get());
    }
    root->setProperty("mics", mics);

    // --- Globals ---
    auto& harm = audioEngine.getHarmonizerProcessor();
    root->setProperty("harmonizer", harmonizerParamsToVar(harm.getParams()));
    root->setProperty("harmonizerBypass", harm.isBypassed());

    auto& verb = audioEngine.getReverbProcessor();
    root->setProperty("reverb", reverbParamsToVar(verb.getParams()));
    root->setProperty("reverbBypass", verb.isBypassed());

    auto& delay = audioEngine.getDelayProcessor();
    root->setProperty("delay", delayParamsToVar(delay.getParams()));
    root->setProperty("delayBypass", delay.isBypassed());

    auto& dynEq = audioEngine.getDynamicEQProcessor();
    root->setProperty("dynamicEQ", dynEqParamsToVar(dynEq));
    root->setProperty("dynEqBypass", dynEq.isBypassed());

    juce::var presetData(root.get());
    juce::String jsonStr = juce::JSON::toString(presetData, true);
    if (file.replaceWithText(jsonStr))
    {
        currentPresetName = file.getFileNameWithoutExtension();
        return true;
    }
    return false;
}

// ==============================================================================
// LOAD
// ==============================================================================
bool PresetManager::loadPreset(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    juce::String content = file.loadFileAsString();
    juce::var parsed = juce::JSON::parse(content);
    if (parsed.isVoid())
        return false;

    auto* root = parsed.getDynamicObject();
    if (!root)
        return false;

    // --- Mics ---
    if (auto* micsArray = root->getProperty("mics").getArray())
    {
        for (int i = 0; i < juce::jmin(2, micsArray->size()); ++i)
        {
            if (auto* micObj = micsArray->getReference(i).getDynamicObject())
            {
                audioEngine.setMicPreampGain(i, (float)micObj->getProperty("preampGain"));
                audioEngine.setMicMute(i, (bool)micObj->getProperty("mute"));
                audioEngine.setFxBypass(i, (bool)micObj->getProperty("fxBypass"));

                auto& eq = audioEngine.getEQProcessor(i);
                if (auto* eqObj = micObj->getProperty("eq").getDynamicObject())
                {
                    eq.setLowFrequency((float)eqObj->getProperty("lowFreq"));
                    eq.setMidFrequency((float)eqObj->getProperty("midFreq"));
                    eq.setHighFrequency((float)eqObj->getProperty("highFreq"));
                    eq.setLowGain((float)eqObj->getProperty("lowGain"));
                    eq.setMidGain((float)eqObj->getProperty("midGain"));
                    eq.setHighGain((float)eqObj->getProperty("highGain"));
                    eq.setLowQ((float)eqObj->getProperty("lowQ"));
                    eq.setMidQ((float)eqObj->getProperty("midQ"));
                    eq.setHighQ((float)eqObj->getProperty("highQ"));
                    eq.setBypassed((bool)micObj->getProperty("eqBypass"));
                }

                auto& comp = audioEngine.getCompressorProcessor(i);
                comp.setParams(varToCompParams(micObj->getProperty("compressor")));
                comp.setBypassed((bool)micObj->getProperty("compBypass"));

                auto& exc = audioEngine.getExciterProcessor(i);
                if (micObj->hasProperty("exciter"))
                {
                    exc.setParams(varToExciterParams(micObj->getProperty("exciter")));
                    exc.setBypassed((bool)micObj->getProperty("excBypass"));
                }
            }
        }
    }

    auto& harm = audioEngine.getHarmonizerProcessor();
    harm.setParams(varToHarmonizerParams(root->getProperty("harmonizer")));
    harm.setBypassed((bool)root->getProperty("harmonizerBypass"));

    auto& verb = audioEngine.getReverbProcessor();
    verb.setParams(varToReverbParams(root->getProperty("reverb")));
    verb.setBypassed((bool)root->getProperty("reverbBypass"));

    auto& delay = audioEngine.getDelayProcessor();
    delay.setParams(varToDelayParams(root->getProperty("delay")));
    delay.setBypassed((bool)root->getProperty("delayBypass"));

    auto& dynEq = audioEngine.getDynamicEQProcessor();
    varToDynEqParams(root->getProperty("dynamicEQ"), dynEq);
    dynEq.setBypassed((bool)root->getProperty("dynEqBypass"));

    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

// ==============================================================================
// HELPERS
// ==============================================================================

juce::var PresetManager::eqParamsToVar(const EQProcessor::Params& params) { return {}; }

juce::var PresetManager::compParamsToVar(const CompressorProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("thresh", p.thresholdDb);
    obj->setProperty("ratio", p.ratio);
    obj->setProperty("attack", p.attackMs);
    obj->setProperty("release", p.releaseMs);
    obj->setProperty("makeup", p.makeupDb);
    return obj.get();
}

CompressorProcessor::Params PresetManager::varToCompParams(const juce::var& v)
{
    CompressorProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.thresholdDb = (float)obj->getProperty("thresh");
        p.ratio = (float)obj->getProperty("ratio");
        p.attackMs = (float)obj->getProperty("attack");
        p.releaseMs = (float)obj->getProperty("release");
        p.makeupDb = (float)obj->getProperty("makeup");
    }
    return p;
}

juce::var PresetManager::exciterParamsToVar(const ExciterProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("freq", p.frequency);
    obj->setProperty("drive", p.amount);
    obj->setProperty("mix", p.mix);
    return obj.get();
}

ExciterProcessor::Params PresetManager::varToExciterParams(const juce::var& v)
{
    ExciterProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.frequency = (float)obj->getProperty("freq");
        p.amount = (float)obj->getProperty("drive");
        p.mix = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::reverbParamsToVar(const ReverbProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("wet", p.wetGain);
    obj->setProperty("loCut", p.lowCutHz);
    obj->setProperty("hiCut", p.highCutHz);
    obj->setProperty("irPath", p.irFilePath);
    return obj.get();
}

ReverbProcessor::Params PresetManager::varToReverbParams(const juce::var& v)
{
    ReverbProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.wetGain = (float)obj->getProperty("wet");
        p.lowCutHz = (float)obj->getProperty("loCut");
        p.highCutHz = (float)obj->getProperty("hiCut");
        p.irFilePath = obj->getProperty("irPath").toString();
    }
    return p;
}

juce::var PresetManager::delayParamsToVar(const DelayProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("time", p.delayMs);
    obj->setProperty("ratio", p.ratio);
    obj->setProperty("stage", p.stage);
    obj->setProperty("mix", p.mix);
    obj->setProperty("width", p.stereoWidth);
    obj->setProperty("loCut", p.lowCutHz);
    obj->setProperty("hiCut", p.highCutHz);
    return obj.get();
}

DelayProcessor::Params PresetManager::varToDelayParams(const juce::var& v)
{
    DelayProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.delayMs = (float)obj->getProperty("time");
        p.ratio = (float)obj->getProperty("ratio");
        p.stage = (float)obj->getProperty("stage");
        p.mix = (float)obj->getProperty("mix");
        p.stereoWidth = (float)obj->getProperty("width");
        p.lowCutHz = (float)obj->getProperty("loCut");
        p.highCutHz = (float)obj->getProperty("hiCut");
    }
    return p;
}

juce::var PresetManager::harmonizerParamsToVar(const HarmonizerProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("enabled", p.enabled);
    obj->setProperty("wet", p.wetDb);
    obj->setProperty("glide", p.glideMs);
    
    // Save all 4 voices
    for (int i = 0; i < 4; ++i)
    {
        juce::DynamicObject::Ptr voice = new juce::DynamicObject();
        voice->setProperty("on", p.voices[i].enabled);
        voice->setProperty("semitones", p.voices[i].semitones);
        voice->setProperty("pan", p.voices[i].pan);
        voice->setProperty("gain", p.voices[i].gainDb);
        voice->setProperty("delay", p.voices[i].delayMs);
        obj->setProperty("v" + juce::String(i + 1), voice.get());
    }
    
    return obj.get();
}

HarmonizerProcessor::Params PresetManager::varToHarmonizerParams(const juce::var& v)
{
    HarmonizerProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.enabled = (bool)obj->getProperty("enabled");
        p.wetDb = (float)obj->getProperty("wet");
        p.glideMs = obj->hasProperty("glide") ? (float)obj->getProperty("glide") : 50.0f;
        
        // Load all 4 voices
        for (int i = 0; i < 4; ++i)
        {
            juce::String voiceName = "v" + juce::String(i + 1);
            if (auto* voice = obj->getProperty(voiceName).getDynamicObject())
            {
                p.voices[i].enabled = (bool)voice->getProperty("on");
                
                // Backward compatibility: try "semitones" first, fall back to "pitch"
                if (voice->hasProperty("semitones"))
                    p.voices[i].semitones = (float)voice->getProperty("semitones");
                else if (voice->hasProperty("pitch"))
                    p.voices[i].semitones = (float)voice->getProperty("pitch");
                
                p.voices[i].pan = voice->hasProperty("pan") ? (float)voice->getProperty("pan") : 0.0f;
                p.voices[i].gainDb = (float)voice->getProperty("gain");
                p.voices[i].delayMs = voice->hasProperty("delay") ? (float)voice->getProperty("delay") : 0.0f;
            }
        }
    }
    return p;
}

juce::var PresetManager::dynEqParamsToVar(DynamicEQProcessor& dynEq)
{
    juce::Array<juce::var> bandArray;
    for (int i = 0; i < 2; ++i)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        auto p = dynEq.getParams(i);
        obj->setProperty("freq", p.duckBandHz);
        obj->setProperty("q", p.q);
        obj->setProperty("shape", p.shape);
        obj->setProperty("thresh", p.threshold);
        obj->setProperty("ratio", p.ratio);
        obj->setProperty("att", p.attack);
        obj->setProperty("rel", p.release);
        bandArray.add(obj.get());
    }
    return juce::var(bandArray);
}

void PresetManager::varToDynEqParams(const juce::var& v, DynamicEQProcessor& dynEq)
{
    if (auto* arr = v.getArray())
    {
        for (int i = 0; i < juce::jmin(2, arr->size()); ++i)
        {
            if (auto* obj = arr->getReference(i).getDynamicObject())
            {
                DynamicEQProcessor::BandParams p;
                p.duckBandHz = (float)obj->getProperty("freq");
                p.q = (float)obj->getProperty("q");
                p.shape = (float)obj->getProperty("shape");
                p.threshold = (float)obj->getProperty("thresh");
                p.ratio = (float)obj->getProperty("ratio");
                p.attack = (float)obj->getProperty("att");
                p.release = (float)obj->getProperty("rel");
                dynEq.setParams(i, p);
            }
        }
    }
}