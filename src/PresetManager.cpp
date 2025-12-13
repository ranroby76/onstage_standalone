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
    audioEngine.setMicMute(0, false);       // Updated: Mute is false
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
    exParams1.amount = 6.96f;  // Updated
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
    eq2.setLowFrequency(648.96f); // Updated
    eq2.setMidFrequency(1000.0f);
    eq2.setHighFrequency(2731.96f); // Updated
    
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
    exParams2.frequency = 2350.0f; // Updated
    exParams2.amount = 1.92f;      // Updated
    exParams2.mix = 0.11f;         // Updated
    exc2.setParams(exParams2);
    exc2.setBypassed(false);

    // ==============================================================================
    // Global Effects
    // ==============================================================================

    // --- Harmonizer ---
    auto& harmonizer = audioEngine.getHarmonizerProcessor();
    HarmonizerProcessor::Params harmParams;
    harmParams.enabled = true;
    harmParams.wetDb = -3.12f;
    harmParams.glideMs = 50.0f; 
    
    harmParams.voices[0].enabled = true;
    harmParams.voices[0].fixedSemitones = 2.88f;
    harmParams.voices[0].gainDb = -6.0f;
    
    harmParams.voices[1].enabled = true;
    harmParams.voices[1].fixedSemitones = 7.20f;
    harmParams.voices[1].gainDb = -6.0f;
    
    harmonizer.setParams(harmParams);
    harmonizer.setBypassed(true); // Updated: Harmonizer bypass is TRUE in new JSON

    // --- Reverb ---
    auto& reverb = audioEngine.getReverbProcessor();
    ReverbProcessor::Params reverbParams;
    reverbParams.wetGain = 1.9f; // Updated
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

    // --- Dynamic EQ (Sidechain) ---
    auto& dynEQ = audioEngine.getDynamicEQProcessor();
    DynamicEQProcessor::Params dynEQParams;
    dynEQParams.duckBandHz = 1838.0f;
    dynEQParams.q = 7.33f;
    dynEQParams.shape = 0.5f;
    dynEQParams.threshold = -14.4f;
    dynEQParams.ratio = 2.52f;
    dynEQParams.attack = 6.09f;
    dynEQParams.release = 128.8f;
    dynEQ.setParams(dynEQParams);
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
    root->setProperty("dynamicEQ", dynEqParamsToVar(dynEq.getParams()));
    root->setProperty("dynEqBypass", dynEq.isBypassed());

    juce::String jsonString = juce::JSON::toString(juce::var(root), false);
    if (file.replaceWithText(jsonString))
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
    if (!file.existsAsFile()) return false;
    auto jsonVar = juce::JSON::parse(file);
    if (!jsonVar.isObject()) return false;

    auto* root = jsonVar.getDynamicObject();

    if (auto* mics = root->getProperty("mics").getArray())
    {
        for (int i = 0; i < juce::jmin(2, mics->size()); ++i)
        {
            if (auto* micObj = mics->getReference(i).getDynamicObject())
            {
                audioEngine.setMicPreampGain(i, (float)micObj->getProperty("preampGain"));
                audioEngine.setMicMute(i, (bool)micObj->getProperty("mute"));
                audioEngine.setFxBypass(i, (bool)micObj->getProperty("fxBypass"));

                if (auto* eqObj = micObj->getProperty("eq").getDynamicObject())
                {
                    auto& eq = audioEngine.getEQProcessor(i);
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
    dynEq.setParams(varToDynEqParams(root->getProperty("dynamicEQ")));
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
    
    juce::DynamicObject::Ptr v1 = new juce::DynamicObject();
    v1->setProperty("on", p.voices[0].enabled);
    v1->setProperty("pitch", p.voices[0].fixedSemitones);
    v1->setProperty("gain", p.voices[0].gainDb);
    obj->setProperty("v1", v1.get());

    juce::DynamicObject::Ptr v2 = new juce::DynamicObject();
    v2->setProperty("on", p.voices[1].enabled);
    v2->setProperty("pitch", p.voices[1].fixedSemitones);
    v2->setProperty("gain", p.voices[1].gainDb);
    obj->setProperty("v2", v2.get());

    return obj.get();
}

HarmonizerProcessor::Params PresetManager::varToHarmonizerParams(const juce::var& v)
{
    HarmonizerProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.enabled = (bool)obj->getProperty("enabled");
        p.wetDb = (float)obj->getProperty("wet");
        
        if (auto* v1 = obj->getProperty("v1").getDynamicObject())
        {
            p.voices[0].enabled = (bool)v1->getProperty("on");
            p.voices[0].fixedSemitones = (float)v1->getProperty("pitch");
            p.voices[0].gainDb = (float)v1->getProperty("gain");
        }
        if (auto* v2 = obj->getProperty("v2").getDynamicObject())
        {
            p.voices[1].enabled = (bool)v2->getProperty("on");
            p.voices[1].fixedSemitones = (float)v2->getProperty("pitch");
            p.voices[1].gainDb = (float)v2->getProperty("gain");
        }
    }
    return p;
}

juce::var PresetManager::dynEqParamsToVar(const DynamicEQProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("freq", p.duckBandHz);
    obj->setProperty("q", p.q);
    obj->setProperty("shape", p.shape);
    obj->setProperty("thresh", p.threshold);
    obj->setProperty("ratio", p.ratio);
    obj->setProperty("att", p.attack);
    obj->setProperty("rel", p.release);
    return obj.get();
}

DynamicEQProcessor::Params PresetManager::varToDynEqParams(const juce::var& v)
{
    DynamicEQProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.duckBandHz = (float)obj->getProperty("freq");
        p.q = (float)obj->getProperty("q");
        p.shape = (float)obj->getProperty("shape");
        p.threshold = (float)obj->getProperty("thresh");
        p.ratio = (float)obj->getProperty("ratio");
        p.attack = (float)obj->getProperty("att");
        p.release = (float)obj->getProperty("rel");
    }
    return p;
}