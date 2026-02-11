// ==============================================================================
//  MasterPanel.h
//  OnStage - Master Node UI Panel (Airwindows Mastering2)
//
//  6 vertical sliders: Sidepass, Glue, Scope, Skronk, Girth, Drive
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/MasterProcessor.h"

class MasterPanel : public juce::Component
{
public:
    MasterPanel (MasterProcessor& proc)
        : processor (proc)
    {
        goldenLAF = std::make_unique<GoldenSliderLookAndFeel>();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!processor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { processor.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        titleLabel.setText ("MASTER", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (titleLabel);

        auto make = [&](std::unique_ptr<VerticalSlider>& s,
                        const juce::String& name, double min, double max, double def)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText (name);
            s->setRange (min, max, 0.01);
            s->setValue (def, juce::dontSendNotification);
            s->getSlider().setLookAndFeel (goldenLAF.get());
            s->getSlider().onValueChange = [this] { pushToProcessor(); };
            addAndMakeVisible (*s);
        };

        make (sidepassSlider, "Sidepass", 0.0, 1.0, 0.0);
        make (glueSlider,     "Glue",     0.0, 1.0, 0.0);
        make (scopeSlider,    "Scope",    0.0, 1.0, 0.5);
        make (skronkSlider,   "Skronk",   0.0, 1.0, 0.5);
        make (girthSlider,    "Girth",    0.0, 1.0, 0.5);
        make (driveSlider,    "Drive",    0.0, 1.0, 0.5);

        // Apply current params
        auto p = processor.getParams();
        sidepassSlider->setValue (p.sidepass, juce::dontSendNotification);
        glueSlider->setValue     (p.glue,     juce::dontSendNotification);
        scopeSlider->setValue    (p.scope,    juce::dontSendNotification);
        skronkSlider->setValue   (p.skronk,   juce::dontSendNotification);
        girthSlider->setValue    (p.girth,    juce::dontSendNotification);
        driveSlider->setValue    (p.drive,    juce::dontSendNotification);

        setSize (420, 280);
    }

    ~MasterPanel() override
    {
        sidepassSlider->getSlider().setLookAndFeel (nullptr);
        glueSlider->getSlider().setLookAndFeel (nullptr);
        scopeSlider->getSlider().setLookAndFeel (nullptr);
        skronkSlider->getSlider().setLookAndFeel (nullptr);
        girthSlider->getSlider().setLookAndFeel (nullptr);
        driveSlider->getSlider().setLookAndFeel (nullptr);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        auto titleRow = area.removeFromTop (24);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds (titleRow);
        area.removeFromTop (4);

        constexpr int n = 6;
        constexpr int sw = 56;
        constexpr int sp = 8;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX (area.getX() + (area.getWidth() - tw) / 2).withWidth (tw);

        auto placeSlider = [&](std::unique_ptr<VerticalSlider>& s)
        {
            s->setBounds (sa.removeFromLeft (sw));
            sa.removeFromLeft (sp);
        };

        placeSlider (sidepassSlider);
        placeSlider (glueSlider);
        placeSlider (scopeSlider);
        placeSlider (skronkSlider);
        placeSlider (girthSlider);
        placeSlider (driveSlider);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1E1E1E));
        g.setColour (juce::Colour (0xFFD4AF37).withAlpha (0.3f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2), 6.0f, 1.5f);
    }

private:
    void pushToProcessor()
    {
        MasterProcessor::Params p;
        p.sidepass = (float) sidepassSlider->getValue();
        p.glue     = (float) glueSlider->getValue();
        p.scope    = (float) scopeSlider->getValue();
        p.skronk   = (float) skronkSlider->getValue();
        p.girth    = (float) girthSlider->getValue();
        p.drive    = (float) driveSlider->getValue();
        processor.setParams (p);
    }

    MasterProcessor& processor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLAF;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;

    std::unique_ptr<VerticalSlider> sidepassSlider, glueSlider, scopeSlider;
    std::unique_ptr<VerticalSlider> skronkSlider, girthSlider, driveSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterPanel)
};
