#include "PresetManager.h"
#include <juce_core/juce_core.h>

PresetManager::PresetManager(AudioEngine& engine)
    : audioEngine(engine), currentPresetName("Default")
{
    loadDefaultPreset();
}

void PresetManager::loadDefaultPreset()
{
    currentPresetName = "Default";
}

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName;
}

bool PresetManager::savePreset(const juce::File& file)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("presetName", file.getFileNameWithoutExtension());
    root->setProperty("version", "2.0");

    juce::var presetData(root.get());
    juce::String jsonStr = juce::JSON::toString(presetData, true);
    if (file.replaceWithText(jsonStr))
    {
        currentPresetName = file.getFileNameWithoutExtension();
        return true;
    }
    return false;
}

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

    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

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
    
    // Type
    obj->setProperty("type", static_cast<int>(p.type));
    
    // Common parameters
    obj->setProperty("mix", p.mix);
    obj->setProperty("preDelay", p.preDelay);
    obj->setProperty("decay", p.decay);
    obj->setProperty("lowCut", p.lowCut);
    obj->setProperty("highCut", p.highCut);
    obj->setProperty("duck", p.duck);
    
    // Hall-specific
    obj->setProperty("hallDiffusion", p.hallDiffusion);
    obj->setProperty("hallModulation", p.hallModulation);
    obj->setProperty("hallWidth", p.hallWidth);
    
    // Plate-specific
    obj->setProperty("plateDamping", p.plateDamping);
    obj->setProperty("plateBrightness", p.plateBrightness);
    obj->setProperty("plateDensity", p.plateDensity);
    
    // Ambiance-specific
    obj->setProperty("ambSize", p.ambSize);
    obj->setProperty("ambEarlyLate", p.ambEarlyLate);
    obj->setProperty("ambLiveliness", p.ambLiveliness);
    
    // IR-specific
    obj->setProperty("irFilePath", p.irFilePath);
    obj->setProperty("gateThreshold", p.gateThreshold);
    obj->setProperty("gateSpeed", p.gateSpeed);
    
    return obj.get();
}

ReverbProcessor::Params PresetManager::varToReverbParams(const juce::var& v)
{
    ReverbProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        // Type
        if (obj->hasProperty("type"))
            p.type = static_cast<ReverbProcessor::Type>((int)obj->getProperty("type"));
        
        // Common parameters
        if (obj->hasProperty("mix"))
            p.mix = (float)obj->getProperty("mix");
        if (obj->hasProperty("preDelay"))
            p.preDelay = (float)obj->getProperty("preDelay");
        if (obj->hasProperty("decay"))
            p.decay = (float)obj->getProperty("decay");
        if (obj->hasProperty("lowCut"))
            p.lowCut = (float)obj->getProperty("lowCut");
        if (obj->hasProperty("highCut"))
            p.highCut = (float)obj->getProperty("highCut");
        if (obj->hasProperty("duck"))
            p.duck = (float)obj->getProperty("duck");
        
        // Hall-specific
        if (obj->hasProperty("hallDiffusion"))
            p.hallDiffusion = (float)obj->getProperty("hallDiffusion");
        if (obj->hasProperty("hallModulation"))
            p.hallModulation = (float)obj->getProperty("hallModulation");
        if (obj->hasProperty("hallWidth"))
            p.hallWidth = (float)obj->getProperty("hallWidth");
        
        // Plate-specific
        if (obj->hasProperty("plateDamping"))
            p.plateDamping = (float)obj->getProperty("plateDamping");
        if (obj->hasProperty("plateBrightness"))
            p.plateBrightness = (float)obj->getProperty("plateBrightness");
        if (obj->hasProperty("plateDensity"))
            p.plateDensity = (float)obj->getProperty("plateDensity");
        
        // Ambiance-specific
        if (obj->hasProperty("ambSize"))
            p.ambSize = (float)obj->getProperty("ambSize");
        if (obj->hasProperty("ambEarlyLate"))
            p.ambEarlyLate = (float)obj->getProperty("ambEarlyLate");
        if (obj->hasProperty("ambLiveliness"))
            p.ambLiveliness = (float)obj->getProperty("ambLiveliness");
        
        // IR-specific
        if (obj->hasProperty("irFilePath"))
            p.irFilePath = obj->getProperty("irFilePath").toString();
        if (obj->hasProperty("gateThreshold"))
            p.gateThreshold = (float)obj->getProperty("gateThreshold");
        if (obj->hasProperty("gateSpeed"))
            p.gateSpeed = (float)obj->getProperty("gateSpeed");
    }
    return p;
}

juce::var PresetManager::delayParamsToVar(const DelayProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("type", (int)p.type);
    for (int i = 0; i < DelayProcessor::MAX_PARAMS; ++i)
        obj->setProperty("p" + juce::String(i), p.p[i]);
    return obj.get();
}

DelayProcessor::Params PresetManager::varToDelayParams(const juce::var& v)
{
    DelayProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        p.type = static_cast<DelayProcessor::Type>((int)obj->getProperty("type"));
        for (int i = 0; i < DelayProcessor::MAX_PARAMS; ++i)
        {
            auto key = "p" + juce::String(i);
            if (obj->hasProperty(key))
                p.p[i] = (float)obj->getProperty(key);
            else
                p.p[i] = DelayProcessor::getDefaultValue(p.type, i);
        }
    }
    return p;
}

juce::var PresetManager::harmonizerParamsToVar(const HarmonizerProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("enabled", p.enabled);
    obj->setProperty("wet", p.wetDb);
    obj->setProperty("glide", p.glideMs);
    
    for (int i = 0; i < 4; ++i)
    {
        juce::DynamicObject::Ptr voice = new juce::DynamicObject();
        voice->setProperty("on", p.voices[i].enabled);
        voice->setProperty("semitones", p.voices[i].semitones);
        voice->setProperty("pan", p.voices[i].pan);
        voice->setProperty("gain", p.voices[i].gainDb);
        voice->setProperty("delay", p.voices[i].delayMs);
        voice->setProperty("formant", p.voices[i].formant);
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
        
        for (int i = 0; i < 4; ++i)
        {
            juce::String voiceName = "v" + juce::String(i + 1);
            if (auto* voice = obj->getProperty(voiceName).getDynamicObject())
            {
                p.voices[i].enabled = (bool)voice->getProperty("on");
                
                if (voice->hasProperty("semitones"))
                    p.voices[i].semitones = (float)voice->getProperty("semitones");
                else if (voice->hasProperty("pitch"))
                    p.voices[i].semitones = (float)voice->getProperty("pitch");
                
                p.voices[i].pan = voice->hasProperty("pan") ? (float)voice->getProperty("pan") : 0.0f;
                p.voices[i].gainDb = (float)voice->getProperty("gain");
                p.voices[i].delayMs = voice->hasProperty("delay") ? (float)voice->getProperty("delay") : 0.0f;
                p.voices[i].formant = voice->hasProperty("formant") ? (float)voice->getProperty("formant") : 0.0f;
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

juce::var PresetManager::pitchParamsToVar(const PitchProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("sensitivity", p.sensitivity);
    obj->setProperty("referencePitch", p.referencePitch);
    obj->setProperty("gateThreshold", p.gateThreshold);
    return obj.get();
}

PitchProcessor::Params PresetManager::varToPitchParams(const juce::var& v)
{
    PitchProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        if (obj->hasProperty("sensitivity"))
            p.sensitivity = (float)obj->getProperty("sensitivity");
        if (obj->hasProperty("referencePitch"))
            p.referencePitch = (float)obj->getProperty("referencePitch");
        if (obj->hasProperty("gateThreshold"))
            p.gateThreshold = (float)obj->getProperty("gateThreshold");
    }
    return p;
}

juce::var PresetManager::masterParamsToVar(const MasterProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("sidepass", p.sidepass);
    obj->setProperty("glue",     p.glue);
    obj->setProperty("scope",    p.scope);
    obj->setProperty("skronk",   p.skronk);
    obj->setProperty("girth",    p.girth);
    obj->setProperty("drive",    p.drive);
    return obj.get();
}

MasterProcessor::Params PresetManager::varToMasterParams(const juce::var& v)
{
    MasterProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        if (obj->hasProperty("sidepass")) p.sidepass = (float)obj->getProperty("sidepass");
        if (obj->hasProperty("glue"))     p.glue     = (float)obj->getProperty("glue");
        if (obj->hasProperty("scope"))    p.scope    = (float)obj->getProperty("scope");
        if (obj->hasProperty("skronk"))   p.skronk   = (float)obj->getProperty("skronk");
        if (obj->hasProperty("girth"))    p.girth    = (float)obj->getProperty("girth");
        if (obj->hasProperty("drive"))    p.drive    = (float)obj->getProperty("drive");
    }
    return p;
}
juce::var PresetManager::doublerParamsToVar(const DoublerProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("headroom", p.headroom);
    obj->setProperty("delayA",   p.delayA);
    obj->setProperty("levelA",   p.levelA);
    obj->setProperty("delayB",   p.delayB);
    obj->setProperty("levelB",   p.levelB);
    obj->setProperty("output",   p.output);
    return obj.get();
}

DoublerProcessor::Params PresetManager::varToDoublerParams(const juce::var& v)
{
    DoublerProcessor::Params p;
    if (auto* obj = v.getDynamicObject())
    {
        if (obj->hasProperty("headroom")) p.headroom = (float)obj->getProperty("headroom");
        if (obj->hasProperty("delayA"))   p.delayA   = (float)obj->getProperty("delayA");
        if (obj->hasProperty("levelA"))   p.levelA   = (float)obj->getProperty("levelA");
        if (obj->hasProperty("delayB"))   p.delayB   = (float)obj->getProperty("delayB");
        if (obj->hasProperty("levelB"))   p.levelB   = (float)obj->getProperty("levelB");
        if (obj->hasProperty("output"))   p.output   = (float)obj->getProperty("output");
    }
    return p;
}

juce::var PresetManager::studioReverbParamsToVar(const StudioReverbProcessor::Params& p, int modelIndex)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("model", modelIndex);

    // Shared dry/wet
    obj->setProperty("dry", p.dry);
    obj->setProperty("wet", p.wet);

    // Room
    obj->setProperty("roomSize",    p.roomSize);
    obj->setProperty("roomSustain", p.roomSustain);
    obj->setProperty("roomMulch",   p.roomMulch);

    // Chamber
    obj->setProperty("chamberDelay", p.chamberDelay);
    obj->setProperty("chamberRegen", p.chamberRegen);
    obj->setProperty("chamberThick", p.chamberThick);

    // Space
    obj->setProperty("spaceReplace",    p.spaceReplace);
    obj->setProperty("spaceBrightness", p.spaceBrightness);
    obj->setProperty("spaceDetune",     p.spaceDetune);
    obj->setProperty("spaceDerez",      p.spaceDerez);
    obj->setProperty("spaceBigness",    p.spaceBigness);

    // Plate
    obj->setProperty("plateInputPad", p.plateInputPad);
    obj->setProperty("plateDamping",  p.plateDamping);
    obj->setProperty("plateLowCut",   p.plateLowCut);
    obj->setProperty("platePredelay", p.platePredelay);

    return obj.get();
}

StudioReverbProcessor::Params PresetManager::varToStudioReverbParams(const juce::var& v, int& modelIndex)
{
    StudioReverbProcessor::Params p;
    modelIndex = 0;
    if (auto* obj = v.getDynamicObject())
    {
        if (obj->hasProperty("model")) modelIndex = (int)obj->getProperty("model");

        // Shared dry/wet
        if (obj->hasProperty("dry")) p.dry = (float)obj->getProperty("dry");
        if (obj->hasProperty("wet")) p.wet = (float)obj->getProperty("wet");

        // Room
        if (obj->hasProperty("roomSize"))    p.roomSize    = (float)obj->getProperty("roomSize");
        if (obj->hasProperty("roomSustain")) p.roomSustain = (float)obj->getProperty("roomSustain");
        if (obj->hasProperty("roomMulch"))   p.roomMulch   = (float)obj->getProperty("roomMulch");

        // Chamber
        if (obj->hasProperty("chamberDelay")) p.chamberDelay = (float)obj->getProperty("chamberDelay");
        if (obj->hasProperty("chamberRegen")) p.chamberRegen = (float)obj->getProperty("chamberRegen");
        if (obj->hasProperty("chamberThick")) p.chamberThick = (float)obj->getProperty("chamberThick");

        // Space
        if (obj->hasProperty("spaceReplace"))    p.spaceReplace    = (float)obj->getProperty("spaceReplace");
        if (obj->hasProperty("spaceBrightness")) p.spaceBrightness = (float)obj->getProperty("spaceBrightness");
        if (obj->hasProperty("spaceDetune"))     p.spaceDetune     = (float)obj->getProperty("spaceDetune");
        if (obj->hasProperty("spaceDerez"))      p.spaceDerez      = (float)obj->getProperty("spaceDerez");
        if (obj->hasProperty("spaceBigness"))    p.spaceBigness    = (float)obj->getProperty("spaceBigness");

        // Plate
        if (obj->hasProperty("plateInputPad")) p.plateInputPad = (float)obj->getProperty("plateInputPad");
        if (obj->hasProperty("plateDamping"))  p.plateDamping  = (float)obj->getProperty("plateDamping");
        if (obj->hasProperty("plateLowCut"))   p.plateLowCut   = (float)obj->getProperty("plateLowCut");
        if (obj->hasProperty("platePredelay")) p.platePredelay = (float)obj->getProperty("platePredelay");

        // Legacy: migrate old per-model wet to shared wet
        if (!obj->hasProperty("wet"))
        {
            if (obj->hasProperty("roomWet"))    p.wet = (float)obj->getProperty("roomWet");
            else if (obj->hasProperty("chamberWet")) p.wet = (float)obj->getProperty("chamberWet");
            else if (obj->hasProperty("spaceWet"))   p.wet = (float)obj->getProperty("spaceWet");
            else if (obj->hasProperty("plateWet"))   p.wet = (float)obj->getProperty("plateWet");
        }
    }
    return p;
}

// ==============================================================================
//  Guitar effect serializers
// ==============================================================================

juce::var PresetManager::overdriveParamsToVar(const OverdriveProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("drive", p.drive); obj->setProperty("tone", p.tone);
    obj->setProperty("level", p.level); obj->setProperty("mix", p.mix);
    return obj.get();
}
OverdriveProcessor::Params PresetManager::varToOverdriveParams(const juce::var& v)
{
    OverdriveProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("drive")) p.drive = (float)obj->getProperty("drive");
        if (obj->hasProperty("tone"))  p.tone  = (float)obj->getProperty("tone");
        if (obj->hasProperty("level")) p.level = (float)obj->getProperty("level");
        if (obj->hasProperty("mix"))   p.mix   = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::distortionParamsToVar(const DistortionProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("drive", p.drive); obj->setProperty("tone", p.tone);
    obj->setProperty("level", p.level); obj->setProperty("mix", p.mix);
    return obj.get();
}
DistortionProcessor::Params PresetManager::varToDistortionParams(const juce::var& v)
{
    DistortionProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("drive")) p.drive = (float)obj->getProperty("drive");
        if (obj->hasProperty("tone"))  p.tone  = (float)obj->getProperty("tone");
        if (obj->hasProperty("level")) p.level = (float)obj->getProperty("level");
        if (obj->hasProperty("mix"))   p.mix   = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::fuzzParamsToVar(const FuzzProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("fuzz", p.fuzz); obj->setProperty("tone", p.tone);
    obj->setProperty("sustain", p.sustain); obj->setProperty("level", p.level);
    return obj.get();
}
FuzzProcessor::Params PresetManager::varToFuzzParams(const juce::var& v)
{
    FuzzProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("fuzz"))    p.fuzz    = (float)obj->getProperty("fuzz");
        if (obj->hasProperty("tone"))    p.tone    = (float)obj->getProperty("tone");
        if (obj->hasProperty("sustain")) p.sustain = (float)obj->getProperty("sustain");
        if (obj->hasProperty("level"))   p.level   = (float)obj->getProperty("level");
    }
    return p;
}

juce::var PresetManager::guitarChorusParamsToVar(const GuitarChorusProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("rate", p.rate); obj->setProperty("depth", p.depth);
    obj->setProperty("mix", p.mix); obj->setProperty("width", p.width);
    return obj.get();
}
GuitarChorusProcessor::Params PresetManager::varToGuitarChorusParams(const juce::var& v)
{
    GuitarChorusProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("rate"))  p.rate  = (float)obj->getProperty("rate");
        if (obj->hasProperty("depth")) p.depth = (float)obj->getProperty("depth");
        if (obj->hasProperty("mix"))   p.mix   = (float)obj->getProperty("mix");
        if (obj->hasProperty("width")) p.width = (float)obj->getProperty("width");
    }
    return p;
}

juce::var PresetManager::guitarFlangerParamsToVar(const GuitarFlangerProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("rate", p.rate); obj->setProperty("depth", p.depth);
    obj->setProperty("feedback", p.feedback); obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarFlangerProcessor::Params PresetManager::varToGuitarFlangerParams(const juce::var& v)
{
    GuitarFlangerProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("rate"))     p.rate     = (float)obj->getProperty("rate");
        if (obj->hasProperty("depth"))    p.depth    = (float)obj->getProperty("depth");
        if (obj->hasProperty("feedback")) p.feedback = (float)obj->getProperty("feedback");
        if (obj->hasProperty("mix"))      p.mix      = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::guitarPhaserParamsToVar(const GuitarPhaserProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("center", p.center); obj->setProperty("rate", p.rate);
    obj->setProperty("depth", p.depth); obj->setProperty("feedback", p.feedback);
    obj->setProperty("stages", p.stages); obj->setProperty("spread", p.spread);
    obj->setProperty("sharpness", p.sharpness); obj->setProperty("stereo", p.stereo);
    obj->setProperty("waveform", p.waveform); obj->setProperty("tone", p.tone);
    obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarPhaserProcessor::Params PresetManager::varToGuitarPhaserParams(const juce::var& v)
{
    GuitarPhaserProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("center"))    p.center    = (float)obj->getProperty("center");
        if (obj->hasProperty("rate"))      p.rate      = (float)obj->getProperty("rate");
        if (obj->hasProperty("depth"))     p.depth     = (float)obj->getProperty("depth");
        if (obj->hasProperty("feedback"))  p.feedback  = (float)obj->getProperty("feedback");
        if (obj->hasProperty("stages"))    p.stages    = (int)obj->getProperty("stages");
        if (obj->hasProperty("spread"))    p.spread    = (float)obj->getProperty("spread");
        if (obj->hasProperty("sharpness")) p.sharpness = (float)obj->getProperty("sharpness");
        if (obj->hasProperty("stereo"))    p.stereo    = (float)obj->getProperty("stereo");
        if (obj->hasProperty("waveform"))  p.waveform  = (int)obj->getProperty("waveform");
        if (obj->hasProperty("tone"))      p.tone      = (float)obj->getProperty("tone");
        if (obj->hasProperty("mix"))       p.mix       = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::guitarTremoloParamsToVar(const GuitarTremoloProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("rate", p.rate); obj->setProperty("depth", p.depth);
    obj->setProperty("wave", p.wave); obj->setProperty("stereo", p.stereo);
    obj->setProperty("bias", p.bias); obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarTremoloProcessor::Params PresetManager::varToGuitarTremoloParams(const juce::var& v)
{
    GuitarTremoloProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("rate"))   p.rate   = (float)obj->getProperty("rate");
        if (obj->hasProperty("depth"))  p.depth  = (float)obj->getProperty("depth");
        if (obj->hasProperty("wave"))   p.wave   = (int)obj->getProperty("wave");
        if (obj->hasProperty("stereo")) p.stereo = (float)obj->getProperty("stereo");
        if (obj->hasProperty("bias"))   p.bias   = (float)obj->getProperty("bias");
        if (obj->hasProperty("mix"))    p.mix    = (float)obj->getProperty("mix");
        // Legacy: old presets had "shape" instead of "wave"
        if (!obj->hasProperty("wave") && obj->hasProperty("shape"))
            p.wave = (int)obj->getProperty("shape");
    }
    return p;
}

juce::var PresetManager::guitarVibratoParamsToVar(const GuitarVibratoProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("rate", p.rate); obj->setProperty("depth", p.depth);
    obj->setProperty("wave", p.wave); obj->setProperty("stereo", p.stereo);
    obj->setProperty("delay", p.delay); obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarVibratoProcessor::Params PresetManager::varToGuitarVibratoParams(const juce::var& v)
{
    GuitarVibratoProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("rate"))   p.rate   = (float)obj->getProperty("rate");
        if (obj->hasProperty("depth"))  p.depth  = (float)obj->getProperty("depth");
        if (obj->hasProperty("wave"))   p.wave   = (int)obj->getProperty("wave");
        if (obj->hasProperty("stereo")) p.stereo = (float)obj->getProperty("stereo");
        if (obj->hasProperty("delay"))  p.delay  = (float)obj->getProperty("delay");
        if (obj->hasProperty("mix"))    p.mix    = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::guitarToneParamsToVar(const GuitarToneProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("bass", p.bass); obj->setProperty("mid", p.mid);
    obj->setProperty("treble", p.treble); obj->setProperty("midFreq", p.midFreq);
    obj->setProperty("presence", p.presence); obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarToneProcessor::Params PresetManager::varToGuitarToneParams(const juce::var& v)
{
    GuitarToneProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("bass"))     p.bass     = (float)obj->getProperty("bass");
        if (obj->hasProperty("mid"))      p.mid      = (float)obj->getProperty("mid");
        if (obj->hasProperty("treble"))   p.treble   = (float)obj->getProperty("treble");
        if (obj->hasProperty("midFreq"))  p.midFreq  = (float)obj->getProperty("midFreq");
        if (obj->hasProperty("presence")) p.presence = (float)obj->getProperty("presence");
        if (obj->hasProperty("mix"))      p.mix      = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::guitarRotaryParamsToVar(const GuitarRotaryProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("hornRate", p.hornRate); obj->setProperty("doppler", p.doppler);
    obj->setProperty("tremolo", p.tremolo); obj->setProperty("rotorRate", p.rotorRate);
    obj->setProperty("drive", p.drive); obj->setProperty("waveshape", p.waveshape);
    obj->setProperty("width", p.width); obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarRotaryProcessor::Params PresetManager::varToGuitarRotaryParams(const juce::var& v)
{
    GuitarRotaryProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("hornRate"))  p.hornRate  = (float)obj->getProperty("hornRate");
        if (obj->hasProperty("doppler"))   p.doppler   = (float)obj->getProperty("doppler");
        if (obj->hasProperty("tremolo"))   p.tremolo   = (float)obj->getProperty("tremolo");
        if (obj->hasProperty("rotorRate")) p.rotorRate = (float)obj->getProperty("rotorRate");
        if (obj->hasProperty("drive"))     p.drive     = (float)obj->getProperty("drive");
        if (obj->hasProperty("waveshape")) p.waveshape = (int)obj->getProperty("waveshape");
        if (obj->hasProperty("width"))     p.width     = (float)obj->getProperty("width");
        if (obj->hasProperty("mix"))       p.mix       = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::guitarWahParamsToVar(const GuitarWahProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("pedal", p.pedal); obj->setProperty("mode", p.mode);
    obj->setProperty("model", p.model); obj->setProperty("q", p.q);
    obj->setProperty("sensitivity", p.sens); obj->setProperty("attack", p.attack);
    obj->setProperty("lfoRate", p.lfoRate); obj->setProperty("mix", p.mix);
    return obj.get();
}
GuitarWahProcessor::Params PresetManager::varToGuitarWahParams(const juce::var& v)
{
    GuitarWahProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("pedal"))       p.pedal       = (float)obj->getProperty("pedal");
        if (obj->hasProperty("mode"))        p.mode        = (int)obj->getProperty("mode");
        if (obj->hasProperty("model"))       p.model       = (int)obj->getProperty("model");
        if (obj->hasProperty("q"))           p.q           = (float)obj->getProperty("q");
        if (obj->hasProperty("sensitivity")) p.sens        = (float)obj->getProperty("sensitivity");
        if (obj->hasProperty("attack"))      p.attack      = (float)obj->getProperty("attack");
        if (obj->hasProperty("lfoRate"))     p.lfoRate     = (float)obj->getProperty("lfoRate");
        if (obj->hasProperty("mix"))         p.mix         = (float)obj->getProperty("mix");
    }
    return p;
}

juce::var PresetManager::guitarReverbParamsToVar(const GuitarReverbProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("size", p.size); obj->setProperty("damping", p.damping);
    obj->setProperty("mix", p.mix); obj->setProperty("width", p.width);
    return obj.get();
}
GuitarReverbProcessor::Params PresetManager::varToGuitarReverbParams(const juce::var& v)
{
    GuitarReverbProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("size"))    p.size    = (float)obj->getProperty("size");
        if (obj->hasProperty("damping")) p.damping = (float)obj->getProperty("damping");
        if (obj->hasProperty("mix"))     p.mix     = (float)obj->getProperty("mix");
        if (obj->hasProperty("width"))   p.width   = (float)obj->getProperty("width");
    }
    return p;
}

juce::var PresetManager::guitarNoiseGateParamsToVar(const GuitarNoiseGateProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("threshold", p.thresholdDb); obj->setProperty("attack", p.attackMs);
    obj->setProperty("hold", p.holdMs); obj->setProperty("release", p.releaseMs);
    return obj.get();
}
GuitarNoiseGateProcessor::Params PresetManager::varToGuitarNoiseGateParams(const juce::var& v)
{
    GuitarNoiseGateProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("threshold")) p.thresholdDb = (float)obj->getProperty("threshold");
        if (obj->hasProperty("attack"))    p.attackMs    = (float)obj->getProperty("attack");
        if (obj->hasProperty("hold"))      p.holdMs      = (float)obj->getProperty("hold");
        if (obj->hasProperty("release"))   p.releaseMs   = (float)obj->getProperty("release");
    }
    return p;
}

juce::var PresetManager::toneStackParamsToVar(const ToneStackProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("model", p.model); obj->setProperty("bass", p.bass);
    obj->setProperty("mid", p.mid); obj->setProperty("treble", p.treble);
    obj->setProperty("gain", p.gain);
    return obj.get();
}
ToneStackProcessor::Params PresetManager::varToToneStackParams(const juce::var& v)
{
    ToneStackProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("model"))  p.model  = (int)obj->getProperty("model");
        if (obj->hasProperty("bass"))   p.bass   = (float)obj->getProperty("bass");
        if (obj->hasProperty("mid"))    p.mid    = (float)obj->getProperty("mid");
        if (obj->hasProperty("treble")) p.treble = (float)obj->getProperty("treble");
        if (obj->hasProperty("gain"))   p.gain   = (float)obj->getProperty("gain");
    }
    return p;
}

juce::var PresetManager::cabSimParamsToVar(const CabSimProcessor::Params& p)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("cabinet", p.cabinet); obj->setProperty("mic", p.mic);
    obj->setProperty("micPos", p.micPos); obj->setProperty("level", p.level);
    return obj.get();
}
CabSimProcessor::Params PresetManager::varToCabSimParams(const juce::var& v)
{
    CabSimProcessor::Params p;
    if (auto* obj = v.getDynamicObject()) {
        if (obj->hasProperty("cabinet")) p.cabinet = (int)obj->getProperty("cabinet");
        if (obj->hasProperty("mic"))     p.mic     = (int)obj->getProperty("mic");
        if (obj->hasProperty("micPos"))  p.micPos  = (float)obj->getProperty("micPos");
        if (obj->hasProperty("level"))   p.level   = (float)obj->getProperty("level");
    }
    return p;
}