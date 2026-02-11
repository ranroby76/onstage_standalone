================================================================================
  GUITAR NODES — INTEGRATION PATCHES
  Apply these changes to existing OnStage files
================================================================================


================================================================================
1. EffectNodes.h  (src/graph/EffectNodes.h)
================================================================================

--- PATCH A: Add virtual getNodeCategory() to EffectProcessorNode base class ---

FIND:
    virtual juce::String getEffectType() const = 0;

ADD AFTER:
    virtual juce::String getNodeCategory() const { return ""; }


--- PATCH B: Add #include for GuitarNodes.h ---

ADD near the other DSP includes:
    #include "../guitar/GuitarNodes.h"


--- PATCH C: Add guitar nodes to createEffectNode() factory ---

ADD before the final "return nullptr;":

    // --- Guitar nodes ---
    if (type == "GuitarOverdrive")  return std::make_unique<OverdriveProcessorNode>();
    if (type == "GuitarDistortion") return std::make_unique<DistortionProcessorNode>();
    if (type == "GuitarFuzz")       return std::make_unique<FuzzProcessorNode>();
    if (type == "GuitarChorus")     return std::make_unique<GuitarChorusProcessorNode>();
    if (type == "GuitarFlanger")    return std::make_unique<GuitarFlangerProcessorNode>();
    if (type == "GuitarPhaser")     return std::make_unique<GuitarPhaserProcessorNode>();
    if (type == "GuitarTremolo")    return std::make_unique<GuitarTremoloProcessorNode>();
    if (type == "GuitarReverb")     return std::make_unique<GuitarReverbProcessorNode>();
    if (type == "GuitarNoiseGate")  return std::make_unique<GuitarNoiseGateProcessorNode>();
    if (type == "GuitarToneStack")  return std::make_unique<ToneStackProcessorNode>();
    if (type == "GuitarCabSim")     return std::make_unique<CabSimProcessorNode>();


--- PATCH D: Add guitar effects to getAvailableEffectTypes() ---

ADD the guitar types to the returned vector. You can append them:

    // existing types...
    "GuitarOverdrive", "GuitarDistortion", "GuitarFuzz",
    "GuitarChorus", "GuitarFlanger", "GuitarPhaser", "GuitarTremolo",
    "GuitarReverb", "GuitarNoiseGate", "GuitarToneStack", "GuitarCabSim"


================================================================================
2. WiringStyle.h  (src/graph/WiringStyle.h)
================================================================================

ADD these constants alongside the existing colour definitions:

    // Guitar node colours (deep purple theme)
    static inline const auto colGuitarNodeTitle    = juce::Colour (0xFF663399);
    static inline const auto colGuitarNodeBody     = juce::Colour (0xFF2A1A3D);
    static inline const auto colGuitarNodeBorder   = juce::Colour (0xFF9B59B6);


================================================================================
3. WiringCanvas_Paint.cpp  (src/graph/WiringCanvas_Paint.cpp)
================================================================================

In the drawNode() function, where the title bar colour is set, add a check
for guitar category. Find where colNodeTitle is used and add:

    // Check if this is a guitar node for purple theming
    juce::Colour titleColour = WiringStyle::colNodeTitle;
    juce::Colour bodyColour  = WiringStyle::colNodeBody;

    if (auto* effect = dynamic_cast<EffectProcessorNode*>(node->getProcessor()))
    {
        if (effect->getNodeCategory() == "Guitar")
        {
            titleColour = WiringStyle::colGuitarNodeTitle;
            bodyColour  = WiringStyle::colGuitarNodeBody;
        }
    }

Then use titleColour/bodyColour instead of the hardcoded colours for the
node title bar fill and body fill respectively.


================================================================================
4. WiringCanvas_NodeWindows.cpp  (src/graph/WiringCanvas_NodeWindows.cpp)
================================================================================

In createPanelForEffect(), add cases for all guitar nodes.
ADD before the final return nullptr:

    // --- Guitar panels ---
    if (type == "GuitarOverdrive")  return std::make_unique<OverdrivePanel>    (dynamic_cast<OverdriveProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarDistortion") return std::make_unique<DistortionPanel>   (dynamic_cast<DistortionProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarFuzz")       return std::make_unique<FuzzPanel>         (dynamic_cast<FuzzProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarChorus")     return std::make_unique<GuitarChorusPanel> (dynamic_cast<GuitarChorusProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarFlanger")    return std::make_unique<GuitarFlangerPanel>(dynamic_cast<GuitarFlangerProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarPhaser")     return std::make_unique<GuitarPhaserPanel> (dynamic_cast<GuitarPhaserProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarTremolo")    return std::make_unique<GuitarTremoloPanel>(dynamic_cast<GuitarTremoloProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarReverb")     return std::make_unique<GuitarReverbPanel> (dynamic_cast<GuitarReverbProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarNoiseGate")  return std::make_unique<GuitarNoiseGatePanel>(dynamic_cast<GuitarNoiseGateProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarToneStack")  return std::make_unique<ToneStackPanel>    (dynamic_cast<ToneStackProcessorNode&>(*node).getProcessor(), presetManager);
    if (type == "GuitarCabSim")     return std::make_unique<CabSimPanel>       (dynamic_cast<CabSimProcessorNode&>(*node).getProcessor(), presetManager);

And add includes at the top:
    #include "../guitar/GuitarPanels.h"


================================================================================
5. Right-click Menu (if using submenu for Guitar effects)
================================================================================

If getAvailableEffectTypes() is used to populate a flat menu, you can 
optionally create a "Guitar" submenu. In the code that builds the PopupMenu
from the effect types, check if the type starts with "Guitar" and add it
to a submenu instead:

    juce::PopupMenu guitarMenu;
    for (auto& type : getAvailableEffectTypes())
    {
        if (type.startsWith ("Guitar"))
            guitarMenu.addItem (type, type.fromFirstOccurrenceOf ("Guitar", false, true));
        else
            menu.addItem (type, type);
    }
    if (guitarMenu.getNumItems() > 0)
        menu.addSubMenu ("Guitar", guitarMenu);


================================================================================
6. CMakeLists.txt
================================================================================

ADD the guitar source directory. Find where source files are listed and add:

    file(GLOB GUITAR_SOURCES
        src/guitar/*.h
        src/guitar/*.cpp
    )

Then add ${GUITAR_SOURCES} to your target_sources() call.
