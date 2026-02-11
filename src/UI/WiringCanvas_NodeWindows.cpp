// ==============================================================================
//  WiringCanvas_NodeWindows.cpp
//  OnStage — Editor window management
//
//  When the user clicks E on an effect node, we open a DocumentWindow
//  containing our full-size, touch-friendly panel for that effect type.
//  PreAmp has no popup — it uses an inline slider on the canvas instead.
//
//  Window sizes are remembered per effect type for the session and saved
//  in the project patch via OnStageGraph::editorWindowSizes.
// ==============================================================================

#include "WiringCanvas.h"

// Include all effect panel headers
#include "../UI/EQPanel.h"
#include "../UI/CompressorPanel.h"
#include "../UI/GatePanel.h"
#include "../UI/ExciterPanel.h"
#include "../UI/SculptPanel.h"
#include "../UI/ReverbPanel.h"
#include "../UI/StudioReverbPanel.h"
#include "../UI/DelayPanel.h"
#include "../UI/HarmonizerPanel.h"
#include "../UI/DynamicEQPanel.h"
#include "../UI/DeEsserPanel.h"
#include "../UI/SaturationPanel.h"
#include "../UI/DoublerPanel.h"
#include "../UI/MasterPanel.h"
#include "../UI/TunerPanel.h"

// Guitar panels
#include "../guitar/GuitarPanels.h"
#include "../guitar/CabIRPanel.h"

// ==============================================================================
//  Default window sizes per effect type
// ==============================================================================

static juce::Point<int> getDefaultWindowSize (const juce::String& effectType)
{
    if (effectType == "Tuner")
        return { 780, 400 };

    return { 1200, 600 };
}

// ==============================================================================
//  EffectEditorWindow — DocumentWindow with cleanup + size capture
// ==============================================================================

class EffectEditorWindow : public juce::DocumentWindow
{
public:
    EffectEditorWindow (const juce::String& name,
                        WiringCanvas* canvas,
                        juce::AudioProcessorGraph::NodeID nodeID,
                        const juce::String& effectType)
        : juce::DocumentWindow (name, juce::Colours::darkgrey,
              juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          graphCanvas (canvas),
          ownerNodeID (nodeID),
          type (effectType)
    {
    }

    ~EffectEditorWindow() override
    {
        saveWindowSize();
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        saveWindowSize();
        clearContentComponent();
        if (graphCanvas)
            graphCanvas->editorWindows.erase (ownerNodeID);
    }

    void resized() override
    {
        juce::DocumentWindow::resized();
        saveWindowSize();
    }

private:
    void saveWindowSize()
    {
        if (graphCanvas && type.isNotEmpty())
        {
            auto bounds = getBounds();
            graphCanvas->getStageGraph().editorWindowSizes[type] =
                { bounds.getWidth(), bounds.getHeight() };
        }
    }

    WiringCanvas* graphCanvas;
    juce::AudioProcessorGraph::NodeID ownerNodeID;
    juce::String type;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEditorWindow)
};

// ==============================================================================
//  Create the correct panel component for a given effect type
// ==============================================================================

static std::unique_ptr<juce::Component> createPanelForEffect (
    EffectProcessorNode* effectNode, PresetManager& presets)
{
    if (! effectNode) return nullptr;

    juce::String type = effectNode->getEffectType();

    if (type == "PreAmp")       return nullptr;

    if (type == "EQ")           { auto& p = static_cast<EQProcessorNode*>(effectNode)->getProcessor();           return std::make_unique<EQPanel> (p, presets); }
    if (type == "Compressor")   { auto& p = static_cast<CompressorProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<CompressorPanel> (p, presets); }
    if (type == "Gate")         { auto& p = static_cast<GateProcessorNode*>(effectNode)->getProcessor();         return std::make_unique<GatePanel> (p, presets); }
    if (type == "Exciter")      { auto& p = static_cast<ExciterProcessorNode*>(effectNode)->getProcessor();      return std::make_unique<ExciterPanel> (p, presets); }
    if (type == "Sculpt")       { auto& p = static_cast<SculptProcessorNode*>(effectNode)->getProcessor();       return std::make_unique<SculptPanel> (p, presets); }
    if (type == "Reverb")       { auto& p = static_cast<ReverbProcessorNode*>(effectNode)->getProcessor();       return std::make_unique<ReverbPanel> (p, presets); }
    if (type == "StudioReverb") { auto& p = static_cast<StudioReverbProcessorNode*>(effectNode)->getProcessor(); return std::make_unique<StudioReverbPanel> (p, presets); }
    if (type == "Delay")        { auto& p = static_cast<DelayProcessorNode*>(effectNode)->getProcessor();        return std::make_unique<DelayPanel> (p, presets); }
    if (type == "Harmonizer")   { auto& p = static_cast<HarmonizerProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<HarmonizerPanel> (p, presets); }
    if (type == "DynamicEQ")    { auto& p = static_cast<DynamicEQProcessorNode*>(effectNode)->getProcessor();    return std::make_unique<DynamicEQPanel> (p, presets); }
    if (type == "DeEsser")      { auto& p = static_cast<DeEsserProcessorNode*>(effectNode)->getProcessor();      return std::make_unique<DeEsserPanel> (p, presets); }
    if (type == "Saturation")   { auto& p = static_cast<SaturationProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<SaturationPanel> (p, presets); }
    if (type == "Doubler")      { auto& p = static_cast<DoublerProcessorNode*>(effectNode)->getProcessor();      return std::make_unique<DoublerPanel> (p, presets); }
    if (type == "Master")       { auto& p = static_cast<MasterProcessorNode*>(effectNode)->getProcessor();       return std::make_unique<MasterPanel> (p); }
    if (type == "Tuner")        { auto& p = static_cast<TunerProcessorNode*>(effectNode)->getProcessor();        return std::make_unique<TunerPanel> (p); }

    // --- Guitar panels ---
    if (type == "GuitarOverdrive")  { auto& p = dynamic_cast<OverdriveProcessorNode*>(effectNode)->getProcessor();      return std::make_unique<OverdrivePanel> (p, presets); }
    if (type == "GuitarDistortion") { auto& p = dynamic_cast<DistortionProcessorNode*>(effectNode)->getProcessor();     return std::make_unique<DistortionPanel> (p, presets); }
    if (type == "GuitarFuzz")       { auto& p = dynamic_cast<FuzzProcessorNode*>(effectNode)->getProcessor();           return std::make_unique<FuzzPanel> (p, presets); }
    if (type == "GuitarChorus")     { auto& p = dynamic_cast<GuitarChorusProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<GuitarChorusPanel> (p, presets); }
    if (type == "GuitarFlanger")    { auto& p = dynamic_cast<GuitarFlangerProcessorNode*>(effectNode)->getProcessor();  return std::make_unique<GuitarFlangerPanel> (p, presets); }
    if (type == "GuitarPhaser")     { auto& p = dynamic_cast<GuitarPhaserProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<GuitarPhaserPanel> (p, presets); }
    if (type == "GuitarTremolo")    { auto& p = dynamic_cast<GuitarTremoloProcessorNode*>(effectNode)->getProcessor();  return std::make_unique<GuitarTremoloPanel> (p, presets); }
    if (type == "GuitarVibrato")    { auto& p = dynamic_cast<GuitarVibratoProcessorNode*>(effectNode)->getProcessor();  return std::make_unique<GuitarVibratoPanel> (p, presets); }
    if (type == "GuitarTone")       { auto& p = dynamic_cast<GuitarToneProcessorNode*>(effectNode)->getProcessor();     return std::make_unique<GuitarTonePanel> (p, presets); }
    if (type == "GuitarRotary")     { auto& p = dynamic_cast<GuitarRotaryProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<GuitarRotaryPanel> (p, presets); }
    if (type == "GuitarWah")        { auto& p = dynamic_cast<GuitarWahProcessorNode*>(effectNode)->getProcessor();      return std::make_unique<GuitarWahPanel> (p, presets); }
    if (type == "GuitarReverb")     { auto& p = dynamic_cast<GuitarReverbProcessorNode*>(effectNode)->getProcessor();   return std::make_unique<GuitarReverbPanel> (p, presets); }
    if (type == "GuitarNoiseGate")  { auto& p = dynamic_cast<GuitarNoiseGateProcessorNode*>(effectNode)->getProcessor();return std::make_unique<GuitarNoiseGatePanel> (p, presets); }
    if (type == "GuitarToneStack")  { auto& p = dynamic_cast<ToneStackProcessorNode*>(effectNode)->getProcessor();      return std::make_unique<ToneStackPanel> (p, presets); }
    if (type == "GuitarCabSim")     { auto& p = dynamic_cast<CabSimProcessorNode*>(effectNode)->getProcessor();         return std::make_unique<CabSimPanel> (p, presets); }
    if (type == "GuitarCabIR")      { auto& p = dynamic_cast<CabIRProcessorNode*>(effectNode)->getProcessor();          return std::make_unique<CabIRPanel> (p, presets); }

    return nullptr;
}

// ==============================================================================
//  Open editor window for a node
// ==============================================================================

void WiringCanvas::openEditorWindow (juce::AudioProcessorGraph::Node* node)
{
    if (! node) return;

    auto nodeID = node->nodeID;
    auto* cache = getCached (nodeID);
    if (! cache || ! cache->effectNode) return;

    // If window already exists, just bring it to front
    auto it = editorWindows.find (nodeID);
    if (it != editorWindows.end() && it->second)
    {
        if (it->second->isVisible() && it->second->getPeer() != nullptr)
        {
            it->second->toFront (true);
            return;
        }
        editorWindows.erase (it);
    }

    // Create the panel (returns nullptr for PreAmp — no popup)
    auto panel = createPanelForEffect (cache->effectNode, presetManager);
    if (! panel) return;

    juce::String effectType = cache->effectNode->getEffectType();

    // --- Determine window size -----------------------------------------------
    // Priority: 1) saved in project/session map  2) built-in default
    int panelWidth, panelHeight;

    auto sizeIt = stageGraph.editorWindowSizes.find (effectType);
    if (sizeIt != stageGraph.editorWindowSizes.end()
        && sizeIt->second.x > 0 && sizeIt->second.y > 0)
    {
        panelWidth  = sizeIt->second.x;
        panelHeight = sizeIt->second.y;
    }
    else
    {
        auto def = getDefaultWindowSize (effectType);
        panelWidth  = def.x;
        panelHeight = def.y;
    }

    panel->setSize (panelWidth, panelHeight);

    // Create the window (passes effectType for size capture)
    auto window = std::make_unique<EffectEditorWindow> (
        cache->displayName, this, nodeID, effectType);

    window->setContentOwned (panel.release(), true);
    window->setResizable (true, false);
    window->centreWithSize (panelWidth, panelHeight);
    window->setVisible (true);

    editorWindows[nodeID] = std::move (window);
}

// ==============================================================================
//  Close editor window for a node
// ==============================================================================

void WiringCanvas::closeEditorWindow (juce::AudioProcessorGraph::NodeID id)
{
    auto it = editorWindows.find (id);
    if (it != editorWindows.end())
        editorWindows.erase (it);
}
