// ==============================================================================
//  GuitarPanels.h
//  OnStage — UI panels for all Guitar effect nodes
//
//  Synced to match Studio effect panel style:
//    - VerticalSlider with GoldenSliderLookAndFeel
//    - Dark panel background with golden title
//    - EffectToggleButton (no label) on top-right
//    - 15Hz bidirectional timer sync
//    - updateFromPreset() public method
//    - Sliders horizontally centered in panel
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/StyledSlider.h"
#include "../UI/EffectToggleButton.h"

#include "OverdriveProcessor.h"
#include "DistortionProcessor.h"
#include "FuzzProcessor.h"
#include "GuitarChorusProcessor.h"
#include "GuitarFlangerProcessor.h"
#include "GuitarPhaserProcessor.h"
#include "GuitarTremoloProcessor.h"
#include "GuitarVibratoProcessor.h"
#include "GuitarToneProcessor.h"
#include "GuitarRotaryProcessor.h"
#include "GuitarWahProcessor.h"
#include "GuitarReverbProcessor.h"
#include "GuitarNoiseGateProcessor.h"
#include "ToneStackProcessor.h"
#include "CabSimProcessor.h"

class PresetManager;  // forward declaration

// ==============================================================================
//  Overdrive Panel
// ==============================================================================
class OverdrivePanel : public juce::Component, private juce::Timer
{
public:
    OverdrivePanel(OverdriveProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Overdrive", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(driveSlider, "Drive", 0.0, 10.0, p0.drive, "");
        makeSlider(toneSlider, "Tone", 0.0, 1.0, p0.tone, "");
        makeSlider(levelSlider, "Level", 0.0, 1.0, p0.level, "");
        makeSlider(mixSlider, "Mix", 0.0, 1.0, p0.mix, "");

        startTimerHz(15);
    }

    ~OverdrivePanel() override
    {
        stopTimer();
        driveSlider->getSlider().setLookAndFeel(nullptr);
        toneSlider->getSlider().setLookAndFeel(nullptr);
        levelSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        driveSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        toneSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        levelSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        driveSlider->setValue(p.drive, juce::dontSendNotification);
        toneSlider->setValue(p.tone, juce::dontSendNotification);
        levelSlider->setValue(p.level, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!driveSlider->getSlider().isMouseOverOrDragging())
            driveSlider->setValue(p.drive, juce::dontSendNotification);
        if (!toneSlider->getSlider().isMouseOverOrDragging())
            toneSlider->setValue(p.tone, juce::dontSendNotification);
        if (!levelSlider->getSlider().isMouseOverOrDragging())
            levelSlider->setValue(p.level, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(p.mix, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        OverdriveProcessor::Params p;
        p.drive = (float)driveSlider->getValue();
        p.tone  = (float)toneSlider->getValue();
        p.level = (float)levelSlider->getValue();
        p.mix   = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    OverdriveProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> driveSlider, toneSlider, levelSlider, mixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverdrivePanel)
};

// ==============================================================================
//  Distortion Panel
// ==============================================================================
class DistortionPanel : public juce::Component, private juce::Timer
{
public:
    DistortionPanel(DistortionProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Distortion", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(driveSlider, "Drive", 0.0, 10.0, p0.drive, "");
        makeSlider(toneSlider, "Tone", 0.0, 1.0, p0.tone, "");
        makeSlider(levelSlider, "Level", 0.0, 1.0, p0.level, "");
        makeSlider(mixSlider, "Mix", 0.0, 1.0, p0.mix, "");

        startTimerHz(15);
    }

    ~DistortionPanel() override
    {
        stopTimer();
        driveSlider->getSlider().setLookAndFeel(nullptr);
        toneSlider->getSlider().setLookAndFeel(nullptr);
        levelSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        driveSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        toneSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        levelSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        driveSlider->setValue(p.drive, juce::dontSendNotification);
        toneSlider->setValue(p.tone, juce::dontSendNotification);
        levelSlider->setValue(p.level, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!driveSlider->getSlider().isMouseOverOrDragging())
            driveSlider->setValue(p.drive, juce::dontSendNotification);
        if (!toneSlider->getSlider().isMouseOverOrDragging())
            toneSlider->setValue(p.tone, juce::dontSendNotification);
        if (!levelSlider->getSlider().isMouseOverOrDragging())
            levelSlider->setValue(p.level, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(p.mix, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        DistortionProcessor::Params p;
        p.drive = (float)driveSlider->getValue();
        p.tone  = (float)toneSlider->getValue();
        p.level = (float)levelSlider->getValue();
        p.mix   = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    DistortionProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> driveSlider, toneSlider, levelSlider, mixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionPanel)
};

// ==============================================================================
//  Fuzz Panel
// ==============================================================================
class FuzzPanel : public juce::Component, private juce::Timer
{
public:
    FuzzPanel(FuzzProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Fuzz", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(fuzzSlider, "Fuzz", 0.0, 10.0, p0.fuzz, "");
        makeSlider(toneSlider, "Tone", 0.0, 1.0, p0.tone, "");
        makeSlider(sustainSlider, "Sustain", 0.0, 1.0, p0.sustain, "");
        makeSlider(levelSlider, "Level", 0.0, 1.0, p0.level, "");

        startTimerHz(15);
    }

    ~FuzzPanel() override
    {
        stopTimer();
        fuzzSlider->getSlider().setLookAndFeel(nullptr);
        toneSlider->getSlider().setLookAndFeel(nullptr);
        sustainSlider->getSlider().setLookAndFeel(nullptr);
        levelSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        fuzzSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        toneSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        sustainSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        levelSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        fuzzSlider->setValue(p.fuzz, juce::dontSendNotification);
        toneSlider->setValue(p.tone, juce::dontSendNotification);
        sustainSlider->setValue(p.sustain, juce::dontSendNotification);
        levelSlider->setValue(p.level, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!fuzzSlider->getSlider().isMouseOverOrDragging())
            fuzzSlider->setValue(p.fuzz, juce::dontSendNotification);
        if (!toneSlider->getSlider().isMouseOverOrDragging())
            toneSlider->setValue(p.tone, juce::dontSendNotification);
        if (!sustainSlider->getSlider().isMouseOverOrDragging())
            sustainSlider->setValue(p.sustain, juce::dontSendNotification);
        if (!levelSlider->getSlider().isMouseOverOrDragging())
            levelSlider->setValue(p.level, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        FuzzProcessor::Params p;
        p.fuzz    = (float)fuzzSlider->getValue();
        p.tone    = (float)toneSlider->getValue();
        p.sustain = (float)sustainSlider->getValue();
        p.level   = (float)levelSlider->getValue();
        proc.setParams(p);
    }

    FuzzProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> fuzzSlider, toneSlider, sustainSlider, levelSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FuzzPanel)
};

// ==============================================================================
//  Guitar Chorus Panel
// ==============================================================================
class GuitarChorusPanel : public juce::Component, private juce::Timer
{
public:
    GuitarChorusPanel(GuitarChorusProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Chorus", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(rateSlider, "Rate", 0.1, 10.0, p0.rate, " Hz");
        makeSlider(depthSlider, "Depth", 0.0, 1.0, p0.depth, "");
        makeSlider(mixSlider, "Mix", 0.0, 1.0, p0.mix, "");
        makeSlider(widthSlider, "Width", 0.0, 1.0, p0.width, "");

        startTimerHz(15);
    }

    ~GuitarChorusPanel() override
    {
        stopTimer();
        rateSlider->getSlider().setLookAndFeel(nullptr);
        depthSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
        widthSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        rateSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        depthSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        widthSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        rateSlider->setValue(p.rate, juce::dontSendNotification);
        depthSlider->setValue(p.depth, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
        widthSlider->setValue(p.width, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!rateSlider->getSlider().isMouseOverOrDragging())
            rateSlider->setValue(p.rate, juce::dontSendNotification);
        if (!depthSlider->getSlider().isMouseOverOrDragging())
            depthSlider->setValue(p.depth, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(p.mix, juce::dontSendNotification);
        if (!widthSlider->getSlider().isMouseOverOrDragging())
            widthSlider->setValue(p.width, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarChorusProcessor::Params p;
        p.rate  = (float)rateSlider->getValue();
        p.depth = (float)depthSlider->getValue();
        p.mix   = (float)mixSlider->getValue();
        p.width = (float)widthSlider->getValue();
        proc.setParams(p);
    }

    GuitarChorusProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> rateSlider, depthSlider, mixSlider, widthSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarChorusPanel)
};

// ==============================================================================
//  Guitar Flanger Panel
// ==============================================================================
class GuitarFlangerPanel : public juce::Component, private juce::Timer
{
public:
    GuitarFlangerPanel(GuitarFlangerProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Flanger", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(rateSlider, "Rate", 0.05, 5.0, p0.rate, " Hz");
        makeSlider(depthSlider, "Depth", 0.0, 1.0, p0.depth, "");
        makeSlider(feedbackSlider, "Feedback", 0.0, 0.95, p0.feedback, "");
        makeSlider(mixSlider, "Mix", 0.0, 1.0, p0.mix, "");

        startTimerHz(15);
    }

    ~GuitarFlangerPanel() override
    {
        stopTimer();
        rateSlider->getSlider().setLookAndFeel(nullptr);
        depthSlider->getSlider().setLookAndFeel(nullptr);
        feedbackSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        rateSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        depthSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        feedbackSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        rateSlider->setValue(p.rate, juce::dontSendNotification);
        depthSlider->setValue(p.depth, juce::dontSendNotification);
        feedbackSlider->setValue(p.feedback, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!rateSlider->getSlider().isMouseOverOrDragging())
            rateSlider->setValue(p.rate, juce::dontSendNotification);
        if (!depthSlider->getSlider().isMouseOverOrDragging())
            depthSlider->setValue(p.depth, juce::dontSendNotification);
        if (!feedbackSlider->getSlider().isMouseOverOrDragging())
            feedbackSlider->setValue(p.feedback, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(p.mix, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarFlangerProcessor::Params p;
        p.rate     = (float)rateSlider->getValue();
        p.depth    = (float)depthSlider->getValue();
        p.feedback = (float)feedbackSlider->getValue();
        p.mix      = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarFlangerProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> rateSlider, depthSlider, feedbackSlider, mixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFlangerPanel)
};

// ==============================================================================
//  Guitar Phaser Panel (textbook model — 9 sliders)
// ==============================================================================
class GuitarPhaserPanel : public juce::Component, private juce::Timer
{
public:
    GuitarPhaserPanel(GuitarPhaserProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Phaser", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix,
                              double step = 0.0)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, step > 0.0 ? step : (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(baseFreqSlider,    "Base",     50.0,   1000.0, p0.baseFreq,            " Hz");
        makeSlider(sweepWidthSlider,  "Sweep",    50.0,   5000.0, p0.sweepWidth,           " Hz");
        makeSlider(rateSlider,        "Rate",      0.05,     2.0, p0.rate,                 " Hz");
        makeSlider(depthSlider,       "Depth",     0.0,      1.0, p0.depth,                "");
        makeSlider(feedbackSlider,    "Feedbk",    0.0,      0.99, p0.feedback,             "");
        makeSlider(stereoSlider,      "Stereo",    0.0,      1.0, p0.stereo,               "", 1.0);
        makeSlider(waveformSlider,    "Wave",      0.0,      3.0, (double)p0.waveform,     "", 1.0);
        makeSlider(stagesSlider,      "Stages",    2.0,     10.0, (double)p0.stages,        "", 2.0);
        makeSlider(mixSlider,         "Mix",       0.0,      1.0, p0.mix,                  "");

        startTimerHz(15);
    }

    ~GuitarPhaserPanel() override
    {
        stopTimer();
        for (auto* s : getAllSliders())
            s->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(10);

        int sw = 70, sp = 10, n = 9;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        baseFreqSlider->setBounds(sa.removeFromLeft(sw));    sa.removeFromLeft(sp);
        sweepWidthSlider->setBounds(sa.removeFromLeft(sw));  sa.removeFromLeft(sp);
        rateSlider->setBounds(sa.removeFromLeft(sw));        sa.removeFromLeft(sp);
        depthSlider->setBounds(sa.removeFromLeft(sw));       sa.removeFromLeft(sp);
        feedbackSlider->setBounds(sa.removeFromLeft(sw));    sa.removeFromLeft(sp);
        stereoSlider->setBounds(sa.removeFromLeft(sw));      sa.removeFromLeft(sp);
        waveformSlider->setBounds(sa.removeFromLeft(sw));    sa.removeFromLeft(sp);
        stagesSlider->setBounds(sa.removeFromLeft(sw));      sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        baseFreqSlider->setValue(p.baseFreq, juce::dontSendNotification);
        sweepWidthSlider->setValue(p.sweepWidth, juce::dontSendNotification);
        rateSlider->setValue(p.rate, juce::dontSendNotification);
        depthSlider->setValue(p.depth, juce::dontSendNotification);
        feedbackSlider->setValue(p.feedback, juce::dontSendNotification);
        stereoSlider->setValue(p.stereo, juce::dontSendNotification);
        waveformSlider->setValue((double)p.waveform, juce::dontSendNotification);
        stagesSlider->setValue((double)p.stages, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    std::vector<VerticalSlider*> getAllSliders()
    {
        return { baseFreqSlider.get(), sweepWidthSlider.get(), rateSlider.get(),
                 depthSlider.get(), feedbackSlider.get(), stereoSlider.get(),
                 waveformSlider.get(), stagesSlider.get(), mixSlider.get() };
    }

    void timerCallback() override
    {
        auto p = proc.getParams();
        auto sync = [](VerticalSlider* s, double v) {
            if (!s->getSlider().isMouseOverOrDragging())
                s->setValue(v, juce::dontSendNotification);
        };
        sync(baseFreqSlider.get(), p.baseFreq);
        sync(sweepWidthSlider.get(), p.sweepWidth);
        sync(rateSlider.get(), p.rate);
        sync(depthSlider.get(), p.depth);
        sync(feedbackSlider.get(), p.feedback);
        sync(stereoSlider.get(), p.stereo);
        sync(waveformSlider.get(), (double)p.waveform);
        sync(stagesSlider.get(), (double)p.stages);
        sync(mixSlider.get(), p.mix);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarPhaserProcessor::Params p;
        p.baseFreq   = (float)baseFreqSlider->getValue();
        p.sweepWidth = (float)sweepWidthSlider->getValue();
        p.rate       = (float)rateSlider->getValue();
        p.depth      = (float)depthSlider->getValue();
        p.feedback   = (float)feedbackSlider->getValue();
        p.stereo     = (float)stereoSlider->getValue();
        p.waveform   = (int)waveformSlider->getValue();
        p.stages     = (int)stagesSlider->getValue();
        p.mix        = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarPhaserProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> baseFreqSlider, sweepWidthSlider, rateSlider,
                                    depthSlider, feedbackSlider, stereoSlider,
                                    waveformSlider, stagesSlider, mixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarPhaserPanel)
};

// ==============================================================================
//  Guitar Tremolo Panel (6 sliders — from-scratch rewrite)
// ==============================================================================
class GuitarTremoloPanel : public juce::Component, private juce::Timer
{
public:
    GuitarTremoloPanel(GuitarTremoloProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Tremolo", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix,
                              double step = 0.0)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, step > 0.0 ? step : (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(rateSlider,   "Rate",   0.5, 15.0, p0.rate,        " Hz");
        makeSlider(depthSlider,  "Depth",  0.0,  1.0, p0.depth,       "");
        makeSlider(waveSlider,   "Wave",   0.0,  5.0, (double)p0.wave,"", 1.0);
        makeSlider(stereoSlider, "Stereo", 0.0,  1.0, p0.stereo,      "");
        makeSlider(biasSlider,   "Bias",   0.0,  1.0, p0.bias,        "");
        makeSlider(mixSlider,    "Mix",    0.0,  1.0, p0.mix,         "");

        startTimerHz(15);
    }

    ~GuitarTremoloPanel() override
    {
        stopTimer();
        for (auto* s : getAllSliders()) s->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 48, sp = 6, n = 6;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        rateSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        depthSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        waveSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        stereoSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        biasSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        rateSlider->setValue(p.rate, juce::dontSendNotification);
        depthSlider->setValue(p.depth, juce::dontSendNotification);
        waveSlider->setValue((double)p.wave, juce::dontSendNotification);
        stereoSlider->setValue(p.stereo, juce::dontSendNotification);
        biasSlider->setValue(p.bias, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    std::vector<VerticalSlider*> getAllSliders()
    { return { rateSlider.get(), depthSlider.get(), waveSlider.get(),
               stereoSlider.get(), biasSlider.get(), mixSlider.get() }; }

    void timerCallback() override
    {
        auto p = proc.getParams();
        auto sync = [](VerticalSlider* s, double v) {
            if (!s->getSlider().isMouseOverOrDragging()) s->setValue(v, juce::dontSendNotification); };
        sync(rateSlider.get(), p.rate); sync(depthSlider.get(), p.depth);
        sync(waveSlider.get(), (double)p.wave); sync(stereoSlider.get(), p.stereo);
        sync(biasSlider.get(), p.bias); sync(mixSlider.get(), p.mix);
        bool on = !proc.isBypassed();
        if (toggleButton->getToggleState() != on)
            toggleButton->setToggleState(on, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarTremoloProcessor::Params p;
        p.rate   = (float)rateSlider->getValue();
        p.depth  = (float)depthSlider->getValue();
        p.wave   = (int)waveSlider->getValue();
        p.stereo = (float)stereoSlider->getValue();
        p.bias   = (float)biasSlider->getValue();
        p.mix    = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarTremoloProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> rateSlider, depthSlider, waveSlider,
                                    stereoSlider, biasSlider, mixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTremoloPanel)
};

// ==============================================================================
//  Guitar Vibrato Panel (6 sliders)
// ==============================================================================
class GuitarVibratoPanel : public juce::Component, private juce::Timer
{
public:
    GuitarVibratoPanel(GuitarVibratoProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Vibrato", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();
        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix,
                              double step = 0.0) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, step > 0.0 ? step : (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(rateSlider,   "Rate",   0.1, 10.0, p0.rate,          " Hz");
        makeSlider(depthSlider,  "Depth",  0.0,  1.0, p0.depth,         "");
        makeSlider(waveSlider,   "Wave",   0.0,  1.0, (double)p0.wave,  "", 1.0);
        makeSlider(stereoSlider, "Stereo", 0.0,  1.0, p0.stereo,        "");
        makeSlider(delaySlider,  "Delay",  0.0,  1.0, p0.delay,         "");
        makeSlider(mixSlider,    "Mix",    0.0,  1.0, p0.mix,           "");

        startTimerHz(15);
    }

    ~GuitarVibratoPanel() override
    {
        stopTimer();
        for (auto* s : getAllSliders()) s->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 48, sp = 6, n = 6;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        rateSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        depthSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        waveSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        stereoSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        delaySlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        rateSlider->setValue(p.rate, juce::dontSendNotification);
        depthSlider->setValue(p.depth, juce::dontSendNotification);
        waveSlider->setValue((double)p.wave, juce::dontSendNotification);
        stereoSlider->setValue(p.stereo, juce::dontSendNotification);
        delaySlider->setValue(p.delay, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    std::vector<VerticalSlider*> getAllSliders()
    { return { rateSlider.get(), depthSlider.get(), waveSlider.get(),
               stereoSlider.get(), delaySlider.get(), mixSlider.get() }; }

    void timerCallback() override
    {
        auto p = proc.getParams();
        auto sync = [](VerticalSlider* s, double v) {
            if (!s->getSlider().isMouseOverOrDragging()) s->setValue(v, juce::dontSendNotification); };
        sync(rateSlider.get(), p.rate); sync(depthSlider.get(), p.depth);
        sync(waveSlider.get(), (double)p.wave); sync(stereoSlider.get(), p.stereo);
        sync(delaySlider.get(), p.delay); sync(mixSlider.get(), p.mix);
        bool on = !proc.isBypassed();
        if (toggleButton->getToggleState() != on)
            toggleButton->setToggleState(on, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarVibratoProcessor::Params p;
        p.rate   = (float)rateSlider->getValue();
        p.depth  = (float)depthSlider->getValue();
        p.wave   = (int)waveSlider->getValue();
        p.stereo = (float)stereoSlider->getValue();
        p.delay  = (float)delaySlider->getValue();
        p.mix    = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarVibratoProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> rateSlider, depthSlider, waveSlider,
                                    stereoSlider, delaySlider, mixSlider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarVibratoPanel)
};

// ==============================================================================
//  Guitar Tone Panel (6 sliders — Baxandall EQ)
// ==============================================================================
class GuitarTonePanel : public juce::Component, private juce::Timer
{
public:
    GuitarTonePanel(GuitarToneProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Tone", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();
        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(bassSlider,     "Bass",    -12.0, 12.0, p0.bass,         " dB");
        makeSlider(midSlider,      "Mid",     -12.0, 12.0, p0.mid,          " dB");
        makeSlider(trebleSlider,   "Treble",  -12.0, 12.0, p0.treble,       " dB");
        makeSlider(midFreqSlider,  "MidF",    200.0, 3000.0, p0.midFreq,    " Hz");
        makeSlider(presenceSlider, "Pres",    -1.0,  1.0, p0.presence,      "");
        makeSlider(mixSlider,      "Mix",      0.0,  1.0, p0.mix,           "");

        startTimerHz(15);
    }

    ~GuitarTonePanel() override
    {
        stopTimer();
        for (auto* s : getAllSliders()) s->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 48, sp = 6, n = 6;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        bassSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        midSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        trebleSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        midFreqSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        presenceSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        bassSlider->setValue(p.bass, juce::dontSendNotification);
        midSlider->setValue(p.mid, juce::dontSendNotification);
        trebleSlider->setValue(p.treble, juce::dontSendNotification);
        midFreqSlider->setValue(p.midFreq, juce::dontSendNotification);
        presenceSlider->setValue(p.presence, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    std::vector<VerticalSlider*> getAllSliders()
    { return { bassSlider.get(), midSlider.get(), trebleSlider.get(),
               midFreqSlider.get(), presenceSlider.get(), mixSlider.get() }; }

    void timerCallback() override
    {
        auto p = proc.getParams();
        auto sync = [](VerticalSlider* s, double v) {
            if (!s->getSlider().isMouseOverOrDragging()) s->setValue(v, juce::dontSendNotification); };
        sync(bassSlider.get(), p.bass); sync(midSlider.get(), p.mid);
        sync(trebleSlider.get(), p.treble); sync(midFreqSlider.get(), p.midFreq);
        sync(presenceSlider.get(), p.presence); sync(mixSlider.get(), p.mix);
        bool on = !proc.isBypassed();
        if (toggleButton->getToggleState() != on)
            toggleButton->setToggleState(on, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarToneProcessor::Params p;
        p.bass     = (float)bassSlider->getValue();
        p.mid      = (float)midSlider->getValue();
        p.treble   = (float)trebleSlider->getValue();
        p.midFreq  = (float)midFreqSlider->getValue();
        p.presence = (float)presenceSlider->getValue();
        p.mix      = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarToneProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> bassSlider, midSlider, trebleSlider,
                                    midFreqSlider, presenceSlider, mixSlider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTonePanel)
};

// ==============================================================================
//  Guitar Rotary Speaker Panel (2 rows: 4+4 sliders)
// ==============================================================================
class GuitarRotaryPanel : public juce::Component, private juce::Timer
{
public:
    GuitarRotaryPanel(GuitarRotaryProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Rotary", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();
        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix,
                              double step = 0.0) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, step > 0.0 ? step : (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        // Row 1
        makeSlider(hornRateSlider, "Horn",    0.1, 10.0, p0.hornRate,       " Hz");
        makeSlider(dopplerSlider,  "Doppler", 0.0,  1.0, p0.doppler,        "");
        makeSlider(tremoloSlider,  "Trem",    0.0,  1.0, p0.tremolo,        "");
        makeSlider(rotorSlider,    "Rotor",   0.0,  2.0, p0.rotorRate,      "x");
        // Row 2
        makeSlider(driveSlider,    "Drive",   0.0,  1.0, p0.drive,          "");
        makeSlider(waveShpSlider,  "Shape",   0.0,  7.0, (double)p0.waveshape, "", 1.0);
        makeSlider(widthSlider,    "Width",   0.0,  2.0, p0.width,          "");
        makeSlider(mixSlider,      "Mix",     0.0,  1.0, p0.mix,            "");

        startTimerHz(15);
    }

    ~GuitarRotaryPanel() override
    {
        stopTimer();
        for (auto* s : getAllSliders()) s->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(10);

        // Single row — 8 sliders evenly spaced
        auto allSliders = getAllSliders();
        int n = (int)allSliders.size();
        int sw = 60, sp = 8;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        for (int i = 0; i < n; ++i)
        {
            allSliders[(size_t)i]->setBounds(sa.removeFromLeft(sw));
            if (i < n - 1) sa.removeFromLeft(sp);
        }
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        hornRateSlider->setValue(p.hornRate, juce::dontSendNotification);
        dopplerSlider->setValue(p.doppler, juce::dontSendNotification);
        tremoloSlider->setValue(p.tremolo, juce::dontSendNotification);
        rotorSlider->setValue(p.rotorRate, juce::dontSendNotification);
        driveSlider->setValue(p.drive, juce::dontSendNotification);
        waveShpSlider->setValue((double)p.waveshape, juce::dontSendNotification);
        widthSlider->setValue(p.width, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    std::vector<VerticalSlider*> getAllSliders()
    { return { hornRateSlider.get(), dopplerSlider.get(), tremoloSlider.get(),
               rotorSlider.get(), driveSlider.get(), waveShpSlider.get(),
               widthSlider.get(), mixSlider.get() }; }

    void timerCallback() override
    {
        auto p = proc.getParams();
        auto sync = [](VerticalSlider* s, double v) {
            if (!s->getSlider().isMouseOverOrDragging()) s->setValue(v, juce::dontSendNotification); };
        sync(hornRateSlider.get(), p.hornRate); sync(dopplerSlider.get(), p.doppler);
        sync(tremoloSlider.get(), p.tremolo); sync(rotorSlider.get(), p.rotorRate);
        sync(driveSlider.get(), p.drive); sync(waveShpSlider.get(), (double)p.waveshape);
        sync(widthSlider.get(), p.width); sync(mixSlider.get(), p.mix);
        bool on = !proc.isBypassed();
        if (toggleButton->getToggleState() != on)
            toggleButton->setToggleState(on, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarRotaryProcessor::Params p;
        p.hornRate  = (float)hornRateSlider->getValue();
        p.doppler   = (float)dopplerSlider->getValue();
        p.tremolo   = (float)tremoloSlider->getValue();
        p.rotorRate = (float)rotorSlider->getValue();
        p.drive     = (float)driveSlider->getValue();
        p.waveshape = (int)waveShpSlider->getValue();
        p.width     = (float)widthSlider->getValue();
        p.mix       = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarRotaryProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> hornRateSlider, dopplerSlider, tremoloSlider, rotorSlider,
                                    driveSlider, waveShpSlider, widthSlider, mixSlider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarRotaryPanel)
};

// ==============================================================================
//  Guitar Wah Panel (2 rows: 4+4 sliders)
// ==============================================================================
class GuitarWahPanel : public juce::Component, private juce::Timer
{
public:
    GuitarWahPanel(GuitarWahProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Wah", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();
        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix,
                              double step = 0.0) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, step > 0.0 ? step : (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        // Row 1
        makeSlider(pedalSlider,  "Pedal",  0.0, 1.0, p0.pedal,           "");
        makeSlider(modeSlider,   "Mode",   0.0, 2.0, (double)p0.mode,    "", 1.0);
        makeSlider(modelSlider,  "Model",  0.0, 2.0, (double)p0.model,   "", 1.0);
        makeSlider(qSlider,      "Q",      1.0, 15.0, p0.q,              "");
        // Row 2
        makeSlider(sensSlider,   "Sens",   0.0, 1.0, p0.sens,            "");
        makeSlider(attackSlider, "Attack", 0.0, 1.0, p0.attack,          "");
        makeSlider(lfoSlider,    "LFO",    0.1, 10.0, p0.lfoRate,        " Hz");
        makeSlider(mixSlider,    "Mix",    0.0, 1.0, p0.mix,             "");

        startTimerHz(15);
    }

    ~GuitarWahPanel() override
    {
        stopTimer();
        for (auto* s : getAllSliders()) s->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(10);

        // Single row — 8 sliders evenly spaced
        auto allSliders = getAllSliders();
        int n = (int)allSliders.size();
        int sw = 60, sp = 8;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        for (int i = 0; i < n; ++i)
        {
            allSliders[(size_t)i]->setBounds(sa.removeFromLeft(sw));
            if (i < n - 1) sa.removeFromLeft(sp);
        }
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        pedalSlider->setValue(p.pedal, juce::dontSendNotification);
        modeSlider->setValue((double)p.mode, juce::dontSendNotification);
        modelSlider->setValue((double)p.model, juce::dontSendNotification);
        qSlider->setValue(p.q, juce::dontSendNotification);
        sensSlider->setValue(p.sens, juce::dontSendNotification);
        attackSlider->setValue(p.attack, juce::dontSendNotification);
        lfoSlider->setValue(p.lfoRate, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
    }

private:
    std::vector<VerticalSlider*> getAllSliders()
    { return { pedalSlider.get(), modeSlider.get(), modelSlider.get(), qSlider.get(),
               sensSlider.get(), attackSlider.get(), lfoSlider.get(), mixSlider.get() }; }

    void timerCallback() override
    {
        auto p = proc.getParams();
        auto sync = [](VerticalSlider* s, double v) {
            if (!s->getSlider().isMouseOverOrDragging()) s->setValue(v, juce::dontSendNotification); };
        sync(pedalSlider.get(), p.pedal); sync(modeSlider.get(), (double)p.mode);
        sync(modelSlider.get(), (double)p.model); sync(qSlider.get(), p.q);
        sync(sensSlider.get(), p.sens); sync(attackSlider.get(), p.attack);
        sync(lfoSlider.get(), p.lfoRate); sync(mixSlider.get(), p.mix);
        bool on = !proc.isBypassed();
        if (toggleButton->getToggleState() != on)
            toggleButton->setToggleState(on, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarWahProcessor::Params p;
        p.pedal       = (float)pedalSlider->getValue();
        p.mode        = (int)modeSlider->getValue();
        p.model       = (int)modelSlider->getValue();
        p.q           = (float)qSlider->getValue();
        p.sens = (float)sensSlider->getValue();
        p.attack      = (float)attackSlider->getValue();
        p.lfoRate     = (float)lfoSlider->getValue();
        p.mix         = (float)mixSlider->getValue();
        proc.setParams(p);
    }

    GuitarWahProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> pedalSlider, modeSlider, modelSlider, qSlider,
                                    sensSlider, attackSlider, lfoSlider, mixSlider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarWahPanel)
};

// ==============================================================================
//  Guitar Reverb Panel
// ==============================================================================
class GuitarReverbPanel : public juce::Component, private juce::Timer
{
public:
    GuitarReverbPanel(GuitarReverbProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Reverb", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(sizeSlider, "Size", 0.0, 1.0, p0.size, "");
        makeSlider(dampingSlider, "Damping", 0.0, 1.0, p0.damping, "");
        makeSlider(mixSlider, "Mix", 0.0, 1.0, p0.mix, "");
        makeSlider(widthSlider, "Width", 0.0, 1.0, p0.width, "");

        startTimerHz(15);
    }

    ~GuitarReverbPanel() override
    {
        stopTimer();
        sizeSlider->getSlider().setLookAndFeel(nullptr);
        dampingSlider->getSlider().setLookAndFeel(nullptr);
        mixSlider->getSlider().setLookAndFeel(nullptr);
        widthSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        sizeSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        dampingSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        mixSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        widthSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        sizeSlider->setValue(p.size, juce::dontSendNotification);
        dampingSlider->setValue(p.damping, juce::dontSendNotification);
        mixSlider->setValue(p.mix, juce::dontSendNotification);
        widthSlider->setValue(p.width, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!sizeSlider->getSlider().isMouseOverOrDragging())
            sizeSlider->setValue(p.size, juce::dontSendNotification);
        if (!dampingSlider->getSlider().isMouseOverOrDragging())
            dampingSlider->setValue(p.damping, juce::dontSendNotification);
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(p.mix, juce::dontSendNotification);
        if (!widthSlider->getSlider().isMouseOverOrDragging())
            widthSlider->setValue(p.width, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarReverbProcessor::Params p;
        p.size    = (float)sizeSlider->getValue();
        p.damping = (float)dampingSlider->getValue();
        p.mix     = (float)mixSlider->getValue();
        p.width   = (float)widthSlider->getValue();
        proc.setParams(p);
    }

    GuitarReverbProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> sizeSlider, dampingSlider, mixSlider, widthSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarReverbPanel)
};

// ==============================================================================
//  Guitar Noise Gate Panel
// ==============================================================================
class GuitarNoiseGatePanel : public juce::Component, private juce::Timer
{
public:
    GuitarNoiseGatePanel(GuitarNoiseGateProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Noise Gate", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(threshSlider, "Threshold", -80.0, 0.0, p0.thresholdDb, " dB");
        makeSlider(attackSlider, "Attack", 0.1, 20.0, p0.attackMs, " ms");
        makeSlider(holdSlider, "Hold", 0.0, 500.0, p0.holdMs, " ms");
        makeSlider(releaseSlider, "Release", 5.0, 500.0, p0.releaseMs, " ms");

        startTimerHz(15);
    }

    ~GuitarNoiseGatePanel() override
    {
        stopTimer();
        threshSlider->getSlider().setLookAndFeel(nullptr);
        attackSlider->getSlider().setLookAndFeel(nullptr);
        holdSlider->getSlider().setLookAndFeel(nullptr);
        releaseSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        threshSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        attackSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        holdSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        releaseSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        threshSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        holdSlider->setValue(p.holdMs, juce::dontSendNotification);
        releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!threshSlider->getSlider().isMouseOverOrDragging())
            threshSlider->setValue(p.thresholdDb, juce::dontSendNotification);
        if (!attackSlider->getSlider().isMouseOverOrDragging())
            attackSlider->setValue(p.attackMs, juce::dontSendNotification);
        if (!holdSlider->getSlider().isMouseOverOrDragging())
            holdSlider->setValue(p.holdMs, juce::dontSendNotification);
        if (!releaseSlider->getSlider().isMouseOverOrDragging())
            releaseSlider->setValue(p.releaseMs, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        GuitarNoiseGateProcessor::Params p;
        p.thresholdDb = (float)threshSlider->getValue();
        p.attackMs    = (float)attackSlider->getValue();
        p.holdMs      = (float)holdSlider->getValue();
        p.releaseMs   = (float)releaseSlider->getValue();
        proc.setParams(p);
    }

    GuitarNoiseGateProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> threshSlider, attackSlider, holdSlider, releaseSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarNoiseGatePanel)
};

// ==============================================================================
//  Tone Stack Panel (5 sliders)
// ==============================================================================
class ToneStackPanel : public juce::Component, private juce::Timer
{
public:
    ToneStackPanel(ToneStackProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Tone Stack", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(modelSlider, "Model", 0.0, 2.0, (double)p0.model, "");
        makeSlider(bassSlider, "Bass", 0.0, 1.0, p0.bass, "");
        makeSlider(midSlider, "Mid", 0.0, 1.0, p0.mid, "");
        makeSlider(trebleSlider, "Treble", 0.0, 1.0, p0.treble, "");
        makeSlider(gainSlider, "Gain", 0.0, 2.0, p0.gain, "");

        startTimerHz(15);
    }

    ~ToneStackPanel() override
    {
        stopTimer();
        modelSlider->getSlider().setLookAndFeel(nullptr);
        bassSlider->getSlider().setLookAndFeel(nullptr);
        midSlider->getSlider().setLookAndFeel(nullptr);
        trebleSlider->getSlider().setLookAndFeel(nullptr);
        gainSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 5;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        modelSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        bassSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        midSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        trebleSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        gainSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        modelSlider->setValue((double)p.model, juce::dontSendNotification);
        bassSlider->setValue(p.bass, juce::dontSendNotification);
        midSlider->setValue(p.mid, juce::dontSendNotification);
        trebleSlider->setValue(p.treble, juce::dontSendNotification);
        gainSlider->setValue(p.gain, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!modelSlider->getSlider().isMouseOverOrDragging())
            modelSlider->setValue((double)p.model, juce::dontSendNotification);
        if (!bassSlider->getSlider().isMouseOverOrDragging())
            bassSlider->setValue(p.bass, juce::dontSendNotification);
        if (!midSlider->getSlider().isMouseOverOrDragging())
            midSlider->setValue(p.mid, juce::dontSendNotification);
        if (!trebleSlider->getSlider().isMouseOverOrDragging())
            trebleSlider->setValue(p.treble, juce::dontSendNotification);
        if (!gainSlider->getSlider().isMouseOverOrDragging())
            gainSlider->setValue(p.gain, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        ToneStackProcessor::Params p;
        p.model  = (int)modelSlider->getValue();
        p.bass   = (float)bassSlider->getValue();
        p.mid    = (float)midSlider->getValue();
        p.treble = (float)trebleSlider->getValue();
        p.gain   = (float)gainSlider->getValue();
        proc.setParams(p);
    }

    ToneStackProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> modelSlider, bassSlider, midSlider, trebleSlider, gainSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToneStackPanel)
};

// ==============================================================================
//  Cabinet Simulator Panel
// ==============================================================================
class CabSimPanel : public juce::Component, private juce::Timer
{
public:
    CabSimPanel(CabSimProcessor& p, PresetManager&) : proc(p)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { proc.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Cabinet Sim", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto p0 = proc.getParams();

        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              double min, double max, double value, const juce::String& suffix,
                              double step = 0.0)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setRange(min, max, step > 0.0 ? step : (max - min) / 100.0);
            s->setValue(value);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(s.get());
        };

        makeSlider(cabinetSlider, "Cabinet", 0.0, 3.0, (double)p0.cabinet, "", 1.0);
        makeSlider(micSlider, "Mic", 0.0, 2.0, (double)p0.mic, "", 1.0);
        makeSlider(micPosSlider, "Mic Pos", 0.0, 1.0, p0.micPos, "");
        makeSlider(levelSlider, "Level", 0.0, 2.0, p0.level, "");

        startTimerHz(15);
    }

    ~CabSimPanel() override
    {
        stopTimer();
        cabinetSlider->getSlider().setLookAndFeel(nullptr);
        micSlider->getSlider().setLookAndFeel(nullptr);
        micPosSlider->getSlider().setLookAndFeel(nullptr);
        levelSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(15);
        int sw = 60, sp = 12, n = 4;
        int tw = n * sw + (n - 1) * sp;
        auto sa = area.withX(area.getX() + (area.getWidth() - tw) / 2).withWidth(tw);
        cabinetSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        micSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        micPosSlider->setBounds(sa.removeFromLeft(sw)); sa.removeFromLeft(sp);
        levelSlider->setBounds(sa.removeFromLeft(sw));
    }

    void updateFromPreset()
    {
        auto p = proc.getParams();
        toggleButton->setToggleState(!proc.isBypassed(), juce::dontSendNotification);
        cabinetSlider->setValue((double)p.cabinet, juce::dontSendNotification);
        micSlider->setValue((double)p.mic, juce::dontSendNotification);
        micPosSlider->setValue(p.micPos, juce::dontSendNotification);
        levelSlider->setValue(p.level, juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto p = proc.getParams();
        if (!cabinetSlider->getSlider().isMouseOverOrDragging())
            cabinetSlider->setValue((double)p.cabinet, juce::dontSendNotification);
        if (!micSlider->getSlider().isMouseOverOrDragging())
            micSlider->setValue((double)p.mic, juce::dontSendNotification);
        if (!micPosSlider->getSlider().isMouseOverOrDragging())
            micPosSlider->setValue(p.micPos, juce::dontSendNotification);
        if (!levelSlider->getSlider().isMouseOverOrDragging())
            levelSlider->setValue(p.level, juce::dontSendNotification);

        bool shouldBeOn = !proc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        CabSimProcessor::Params p;
        p.cabinet = (int)cabinetSlider->getValue();
        p.mic     = (int)micSlider->getValue();
        p.micPos  = (float)micPosSlider->getValue();
        p.level   = (float)levelSlider->getValue();
        proc.setParams(p);
    }

    CabSimProcessor& proc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> cabinetSlider, micSlider, micPosSlider, levelSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabSimPanel)
};