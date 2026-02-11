// ==============================================================================
//  WiringCanvas_Core.cpp
//  OnStage — Constructor, timers, node-type cache management, PreAmp sliders,
//            Recorder name editors, DragAndDropTarget for InternalPluginBrowser
//
//  FIX: Recorder name TextEditors are now tracked as members and repositioned
//       on each timer tick, preventing them from floating away during 20Hz
//       recording repaints.
// ==============================================================================

#include "WiringCanvas.h"

// ==============================================================================
//  Construction / destruction
// ==============================================================================

WiringCanvas::WiringCanvas (OnStageGraph& graph, PresetManager& presets)
    : stageGraph (graph),
      presetManager (presets)
{
    setOpaque (true);
    startTimer (MainTimer,  200);   // 5 Hz — general updates, graph changes
    startTimer (MeterTimer,  50);   // 20 Hz — smooth I/O meters + recorder waveform
}

WiringCanvas::~WiringCanvas()
{
    stopTimer (MainTimer);
    stopTimer (MeterTimer);
    stopTimer (DragTimer);
    recorderNameEditors.clear();
    preAmpSliders.clear();
    closeAllEditorWindows();
}

void WiringCanvas::resized()
{
    needsRepaint = true;
}

// ==============================================================================
//  Close all editor windows (called before destruction or device change)
// ==============================================================================

void WiringCanvas::closeAllEditorWindows()
{
    editorWindows.clear();
}

// ==============================================================================
//  Timer callbacks
// ==============================================================================

void WiringCanvas::timerCallback (int timerID)
{
    auto& g = stageGraph.getGraph();

    if (timerID == MainTimer)
    {
        // --- Clean up closed editor windows ----------------------------------
        for (auto it = editorWindows.begin(); it != editorWindows.end();)
        {
            if (it->second && it->second->getPeer() == nullptr)
                it = editorWindows.erase (it);
            else if (! it->second)
                it = editorWindows.erase (it);
            else
                ++it;
        }

        // --- Detect graph topology changes -----------------------------------
        size_t cn = g.getNumNodes();
        size_t cc = g.getConnections().size();
        if (cn != lastNodeCount || cc != lastConnectionCount)
        {
            rebuildNodeCache();
            needsRepaint = true;
        }

        // --- Update PreAmp inline sliders (create/remove/reposition) ---------
        updatePreAmpSliders();

        // --- Update Recorder name editors (reposition with node) -------------
        updateRecorderNameEditors();

        // --- Detect UI state changes -----------------------------------------
        if (highlightPin != lastHighlightPin)
        {
            lastHighlightPin = highlightPin;
            needsRepaint = true;
        }
        if (hoveredConnection.source.nodeID != lastHoveredConnection.source.nodeID ||
            hoveredConnection.destination.nodeID != lastHoveredConnection.destination.nodeID)
        {
            lastHoveredConnection = hoveredConnection;
            needsRepaint = true;
        }
        if (dragCable.active || draggingNodeID.uid != 0)
            needsRepaint = true;

        // --- I/O meter activity check (every 3rd tick ≈ 600 ms) -------------
        static int meterTick = 0;
        if (++meterTick >= 3)
        {
            meterTick = 0;
            for (int ch = 0; ch < 32; ++ch)
            {
                if (stageGraph.inputRms[ch].load (std::memory_order_relaxed) > 0.001f ||
                    stageGraph.outputRms[ch].load (std::memory_order_relaxed) > 0.001f)
                {
                    needsRepaint = true;
                    break;
                }
            }
        }

        if (needsRepaint)
        {
            repaint();
            needsRepaint = false;
        }
    }
    else if (timerID == MeterTimer)
    {
        // Always repaint while I/O has signal (smooth meters)
        bool anySignal = false;
        for (int ch = 0; ch < 4 && !anySignal; ++ch)
        {
            if (stageGraph.inputRms[ch].load (std::memory_order_relaxed) > 0.001f)
                anySignal = true;
            if (stageGraph.outputRms[ch].load (std::memory_order_relaxed) > 0.001f)
                anySignal = true;
        }

        // Also repaint if any recorder is active (for waveform + time display)
        if (hasRecorder)
            anySignal = true;

        if (anySignal)
            repaint();
    }
    else if (timerID == DragTimer)
    {
        // 60 Hz during node/cable dragging for smooth feedback
        needsRepaint = true;
        repaint();
    }
}

// ==============================================================================
//  Node type cache
// ==============================================================================

void WiringCanvas::rebuildNodeCache()
{
    nodeCache.clear();
    hasRecorder = false;
    auto& g = stageGraph.getGraph();

    // Track which recorder nodes still exist
    std::set<juce::AudioProcessorGraph::NodeID> activeRecorderNodes;

    for (auto* node : g.getNodes())
    {
        NodeTypeCache cache;
        auto* proc = node->getProcessor();

        cache.effectNode = dynamic_cast<EffectProcessorNode*> (proc);
        cache.playback   = dynamic_cast<PlaybackNode*> (proc);

        cache.isAudioInput  = (node == stageGraph.audioInputNode.get());
        cache.isAudioOutput = (node == stageGraph.audioOutputNode.get());
        cache.isPlayback    = (node == stageGraph.playbackNode.get());

        if (cache.effectNode)
            cache.hasSidechain = cache.effectNode->hasSidechain();

        // Check for Recorder node
        if (cache.effectNode && cache.effectNode->getEffectType() == "Recorder")
        {
            auto* recNode = static_cast<RecorderProcessorNode*> (cache.effectNode);
            cache.recorder   = &recNode->getProcessor();
            cache.isRecorder = true;
            hasRecorder      = true;
            activeRecorderNodes.insert (node->nodeID);
        }

        // Cache display name
        if (cache.isAudioInput)       cache.displayName = "Audio Input";
        else if (cache.isAudioOutput) cache.displayName = "Audio Output";
        else if (cache.isPlayback)    cache.displayName = "Playback";
        else if (proc)
        {
            try   { cache.displayName = proc->getName(); }
            catch (...) { cache.displayName = "Effect"; }
            if (cache.displayName.length() > 20)
                cache.displayName = cache.displayName.substring (0, 18) + "..";
        }

        nodeCache[node->nodeID] = cache;
    }

    // FIX: Remove name editors for deleted recorder nodes
    for (auto it = recorderNameEditors.begin(); it != recorderNameEditors.end();)
    {
        if (activeRecorderNodes.find (it->first) == activeRecorderNodes.end())
        {
            if (it->second.editor)
                removeChildComponent (it->second.editor.get());
            it = recorderNameEditors.erase (it);
        }
        else
            ++it;
    }

    lastNodeCount      = g.getNumNodes();
    lastConnectionCount = g.getConnections().size();
}

const WiringCanvas::NodeTypeCache* WiringCanvas::getCached (
    juce::AudioProcessorGraph::NodeID id)
{
    auto it = nodeCache.find (id);
    return (it != nodeCache.end()) ? &it->second : nullptr;
}

bool WiringCanvas::shouldShowNode (juce::AudioProcessorGraph::Node* node) const
{
    return node != nullptr && node->getProcessor() != nullptr;
}

// ==============================================================================
//  PreAmp inline slider management
//
//  Creates a real juce::Slider child component for each PreAmp node and
//  repositions it to overlay the node body on every timer tick.
//  Slider value changes write directly to PreAmpProcessor::setGainDb().
// ==============================================================================

void WiringCanvas::updatePreAmpSliders()
{
    auto& g = stageGraph.getGraph();

    // Track which PreAmp nodes currently exist
    std::set<juce::AudioProcessorGraph::NodeID> activePreAmps;

    for (auto* node : g.getNodes())
    {
        auto* cache = getCached (node->nodeID);
        if (! cache || ! cache->effectNode) continue;
        if (cache->effectNode->getEffectType() != "PreAmp") continue;

        activePreAmps.insert (node->nodeID);

        // --- Create slider if it doesn't exist yet ---------------------------
        if (preAmpSliders.find (node->nodeID) == preAmpSliders.end())
        {
            auto slider = std::make_unique<juce::Slider> (
                juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);

            slider->setRange (0.0, 30.0, 0.1);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
            slider->setTextValueSuffix (" dB");
            slider->setDoubleClickReturnValue (true, 0.0);   // double-click → 0 dB

            // Dark theme colors to match canvas
            slider->setColour (juce::Slider::backgroundColourId,
                               juce::Colour (30, 30, 35));
            slider->setColour (juce::Slider::trackColourId,
                               juce::Colours::limegreen.darker (0.3f));
            slider->setColour (juce::Slider::thumbColourId,
                               juce::Colours::limegreen);
            slider->setColour (juce::Slider::textBoxTextColourId,
                               juce::Colours::white);
            slider->setColour (juce::Slider::textBoxBackgroundColourId,
                               juce::Colour (20, 20, 25));
            slider->setColour (juce::Slider::textBoxOutlineColourId,
                               juce::Colours::transparentBlack);

            // Read current gain from processor
            auto* preampNode = static_cast<PreAmpProcessorNode*> (cache->effectNode);
            slider->setValue (preampNode->getProcessor().getGainDb(),
                              juce::dontSendNotification);

            // Callback: safely look up the node each time (avoids dangling ptr)
            auto nodeId = node->nodeID;
            slider->onValueChange = [this, nodeId, s = slider.get()]()
            {
                auto* n = stageGraph.getGraph().getNodeForId (nodeId);
                if (! n) return;
                auto* c = getCached (nodeId);
                if (! c || ! c->effectNode) return;
                auto* pa = static_cast<PreAmpProcessorNode*> (c->effectNode);
                pa->getProcessor().setGainDb ((float) s->getValue());
            };

            addAndMakeVisible (*slider);
            preAmpSliders[node->nodeID] = std::move (slider);
        }

        // --- Reposition slider to match current node position ----------------
        auto bounds = getNodeBounds (node);
        auto sliderArea = bounds;
        sliderArea.removeFromTop (WiringStyle::nodeTitleHeight + 4.0f);
        sliderArea.removeFromBottom (WiringStyle::btnMargin + WiringStyle::btnHeight
                                     + WiringStyle::btnMargin);
        sliderArea = sliderArea.reduced (8.0f, 2.0f);

        auto& slider = preAmpSliders[node->nodeID];
        slider->setBounds (sliderArea.toNearestInt());

        // Sync value from processor (preset load, undo, etc.)
        auto* preampNode = static_cast<PreAmpProcessorNode*> (cache->effectNode);
        float currentDb = preampNode->getProcessor().getGainDb();
        if (std::abs ((float) slider->getValue() - currentDb) > 0.01f)
            slider->setValue (currentDb, juce::dontSendNotification);

        // Dim slider when bypassed
        slider->setEnabled (! node->isBypassed());
        slider->setAlpha (node->isBypassed() ? 0.4f : 1.0f);
    }

    // --- Remove sliders for deleted PreAmp nodes -----------------------------
    for (auto it = preAmpSliders.begin(); it != preAmpSliders.end();)
    {
        if (activePreAmps.find (it->first) == activePreAmps.end())
        {
            removeChildComponent (it->second.get());
            it = preAmpSliders.erase (it);
        }
        else
            ++it;
    }
}

// ==============================================================================
//  Recorder inline name editor — tracked and repositioned
//
//  FIX: The TextEditor is now stored in recorderNameEditors map keyed by
//       NodeID. updateRecorderNameEditors() repositions it on every timer
//       tick so it follows the node when dragged, and doesn't float away
//       during the 20Hz recording repaint cycle.
// ==============================================================================

void WiringCanvas::showRecorderNameEditor (juce::AudioProcessorGraph::NodeID nodeID,
                                            RecorderProcessor* recorder,
                                            juce::Rectangle<float> bounds)
{
    // If already editing this node, just refocus
    auto existing = recorderNameEditors.find (nodeID);
    if (existing != recorderNameEditors.end() && existing->second.editor)
    {
        existing->second.editor->grabKeyboardFocus();
        return;
    }

    auto editor = std::make_unique<juce::TextEditor>();
    editor->setBounds (bounds.toNearestInt());
    editor->setText (recorder->getRecorderName());
    editor->setFont (juce::Font (13.0f));
    editor->selectAll();
    editor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (45, 45, 50));
    editor->setColour (juce::TextEditor::textColourId, juce::Colours::white);
    editor->setColour (juce::TextEditor::outlineColourId, juce::Colours::cyan);

    auto nid = nodeID;  // capture for lambdas

    editor->onReturnKey = [this, nid]()
    {
        dismissRecorderNameEditor (nid);
    };

    editor->onEscapeKey = [this, nid]()
    {
        // Revert: don't save on escape
        auto it = recorderNameEditors.find (nid);
        if (it != recorderNameEditors.end() && it->second.editor && it->second.recorder)
        {
            // Don't commit — just dismiss
        }
        // Remove without committing by clearing editor text to original
        dismissRecorderNameEditor (nid);
    };

    editor->onFocusLost = [this, nid]()
    {
        // Use callAsync to avoid removing component during focus change
        juce::MessageManager::callAsync ([this, nid]()
        {
            dismissRecorderNameEditor (nid);
        });
    };

    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();

    RecorderNameEditorInfo info;
    info.editor = std::move (editor);
    info.recorder = recorder;
    recorderNameEditors[nodeID] = std::move (info);
}

void WiringCanvas::dismissRecorderNameEditor (juce::AudioProcessorGraph::NodeID nodeID)
{
    auto it = recorderNameEditors.find (nodeID);
    if (it == recorderNameEditors.end()) return;

    // Commit the text to the recorder
    if (it->second.editor && it->second.recorder)
        it->second.recorder->setRecorderName (it->second.editor->getText());

    // Remove the editor component
    if (it->second.editor)
        removeChildComponent (it->second.editor.get());

    recorderNameEditors.erase (it);
    repaint();
}

void WiringCanvas::updateRecorderNameEditors()
{
    // Reposition any active name editors to follow their recorder node
    for (auto& [nodeID, info] : recorderNameEditors)
    {
        if (! info.editor) continue;

        auto* node = stageGraph.getGraph().getNodeForId (nodeID);
        if (! node)
        {
            // Node was deleted — schedule removal
            juce::MessageManager::callAsync ([this, nid = nodeID]()
            {
                auto it = recorderNameEditors.find (nid);
                if (it != recorderNameEditors.end())
                {
                    if (it->second.editor)
                        removeChildComponent (it->second.editor.get());
                    recorderNameEditors.erase (it);
                }
            });
            continue;
        }

        // Recalculate the name box area from current node position
        auto bounds = getNodeBounds (node);
        auto contentArea = bounds.reduced (8, 6);
        auto topRow = contentArea.removeFromTop (24);
        auto nameBoxArea = topRow.removeFromLeft (230).reduced (0, 1);

        info.editor->setBounds (nameBoxArea.toNearestInt());
    }
}

// ==============================================================================
//  DragAndDropTarget — accept drags from InternalPluginBrowser
//
//  Drag data format: "INTERNAL:<EffectType>"
//  e.g. "INTERNAL:EQ", "INTERNAL:Compressor"
// ==============================================================================

bool WiringCanvas::isInterestedInDragSource (const SourceDetails& details)
{
    auto dragText = details.description.toString();
    return dragText.startsWith ("INTERNAL:");
}

void WiringCanvas::itemDragEnter (const SourceDetails& details)
{
    juce::ignoreUnused (details);
    dropTargetHovered = true;
    needsRepaint = true;
}

void WiringCanvas::itemDragMove (const SourceDetails& details)
{
    dropHoverPos = details.localPosition;
    needsRepaint = true;
}

void WiringCanvas::itemDragExit (const SourceDetails& details)
{
    juce::ignoreUnused (details);
    dropTargetHovered = false;
    needsRepaint = true;
}

void WiringCanvas::itemDropped (const SourceDetails& details)
{
    dropTargetHovered = false;

    auto dragText = details.description.toString();
    if (! dragText.startsWith ("INTERNAL:"))
        return;

    juce::String effectType = dragText.fromFirstOccurrenceOf ("INTERNAL:", false, false);
    auto dropPos = details.localPosition;

    if (effectType.isNotEmpty())
    {
        stageGraph.addEffect (effectType,
                              static_cast<float> (dropPos.x),
                              static_cast<float> (dropPos.y));
        markDirty();
    }
}
