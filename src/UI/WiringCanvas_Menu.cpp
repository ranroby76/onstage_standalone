// ==============================================================================
//  WiringCanvas_Menu.cpp
//  OnStage — Right-click menus for adding effects, node context, wire context
//
//  Menu layout:
//    Studio Effects  ▸  (submenu with all studio DSP)
//    Guitar Effects  ▸  (submenu with all guitar DSP)
//    System Tools    ▸  Pre-Amp, Recorder, Tuner
// ==============================================================================

#include "WiringCanvas.h"

// ==============================================================================
//  "Add Effect" menu  (right-click on empty canvas)
// ==============================================================================

void WiringCanvas::showAddEffectMenu()
{
    juce::PopupMenu m;

    // ── Studio Effects submenu ──────────────────────────────────────────────
    {
        juce::PopupMenu studioMenu;

        studioMenu.addSectionHeader ("EQ & Dynamics");
        studioMenu.addItem (102, "EQ");
        studioMenu.addItem (103, "Compressor");
        studioMenu.addItem (101, "Gate");
        studioMenu.addItem (104, "De-Esser");
        studioMenu.addItem (105, "Dynamic EQ");
        studioMenu.addSeparator();

        studioMenu.addSectionHeader ("Color & Character");
        studioMenu.addItem (106, "Exciter");
        studioMenu.addItem (107, "Sculpt");
        studioMenu.addItem (108, "Saturation");
        studioMenu.addItem (109, "Doubler");
        studioMenu.addSeparator();

        studioMenu.addSectionHeader ("Time-based");
        studioMenu.addItem (110, "Convo. Reverb");
        studioMenu.addItem (114, "Studio Reverb");
        studioMenu.addItem (111, "Delay");
        studioMenu.addSeparator();

        studioMenu.addSectionHeader ("Pitch");
        studioMenu.addItem (112, "Harmonizer");
        studioMenu.addSeparator();

        studioMenu.addSectionHeader ("Mastering");
        studioMenu.addItem (115, "Master");

        m.addSubMenu ("Studio Effects", studioMenu);
    }

    // ── Guitar Effects submenu ──────────────────────────────────────────────
    {
        juce::PopupMenu guitarMenu;

        guitarMenu.addSectionHeader ("Drive");
        guitarMenu.addItem (200, "Overdrive");
        guitarMenu.addItem (201, "Distortion");
        guitarMenu.addItem (202, "Fuzz");
        guitarMenu.addSeparator();

        guitarMenu.addSectionHeader ("Modulation");
        guitarMenu.addItem (203, "Chorus");
        guitarMenu.addItem (204, "Flanger");
        guitarMenu.addItem (205, "Phaser");
        guitarMenu.addItem (206, "Tremolo");
        guitarMenu.addSeparator();

        guitarMenu.addSectionHeader ("Ambience");
        guitarMenu.addItem (207, "Reverb");
        guitarMenu.addSeparator();

        guitarMenu.addSectionHeader ("Utility");
        guitarMenu.addItem (208, "Noise Gate");
        guitarMenu.addItem (209, "Tone Stack");
        guitarMenu.addSeparator();

        guitarMenu.addSectionHeader ("Cabinets");
        guitarMenu.addItem (210, "Cab Sim");
        guitarMenu.addItem (211, "Cab IR (Convolution)");

        m.addSubMenu ("Guitar Effects", guitarMenu);
    }

    // ── System Tools submenu ────────────────────────────────────────────────
    {
        juce::PopupMenu systemMenu;
        systemMenu.addItem (100, "Pre-Amp");
        systemMenu.addItem (113, "Recorder");
        // systemMenu.addItem (116, "Tuner");  // DISABLED — needs pitch detection fixes

        m.addSubMenu ("System Tools", systemMenu);
    }

    // ── Show and handle ─────────────────────────────────────────────────────
    juce::Component::SafePointer<WiringCanvas> safeThis (this);
    auto clickPos = lastRightClickPos;

    m.showMenuAsync (juce::PopupMenu::Options(),
        [safeThis, clickPos] (int result)
        {
            if (safeThis == nullptr || result == 0) return;

            juce::String type;
            switch (result)
            {
                // Studio
                case 100: type = "PreAmp";       break;
                case 101: type = "Gate";          break;
                case 102: type = "EQ";            break;
                case 103: type = "Compressor";    break;
                case 104: type = "DeEsser";       break;
                case 105: type = "DynamicEQ";     break;
                case 106: type = "Exciter";       break;
                case 107: type = "Sculpt";        break;
                case 108: type = "Saturation";    break;
                case 109: type = "Doubler";       break;
                case 110: type = "Reverb";        break;
                case 111: type = "Delay";         break;
                case 112: type = "Harmonizer";    break;
                case 113: type = "Recorder";      break;
                case 114: type = "StudioReverb";  break;
                case 115: type = "Master";        break;
                case 116: type = "Tuner";         break;

                // Guitar
                case 200: type = "GuitarOverdrive";  break;
                case 201: type = "GuitarDistortion";  break;
                case 202: type = "GuitarFuzz";        break;
                case 203: type = "GuitarChorus";      break;
                case 204: type = "GuitarFlanger";     break;
                case 205: type = "GuitarPhaser";      break;
                case 206: type = "GuitarTremolo";     break;
                case 207: type = "GuitarReverb";      break;
                case 208: type = "GuitarNoiseGate";   break;
                case 209: type = "GuitarToneStack";   break;
                case 210: type = "GuitarCabSim";      break;
                case 211: type = "GuitarCabIR";       break;

                default: return;
            }

            safeThis->stageGraph.addEffect (type, clickPos.x, clickPos.y);
            safeThis->markDirty();
        });
}

// ==============================================================================
//  Node context menu  (right-click on node title bar)
// ==============================================================================

void WiringCanvas::showNodeContextMenu (juce::AudioProcessorGraph::Node* node,
                                         juce::Point<float>)
{
    auto* cache = getCached (node->nodeID);
    bool isIO = cache && (cache->isAudioInput || cache->isAudioOutput || cache->isPlayback);

    juce::PopupMenu m;
    m.addItem (1, "Disconnect All Wires");

    if (! isIO)
    {
        m.addSeparator();
        m.addItem (2, "Delete");
    }

    auto nodeID = node->nodeID;
    juce::Component::SafePointer<WiringCanvas> safeThis (this);

    m.showMenuAsync (juce::PopupMenu::Options(),
        [safeThis, nodeID, isIO] (int result)
        {
            if (safeThis == nullptr) return;

            if (result == 1)
            {
                safeThis->stageGraph.disconnectNode (nodeID);
                safeThis->markDirty();
            }
            else if (result == 2 && ! isIO)
            {
                safeThis->closeEditorWindow (nodeID);
                safeThis->stageGraph.removeNode (nodeID);
                safeThis->markDirty();
            }
        });
}

// ==============================================================================
//  Wire context menu  (right-click on a wire)
// ==============================================================================

void WiringCanvas::showWireMenu (const juce::AudioProcessorGraph::Connection& conn,
                                  juce::Point<float>)
{
    juce::PopupMenu m;
    m.addItem (1, "Delete Wire");

    juce::Component::SafePointer<WiringCanvas> safeThis (this);

    m.showMenuAsync (juce::PopupMenu::Options(),
        [safeThis, conn] (int result)
        {
            if (safeThis == nullptr || result != 1) return;
            safeThis->stageGraph.getGraph().removeConnection (conn);
            safeThis->markDirty();
        });
}
