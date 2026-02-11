// ==============================================================================
//  ReverbPanel.h
//  OnStage — Reverb UI (IR Convolution only)
//
//  Layout: [Title + toggle] top row
//          [LOAD IR button | IR name] second row (horizontal)
//          [sliders in a row] + [graph] bottom area
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/ReverbProcessor.h"

class PresetManager;

// ==============================================================================
// IR Load Strip — horizontal: [LOAD IR button] [IR name display]
// ==============================================================================
class IRLoadStrip : public juce::Component
{
public:
    IRLoadStrip() { setRepaintsOnMouseActivity (true); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Button area (left)
        auto btnArea = bounds.removeFromLeft (70.0f).reduced (1);
        juce::Colour btnColor = isMouseOver() ? juce::Colour (0xFF4A4A4A) : juce::Colour (0xFF3A3A3A);
        g.setColour (btnColor);
        g.fillRoundedRectangle (btnArea, 3.0f);
        g.setColour (juce::Colour (0xFF606060));
        g.drawRoundedRectangle (btnArea, 3.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText ("LOAD IR", btnArea, juce::Justification::centred);

        // Name area (right)
        bounds.removeFromLeft (4);
        auto nameArea = bounds.reduced (0, 1);
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.fillRoundedRectangle (nameArea, 3.0f);
        g.setColour (juce::Colour (0xFF404040));
        g.drawRoundedRectangle (nameArea, 3.0f, 1.0f);
        g.setColour (juce::Colour (0xFFD4AF37));
        g.setFont (11.0f);
        g.drawText (irName, nameArea.reduced (8, 0), juce::Justification::centredLeft, true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && onLoadClicked)
            onLoadClicked();
    }

    void setIRName (const juce::String& name) { irName = name; repaint(); }

    std::function<void()> onLoadClicked;

private:
    juce::String irName { "Default (Internal)" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRLoadStrip)
};

// ==============================================================================
// Reverb Graph — IR impulse decay visualization
//
// Shows: decaying impulse shape (exponential envelope with dense reflections),
//        low/high cut filter overlay, duck indicator, gate threshold line,
//        live energy glow driven by actual reverb output level.
// ==============================================================================
class ReverbGraphComponent : public juce::Component, private juce::Timer
{
public:
    ReverbGraphComponent (ReverbProcessor& proc) : reverbProc (proc)
    {
        startTimerHz (30);
    }

    ~ReverbGraphComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto p = reverbProc.getParams();
        float level = reverbProc.getCurrentDecayLevel();

        // Background
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillRect (bounds);

        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float margin = 12.0f;
        float plotL = bounds.getX() + margin;
        float plotR = bounds.getRight() - margin;
        float plotW = plotR - plotL;
        float plotT = bounds.getY() + margin;
        float plotB = bounds.getBottom() - margin;
        float plotH = plotB - plotT;

        // === IR Impulse Envelope ===
        // Decaying energy shape — denser at start, exponential tail
        juce::Path irEnvelopeTop, irEnvelopeBot;
        irEnvelopeTop.startNewSubPath (plotL, cy);
        irEnvelopeBot.startNewSubPath (plotL, cy);

        float liveBoost = 1.0f + level * 2.5f;

        for (float x = 0; x <= plotW; x += 1.5f)
        {
            float t = x / plotW;  // 0..1 across time

            // Exponential decay envelope
            float decay = std::exp (-t * 4.5f);

            // Dense early reflections → sparser late
            float density = 12.0f + t * 30.0f;
            float reflections = std::sin (t * density + animPhase * 2.0f)
                              * std::cos (t * density * 0.7f + animPhase * 1.3f);

            // Initial transient spike
            float spike = (t < 0.02f) ? (1.0f - t / 0.02f) * 0.4f : 0.0f;

            float amplitude = (decay * (0.5f + std::abs (reflections) * 0.5f) + spike)
                            * plotH * 0.42f * liveBoost;

            float px = plotL + x;
            irEnvelopeTop.lineTo (px, cy - amplitude);
            irEnvelopeBot.lineTo (px, cy + amplitude);
        }

        // Close envelope paths
        irEnvelopeTop.lineTo (plotR, cy);
        irEnvelopeBot.lineTo (plotR, cy);

        // Fill envelope glow
        g.setColour (juce::Colour (0xFFD4AF37).withAlpha (0.06f + level * 0.1f));
        g.fillPath (irEnvelopeTop);
        g.fillPath (irEnvelopeBot);

        // Stroke envelope outline
        g.setColour (juce::Colour (0xFFD4AF37).withAlpha (0.4f + level * 0.4f));
        g.strokePath (irEnvelopeTop, juce::PathStrokeType (1.2f));
        g.strokePath (irEnvelopeBot, juce::PathStrokeType (1.2f));

        // Center line (time axis)
        g.setColour (juce::Colour (0xFF333333));
        g.drawHorizontalLine ((int)cy, plotL, plotR);

        // === Filter indicators ===
        // Low cut: shade left region
        float lcNorm = juce::jmap (p.lowCut, 20.0f, 500.0f, 0.0f, 0.15f);
        if (lcNorm > 0.005f)
        {
            g.setColour (juce::Colour (0xFF2244AA).withAlpha (0.15f));
            g.fillRect (plotL, plotT, lcNorm * plotW, plotH);
        }
        // High cut: shade right region
        float hcNorm = juce::jmap (p.highCut, 2000.0f, 20000.0f, 0.85f, 1.0f);
        if (hcNorm < 0.995f)
        {
            float hcX = plotL + hcNorm * plotW;
            g.setColour (juce::Colour (0xFFAA4422).withAlpha (0.12f));
            g.fillRect (hcX, plotT, plotR - hcX, plotH);
        }

        // === Gate threshold line ===
        if (p.gateSpeed > 0.01f)
        {
            float gateThreshNorm = juce::jmap (p.gateThreshold, -60.0f, 0.0f, 0.0f, 1.0f);
            float gateY = cy - gateThreshNorm * plotH * 0.42f;
            float gateY2 = cy + gateThreshNorm * plotH * 0.42f;

            g.setColour (juce::Colour (0xFFCC4444).withAlpha (0.4f));
            float dashLens[] = { 3.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (plotL, gateY, plotR, gateY), dashLens, 2, 0.8f);
            g.drawDashedLine (juce::Line<float> (plotL, gateY2, plotR, gateY2), dashLens, 2, 0.8f);

            g.setFont (8.0f);
            g.drawText ("GATE", plotR - 28, gateY - 10, 26, 10, juce::Justification::centredRight);
        }

        // === Duck indicator ===
        if (p.duck > 0.01f)
        {
            float duckAlpha = p.duck * 0.5f;
            g.setColour (juce::Colour (0xFF44AADD).withAlpha (duckAlpha * (0.3f + level * 0.5f)));
            g.setFont (9.0f);
            g.drawText ("DUCK", plotL + 4, plotT + 2, 32, 12, juce::Justification::centredLeft);
        }

        // Border
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (bounds, 1.0f);

        animPhase += 0.02f;
        if (animPhase > juce::MathConstants<float>::twoPi * 10.0f)
            animPhase -= juce::MathConstants<float>::twoPi * 10.0f;
    }

    void timerCallback() override { repaint(); }

private:
    ReverbProcessor& reverbProc;
    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbGraphComponent)
};

// ==============================================================================
// Main Reverb Panel — IR-only, clean layout
// ==============================================================================
class ReverbPanel : public juce::Component, private juce::Timer
{
public:
    ReverbPanel (ReverbProcessor& proc, PresetManager& /*presets*/)
        : reverbProc (proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        // Force IR mode
        auto params = reverbProc.getParams();
        params.type = ReverbProcessor::Type::IR;
        reverbProc.setParams (params);

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState (!reverbProc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]()
        {
            reverbProc.setBypassed (!toggleButton->getToggleState());
        };
        addAndMakeVisible (toggleButton.get());

        // Title
        titleLabel.setText ("Convo. Reverb", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (titleLabel);

        // Slider factory
        auto createSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                                double min, double max, double value,
                                const juce::String& suffix, double skew = 1.0)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText (name);
            s->setRange (min, max, (max - min) / 100.0);
            s->setValue (value);
            s->setTextValueSuffix (suffix);
            s->getSlider().setLookAndFeel (goldenLookAndFeel.get());
            s->getSlider().setSkewFactor (skew);
            s->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible (s.get());
        };

        createSlider (mixSlider,          "Mix",       0.0, 1.0,       params.mix,           "");
        createSlider (lowCutSlider,       "LowCut",    20.0, 500.0,    params.lowCut,        " Hz", 0.5);
        createSlider (highCutSlider,      "HighCut",   2000.0, 20000.0, params.highCut,      " Hz", 0.3);
        createSlider (duckSlider,         "Duck",      0.0, 1.0,       params.duck,          "");
        createSlider (irGateThreshSlider, "Gate Thr",  -60.0, 0.0,     params.gateThreshold, " dB");
        createSlider (irGateSpeedSlider,  "Gate Spd",  0.0, 1.0,       params.gateSpeed,     "");

        // IR load strip (horizontal)
        irLoadStrip = std::make_unique<IRLoadStrip>();
        irLoadStrip->setIRName (reverbProc.getCurrentIrName());
        irLoadStrip->onLoadClicked = [this]() { loadIRFile(); };
        addAndMakeVisible (irLoadStrip.get());

        // Graph
        graphComponent = std::make_unique<ReverbGraphComponent> (reverbProc);
        addAndMakeVisible (graphComponent.get());

        startTimerHz (15);
    }

    ~ReverbPanel() override
    {
        stopTimer();
        mixSlider->getSlider().setLookAndFeel (nullptr);
        lowCutSlider->getSlider().setLookAndFeel (nullptr);
        highCutSlider->getSlider().setLookAndFeel (nullptr);
        duckSlider->getSlider().setLookAndFeel (nullptr);
        irGateThreshSlider->getSlider().setLookAndFeel (nullptr);
        irGateSpeedSlider->getSlider().setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1E1E1E));
        g.setColour (juce::Colour (0xFFD4AF37).withAlpha (0.3f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2), 6.0f, 1.5f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);

        // Row 1: title + toggle
        auto titleRow = area.removeFromTop (24);
        toggleButton->setBounds (titleRow.removeFromRight (40).withSizeKeepingCentre (40, 40));
        titleLabel.setBounds (titleRow.removeFromLeft (140));
        area.removeFromTop (4);

        // Row 2: IR load strip (horizontal: button + name)
        irLoadStrip->setBounds (area.removeFromTop (28));
        area.removeFromTop (6);

        // Row 3: sliders + graph
        constexpr int numSliders = 6;
        constexpr int sw = 58;
        constexpr int sp = 6;
        int slidersWidth = numSliders * sw + (numSliders - 1) * sp;

        auto sliderArea = area.removeFromLeft (slidersWidth);
        area.removeFromLeft (10);
        graphComponent->setBounds (area);

        // Place sliders
        auto sa = sliderArea;
        auto place = [&](std::unique_ptr<VerticalSlider>& s)
        {
            s->setBounds (sa.removeFromLeft (sw));
            sa.removeFromLeft (sp);
        };

        place (mixSlider);
        place (lowCutSlider);
        place (highCutSlider);
        place (duckSlider);
        place (irGateThreshSlider);
        place (irGateSpeedSlider);
    }

    void updateFromPreset()
    {
        auto p = reverbProc.getParams();
        p.type = ReverbProcessor::Type::IR;
        reverbProc.setParams (p);

        toggleButton->setToggleState (!reverbProc.isBypassed(), juce::dontSendNotification);
        mixSlider->setValue (p.mix, juce::dontSendNotification);
        lowCutSlider->setValue (p.lowCut, juce::dontSendNotification);
        highCutSlider->setValue (p.highCut, juce::dontSendNotification);
        duckSlider->setValue (p.duck, juce::dontSendNotification);
        irGateThreshSlider->setValue (p.gateThreshold, juce::dontSendNotification);
        irGateSpeedSlider->setValue (p.gateSpeed, juce::dontSendNotification);
        irLoadStrip->setIRName (reverbProc.getCurrentIrName());
    }

private:
    void timerCallback() override
    {
        auto p = reverbProc.getParams();

        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue (p.mix, juce::dontSendNotification);
        if (!lowCutSlider->getSlider().isMouseOverOrDragging())
            lowCutSlider->setValue (p.lowCut, juce::dontSendNotification);
        if (!highCutSlider->getSlider().isMouseOverOrDragging())
            highCutSlider->setValue (p.highCut, juce::dontSendNotification);
        if (!duckSlider->getSlider().isMouseOverOrDragging())
            duckSlider->setValue (p.duck, juce::dontSendNotification);

        irLoadStrip->setIRName (reverbProc.getCurrentIrName());

        bool shouldBeOn = !reverbProc.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState (shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        ReverbProcessor::Params p = reverbProc.getParams();
        p.type = ReverbProcessor::Type::IR;
        p.mix = (float)mixSlider->getValue();
        p.lowCut = (float)lowCutSlider->getValue();
        p.highCut = (float)highCutSlider->getValue();
        p.duck = (float)duckSlider->getValue();
        p.gateThreshold = (float)irGateThreshSlider->getValue();
        p.gateSpeed = (float)irGateSpeedSlider->getValue();
        reverbProc.setParams (p);
    }

    void loadIRFile()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select Impulse Response",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.wav;*.aif;*.aiff;*.flac");

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (flags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                auto p = reverbProc.getParams();
                p.irFilePath = file.getFullPathName();
                reverbProc.setParams (p);
                irLoadStrip->setIRName (reverbProc.getCurrentIrName());
            }
        });
    }

    ReverbProcessor& reverbProc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;

    std::unique_ptr<VerticalSlider> mixSlider, lowCutSlider, highCutSlider;
    std::unique_ptr<VerticalSlider> duckSlider, irGateThreshSlider, irGateSpeedSlider;

    std::unique_ptr<IRLoadStrip> irLoadStrip;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<ReverbGraphComponent> graphComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbPanel)
};
