// ============================================================================= 
// FIXED drawNodeButtons() function for GraphCanvas_Paint.cpp
// FIX: Stereo Meter and MIDI Monitor get only X button (no M button)
// They don't produce audio, so mute makes no sense
// Replace the existing drawNodeButtons function with this version
// =============================================================================

void GraphCanvas::drawNodeButtons(juce::Graphics& g, juce::AudioProcessorGraph::Node* node)
{
    auto nodeBounds = getNodeBounds(node);
    auto* cache = getCachedNodeType(node->nodeID);
    auto* proc = node->getProcessor();
    
    // Check if this is a SimpleConnector (system tool)
    SimpleConnectorProcessor* simpleConnector = cache ? cache->simpleConnector 
                                                       : dynamic_cast<SimpleConnectorProcessor*>(proc);
    
    // FIX: Check if this is a StereoMeter or MIDIMonitor (system tools with no audio output)
    StereoMeterProcessor* stereoMeter = cache ? cache->stereoMeter 
                                               : dynamic_cast<StereoMeterProcessor*>(proc);
    MidiMonitorProcessor* midiMonitor = cache ? cache->midiMonitor
                                               : dynamic_cast<MidiMonitorProcessor*>(proc);
    
    if (simpleConnector)
    {
        // =====================================================================
        // SIMPLE CONNECTOR BUTTONS - Mute/Delete only
        // =====================================================================
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        // M button (Mute)
        auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(simpleConnector->isMuted() ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("M", muteRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        
        // X button (Delete)
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // FIX: Stereo Meter and MIDI Monitor - X button ONLY (no M button)
    // These modules don't produce audio, so mute makes no sense
    if (stereoMeter || midiMonitor)
    {
        nodeBounds.removeFromTop(Style::nodeTitleHeight);
        float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
        float btnX = nodeBounds.getX() + Style::bottomBtnMargin;
        
        // X button (Delete) - ONLY button for these modules
        auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle(deleteRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("X", deleteRect, juce::Justification::centred);
        return;
    }
    
    // Regular MeteringProcessor buttons (plugins)
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(proc);
        // FIX: NEVER call getPluginDescription() - it freezes some plugins
        bool isInstrument = cache && cache->isInstrument;

    nodeBounds.removeFromTop(Style::nodeTitleHeight);
    float btnY = nodeBounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    float btnX = nodeBounds.getX() + Style::bottomBtnMargin;

    // E button (only if has editor)
    if (meteringProc && meteringProc->hasEditor())
    {
        auto editRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::cyan.darker());
        g.fillRoundedRectangle(editRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("E", editRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // CH button (instruments only) - Shows selected channel number
    if (isInstrument && meteringProc)
    {
        auto chRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(juce::Colours::orange.darker());
        g.fillRoundedRectangle(chRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        
        // Get selected channel from mask (single channel mode)
        int mask = meteringProc->getMidiChannelMask();
        juce::String text = "CH";
        if (mask != 0) {
            // Find which channel is selected (single bit set)
            for (int i = 0; i < 16; ++i) {
                if ((mask >> i) & 1) {
                    text = juce::String(i + 1);  // Show channel number 1-16
                    break;
                }
            }
        }
        
        g.drawText(text, chRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // M button (mute/bypass)
    auto muteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(node->isBypassed() ? juce::Colours::red : juce::Colours::lightgreen);
    g.fillRoundedRectangle(muteRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("M", muteRect, juce::Justification::centred);
    btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;

    // P button (effects only - pass-through)
    if (!isInstrument && meteringProc)
    {
        bool passThrough = meteringProc->isPassThrough();
        auto passRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
        g.setColour(passThrough ? juce::Colours::yellow : juce::Colours::grey.darker());
        g.fillRoundedRectangle(passRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("P", passRect, juce::Justification::centred);
        btnX += Style::bottomBtnWidth + Style::bottomBtnSpacing;
    }

    // X button (delete)
    auto deleteRect = juce::Rectangle<float>(btnX, btnY, Style::bottomBtnWidth, Style::bottomBtnHeight);
    g.setColour(juce::Colours::darkred);
    g.fillRoundedRectangle(deleteRect, 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("X", deleteRect, juce::Justification::centred);
}
