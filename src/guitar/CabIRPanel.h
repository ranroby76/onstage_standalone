// ==============================================================================
//  CabIRPanel.h
//  OnStage — UI panel for the Cab IR (convolution) effect
//
//  Matches CabIRProcessor params: Mix, Level, HighCut, LowCut
//  Plus an IR file loader button showing the current IR name.
//  Style synced with all other guitar panels (golden sliders, dark panel).
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/StyledSlider.h"
#include "../UI/EffectToggleButton.h"
#include "CabIRProcessor.h"

class PresetManager;

class CabIRPanel : public juce::Component, private juce::Timer
{
public:
    CabIRPanel (CabIRProcessor& p, PresetManager&) : proc (p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState (!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed (!toggleButton->getToggleState()); };
        addAndMakeVisible (toggleButton.get());

        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Cab IR", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centredLeft);

        // --- IR file loader ---
        addAndMakeVisible (loadButton);
        loadButton.setButtonText ("Load IR...");
        loadButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF333333));
        loadButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFD4AF37));
        loadButton.onClick = [this]() { loadIRFile(); };

        addAndMakeVisible (irNameLabel);
        irNameLabel.setFont (juce::Font (13.0f, juce::Font::italic));
        irNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFAAAAAA));
        irNameLabel.setJustificationType (juce::Justification::centredLeft);
        updateIRNameLabel();

        // --- Sliders ---
        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText (name);
            s->setRange (min, max, (max - min) / 100.0);
            s->setValue (value);
            s->setTextValueSuffix (suffix);
            s->getSlider().setLookAndFeel (goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible (s.get());
        };

        makeSlider (mixSlider,     "Mix",      0.0, 1.0,     p0.mix,              "");
        makeSlider (levelSlider,   "Level",    0.0, 2.0,     p0.level,            "");
        makeSlider (highCutSlider, "Hi Cut",   1000.0, 20000.0, p0.highCutHz,     " Hz");
        makeSlider (lowCutSlider,  "Lo Cut",   20.0, 500.0,  p0.lowCutHz,         " Hz");

        // Use skewed range for frequency sliders
        highCutSlider->getSlider().setSkewFactorFromMidPoint (6000.0);
        lowCutSlider->getSlider().setSkewFactorFromMidPoint (100.0);

        startTimerHz (15);
    }

    ~CabIRPanel() override
    {
        stopTimer();
        mixSlider->getSlider().setLookAndFeel (nullptr);
        levelSlider->getSlider().setLookAndFeel (nullptr);
        highCutSlider->getSlider().setLookAndFeel (nullptr);
        lowCutSlider->getSlider().setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1A1A1A));
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (getLocalBounds(), 2);
        g.setColour (juce::Colour (0xFF2A2A2A));
        g.fillRect (getLocalBounds().reduced (10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (15);

        // Title row
        auto titleRow = area.removeFromTop (35);
        toggleButton->setBounds (titleRow.removeFromRight (40).withSizeKeepingCentre (40, 40));
        titleLabel.setBounds (titleRow);

        // IR loader row
        area.removeFromTop (10);
        auto loaderRow = area.removeFromTop (30);
        loadButton.setBounds (loaderRow.removeFromLeft (100));
        loaderRow.removeFromLeft (10);
        irNameLabel.setBounds (loaderRow);

        // Sliders
        area.removeFromTop (15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX (area.getX() + (area.getWidth() - tw) / 2).withWidth (tw);
        mixSlider->setBounds (sa.removeFromLeft (sw)); sa.removeFromLeft (sp);
        levelSlider->setBounds (sa.removeFromLeft (sw)); sa.removeFromLeft (sp);
        highCutSlider->setBounds (sa.removeFromLeft (sw)); sa.removeFromLeft (sp);
        lowCutSlider->setBounds (sa.removeFromLeft (sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState (!proc.isBypassed(), juce::dontSendNotification);
        mixSlider->setValue (p.mix, juce::dontSendNotification);
        levelSlider->setValue (p.level, juce::dontSendNotification);
        highCutSlider->setValue (p.highCutHz, juce::dontSendNotification);
        lowCutSlider->setValue (p.lowCutHz, juce::dontSendNotification);
        updateIRNameLabel();
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue (p.mix, juce::dontSendNotification);
        if (!levelSlider->getSlider().isMouseOverOrDragging())
            levelSlider->setValue (p.level, juce::dontSendNotification);
        if (!highCutSlider->getSlider().isMouseOverOrDragging())
            highCutSlider->setValue (p.highCutHz, juce::dontSendNotification);
        if (!lowCutSlider->getSlider().isMouseOverOrDragging())
            lowCutSlider->setValue (p.lowCutHz, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState (shouldBeOn, juce::dontSendNotification);

        // Update IR name in case it changed externally
        juce::String currentName = proc.getIRName();
        if (currentName != lastIRName)
            updateIRNameLabel();
    }

    void updateProcessor()
    {
        CabIRProcessor::Params p;
        p.mix       = (float) mixSlider->getValue();
        p.level     = (float) levelSlider->getValue();
        p.highCutHz = (float) highCutSlider->getValue();
        p.lowCutHz  = (float) lowCutSlider->getValue();
        proc.setParams (p);
    }

    void updateIRNameLabel()
    {
        lastIRName = proc.getIRName();
        if (lastIRName.isEmpty())
            irNameLabel.setText ("No IR loaded", juce::dontSendNotification);
        else
            irNameLabel.setText (lastIRName, juce::dontSendNotification);
    }

    void loadIRFile()
    {
        fileChooser = std::make_shared<juce::FileChooser> (
            "Load Impulse Response",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.wav;*.aiff;*.flac",
            true);

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.existsAsFile())
                {
                    proc.loadIRFromFile (result);
                    updateIRNameLabel();
                }
            });
    }

    CabIRProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;

    juce::TextButton loadButton;
    juce::Label irNameLabel;
    juce::String lastIRName;
    std::shared_ptr<juce::FileChooser> fileChooser;

    std::unique_ptr<VerticalSlider> mixSlider, levelSlider, highCutSlider, lowCutSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabIRPanel)
};
