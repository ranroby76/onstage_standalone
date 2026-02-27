
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!
// BUG FIX: Suspend audio processing during mode switching to prevent crashes and silence
// NEW: MIDI CC control for instrument selection (CC 20-51 = Instruments 1-32)
// FIX: Instrument list refreshes when workspace switches via MIDI CC

#include "InstrumentSelector.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

InstrumentSelector::InstrumentSelector(SubterraneumAudioProcessor& p) : processor(p) { 
    titleLabel.setText("Instruments Selector", juce::dontSendNotification); 
    titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold))); 
    titleLabel.setJustificationType(juce::Justification::centredLeft); 
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white); 
    addAndMakeVisible(titleLabel);
    
    multiModeBtn.setButtonText("Multi-Mode");
    multiModeBtn.setClickingTogglesState(true);
    multiModeBtn.setColour(juce::ToggleButton::tickColourId, juce::Colours::green);
    multiModeBtn.addListener(this);
    multiModeBtn.setToggleState(processor.instrumentSelectorMultiMode, juce::dontSendNotification);
    addAndMakeVisible(multiModeBtn);
    
    // FIX: Increased from 16 to 32 instruments (2 rows x 16 columns)
    for (int i=0; i<32; ++i) { 
        auto* btn = instButtons.add(new juce::TextButton(juce::String(i+1))); 
        btn->setClickingTogglesState(false);
        
        // FIX: Apply table-style LookAndFeel (no rounding, connected buttons)
        btn->setLookAndFeel(&tableLookAndFeel);
        
        // FIX: Dark red with white text for ON state (active instrument)
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8B0000)); // Dark red
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        
        // FIX: Light silver gradient for OFF state (inactive instrument)
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xffC0C0C0)); // Light silver
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff2d2d2d)); // Dark grey text
        
        // MIDI CC mapping: CC 20-51 for instruments 1-32
        int midiCC = SubterraneumAudioProcessor::midiCCInstrumentBase + i;
        juce::String tooltip = "Instrument " + juce::String(i + 1) + 
                               " (MIDI CC " + juce::String(midiCC) + ")";
        btn->setTooltip(tooltip);
        
        btn->addListener(this); 
        btn->addMouseListener(this, false); 
        addAndMakeVisible(btn); 
    }
    
    // CPU OPTIMIZATION: Increased from 100ms to 200ms - bypass state changes are infrequent
    startTimer(200);
}

InstrumentSelector::~InstrumentSelector() { 
    // Clean up LookAndFeel before buttons are destroyed
    for (auto* btn : instButtons) {
        btn->setLookAndFeel(nullptr);
    }
    stopTimer(); 
}

void InstrumentSelector::updateList() { 
    nodeIDs.clear(); 
    if (!processor.mainGraph) return; 
    
    std::vector<juce::AudioProcessorGraph::Node*> instrumentNodes;
    for (auto* node : processor.mainGraph->getNodes()) { 
        auto* proc = node->getProcessor();
        if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) { 
            // =========================================================================
            // CRITICAL FIX: Use isInstrument() instead of getPluginDescription()
            // getPluginDescription() freezes some plugins when called!
            // =========================================================================
            if (mp->getInnerPlugin() && mp->isInstrument()) 
                instrumentNodes.push_back(node);
        }
        // NOTE: Removed raw AudioPluginInstance case - all plugins should be wrapped in MeteringProcessor
    } 
    
    bool showModes = (instrumentNodes.size() > 1); 
    multiModeBtn.setVisible(showModes);
    
    // FIX: Updated from 16 to 32 instruments
    for (int i = 0; i < 32; ++i) { 
        auto* btn = instButtons[i];
        if (i < (int)instrumentNodes.size()) { 
            auto* node = instrumentNodes[i];
            nodeIDs.push_back(node->nodeID); 
            // FREEZE FIX: Use cached name from MeteringProcessor
            juce::String name = "Unknown";
            if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                name = mp->getCachedName();
            }
            btn->setButtonText("[" + juce::String(i+1) + "] " + name); 
            btn->setEnabled(true); 
            btn->setClickingTogglesState(false); 
            btn->setToggleState(!node->isBypassed(), juce::dontSendNotification); 
            btn->setAlpha(1.0f);
        } else { 
            btn->setButtonText("Empty"); 
            btn->setEnabled(false); 
            btn->setToggleState(false, juce::dontSendNotification);
            btn->setAlpha(0.3f);
        } 
    }
    
    // FIX #2: Apply single-mode rules on app load
    // If we're in single mode and multiple instruments exist, ensure only one is active
    if (!processor.instrumentSelectorMultiMode && instrumentNodes.size() > 1) {
        // Find first non-bypassed instrument, or make first one active
        bool foundActive = false;
        int activeIndex = 0;
        
        for (int i = 0; i < (int)instrumentNodes.size(); ++i) {
            if (!instrumentNodes[i]->isBypassed()) {
                foundActive = true;
                activeIndex = i;
                break;
            }
        }
        
        // Apply single-mode: only one active, rest bypassed
        for (int i = 0; i < (int)instrumentNodes.size(); ++i) {
            bool shouldBeActive = (i == activeIndex);
            instrumentNodes[i]->setBypassed(!shouldBeActive);
            if (i < instButtons.size()) {
                instButtons[i]->setToggleState(shouldBeActive, juce::dontSendNotification);
            }
        }
    }
}

void InstrumentSelector::paint(juce::Graphics& g) { 
    g.fillAll(juce::Colours::black.withAlpha(0.5f)); 
    g.setColour(juce::Colours::white.withAlpha(0.1f)); 
    g.drawRect(getLocalBounds());
}

void InstrumentSelector::resized() { 
    auto area = getLocalBounds().reduced(5);
    auto topRow = area.removeFromTop(25); 
    titleLabel.setBounds(topRow.removeFromLeft(120)); 
    multiModeBtn.setBounds(topRow.removeFromLeft(80)); 
    area.removeFromTop(5);
    
    // FIX: 2 rows x 16 columns layout for 32 instruments
    float btnW = (float)area.getWidth() / 16.0f;
    float rowHeight = (float)area.getHeight() / 2.0f;
    
    // Row 1: Instruments 1-16
    for (int i = 0; i < 16; ++i) { 
        if (instButtons[i]) { 
            instButtons[i]->setBounds(area.getX() + (int)(i * btnW), area.getY(), (int)btnW - 2, (int)rowHeight - 2);
        } 
    }
    
    // Row 2: Instruments 17-32
    for (int i = 16; i < 32; ++i) { 
        if (instButtons[i]) { 
            int col = i - 16;
            instButtons[i]->setBounds(area.getX() + (int)(col * btnW), area.getY() + (int)rowHeight, (int)btnW - 2, (int)rowHeight - 2);
        } 
    }
}

void InstrumentSelector::mouseDown(const juce::MouseEvent& e) { 
    if (e.mods.isRightButtonDown()) { 
        for (int i = 0; i < instButtons.size(); ++i) { 
            if (e.eventComponent == instButtons[i] || e.originalComponent == instButtons[i]) { 
                int midiCC = SubterraneumAudioProcessor::midiCCInstrumentBase + i;
                juce::String message = 
                    "MIDI CC Control for Instrument " + juce::String(i + 1) + "\n\n"
                    "MIDI CC " + juce::String(midiCC) + "  (value > 63 = select)\n\n"
                    "Full Instrument CC Mapping:\n"
                    "  Instruments 1-32  =  CC " + 
                    juce::String(SubterraneumAudioProcessor::midiCCInstrumentBase) + "-" +
                    juce::String(SubterraneumAudioProcessor::midiCCInstrumentBase + 31) + "\n\n"
                    "Send CC value > 63 to select/toggle this instrument.\n"
                    "Works on any MIDI channel.\n\n"
                    "Perfect for instant switching with MIDI controllers!";
                
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Instrument " + juce::String(i + 1) + " - MIDI CC " + juce::String(midiCC),
                    message,
                    "Got it!");
                return;
            } 
        } 
    } 
}

void InstrumentSelector::mouseDrag(const juce::MouseEvent& /*e*/) {}

void InstrumentSelector::buttonClicked(juce::Button* b) { 
    if (b == &multiModeBtn) { 
        bool newMultiMode = multiModeBtn.getToggleState();
        if (newMultiMode != processor.instrumentSelectorMultiMode) {
            // CRITICAL FIX: Suspend audio processing BEFORE making any changes
            processor.suspendProcessing(true);
            
            if (newMultiMode) {
                // Switching from single to multi mode
                processor.instrumentSelectorMultiMode = true;
                processor.restoreMultiStates();
            } else {
                // Switching from multi to single mode
                processor.storeMultiStates();
                processor.instrumentSelectorMultiMode = false;
                
                // FIX #1: Freeze plugins BEFORE setting bypass to prevent crashes
                for (auto id : nodeIDs) {
                    if (auto* n = processor.mainGraph->getNodeForId(id)) {
                        // Freeze the plugin first to stop audio processing
                        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(n->getProcessor())) {
                            meteringProc->setFrozen(true);
                        }
                        n->setBypassed(true);
                    }
                }
                
                // Unfreeze and unbypass the first instrument
                if (!nodeIDs.empty()) {
                    if (auto* n = processor.mainGraph->getNodeForId(nodeIDs[0])) {
                        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(n->getProcessor())) {
                            meteringProc->setFrozen(false);
                        }
                        n->setBypassed(false);
                    }
                }
            }
            
            // CRITICAL FIX: Resume audio processing AFTER all changes complete
            processor.suspendProcessing(false);
        }
    }
    
    for (int i = 0; i < instButtons.size(); ++i) { 
        if (b == instButtons[i]) { 
            handleInstrumentClick(i);
            return;
        } 
    } 
    
    if (auto* editor = findParentComponentOfClass<SubterraneumAudioProcessorEditor>()) 
        editor->repaint();
}

void InstrumentSelector::handleInstrumentClick(int index) { 
    if (index < 0 || index >= (int)nodeIDs.size()) return;
    
    auto clickedID = nodeIDs[index]; 
    auto* clickedNode = processor.mainGraph->getNodeForId(clickedID); 
    if (!clickedNode) return;
    
    // CRITICAL FIX: Suspend audio during instrument switching to prevent clicks/pops
    processor.suspendProcessing(true);
    
    if (processor.instrumentSelectorMultiMode) { 
        bool newBypassState = !clickedNode->isBypassed();
        
        // Update frozen state to match bypass state
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(clickedNode->getProcessor())) {
            meteringProc->setFrozen(newBypassState);
        }
        
        clickedNode->setBypassed(newBypassState);
        instButtons[index]->setToggleState(!newBypassState, juce::dontSendNotification);
    } else { 
        // Single mode: activate clicked instrument, bypass all others
        
        // Unfreeze and activate the clicked instrument
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(clickedNode->getProcessor())) {
            meteringProc->setFrozen(false);
        }
        clickedNode->setBypassed(false); 
        instButtons[index]->setToggleState(true, juce::dontSendNotification);
        
        // Freeze and bypass all other instruments
        for (int i = 0; i < (int)nodeIDs.size(); ++i) { 
            if (i != index) { 
                if (auto* otherNode = processor.mainGraph->getNodeForId(nodeIDs[i])) {
                    otherNode->setBypassed(true);
                    
                    // Freeze other instruments
                    if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(otherNode->getProcessor())) {
                        meteringProc->setFrozen(true);
                    }
                    
                    instButtons[i]->setToggleState(false, juce::dontSendNotification); 
                } 
            } 
        } 
    }
    
    // CRITICAL FIX: Resume audio after changes complete
    processor.suspendProcessing(false);
}

void InstrumentSelector::timerCallback() { 
    bool pMulti = processor.instrumentSelectorMultiMode;
    if (multiModeBtn.getToggleState() != pMulti) { 
        multiModeBtn.setToggleState(pMulti, juce::dontSendNotification);
    } 
    if (!processor.mainGraph) return; 
    
    // =========================================================================
    // NEW: Handle pending MIDI CC instrument selection
    // =========================================================================
    int pendingInst = processor.pendingInstrumentSelect.exchange(-1);
    if (pendingInst >= 0 && pendingInst < (int)nodeIDs.size()) {
        handleInstrumentClick(pendingInst);
    }
    
    // Sync button toggle states with actual bypass states
    for (int i = 0; i < (int)nodeIDs.size(); ++i) { 
        if (auto* node = processor.mainGraph->getNodeForId(nodeIDs[i])) { 
            bool isActive = !node->isBypassed();
            if (instButtons[i]->getToggleState() != isActive) 
                instButtons[i]->setToggleState(isActive, juce::dontSendNotification); 
        } 
    } 
}
