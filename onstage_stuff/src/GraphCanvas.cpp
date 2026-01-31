#include "GraphCanvas.h"
#include "PluginEditor.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"

GraphCanvas::GraphCanvas(SubterraneumAudioProcessor& p) : processor(p) { 
    startTimer(40);
}

GraphCanvas::~GraphCanvas() { 
    activePluginWindows.clear();
    stopTimer(); 
}

void GraphCanvas::updateParentSelector() { 
    if (auto* editor = findParentComponentOfClass<SubterraneumAudioProcessorEditor>()) 
        editor->updateInstrumentSelector();
}

void GraphCanvas::timerCallback() { repaint(); }

bool GraphCanvas::isAsioActive() const {
    if (auto* dm = SubterraneumAudioProcessor::standaloneDeviceManager)
        return dm->getCurrentAudioDevice() != nullptr;
    return false;
}

bool GraphCanvas::shouldShowNode(juce::AudioProcessorGraph::Node* node) const {
    bool isAudioIO = (node == processor.audioInputNode.get() || node == processor.audioOutputNode.get());
    if (isAudioIO && !isAsioActive())
        return false;
    return true;
}

void GraphCanvas::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground); 
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    for(int x=0; x<getWidth(); x+=20) g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for(int y=0; y<getHeight(); y+=20) g.drawHorizontalLine(y, 0.0f, (float)getWidth());
    
    if (!processor.mainGraph) return; 
    verifyPositions();
    
    // Draw connections
    for (auto& connection : processor.mainGraph->getConnections()) {
        auto* srcNode = processor.mainGraph->getNodeForId(connection.source.nodeID);
        auto* dstNode = processor.mainGraph->getNodeForId(connection.destination.nodeID);
        if (srcNode && dstNode && shouldShowNode(srcNode) && shouldShowNode(dstNode)) {
            bool isMidi = connection.source.isMIDI();
            PinID srcPin = { srcNode->nodeID, isMidi ? 0 : connection.source.channelIndex, false, isMidi };
            PinID dstPin = { dstNode->nodeID, isMidi ? 0 : connection.destination.channelIndex, true, isMidi };
            auto start = getPinPos(srcNode, srcPin); 
            auto end = getPinPos(dstNode, dstPin);
            
            juce::Colour wireColor = juce::Colours::darkgrey;
            float thickness = 2.0f; 
            bool isHovered = (connection == hoveredConnection);
            bool hasSignal = false;
            
            if (!srcNode->isBypassed() && !dstNode->isBypassed()) {
                if (isMidi) {
                    if (auto* mp = dynamic_cast<MeteringProcessor*>(srcNode->getProcessor())) {
                        hasSignal = mp->isMidiOutActive();
                    } else if (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(srcNode->getProcessor())) {
                        hasSignal = processor.mainMidiInFlash.load();
                    }
                } else {
                    int ch = connection.source.channelIndex;
                    if (auto* mp = dynamic_cast<MeteringProcessor*>(srcNode->getProcessor())) {
                        hasSignal = (mp->getOutputRms(ch) > 0.001f);
                    } else if (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(srcNode->getProcessor())) {
                        if (ch < 2) hasSignal = (processor.mainInputRms[ch].load() > 0.001f);
                    }
                }
                
                wireColor = isMidi ? Style::colPinMidi : Style::colPinAudio;
                thickness = 2.5f;
                
                if (hasSignal) {
                    wireColor = wireColor.brighter(0.5f);
                    thickness = 3.5f;
                }
            }
            
            if (isHovered) { 
                wireColor = wireColor.brighter(0.5f); 
                thickness = 3.5f;
            } 
            
            juce::Path p; 
            p.startNewSubPath(start); 
            p.cubicTo(start.x, start.y + 50, end.x, end.y - 50, end.x, end.y);
            g.setColour(wireColor); 
            g.strokePath(p, juce::PathStrokeType(thickness));
        } 
    }
    
    // Draw dragging cable
    if (dragCable.active) { 
        bool isMidi = dragCable.sourcePin.isMidi; 
        juce::Colour color = isMidi ? Style::colPinMidi : Style::colPinAudio;
        auto* srcNode = processor.mainGraph->getNodeForId(dragCable.sourcePin.nodeID);
        if (srcNode) { 
            auto start = getPinPos(srcNode, dragCable.sourcePin); 
            auto end = dragCable.currentDragPos;
            juce::Path p; 
            p.startNewSubPath(start); 
            p.cubicTo(start.x, start.y + 50, end.x, end.y - 50, end.x, end.y); 
            g.setColour(color.withAlpha(0.6f)); 
            g.strokePath(p, juce::PathStrokeType(2.5f));
        } 
    }
    
    // Draw nodes
    for (auto* node : processor.mainGraph->getNodes()) {
        if (shouldShowNode(node))
            drawNode(g, node);
    }
}

void GraphCanvas::drawNode(juce::Graphics& g, juce::AudioProcessorGraph::Node* node) { 
    bool isAudioInput = (node == processor.audioInputNode.get());
    bool isAudioOutput = (node == processor.audioOutputNode.get());
    bool isAudioIONode = isAudioInput || isAudioOutput;
    bool isIONode = isAudioIONode || (node == processor.midiInputNode.get()) || (node == processor.midiOutputNode.get());
    auto bounds = getNodeBounds(node); 
    auto* proc = node->getProcessor();
    auto* meteringProc = dynamic_cast<MeteringProcessor*>(proc); 
    
    // Detect system tools
    auto* simpleConnector = dynamic_cast<SimpleConnectorProcessor*>(proc);
    bool isSimpleConnector = (simpleConnector != nullptr);
    auto* stereoMeter = dynamic_cast<StereoMeterProcessor*>(proc);
    bool isStereoMeter = (stereoMeter != nullptr);
    bool isSystemTool = isSimpleConnector || isStereoMeter;
    
    // User plugin = not I/O and not system tool
    bool isUserPlugin = !isIONode && !isSystemTool;
    
    // Check if this is an effect (not an instrument)
    bool isEffect = false;
    bool isPassThrough = false;
    if (meteringProc) {
        auto* innerPlugin = meteringProc->getInnerPlugin();
        if (innerPlugin) {
            isEffect = !innerPlugin->getPluginDescription().isInstrument;
            isPassThrough = meteringProc->isPassThrough();
        }
    }
    
    // Check if this audio I/O node is enabled
    bool isIOEnabled = true;
    if (isAudioInput) isIOEnabled = processor.audioInputEnabled.load();
    else if (isAudioOutput) isIOEnabled = processor.audioOutputEnabled.load();
    
    // Determine body color
    juce::Colour bodyColor = Style::colNodeBody;
    if (isIONode) {
        bodyColor = Style::colNodeHeader;
    } else if (isSimpleConnector) {
        bodyColor = juce::Colour(0xFF6B3FA0);  // Purple color for Simple Connector
    } else if (isStereoMeter) {
        bodyColor = juce::Colour(0xFF2A2A2A);  // Dark gray for Stereo Meter
    }
    
    if (node->isBypassed()) bodyColor = bodyColor.darker(0.6f);
    
    // Dim the node if it's an audio I/O node that is disabled
    if (isAudioIONode && !isIOEnabled) {
        bodyColor = bodyColor.darker(0.5f);
    }
    
    // Visual indication for pass-through mode (cyan tint)
    if (isPassThrough) {
        bodyColor = juce::Colours::cyan.darker(0.3f);
    }
    
    // Check if plugin crashed
    bool hasCrashed = false;
    if (meteringProc && meteringProc->hasPluginCrashed()) {
        hasCrashed = true;
        bodyColor = juce::Colours::darkred;
    }
    
    g.setColour(bodyColor); 
    g.fillRoundedRectangle(bounds, 4.0f); 
    g.setColour(juce::Colours::black.withAlpha(0.3f)); 
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    if (node->isBypassed()) { 
        g.setColour(Style::colBypass.withAlpha(0.5f));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
    
    auto innerBounds = bounds; 
    float footerHeight = 15.0f; 
    auto footerRect = innerBounds.removeFromBottom(footerHeight);
    g.setColour(Style::colNodeHeader); 
    g.fillRoundedRectangle(footerRect, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.9f)); 
    g.setFont(juce::Font(11.0f, juce::Font::bold)); 
    g.drawFittedText(proc->getName(), footerRect.reduced(2, 0).toNearestInt(), juce::Justification::centred, 1);
    
    // Show CRASHED indicator
    if (hasCrashed) {
        g.setColour(juce::Colours::red);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("CRASHED", bounds.reduced(5).toNearestInt(), juce::Justification::centred, false);
    }
    
    // Draw buttons for user plugins
    if (isUserPlugin) {
        // Determine number of buttons: 3 for instruments (E, M, X), 4 for effects (E, M, X, P)
        int numButtons = isEffect ? 4 : 3;
        
        for (int i = 0; i < numButtons; ++i) { 
            auto btnRect = getButtonRect(bounds, i); 
            juce::Colour btnColor;
            juce::String btnText;
            
            if (i == 0) {
                btnColor = juce::Colours::grey;
                btnText = "E";
            } else if (i == 1) {
                btnColor = node->isBypassed() ? juce::Colours::yellow : juce::Colours::yellow.darker(0.5f);
                btnText = "M";
            } else if (i == 2) {
                btnColor = juce::Colours::red;
                btnText = "X";
            } else if (i == 3) {
                // P button for effects only - cyan when active
                btnColor = isPassThrough ? juce::Colours::cyan : juce::Colours::cyan.darker(0.5f);
                btnText = "P";
            }
            
            g.setColour(btnColor.withAlpha(0.8f)); 
            g.fillRoundedRectangle(btnRect, 3.0f);
            g.setColour(juce::Colours::black); 
            g.drawRoundedRectangle(btnRect, 3.0f, 0.8f);
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(btnText, btnRect.toNearestInt(), juce::Justification::centred, false);
        } 
        
        // Draw plugin format label (top right corner)
        if (meteringProc) {
            auto* innerPlugin = meteringProc->getInnerPlugin();
            if (innerPlugin) {
                juce::String formatName = innerPlugin->getPluginDescription().pluginFormatName;
                // Shorten format names
                if (formatName.containsIgnoreCase("VST3")) formatName = "VST3";
                else if (formatName.containsIgnoreCase("VST")) formatName = "VST2";
                else if (formatName.containsIgnoreCase("AudioUnit")) formatName = "AU";
                else if (formatName.containsIgnoreCase("CLAP")) formatName = "CLAP";
                
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.setFont(juce::Font(8.0f));
                g.drawText(formatName, (int)(bounds.getRight() - 28), (int)(bounds.getY() + 2), 26, 10, 
                           juce::Justification::centredRight, false);
            }
        }
    }
    
    // Draw Simple Connector specific UI (M, X buttons + volume knob)
    if (isSimpleConnector && simpleConnector) {
        // Draw M and X buttons
        for (int i = 0; i < 2; ++i) {
            auto btnRect = getButtonRect(bounds, i);
            juce::Colour btnColor;
            juce::String btnText;
            
            if (i == 0) {
                // M button (mute)
                btnColor = simpleConnector->isMuted() ? juce::Colours::yellow : juce::Colours::yellow.darker(0.5f);
                btnText = "M";
            } else {
                // X button (delete)
                btnColor = juce::Colours::red;
                btnText = "X";
            }
            
            g.setColour(btnColor.withAlpha(0.8f));
            g.fillRoundedRectangle(btnRect, 3.0f);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(btnRect, 3.0f, 0.8f);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(btnText, btnRect.toNearestInt(), juce::Justification::centred, false);
        }
        
        // Draw volume knob
        float bodyCenterY = innerBounds.getY() + (innerBounds.getHeight() - 15.0f) / 2.0f + 15.0f;
        float bodyCenterX = innerBounds.getCentreX();
        
        float knobSize = 22.0f;
        juce::Rectangle<float> knobRect(bodyCenterX - knobSize/2, bodyCenterY - knobSize/2 - 2, knobSize, knobSize);
        
        // Knob background
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(knobRect);
        
        // Knob border
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawEllipse(knobRect, 1.5f);
        
        // Knob indicator line
        float vol = simpleConnector->getVolume();
        float angle = juce::jmap(vol, 0.0f, 1.0f, -2.4f, 2.4f);  // -135° to +135°
        float indicatorLength = knobSize * 0.35f;
        juce::Point<float> center = knobRect.getCentre();
        juce::Point<float> indicatorEnd(
            center.x + std::sin(angle) * indicatorLength,
            center.y - std::cos(angle) * indicatorLength
        );
        
        // Green indicator at unity (0.5), white otherwise
        g.setColour(std::abs(vol - 0.5f) < 0.02f ? juce::Colours::lightgreen : juce::Colours::white);
        g.drawLine(center.x, center.y, indicatorEnd.x, indicatorEnd.y, 2.0f);
        
        // Draw dB value below knob
        float db = simpleConnector->getVolumeDb();
        juce::String dbText;
        if (db <= -59.0f) dbText = "-inf";
        else if (db >= 0.0f) dbText = "+" + juce::String(db, 0) + "dB";
        else dbText = juce::String(db, 0) + "dB";
        
        g.setFont(8.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawText(dbText, (int)(bodyCenterX - 18), (int)(bodyCenterY + knobSize/2), 36, 10, 
                   juce::Justification::centred, false);
        
        // Draw mute indicator if muted
        if (simpleConnector->isMuted()) {
            g.setColour(juce::Colours::red.withAlpha(0.8f));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText("MUTED", bounds.reduced(2, 0).toNearestInt(), juce::Justification::centredTop, false);
        }
        
        // Store knob rect for mouse interaction
        node->properties.set("knobX", knobRect.getX());
        node->properties.set("knobY", knobRect.getY());
        node->properties.set("knobSize", knobSize);
    }
    
    // Draw Stereo Meter specific UI
    if (isStereoMeter && stereoMeter) {
        float footerHeight = 25.0f;  // Larger footer for clipping LED
        auto meterBounds = bounds;
        meterBounds.removeFromTop(5);  // Top padding
        auto footerArea = meterBounds.removeFromBottom(footerHeight);
        meterBounds.removeFromBottom(5);  // Bottom padding before footer
        
        // Draw meter background
        auto meterArea = meterBounds.reduced(8, 5);
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(meterArea, 3.0f);
        
        // Calculate bar dimensions
        float barWidth = (meterArea.getWidth() - 6) / 2.0f;  // 2 bars with gap
        float barHeight = meterArea.getHeight() - 4;
        float leftBarX = meterArea.getX() + 2;
        float rightBarX = leftBarX + barWidth + 2;
        float barY = meterArea.getY() + 2;
        
        // Get levels (0.0 to 1.0+)
        float leftLevel = juce::jmin(1.0f, stereoMeter->getLeftLevel());
        float rightLevel = juce::jmin(1.0f, stereoMeter->getRightLevel());
        float leftPeak = juce::jmin(1.0f, stereoMeter->getLeftPeak());
        float rightPeak = juce::jmin(1.0f, stereoMeter->getRightPeak());
        
        // Draw left bar
        float leftFillHeight = leftLevel * barHeight;
        auto leftBarRect = juce::Rectangle<float>(leftBarX, barY + barHeight - leftFillHeight, barWidth, leftFillHeight);
        
        // Gradient: green (bottom) -> yellow (middle) -> red (top)
        juce::ColourGradient leftGradient(
            juce::Colours::green, leftBarX, barY + barHeight,
            juce::Colours::red, leftBarX, barY, false);
        leftGradient.addColour(0.6, juce::Colours::yellow);
        leftGradient.addColour(0.85, juce::Colours::orange);
        g.setGradientFill(leftGradient);
        g.fillRect(leftBarRect);
        
        // Draw right bar
        float rightFillHeight = rightLevel * barHeight;
        auto rightBarRect = juce::Rectangle<float>(rightBarX, barY + barHeight - rightFillHeight, barWidth, rightFillHeight);
        
        juce::ColourGradient rightGradient(
            juce::Colours::green, rightBarX, barY + barHeight,
            juce::Colours::red, rightBarX, barY, false);
        rightGradient.addColour(0.6, juce::Colours::yellow);
        rightGradient.addColour(0.85, juce::Colours::orange);
        g.setGradientFill(rightGradient);
        g.fillRect(rightBarRect);
        
        // Draw peak hold indicators
        if (leftPeak > 0.01f) {
            float leftPeakY = barY + barHeight - (leftPeak * barHeight);
            g.setColour(juce::Colours::white);
            g.fillRect(leftBarX, leftPeakY, barWidth, 2.0f);
        }
        if (rightPeak > 0.01f) {
            float rightPeakY = barY + barHeight - (rightPeak * barHeight);
            g.setColour(juce::Colours::white);
            g.fillRect(rightBarX, rightPeakY, barWidth, 2.0f);
        }
        
        // Draw L/R labels
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(9.0f);
        g.drawText("L", (int)leftBarX, (int)(barY + barHeight + 2), (int)barWidth, 10, juce::Justification::centred);
        g.drawText("R", (int)rightBarX, (int)(barY + barHeight + 2), (int)barWidth, 10, juce::Justification::centred);
        
        // Draw footer with clipping LED and X button
        g.setColour(Style::colNodeHeader);
        g.fillRoundedRectangle(footerArea, 4.0f);
        
        // Clipping LED
        bool isClipping = stereoMeter->isClipping();
        float ledSize = 12.0f;
        juce::Rectangle<float> ledRect(footerArea.getCentreX() - ledSize/2 - 20, 
                                         footerArea.getCentreY() - ledSize/2, ledSize, ledSize);
        
        g.setColour(isClipping ? juce::Colours::red : juce::Colours::darkred.darker());
        g.fillEllipse(ledRect);
        if (isClipping) {
            g.setColour(juce::Colours::red.brighter());
            g.drawEllipse(ledRect.reduced(1), 1.5f);
        }
        
        // CLIP label
        g.setColour(isClipping ? juce::Colours::red : juce::Colours::grey);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("CLIP", (int)(ledRect.getRight() + 2), (int)ledRect.getY(), 30, (int)ledSize, 
                   juce::Justification::centredLeft);
        
        // X button (delete)
        auto xBtnRect = juce::Rectangle<float>(footerArea.getRight() - 20, footerArea.getCentreY() - 6, 14, 14);
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.fillRoundedRectangle(xBtnRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(xBtnRect, 3.0f, 0.8f);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("X", xBtnRect.toNearestInt(), juce::Justification::centred, false);
        
        // Store button rects for click handling
        node->properties.set("clipLedX", ledRect.getX());
        node->properties.set("clipLedY", ledRect.getY());
        node->properties.set("clipLedSize", ledSize);
        node->properties.set("xBtnX", xBtnRect.getX());
        node->properties.set("xBtnY", xBtnRect.getY());
        node->properties.set("xBtnSize", 14.0f);
    }
    
    // Draw ON button for Audio I/O nodes
    if (isAudioIONode) {
        auto btnRect = getIOButtonRect(bounds);
        juce::Colour btnColor = isIOEnabled ? juce::Colours::lightgreen : juce::Colours::grey;
        
        g.setColour(btnColor.withAlpha(0.9f));
        g.fillRoundedRectangle(btnRect, 3.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(btnRect, 3.0f, 0.8f);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("ON", btnRect.toNearestInt(), juce::Justification::centred, false);
    }
    
    // Draw MIDI channel indicator for instruments
    if (meteringProc) { 
        auto* innerPlugin = meteringProc->getInnerPlugin();
        if (innerPlugin && innerPlugin->getPluginDescription().isInstrument) { 
            float bodyCenterY = innerBounds.getY() + (innerBounds.getHeight() - 20.0f) / 2.0f + 20.0f;
            int mask = meteringProc->getMidiChannelMask(); 
            int enabledCount = 0; 
            for (int i = 0; i < 16; ++i) if ((mask >> i) & 1) enabledCount++;
            juce::String text = (enabledCount == 16) ? "ALL" : juce::String(enabledCount); 
            juce::Rectangle<float> chRect(innerBounds.getCentreX() - 12, bodyCenterY - 7, 24, 14); 
            g.setColour(juce::Colours::orange.withAlpha(0.7f)); 
            g.fillRoundedRectangle(chRect, 3.0f); 
            g.setColour(juce::Colours::black); 
            g.drawRoundedRectangle(chRect, 3.0f, 1.0f); 
            g.setFont(10.0f); 
            g.setColour(juce::Colours::black); 
            g.drawText(text, chRect.toNearestInt(), juce::Justification::centred, false);
        } 
    }
    
    // Draw input pins
    int numIn = proc->getTotalNumInputChannels(); 
    if (proc->acceptsMidi()) numIn++; 
    for (int i = 0; i < numIn; ++i) { 
        bool isMidi = proc->acceptsMidi() && (i == proc->getTotalNumInputChannels()); 
        PinID pinId = { node->nodeID, isMidi ? 0 : i, true, isMidi }; 
        auto pinPos = getPinPos(node, pinId); 
        auto pinColor = getPinColor(pinId, node);
        bool isHovered = (highlightPin == pinId);
        
        bool hasSignal = false;
        if (!node->isBypassed()) {
            if (meteringProc) {
                if (isMidi) hasSignal = meteringProc->isMidiInActive();
                else hasSignal = (meteringProc->getInputRms(i) > 0.001f);
            } else if (node == processor.audioOutputNode.get()) {
                if (i < 2) hasSignal = (processor.mainOutputRms[i].load() > 0.001f);
            } else if (node == processor.midiOutputNode.get()) {
                hasSignal = processor.mainMidiOutFlash.load();
            }
        }
        if (hasSignal) pinColor = pinColor.brighter(0.5f);
        
        drawPin(g, pinPos, pinColor, isHovered, highlightPin == pinId);
    }
    
    // Draw output pins
    int numOut = proc->getTotalNumOutputChannels(); 
    if (proc->producesMidi()) numOut++; 
    for (int i = 0; i < numOut; ++i) { 
        bool isMidi = proc->producesMidi() && (i == proc->getTotalNumOutputChannels()); 
        PinID pinId = { node->nodeID, isMidi ? 0 : i, false, isMidi }; 
        auto pinPos = getPinPos(node, pinId); 
        auto pinColor = getPinColor(pinId, node); 
        bool isHovered = (highlightPin == pinId);
        
        bool hasSignal = false;
        if (!node->isBypassed()) {
            if (meteringProc) {
                if (isMidi) hasSignal = meteringProc->isMidiOutActive();
                else hasSignal = (meteringProc->getOutputRms(i) > 0.001f);
            } else if (node == processor.audioInputNode.get()) {
                if (i < 2) hasSignal = (processor.mainInputRms[i].load() > 0.001f);
            } else if (node == processor.midiInputNode.get()) {
                hasSignal = processor.mainMidiInFlash.load();
            }
        }
        if (hasSignal) pinColor = pinColor.brighter(0.5f);
        
        drawPin(g, pinPos, pinColor, isHovered, highlightPin == pinId);
    }
}

juce::Rectangle<float> GraphCanvas::getButtonRect(juce::Rectangle<float> nodeBounds, int index) { 
    float startX = nodeBounds.getX() + 5.0f;
    float startY = nodeBounds.getY() + 5.0f;
    return { startX + (index * 15.0f), startY, Style::btnSize, Style::btnSize };
}

juce::Rectangle<float> GraphCanvas::getIOButtonRect(juce::Rectangle<float> nodeBounds) {
    // Position ON button in the top-left corner of IO nodes
    float startX = nodeBounds.getX() + 5.0f;
    float startY = nodeBounds.getY() + 5.0f;
    return { startX, startY, 22.0f, Style::btnSize };
}

void GraphCanvas::mouseDown(const juce::MouseEvent& e) {
    juce::Point<float> pos = e.position.toFloat();
    
    for (int i = processor.mainGraph->getNumNodes() - 1; i >= 0; --i) {
        auto nodePtr = processor.mainGraph->getNode(i);
        auto* node = nodePtr.get();
        
        if (!shouldShowNode(node)) continue;
        
        if (getNodeBounds(node).contains(pos)) {
            // Check if this is an Audio I/O node
            bool isAudioInput = (node == processor.audioInputNode.get());
            bool isAudioOutput = (node == processor.audioOutputNode.get());
            bool isAudioIONode = isAudioInput || isAudioOutput;
            bool isMidiIO = (node == processor.midiInputNode.get() || node == processor.midiOutputNode.get());
            bool isIONode = isAudioIONode || isMidiIO;
            
            // Check for ON button click on Audio I/O nodes
            if (isAudioIONode) {
                auto bounds = getNodeBounds(node);
                if (getIOButtonRect(bounds).contains(pos)) {
                    handleIOButtonClick(node);
                    return;
                }
            }
            
            // Check for Simple Connector interactions
            if (auto* connector = dynamic_cast<SimpleConnectorProcessor*>(node->getProcessor())) {
                auto bounds = getNodeBounds(node);
                
                // Check M button (mute) - index 0
                if (getButtonRect(bounds, 0).contains(pos)) {
                    connector->toggleMute();
                    repaint();
                    return;
                }
                
                // Check X button (delete) - index 1
                if (getButtonRect(bounds, 1).contains(pos)) {
                    processor.mainGraph->removeNode(node->nodeID);
                    repaint();
                    return;
                }
                
                // Check knob interaction
                float knobX = (float)node->properties.getWithDefault("knobX", 0.0f);
                float knobY = (float)node->properties.getWithDefault("knobY", 0.0f);
                float knobSize = (float)node->properties.getWithDefault("knobSize", 22.0f);
                juce::Rectangle<float> knobRect(knobX, knobY, knobSize, knobSize);
                
                if (knobRect.contains(pos)) {
                    draggingKnobNodeID = node->nodeID;
                    knobDragStartY = pos.y;
                    knobDragStartValue = connector->getVolume();
                    return;
                }
            }
            
            // Check for Stereo Meter interactions
            if (auto* meter = dynamic_cast<StereoMeterProcessor*>(node->getProcessor())) {
                // Check X button (delete)
                float xBtnX = (float)node->properties.getWithDefault("xBtnX", 0.0f);
                float xBtnY = (float)node->properties.getWithDefault("xBtnY", 0.0f);
                float xBtnSize = (float)node->properties.getWithDefault("xBtnSize", 14.0f);
                juce::Rectangle<float> xBtnRect(xBtnX, xBtnY, xBtnSize, xBtnSize);
                
                if (xBtnRect.contains(pos)) {
                    processor.mainGraph->removeNode(node->nodeID);
                    repaint();
                    return;
                }
                
                // Check CLIP LED click (reset clipping)
                float clipLedX = (float)node->properties.getWithDefault("clipLedX", 0.0f);
                float clipLedY = (float)node->properties.getWithDefault("clipLedY", 0.0f);
                float clipLedSize = (float)node->properties.getWithDefault("clipLedSize", 12.0f);
                juce::Rectangle<float> clipLedRect(clipLedX, clipLedY, clipLedSize + 35, clipLedSize);  // Include CLIP text
                
                if (clipLedRect.contains(pos)) {
                    meter->resetClipping();
                    repaint();
                    return;
                }
            }
            
            // Only check for button clicks on user plugins, not I/O nodes
            if (!isIONode && (dynamic_cast<juce::AudioPluginInstance*>(node->getProcessor()) || dynamic_cast<MeteringProcessor*>(node->getProcessor()))) {
                auto bounds = getNodeBounds(node);
                
                // Determine number of buttons based on plugin type
                bool isEffect = false;
                if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                    if (auto* innerPlugin = mp->getInnerPlugin()) {
                        isEffect = !innerPlugin->getPluginDescription().isInstrument;
                    }
                }
                int numButtons = isEffect ? 4 : 3;
                
                for(int b = 0; b < numButtons; ++b) { 
                    if (getButtonRect(bounds, b).contains(pos)) { 
                        handleButtonClick(node, b);
                        return;
                    } 
                }
            }
            
            if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                if (mp->getInnerPlugin()->getPluginDescription().isInstrument) {
                    auto fullBounds = getNodeBounds(node);
                    float footerHeight = 15.0f; 
                    auto drawBounds = fullBounds; 
                    drawBounds.removeFromBottom(footerHeight);
                    float bodyCenterY = drawBounds.getY() + (drawBounds.getHeight() - 20.0f) / 2.0f + 20.0f;
                    juce::Rectangle<float> chRect(drawBounds.getCentreX() - 12, bodyCenterY - 7, 24, 14);
                    
                    if (chRect.contains(pos)) {
                        auto selector = std::make_unique<MidiChannelSelector>(mp, [this](){ repaint(); });
                        auto* editor = findParentComponentOfClass<SubterraneumAudioProcessorEditor>();
                        juce::CallOutBox::launchAsynchronously(std::move(selector), chRect.toNearestInt(), editor);
                        return;
                    }
                }
            }
            break;
        }
    }
    
    if (e.mods.isRightButtonDown()) { 
        PinID pin = findPinAt(pos);
        if (pin.isValid()) { 
            showPinInfo(pin, e.getScreenPosition().toFloat());
            return;
        } 
        auto conn = getConnectionAt(pos);
        if (conn.source.nodeID.uid != 0) { 
            showWireMenu(conn, e.getScreenPosition().toFloat()); 
            return;
        } 
        showPluginMenu(); 
        return;
    }
    
    PinID pin = findPinAt(pos);
    if (pin.isValid()) { 
        dragCable.active = true; 
        dragCable.sourcePin = pin; 
        dragCable.currentDragPos = pos;
        return; 
    }
    
    if (auto* node = findNodeAt(pos)) { 
        draggingNodeID = node->nodeID;
        nodeDragOffset = getNodeBounds(node).getPosition() - pos;
    }
}

void GraphCanvas::mouseMove(const juce::MouseEvent& e) { 
    auto conn = getConnectionAt(e.position.toFloat());
    if (conn != hoveredConnection) { 
        hoveredConnection = conn; 
        repaint();
    } 
}

void GraphCanvas::handleButtonClick(juce::AudioProcessorGraph::Node* node, int buttonIndex) { 
    if (buttonIndex == 0) {
        openPluginWindow(node);
    }
    else if (buttonIndex == 1) { 
        processor.toggleBypass(node->nodeID); 
        repaint();
        updateParentSelector();
    } 
    else if (buttonIndex == 2) { 
        activePluginWindows.erase(node->nodeID);
        processor.removeNode(node->nodeID); 
        repaint(); 
        updateParentSelector();
    }
    else if (buttonIndex == 3) {
        // P button - toggle pass-through (effects only)
        if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            mp->setPassThrough(!mp->isPassThrough());
            repaint();
        }
    }
}

void GraphCanvas::handleIOButtonClick(juce::AudioProcessorGraph::Node* node) {
    if (node == processor.audioInputNode.get()) {
        processor.audioInputEnabled.store(!processor.audioInputEnabled.load());
    } else if (node == processor.audioOutputNode.get()) {
        processor.audioOutputEnabled.store(!processor.audioOutputEnabled.load());
    }
    repaint();
}

void GraphCanvas::openPluginWindow(juce::AudioProcessorGraph::Node* node) { 
    if (activePluginWindows.count(node->nodeID)) { 
        auto* win = activePluginWindows[node->nodeID].get();
        win->setVisible(true); 
        win->toFront(true); 
        return;
    }
    
    juce::AudioPluginInstance* plugin = nullptr; 
    if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) 
        plugin = mp->getInnerPlugin();
    else 
        plugin = dynamic_cast<juce::AudioPluginInstance*>(node->getProcessor());
    
    if (plugin) { 
        juce::AudioProcessorEditor* editor = nullptr;
        
        try {
            editor = plugin->createEditor();
        } catch (const std::exception& e) {
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                "Plugin Error", "Failed to open plugin editor: " + juce::String(e.what()));
            return;
        } catch (...) {
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                "Plugin Error", "Failed to open plugin editor. This may be a VST2 compatibility issue.");
            return;
        }
        
        if (editor) { 
            auto* win = new PluginWindow(plugin->getName(), juce::Colours::black, juce::DocumentWindow::allButtons);
            win->setContentOwned(editor, true); 
            win->setResizable(true, true);
            win->setVisible(true); 
            activePluginWindows[node->nodeID].reset(win); 
        }
    } 
}

void GraphCanvas::mouseDrag(const juce::MouseEvent& e) { 
    juce::Point<float> pos = e.position.toFloat();
    
    // Handle knob drag for Simple Connector
    if (draggingKnobNodeID.uid != 0) {
        if (auto* nodePtr = processor.mainGraph->getNodeForId(draggingKnobNodeID)) {
            if (auto* connector = dynamic_cast<SimpleConnectorProcessor*>(nodePtr->getProcessor())) {
                float deltaY = knobDragStartY - pos.y;  // Invert: drag up = increase
                float sensitivity = 0.005f;
                float newValue = juce::jlimit(0.0f, 1.0f, knobDragStartValue + deltaY * sensitivity);
                connector->setVolume(newValue);
                repaint();
            }
        }
        return;
    }
    
    if (dragCable.active) { 
        dragCable.currentDragPos = pos;
        highlightPin = PinID();
        PinID hovered = findPinAt(pos); 
        if (hovered.isValid() && canConnect(dragCable.sourcePin, hovered)) 
            highlightPin = hovered; 
        repaint();
    } else if (draggingNodeID.uid != 0) { 
        if (auto* node = processor.mainGraph->getNodeForId(draggingNodeID)) { 
            node->properties.set("x", pos.x + nodeDragOffset.x);
            node->properties.set("y", pos.y + nodeDragOffset.y); 
            repaint(); 
        } 
    } 
}

void GraphCanvas::mouseUp(const juce::MouseEvent& e) { 
    // Reset knob drag state
    draggingKnobNodeID = juce::AudioProcessorGraph::NodeID();
    
    if (dragCable.active) { 
        PinID endPin = findPinAt(e.position.toFloat());
        if (endPin.isValid()) createConnection(dragCable.sourcePin, endPin); 
        dragCable.active = false; 
        highlightPin = PinID(); 
        repaint(); 
    } 
    draggingNodeID = {};
}

void GraphCanvas::mouseDoubleClick(const juce::MouseEvent& e) { 
    if (e.mods.isRightButtonDown()) deleteConnectionAt(e.position.toFloat());
}

bool GraphCanvas::canConnect(PinID start, PinID end) { 
    if (start.nodeID == end.nodeID) return false;
    if (start.isInput == end.isInput) return false; 
    if (start.isMidi != end.isMidi) return false; 
    return true;
}

void GraphCanvas::createConnection(PinID start, PinID end) { 
    if (!canConnect(start, end)) return; 
    PinID source = start.isInput ? end : start;
    PinID dest = start.isInput ? start : end;
    
    if (source.isMidi) 
        processor.mainGraph->addConnection({ { source.nodeID, juce::AudioProcessorGraph::midiChannelIndex }, { dest.nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    else 
        processor.mainGraph->addConnection({ { source.nodeID, source.pinIndex }, { dest.nodeID, dest.pinIndex } });
}

juce::AudioProcessorGraph::Connection GraphCanvas::getConnectionAt(juce::Point<float> pos) { 
    const float hitTolerance = 5.0f;
    auto connections = processor.mainGraph->getConnections();
    for (const auto& connection : connections) { 
        auto* src = processor.mainGraph->getNodeForId(connection.source.nodeID);
        auto* dst = processor.mainGraph->getNodeForId(connection.destination.nodeID);
        if (src && dst && shouldShowNode(src) && shouldShowNode(dst)) {
            bool isMidi = connection.source.isMIDI();
            PinID p1 = { src->nodeID, isMidi ? 0 : connection.source.channelIndex, false, isMidi }; 
            PinID p2 = { dst->nodeID, isMidi ? 0 : connection.destination.channelIndex, true, isMidi };
            auto start = getPinPos(src, p1); 
            auto end = getPinPos(dst, p2); 
            juce::Path p; 
            p.startNewSubPath(start);
            p.cubicTo(start.x, start.y + 50, end.x, end.y - 50, end.x, end.y);
            if (p.getBounds().expanded(10).contains(pos)) { 
                juce::Point<float> nearest;
                p.getNearestPoint(pos, nearest);
                if (pos.getDistanceFrom(nearest) < hitTolerance) return connection; 
            } 
        } 
    } 
    return { {juce::AudioProcessorGraph::NodeID(), 0}, {juce::AudioProcessorGraph::NodeID(), 0} };
}

void GraphCanvas::deleteConnectionAt(juce::Point<float> pos) { 
    auto conn = getConnectionAt(pos);
    if (conn.source.nodeID.uid != 0) { 
        processor.mainGraph->removeConnection(conn); 
        repaint();
    } 
}

void GraphCanvas::showPinInfo(const PinID& pin, const juce::Point<float>& screenPos) { 
    auto* node = processor.mainGraph->getNodeForId(pin.nodeID);
    if (!node) return;
    
    juce::String text;
    bool isActive = !node->isBypassed();
    
    // Check if this is an I/O node - show real channel name
    bool isAudioInput = (node == processor.audioInputNode.get());
    bool isAudioOutput = (node == processor.audioOutputNode.get());
    bool isMidiInput = (node == processor.midiInputNode.get());
    bool isMidiOutput = (node == processor.midiOutputNode.get());
    
    if (pin.isMidi) {
        if (isMidiInput) text = "MIDI Input";
        else if (isMidiOutput) text = "MIDI Output";
        else text = pin.isInput ? "MIDI In" : "MIDI Out";
    } else if (isAudioInput && !pin.isInput) {
        // Output pins of audio input node = actual input channels
        text = processor.getDeviceInputChannelName(pin.pinIndex);
    } else if (isAudioOutput && pin.isInput) {
        // Input pins of audio output node = actual output channels
        text = processor.getDeviceOutputChannelName(pin.pinIndex);
    } else {
        // Regular plugin pins
        juce::String side = pin.isInput ? "Input" : "Output";
        juce::String type = (pin.isInput && pin.pinIndex >= 2) ? "Sidechain" : "Audio";
        juce::String ch = " Ch " + juce::String(pin.pinIndex + 1);
        text = side + " " + type + ch;
    }
    
    auto* content = new StatusToolTip(text, isActive);
    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(content), juce::Rectangle<int>((int)screenPos.x, (int)screenPos.y, 1, 1), nullptr); 
}

void GraphCanvas::showWireMenu(const juce::AudioProcessorGraph::Connection& conn, const juce::Point<float>& screenPos) { 
    juce::String text = conn.source.isMIDI() ? "MIDI Connection" : "Audio Connection"; 
    bool isActive = false; 
    auto* src = processor.mainGraph->getNodeForId(conn.source.nodeID); 
    auto* dst = processor.mainGraph->getNodeForId(conn.destination.nodeID);
    if (src && dst) 
        isActive = (!src->isBypassed() && !dst->isBypassed());
    
    auto* content = new StatusToolTip(text, isActive, [this, conn]() { 
        processor.mainGraph->removeConnection(conn); 
        repaint(); 
    });
    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(content), juce::Rectangle<int>((int)screenPos.x, (int)screenPos.y, 1, 1), nullptr);
}

void GraphCanvas::showPluginMenu() { 
    juce::PopupMenu m; 
    m.addSectionHeader("Add Module");
    
    if (processor.knownPluginList.getNumTypes() == 0) {
        m.addItem(1, "No plugins found. Click to open Manager...", true, false);
    } else { 
        auto types = processor.knownPluginList.getTypes();
        const int idBase = 100;
        
        std::map<juce::String, std::vector<int>> instrumentsByVendor;
        std::map<juce::String, std::vector<int>> effectsByVendor;
        
        for (int i = 0; i < types.size(); ++i) {
            juce::String vendor = types[i].manufacturerName.isEmpty() ? "Unknown Vendor" : types[i].manufacturerName;
            if (types[i].isInstrument) {
                instrumentsByVendor[vendor].push_back(i);
            } else {
                effectsByVendor[vendor].push_back(i);
            }
        }
        
        // INSTRUMENTS submenu
        juce::PopupMenu instrumentsMenu;
        if (processor.sortPluginsByVendor) {
            for (auto& pair : instrumentsByVendor) {
                juce::PopupMenu vendorMenu;
                for (int idx : pair.second)
                    vendorMenu.addItem(idBase + idx, types[idx].name);
                instrumentsMenu.addSubMenu(pair.first, vendorMenu);
            }
        } else {
            std::vector<int> allInstruments;
            for (auto& pair : instrumentsByVendor)
                for (int idx : pair.second)
                    allInstruments.push_back(idx);
            std::sort(allInstruments.begin(), allInstruments.end(), [&](int a, int b) { 
                return types[a].name.compareIgnoreCase(types[b].name) < 0; 
            });
            for (int idx : allInstruments)
                instrumentsMenu.addItem(idBase + idx, types[idx].name + " (" + types[idx].manufacturerName + ")");
        }
        if (instrumentsByVendor.empty())
            instrumentsMenu.addItem(-1, "(No instruments found)", false);
        m.addSubMenu("INSTRUMENTS", instrumentsMenu);
        
        // EFFECTS submenu
        juce::PopupMenu effectsMenu;
        if (processor.sortPluginsByVendor) {
            for (auto& pair : effectsByVendor) {
                juce::PopupMenu vendorMenu;
                for (int idx : pair.second)
                    vendorMenu.addItem(idBase + idx, types[idx].name);
                effectsMenu.addSubMenu(pair.first, vendorMenu);
            }
        } else {
            std::vector<int> allEffects;
            for (auto& pair : effectsByVendor)
                for (int idx : pair.second)
                    allEffects.push_back(idx);
            std::sort(allEffects.begin(), allEffects.end(), [&](int a, int b) { 
                return types[a].name.compareIgnoreCase(types[b].name) < 0; 
            });
            for (int idx : allEffects)
                effectsMenu.addItem(idBase + idx, types[idx].name + " (" + types[idx].manufacturerName + ")");
        }
        if (effectsByVendor.empty())
            effectsMenu.addItem(-1, "(No effects found)", false);
        m.addSubMenu("EFFECTS", effectsMenu);
        
        // SYSTEM TOOLS submenu
        juce::PopupMenu systemMenu;
        systemMenu.addItem(2001, "Simple Connector (Bus)");
        systemMenu.addItem(2002, "Stereo Meter");
        m.addSubMenu("SYSTEM TOOLS", systemMenu);
    } 
    
    m.addSeparator();
    m.addItem(1004, "Open Plugin Manager"); 
    
    auto* editor = findParentComponentOfClass<SubterraneumAudioProcessorEditor>();
    m.showMenuAsync(juce::PopupMenu::Options(), [this, editor](int result) { 
        if (result == 1004 || result == 1) { 
            if (editor) editor->tabs.setCurrentTabIndex(3); 
        }
        else if (result == 2001) {
            // Add Simple Connector
            auto connector = std::make_unique<SimpleConnectorProcessor>();
            auto* node = processor.mainGraph->addNode(std::move(connector)).get();
            if (node) {
                auto center = getLocalBounds().getCentre().toFloat();
                node->properties.set("x", center.x);
                node->properties.set("y", center.y);
                repaint();
            }
        }
        else if (result == 2002) {
            // Add Stereo Meter
            auto meter = std::make_unique<StereoMeterProcessor>();
            auto* node = processor.mainGraph->addNode(std::move(meter)).get();
            if (node) {
                auto center = getLocalBounds().getCentre().toFloat();
                node->properties.set("x", center.x);
                node->properties.set("y", center.y);
                repaint();
            }
        }
        else if (result >= 100) { 
            int index = result - 100; 
            const auto& desc = processor.knownPluginList.getTypes()[index]; 
            
            if (desc.isInstrument) { 
                int instCount = 0; 
                for (auto* node : processor.mainGraph->getNodes()) { 
                    if (auto* plugin = dynamic_cast<juce::AudioPluginInstance*>(node->getProcessor())) { 
                        if (plugin->getPluginDescription().isInstrument) instCount++; 
                    } else if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) { 
                        if (mp->getInnerPlugin()->getPluginDescription().isInstrument) instCount++; 
                    } 
                } 
                if (instCount >= 16) { 
                    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Limit Reached", "You have reached the maximum limit of 16 VST Instruments.");
                    return; 
                } 
            } 
            
            juce::String msg;
            std::unique_ptr<juce::AudioPluginInstance> newPlugin;
            
            try {
                newPlugin = processor.formatManager.createPluginInstance(desc, processor.getSampleRate(), processor.getBlockSize(), msg);
            } catch (const std::exception& e) {
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                    "Plugin Load Error", "Failed to load plugin: " + juce::String(e.what()));
                return;
            } catch (...) {
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                    "Plugin Load Error", "Failed to load plugin (unknown error). This may be a VST2 compatibility issue.");
                return;
            }
            
            if (newPlugin) { 
                auto* node = processor.mainGraph->addNode(std::make_unique<MeteringProcessor>(std::move(newPlugin))).get();
                if (node) { 
                    auto center = getLocalBounds().getCentre().toFloat();
                    node->properties.set("x", center.x); 
                    node->properties.set("y", center.y); 
                    repaint(); 
                    updateParentSelector(); 
                } 
            } else if (msg.isNotEmpty()) {
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                    "Plugin Load Error", msg);
            }
        } 
    });
}

void GraphCanvas::scanPlugins() {}

void GraphCanvas::verifyPositions() { 
    for (auto* node : processor.mainGraph->getNodes()) {
        if (shouldShowNode(node))
            getNodeBounds(node);
    }
}

void GraphCanvas::resized() { repaint(); }

void GraphCanvas::drawPin(juce::Graphics& g, juce::Point<float> pos, juce::Colour color, bool isHovered, bool isHighlighted) { 
    float r = Style::pinSize / 2.0f;
    g.setColour(isHighlighted ? juce::Colours::yellow : color); 
    g.fillEllipse(pos.x - r, pos.y - r, r * 2, r * 2);
    if (isHovered || isHighlighted) { 
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawEllipse(pos.x - r, pos.y - r, r * 2, r * 2, 2.0f);
    } 
}

juce::Rectangle<float> GraphCanvas::getNodeBounds(juce::AudioProcessorGraph::Node* node) { 
    float w = Style::minNodeWidth; 
    auto* proc = node->getProcessor();
    int pins = std::max(proc->getTotalNumInputChannels() + (proc->acceptsMidi()?1:0), proc->getTotalNumOutputChannels() + (proc->producesMidi()?1:0)); 
    float minW = (pins + 1) * Style::pinSpacing;
    if (minW > w) w = minW; 
    float x = (float)node->properties.getWithDefault("x", getWidth() / 2.0f - w / 2.0f);
    float y = (float)node->properties.getWithDefault("y", getHeight() / 2.0f - Style::nodeHeight / 2.0f);
    if (!node->properties.contains("x")) { 
        node->properties.set("x", x); 
        node->properties.set("y", y);
    }
    
    // StereoMeter is 3x height
    float h = Style::nodeHeight;
    if (dynamic_cast<StereoMeterProcessor*>(proc) != nullptr) {
        h = Style::nodeHeight * 3.0f;
    }
    
    return { x, y, w, h };
}

juce::Point<float> GraphCanvas::getPinPos(juce::AudioProcessorGraph::Node* node, const PinID& pinId) { 
    auto bounds = getNodeBounds(node);
    auto* proc = node->getProcessor();
    int numIn = proc->getTotalNumInputChannels(); 
    int numOut = proc->getTotalNumOutputChannels(); 
    bool hasMidiIn = proc->acceptsMidi(); 
    bool hasMidiOut = proc->producesMidi();
    int index = pinId.pinIndex;
    
    int totalPins = 0; 
    if(pinId.isInput) totalPins = numIn + (hasMidiIn ? 1 : 0);
    else totalPins = numOut + (hasMidiOut ? 1 : 0); 
    if(pinId.isMidi) index = totalPins - 1;
    
    float spacing = bounds.getWidth() / (float)(totalPins + 1); 
    float px = bounds.getX() + spacing * (index + 1);
    float py = pinId.isInput ? (bounds.getY() - Style::hookLength) : (bounds.getBottom() + Style::hookLength); 
    return { px, py };
}

juce::Colour GraphCanvas::getPinColor(const PinID& pinId, juce::AudioProcessorGraph::Node* node) {
    if (pinId.isMidi) return Style::colPinMidi;
    
    // Only inputs can be sidechain (channels 2+) for non-IO nodes
    bool isIONode = (node == processor.audioInputNode.get() || node == processor.audioOutputNode.get());
    if (!isIONode && pinId.isInput && pinId.pinIndex >= 2) {
        return Style::colPinSidechain;
    }
    
    return Style::colPinAudio;
}

GraphCanvas::PinID GraphCanvas::findPinAt(juce::Point<float> pos) { 
    if (!processor.mainGraph) return {};
    for (auto* node : processor.mainGraph->getNodes()) {
        if (!shouldShowNode(node)) continue;
        
        auto* proc = node->getProcessor();
        int numIn = proc->getTotalNumInputChannels(); 
        if (proc->acceptsMidi()) numIn++;
        for (int i=0; i<numIn; ++i) { 
            bool isMidi = proc->acceptsMidi() && (i == proc->getTotalNumInputChannels());
            PinID p = { node->nodeID, isMidi ? 0 : i, true, isMidi }; 
            if (pos.getDistanceFrom(getPinPos(node, p)) <= Style::pinSize) return p;
        } 
        int numOut = proc->getTotalNumOutputChannels(); 
        if (proc->producesMidi()) numOut++;
        for (int i=0; i<numOut; ++i) { 
            bool isMidi = proc->producesMidi() && (i == proc->getTotalNumOutputChannels());
            PinID p = { node->nodeID, isMidi ? 0 : i, false, isMidi }; 
            if (pos.getDistanceFrom(getPinPos(node, p)) <= Style::pinSize) return p;
        } 
    } 
    return {};
}

juce::AudioProcessorGraph::Node* GraphCanvas::findNodeAt(juce::Point<float> pos) { 
    for (auto* node : processor.mainGraph->getNodes()) {
        if (shouldShowNode(node) && getNodeBounds(node).contains(pos)) 
            return node;
    }
    return nullptr;
}
