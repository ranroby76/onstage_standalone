// =============================================================================
// PATCH FILE: MidiMultiFilter Node Face — E + P + X buttons
// =============================================================================
// Contains all remaining changes needed for successful compile.
// Each patch has exact location markers from colosseum_full_code.txt.
// =============================================================================


// #############################################################################
// PATCH A: GraphCanvas.h — Add forward declaration
//
// Location: After line 15279 (after "class LatcherProcessor;")
// Action: INSERT the following line
// #############################################################################

class MidiMultiFilterProcessor;


// #############################################################################
// PATCH B: GraphCanvas.h — Add to NodeTypeCache struct
//
// Location: After line 15450 (after "LatcherProcessor* latcher = nullptr;")
// Action: INSERT the following line
// #############################################################################

        MidiMultiFilterProcessor* midiMultiFilter = nullptr;


// #############################################################################
// PATCH C: GraphCanvas.h — Add showMidiMultiFilterEditor declaration
//
// Location: After line 15613 (after "void showLatcherEditor(juce::AudioProcessorGraph::Node* node);")
// Action: INSERT the following line
// #############################################################################

    void showMidiMultiFilterEditor(juce::AudioProcessorGraph::Node* node);


// #############################################################################
// PATCH D: GraphCanvas_Core.cpp — Add to rebuildNodeTypeCache()
//
// Location: After line 6528 (after "cache.latcher = dynamic_cast<LatcherProcessor*>(proc);")
// Action: INSERT the following line
// #############################################################################

        cache.midiMultiFilter = dynamic_cast<MidiMultiFilterProcessor*>(proc);


// #############################################################################
// PATCH E: GraphCanvas_Paint.cpp — Add #include
//
// Location: After line 10910 (after '#include "LatcherProcessor.h"')
//           This is the includes block of GraphCanvas_Paint.cpp
// Action: INSERT the following line
// #############################################################################

#include "MidiMultiFilterProcessor.h"


// #############################################################################
// PATCH F: GraphCanvas_Paint.cpp — Add MidiMultiFilter to drawNodeButtons()
//
// Location: After line 12415 (after the RecorderProcessor* variable declaration,
//           BEFORE the "// Latcher: E (editor popup) + X (delete)" comment at line 12417)
// Action: INSERT the following block
// #############################################################################

    // MidiMultiFilter: E (editor popup) + P (pass-through) + X (delete)
    MidiMultiFilterProcessor* midiMultiFilter = cache ? cache->midiMultiFilter
                                                       : dynamic_cast<MidiMultiFilterProcessor*>(proc);
    if (midiMultiFilter)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        // E button (editor)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // P button (pass-through)
        bool isPass = midiMultiFilter->passThrough;
        auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(isPass ? juce::Colours::yellow : juce::Colours::grey.darker());
        g.fillRoundedRectangle(passRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("P", passRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        // X button (delete)
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }


// #############################################################################
// PATCH G: GraphCanvas_Mouse.cpp — Add #include
//
// Location: After line 7976 (after '#include "LatcherEditorComponent.h"')
// Action: INSERT the following lines
// #############################################################################

#include "MidiMultiFilterProcessor.h"
#include "MidiMultiFilterEditorComponent.h"


// #############################################################################
// PATCH H: GraphCanvas_Mouse.cpp — Add MidiMultiFilter click handling in mouseDown
//
// Location: Between lines 9164 and 9167, i.e.:
//           After "// END TRANSIENT SPLITTER button handling" block
//           BEFORE "// LATCHER button handling" block
// Action: INSERT the following block
// #############################################################################

        // =========================================================================
        // MIDI MULTI FILTER button handling (E, P, X buttons)
        // =========================================================================
        MidiMultiFilterProcessor* midiMultiFilter = dynamic_cast<MidiMultiFilterProcessor*>(node->getProcessor());
        if (midiMultiFilter)
        {
            nodeBounds.removeFromTop(Style::nodeTitleHeight);
            float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
            float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

            auto mmfEditRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto mmfPassRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
            btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
            auto mmfDeleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);

            if (e.mods.isRightButtonDown())
            {
                auto launchTip = [&](const juce::String& text) {
                    auto* tooltip = new StatusToolTip(text, true);
                    juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(tooltip),
                        juce::Rectangle<int>(screenClickPos.x + 10,
                                             screenClickPos.y + 10, 1, 1),
                        nullptr);
                };

                if (mmfEditRect.contains(pos))   { launchTip("E - Open MIDI Multi Filter editor"); return; }
                if (mmfPassRect.contains(pos))    { launchTip("P - Pass-through (bypass all filtering, yellow when active)"); return; }
                if (mmfDeleteRect.contains(pos))  { launchTip("X - Delete this MIDI Multi Filter"); return; }
                return;
            }

            if (mmfEditRect.contains(pos))
            {
                showMidiMultiFilterEditor(node);
                return;
            }

            if (mmfPassRect.contains(pos))
            {
                midiMultiFilter->passThrough = !midiMultiFilter->passThrough;
                needsRepaint = true;
                repaint();
                return;
            }

            if (mmfDeleteRect.contains(pos))
            {
                auto nodeID = node->nodeID;
                auto windowIt = activePluginWindows.find(nodeID);
                if (windowIt != activePluginWindows.end())
                    activePluginWindows.erase(windowIt);
                disconnectNode(node);
                processor.removeNode(nodeID);
                updateParentSelector();
                markDirty();
                return;
            }

            draggingNodeID = node->nodeID;
            nodeDragOffset = pos - juce::Point<float>((float)node->properties["x"], (float)node->properties["y"]);
            startTimer(MouseInteractionTimerID, 16);
            return;
        }
        // =========================================================================
        // END MIDI MULTI FILTER button handling
        // =========================================================================


// #############################################################################
// PATCH I: GraphCanvas_Mouse.cpp — Add showMidiMultiFilterEditor() implementation
//
// Location: After showLatcherEditor() function (after line 10649, before
//           "void GraphCanvas::mouseWheelMove" at line 10655)
// Action: INSERT the following function
// #############################################################################

void GraphCanvas::showMidiMultiFilterEditor(juce::AudioProcessorGraph::Node* node)
{
    if (!node) return;
    auto* mmf = dynamic_cast<MidiMultiFilterProcessor*>(node->getProcessor());
    if (!mmf) return;

    auto* editorComp = mmf->createEditor();
    auto nodeBounds = getNodeBounds(node);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(editorComp),
        juce::Rectangle<int>(
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).x,
            virtualToScreen(nodeBounds.getCentreX(), nodeBounds.getCentreY()).y,
            1, 1),
        nullptr);
}
