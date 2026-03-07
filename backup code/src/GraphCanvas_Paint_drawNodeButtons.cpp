// #D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_Paint_drawNodeButtons.cpp
// =============================================================================
// INSTRUCTIONS:
// 1. Add this include to GraphCanvas_Paint.cpp (after #include "LatcherProcessor.h"):
//        #include "MidiMultiFilterProcessor.h"
//        #include "ContainerProcessor.h"
//
// 2. Replace the entire drawNodeButtons() function in GraphCanvas_Paint.cpp
//    with the version below.
// =============================================================================

void GraphCanvas::drawNodeButtons(juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto nodeBounds = getNodeBounds(node);
    auto* cache = getCachedNodeType(node->nodeID);
    auto* proc = node->getProcessor();
    
    // =========================================================================
    // FIX 2: Container nodes - M, CH, T, X buttons
    // =========================================================================
    if (dynamic_cast<ContainerProcessor*>(proc))
    {
        drawContainerButtons(g, node);
        return;
    }
    
    SimpleConnectorProcessor* simpleConnector = cache ? cache->simpleConnector 
                                                       : dynamic_cast<SimpleConnectorProcessor*>(proc);
    
    StereoMeterProcessor* stereoMeter = cache ? cache->stereoMeter 
                                               : dynamic_cast<StereoMeterProcessor*>(proc);
    MidiMonitorProcessor* midiMonitor = cache ? cache->midiMonitor
                                               : dynamic_cast<MidiMonitorProcessor*>(proc);
    
    RecorderProcessor* recorder = cache ? cache->recorder
                                         : dynamic_cast<RecorderProcessor*>(proc);

    // =========================================================================
    // MidiMultiFilter: E (editor popup) + P (pass-through) + X (delete)
    // =========================================================================
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

    // Latcher: E (editor popup) + X (delete)
    if (dynamic_cast<LatcherProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    if (simpleConnector)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

        auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(simpleConnector->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("M", muteRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // FIX: Stereo Meter and MIDI Monitor - X button ONLY
    if (stereoMeter || midiMonitor)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // Auto Sampler: E (editor) + F (folder) + X (delete)
    if (dynamic_cast<AutoSamplerProcessor*>(proc))
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
        
        // F button (folder)
        auto folderRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRoundedRectangle(folderRect, 3.0f);
        g.setColour(juce::Colour(200, 180, 100));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("F", folderRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // MIDI Player: E (editor/mute) + X (delete)
    if (dynamic_cast<MidiPlayerProcessor*>(proc))
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        // E button (channel mute table)
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // Step Seq: E (editor popup) + X (delete)
    if (dynamic_cast<CCStepperProcessor*>(proc))
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

        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // Transient Splitter: E (editor popup) + X (delete)
    if (dynamic_cast<TransientSplitterProcessor*>(proc))
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

        auto deleteRect2 = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect2, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("X", deleteRect2, juce::Justification::centred);
        return;
    }
    
    // =========================================================================
    // Default: Regular plugin nodes (wrapped in MeteringProcessor)
    // =========================================================================
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(proc);
    bool isInstrument = cache && cache->isInstrument;

    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

    if (meteringProc && meteringProc->hasEditor())
    {
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    if (isInstrument && meteringProc)
    {
        auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::orange.darker());
        g.fillRoundedRectangle(chRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        
        int mask = meteringProc->getMidiChannelMask();
        juce::String text = "CH";
        if (mask != 0) {
            for (int i = 0; i < 16; ++i) {
                if ((mask >> i) & 1) {
                    text = juce::String(i + 1);
                    break;
                }
            }
        }
        
        g.drawText(text, chRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(node->isBypassed() ? juce::Colours::red : juce::Colours::lightgreen);
    g.fillRoundedRectangle(muteRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("M", muteRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    if (!isInstrument && meteringProc)
    {
        bool passThrough = meteringProc->isPassThrough();
        auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(passThrough ? juce::Colours::yellow : juce::Colours::grey.darker());
        g.fillRoundedRectangle(passRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("P", passRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // T button (transport override) - all plugin nodes
    if (meteringProc)
    {
        bool synced = meteringProc->isTransportSynced();
        auto transportRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(synced ? juce::Colours::yellow.darker(0.4f) : juce::Colours::yellow);
        g.fillRoundedRectangle(transportRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("T", transportRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(juce::Colours::darkred);
    g.fillRoundedRectangle(deleteRect, 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("X", deleteRect, juce::Justification::centred);
}

// =========================================================================
// FIX 2: Container button drawing (M, CH, T, X)
// =========================================================================
void GraphCanvas::drawContainerButtons(juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto* container = dynamic_cast<ContainerProcessor*>(node->getProcessor());
    if (!container) return;
    
    auto nodeBounds = getNodeBounds(node);
    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

    // M button (mute)
    auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(container->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
    g.fillRoundedRectangle(muteRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("M", muteRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    // CH button (MIDI channel selector)
    auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(juce::Colours::orange.darker());
    g.fillRoundedRectangle(chRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    
    int mask = container->getMidiChannelMask();
    juce::String text = "CH";
    if (mask != 0) {
        for (int i = 0; i < 16; ++i) {
            if ((mask >> i) & 1) {
                text = juce::String(i + 1);
                break;
            }
        }
    }
    
    g.drawText(text, chRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    // T button (transport override)
    bool synced = container->isTransportSynced();
    auto transportRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(synced ? juce::Colours::yellow.darker(0.4f) : juce::Colours::yellow);
    g.fillRoundedRectangle(transportRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("T", transportRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    // X button (delete)
    auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(juce::Colours::darkred);
    g.fillRoundedRectangle(deleteRect, 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("X", deleteRect, juce::Justification::centred);
}