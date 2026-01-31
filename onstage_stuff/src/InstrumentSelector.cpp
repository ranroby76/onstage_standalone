#include "InstrumentSelector.h"
#include "PluginEditor.h"

InstrumentSelector::InstrumentSelector(SubterraneumAudioProcessor& p) : processor(p) { 
    addAndMakeVisible(titleLabel);
    titleLabel.setJustificationType(juce::Justification::centredLeft); 
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold)); 
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white); 
    
    multiModeBtn.setButtonText("Multi");
    multiModeBtn.setClickingTogglesState(true);
    multiModeBtn.setColour(juce::ToggleButton::tickColourId, juce::Colours::green);
    multiModeBtn.addListener(this);
    multiModeBtn.setToggleState(processor.instrumentSelectorMultiMode, juce::dontSendNotification);
    addAndMakeVisible(multiModeBtn);
    
    for (int i=0; i<16; ++i) { 
        auto* btn = instButtons.add(new juce::TextButton(juce::String(i+1))); 
        btn->setClickingTogglesState(false);
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        btn->addListener(this); 
        btn->addMouseListener(this, false); 
        addAndMakeVisible(btn); 
    } 
    startTimer(100);
}

InstrumentSelector::~InstrumentSelector() { stopTimer(); }

void InstrumentSelector::updateList() { 
    nodeIDs.clear(); 
    if (!processor.mainGraph) return; 
    
    std::vector<juce::AudioProcessorGraph::Node*> instrumentNodes;
    for (auto* node : processor.mainGraph->getNodes()) { 
        auto* proc = node->getProcessor();
        if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) { 
            if (mp->getInnerPlugin()->getPluginDescription().isInstrument) 
                instrumentNodes.push_back(node);
        } else if (auto* plugin = dynamic_cast<juce::AudioPluginInstance*>(proc)) { 
            if (plugin->getPluginDescription().isInstrument) 
                instrumentNodes.push_back(node);
        } 
    } 
    
    bool showModes = (instrumentNodes.size() > 1); 
    multiModeBtn.setVisible(showModes);
    
    for (int i = 0; i < 16; ++i) { 
        auto* btn = instButtons[i];
        if (i < (int)instrumentNodes.size()) { 
            auto* node = instrumentNodes[i];
            nodeIDs.push_back(node->nodeID); 
            juce::String name = node->getProcessor()->getName();
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
    float btnW = (float)area.getWidth() / 16.0f;
    for (int i = 0; i < 16; ++i) { 
        if (instButtons[i]) { 
            instButtons[i]->setBounds(area.getX() + (int)(i * btnW), area.getY(), (int)btnW - 2, area.getHeight());
        } 
    } 
}

void InstrumentSelector::mouseDown(const juce::MouseEvent& e) { 
    if (e.mods.isRightButtonDown()) { 
        for (int i = 0; i < instButtons.size(); ++i) { 
            if (e.eventComponent == instButtons[i] || e.originalComponent == instButtons[i]) { 
                juce::String tooltip = "MIDI Note: " + juce::String(i + 1);
                auto label = std::make_unique<juce::Label>("tip", tooltip); 
                label->setJustificationType(juce::Justification::centred); 
                label->setSize(120, 30); 
                auto* editor = findParentComponentOfClass<SubterraneumAudioProcessorEditor>(); 
                juce::CallOutBox::launchAsynchronously(std::move(label), instButtons[i]->getScreenBounds(), editor); 
                return;
            } 
        } 
    } 
}

void InstrumentSelector::mouseDrag(const juce::MouseEvent& e) {}

void InstrumentSelector::buttonClicked(juce::Button* b) { 
    if (b == &multiModeBtn) { 
        bool newMultiMode = multiModeBtn.getToggleState();
        if (newMultiMode != processor.instrumentSelectorMultiMode) {
            if (newMultiMode) {
                processor.instrumentSelectorMultiMode = true;
                processor.restoreMultiStates();
            } else {
                processor.storeMultiStates();
                processor.instrumentSelectorMultiMode = false;
                for (auto id : nodeIDs) 
                    if (auto* n = processor.mainGraph->getNodeForId(id)) 
                        n->setBypassed(true);
                if (!nodeIDs.empty()) 
                    if (auto* n = processor.mainGraph->getNodeForId(nodeIDs[0])) 
                        n->setBypassed(false);
            }
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
    
    if (processor.instrumentSelectorMultiMode) { 
        bool newBypassState = !clickedNode->isBypassed(); 
        clickedNode->setBypassed(newBypassState);
        instButtons[index]->setToggleState(!newBypassState, juce::dontSendNotification);
    } else { 
        clickedNode->setBypassed(false); 
        instButtons[index]->setToggleState(true, juce::dontSendNotification);
        for (int i = 0; i < (int)nodeIDs.size(); ++i) { 
            if (i != index) { 
                if (auto* otherNode = processor.mainGraph->getNodeForId(nodeIDs[i])) { 
                    otherNode->setBypassed(true);
                    instButtons[i]->setToggleState(false, juce::dontSendNotification); 
                } 
            } 
        } 
    } 
}

void InstrumentSelector::timerCallback() { 
    bool pMulti = processor.instrumentSelectorMultiMode;
    if (multiModeBtn.getToggleState() != pMulti) { 
        multiModeBtn.setToggleState(pMulti, juce::dontSendNotification);
    } 
    if (!processor.mainGraph) return; 
    for (int i = 0; i < (int)nodeIDs.size(); ++i) { 
        if (auto* node = processor.mainGraph->getNodeForId(nodeIDs[i])) { 
            bool isActive = !node->isBypassed();
            if (instButtons[i]->getToggleState() != isActive) 
                instButtons[i]->setToggleState(isActive, juce::dontSendNotification); 
        } 
    } 
}