
// D:\Workspace\Subterraneum_plugins_daw\src\TransportOverrideComponent.h
// Per-Plugin Transport Override popup
// Shown when user clicks "T" button on a plugin node

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TransportOverrideComponent : public juce::Component,
                                    private juce::Slider::Listener,
                                    private juce::ComboBox::Listener,
                                    private juce::Button::Listener
{
public:
    TransportOverrideComponent(MeteringProcessor* proc)
        : meteringProc(proc)
    {
        setSize(260, 160);
        
        // =====================================================================
        // Title
        // =====================================================================
        titleLabel.setText("Plugin's Transport", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // =====================================================================
        // Sync question + Yes/No toggle buttons
        // =====================================================================
        syncLabel.setText("Synced to master transport?", juce::dontSendNotification);
        syncLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        syncLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(syncLabel);
        
        yesButton.setButtonText("Yes");
        yesButton.setRadioGroupId(1001);
        yesButton.setClickingTogglesState(true);
        yesButton.addListener(this);
        yesButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green.darker());
        yesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
        yesButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        yesButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        addAndMakeVisible(yesButton);
        
        noButton.setButtonText("No");
        noButton.setRadioGroupId(1001);
        noButton.setClickingTogglesState(true);
        noButton.addListener(this);
        noButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange.darker());
        noButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
        noButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        noButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        addAndMakeVisible(noButton);
        
        // =====================================================================
        // Time Signature dropdown
        // =====================================================================
        timeSigLabel.setText("Time Signature:", juce::dontSendNotification);
        timeSigLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        timeSigLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(timeSigLabel);
        
        timeSigCombo.addItem("2/4", 1);
        timeSigCombo.addItem("3/4", 2);
        timeSigCombo.addItem("4/4", 3);
        timeSigCombo.addItem("5/4", 4);
        timeSigCombo.addItem("6/4", 5);
        timeSigCombo.addItem("7/4", 6);
        timeSigCombo.addItem("3/8", 7);
        timeSigCombo.addItem("6/8", 8);
        timeSigCombo.addItem("7/8", 9);
        timeSigCombo.addItem("9/8", 10);
        timeSigCombo.addItem("12/8", 11);
        timeSigCombo.addListener(this);
        addAndMakeVisible(timeSigCombo);
        
        // =====================================================================
        // Tempo slider
        // =====================================================================
        tempoLabel.setText("Tempo:", juce::dontSendNotification);
        tempoLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        tempoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(tempoLabel);
        
        tempoSlider.setRange(20.0, 999.0, 0.1);
        tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
        tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        tempoSlider.setColour(juce::Slider::trackColourId, juce::Colours::yellow.darker());
        tempoSlider.setColour(juce::Slider::thumbColourId, juce::Colours::yellow);
        tempoSlider.addListener(this);
        addAndMakeVisible(tempoSlider);
        
        // =====================================================================
        // Initialize from current state
        // =====================================================================
        if (meteringProc)
        {
            bool synced = meteringProc->isTransportSynced();
            yesButton.setToggleState(synced, juce::dontSendNotification);
            noButton.setToggleState(!synced, juce::dontSendNotification);
            
            tempoSlider.setValue(meteringProc->getCustomTempo(), juce::dontSendNotification);
            
            int num = meteringProc->getCustomTimeSigNumerator();
            int den = meteringProc->getCustomTimeSigDenominator();
            selectTimeSig(num, den);
            
            updateEnabledState(synced);
        }
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        
        titleLabel.setBounds(area.removeFromTop(22));
        area.removeFromTop(4);
        
        // Sync row
        auto syncRow = area.removeFromTop(24);
        syncLabel.setBounds(syncRow.removeFromLeft(170));
        syncRow.removeFromLeft(4);
        yesButton.setBounds(syncRow.removeFromLeft(36));
        syncRow.removeFromLeft(2);
        noButton.setBounds(syncRow.removeFromLeft(36));
        
        area.removeFromTop(8);
        
        // Time signature row
        auto tsRow = area.removeFromTop(24);
        timeSigLabel.setBounds(tsRow.removeFromLeft(100));
        tsRow.removeFromLeft(4);
        timeSigCombo.setBounds(tsRow);
        
        area.removeFromTop(6);
        
        // Tempo row
        auto tempoRow = area.removeFromTop(24);
        tempoLabel.setBounds(tempoRow.removeFromLeft(50));
        tempoRow.removeFromLeft(4);
        tempoSlider.setBounds(tempoRow);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d));
        g.setColour(juce::Colour(0xff555555));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    void buttonClicked(juce::Button* button) override
    {
        if (!meteringProc) return;
        
        if (button == &yesButton || button == &noButton)
        {
            bool synced = yesButton.getToggleState();
            meteringProc->setTransportSynced(synced);
            updateEnabledState(synced);
        }
    }
    
    void comboBoxChanged(juce::ComboBox* combo) override
    {
        if (!meteringProc || combo != &timeSigCombo) return;
        
        int num = 4, den = 4;
        parseTimeSig(timeSigCombo.getSelectedId(), num, den);
        meteringProc->setCustomTimeSignature(num, den);
    }
    
    void sliderValueChanged(juce::Slider* slider) override
    {
        if (!meteringProc || slider != &tempoSlider) return;
        meteringProc->setCustomTempo(tempoSlider.getValue());
    }
    
    void updateEnabledState(bool synced)
    {
        float alpha = synced ? 0.35f : 1.0f;
        
        timeSigLabel.setAlpha(alpha);
        timeSigCombo.setEnabled(!synced);
        timeSigCombo.setAlpha(alpha);
        
        tempoLabel.setAlpha(alpha);
        tempoSlider.setEnabled(!synced);
        tempoSlider.setAlpha(alpha);
    }
    
    void selectTimeSig(int num, int den)
    {
        // Map num/den pair to combo ID
        struct TSig { int num; int den; int id; };
        const TSig sigs[] = {
            {2,4,1}, {3,4,2}, {4,4,3}, {5,4,4}, {6,4,5}, {7,4,6},
            {3,8,7}, {6,8,8}, {7,8,9}, {9,8,10}, {12,8,11}
        };
        
        for (auto& s : sigs)
        {
            if (s.num == num && s.den == den)
            {
                timeSigCombo.setSelectedId(s.id, juce::dontSendNotification);
                return;
            }
        }
        
        timeSigCombo.setSelectedId(3, juce::dontSendNotification); // Default 4/4
    }
    
    void parseTimeSig(int comboId, int& num, int& den)
    {
        switch (comboId)
        {
            case 1:  num = 2;  den = 4; break;
            case 2:  num = 3;  den = 4; break;
            case 3:  num = 4;  den = 4; break;
            case 4:  num = 5;  den = 4; break;
            case 5:  num = 6;  den = 4; break;
            case 6:  num = 7;  den = 4; break;
            case 7:  num = 3;  den = 8; break;
            case 8:  num = 6;  den = 8; break;
            case 9:  num = 7;  den = 8; break;
            case 10: num = 9;  den = 8; break;
            case 11: num = 12; den = 8; break;
            default: num = 4;  den = 4; break;
        }
    }
    
    MeteringProcessor* meteringProc = nullptr;
    
    juce::Label titleLabel;
    juce::Label syncLabel;
    juce::TextButton yesButton;
    juce::TextButton noButton;
    juce::Label timeSigLabel;
    juce::ComboBox timeSigCombo;
    juce::Label tempoLabel;
    juce::Slider tempoSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportOverrideComponent)
};


