// ==============================================================================
//  DoublerPanel.h
//  OnStage — ADT (Automatic Double Tracking) UI Panel
//
//  6 vertical sliders + visualization showing dry signal and two delay taps
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/DoublerProcessor.h"

class PresetManager;

// ==============================================================================
//  ADT Visualization — shows dry signal + two delay tap positions on timeline
// ==============================================================================
class ADTGraphComponent : public juce::Component, private juce::Timer
{
public:
    ADTGraphComponent (DoublerProcessor& proc) : doublerProc (proc)
    {
        startTimerHz (30);
    }

    ~ADTGraphComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto p = doublerProc.getParams();

        // Background
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillRect (bounds);

        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        // Timeline axis
        g.setColour (juce::Colour (0xFF2A2A2A));
        float axisY = cy;
        g.drawLine (bounds.getX() + 10, axisY, bounds.getRight() - 10, axisY, 1.0f);

        // Time arrow label
        g.setColour (juce::Colour (0xFF555555));
        g.setFont (9.0f);
        g.drawText ("TIME", bounds.getRight() - 40, axisY + 4, 35, 12, juce::Justification::centredRight);

        // === Dry signal (left side, golden) ===
        float dryX = bounds.getX() + 30;
        float pulse = 1.0f + std::sin (animPhase * 3.0f) * 0.04f;
        float drySize = 26.0f * pulse;

        // Headroom affects saturation glow
        float headroomGlow = p.headroom;
        g.setColour (juce::Colour (0xFFD4AF37).withAlpha (0.15f + headroomGlow * 0.2f));
        g.fillEllipse (dryX - drySize * 0.7f, axisY - drySize * 0.7f,
                       drySize * 1.4f, drySize * 1.4f);
        g.setColour (juce::Colour (0xFFD4AF37));
        g.fillEllipse (dryX - drySize / 2, axisY - drySize / 2, drySize, drySize);
        g.setColour (juce::Colour (0xFFFFD700));
        g.drawEllipse (dryX - drySize / 2, axisY - drySize / 2, drySize, drySize, 1.5f);

        g.setColour (juce::Colour (0xFF888888));
        g.setFont (9.0f);
        g.drawText ("DRY", dryX - 15, axisY - drySize / 2 - 14, 30, 12, juce::Justification::centred);

        // Available width for delay positioning
        float delayAreaLeft = dryX + drySize / 2 + 15;
        float delayAreaRight = bounds.getRight() - 20;
        float delayRange = delayAreaRight - delayAreaLeft;

        // === Tap A ===
        float intensityA = p.levelA - 0.5f;
        if (std::abs (intensityA) > 0.001f)
        {
            float delayNormA = std::pow (p.delayA, 4.0f);
            float tapAX = delayAreaLeft + delayNormA * delayRange;
            float tapASize = 12.0f + std::abs (intensityA) * 30.0f;
            tapASize *= pulse;

            juce::Colour tapAColor = (intensityA < 0.0f)
                ? juce::Colour (0xFF4466CC)
                : juce::Colour (0xFF44AADD);

            // Connection line from dry to tap
            g.setColour (tapAColor.withAlpha (0.25f));
            float dashA[] = { 4.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (dryX + drySize / 2, axisY, tapAX, axisY - 18),
                              dashA, 2, 1.0f);

            // Glow
            g.setColour (tapAColor.withAlpha (0.12f));
            g.fillEllipse (tapAX - tapASize * 0.7f, axisY - 18 - tapASize * 0.7f,
                           tapASize * 1.4f, tapASize * 1.4f);

            // Main circle
            g.setColour (tapAColor.withAlpha (0.7f));
            g.fillEllipse (tapAX - tapASize / 2, axisY - 18 - tapASize / 2, tapASize, tapASize);
            g.setColour (tapAColor);
            g.drawEllipse (tapAX - tapASize / 2, axisY - 18 - tapASize / 2, tapASize, tapASize, 1.5f);

            // Inversion indicator
            if (intensityA < 0.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.setFont (10.0f);
                g.drawText ("INV", tapAX - 12, axisY - 18 - 5, 24, 10, juce::Justification::centred);
            }

            g.setColour (tapAColor.withAlpha (0.8f));
            g.setFont (9.0f);
            g.drawText ("A", tapAX - 8, axisY - 18 - tapASize / 2 - 14, 16, 12, juce::Justification::centred);
        }

        // === Tap B ===
        float intensityB = p.levelB - 0.5f;
        if (std::abs (intensityB) > 0.001f)
        {
            float delayNormB = std::pow (p.delayB, 4.0f);
            float tapBX = delayAreaLeft + delayNormB * delayRange;
            float tapBSize = 12.0f + std::abs (intensityB) * 30.0f;
            tapBSize *= pulse;

            juce::Colour tapBColor = (intensityB < 0.0f)
                ? juce::Colour (0xFFCC6644)
                : juce::Colour (0xFFDDAA44);

            // Connection line
            g.setColour (tapBColor.withAlpha (0.25f));
            float dashB[] = { 4.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (dryX + drySize / 2, axisY, tapBX, axisY + 18),
                              dashB, 2, 1.0f);

            // Glow
            g.setColour (tapBColor.withAlpha (0.12f));
            g.fillEllipse (tapBX - tapBSize * 0.7f, axisY + 18 - tapBSize * 0.7f,
                           tapBSize * 1.4f, tapBSize * 1.4f);

            // Main circle
            g.setColour (tapBColor.withAlpha (0.7f));
            g.fillEllipse (tapBX - tapBSize / 2, axisY + 18 - tapBSize / 2, tapBSize, tapBSize);
            g.setColour (tapBColor);
            g.drawEllipse (tapBX - tapBSize / 2, axisY + 18 - tapBSize / 2, tapBSize, tapBSize, 1.5f);

            if (intensityB < 0.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.setFont (10.0f);
                g.drawText ("INV", tapBX - 12, axisY + 18 - 5, 24, 10, juce::Justification::centred);
            }

            g.setColour (tapBColor.withAlpha (0.8f));
            g.setFont (9.0f);
            g.drawText ("B", tapBX - 8, axisY + 18 + tapBSize / 2 + 2, 16, 12, juce::Justification::centred);
        }

        // Output level indicator (small bar at right edge)
        float outLevel = p.output * 2.0f;
        float barH = bounds.getHeight() * 0.6f;
        float barY = cy - barH / 2;
        float barX = bounds.getRight() - 8;
        g.setColour (juce::Colour (0xFF333333));
        g.fillRect (barX, barY, 4.0f, barH);
        float fillH = juce::jmin (outLevel / 2.0f, 1.0f) * barH;
        juce::Colour barColor = outLevel > 1.0f ? juce::Colour (0xFFDD6644) : juce::Colour (0xFFD4AF37);
        g.setColour (barColor.withAlpha (0.7f));
        g.fillRect (barX, barY + barH - fillH, 4.0f, fillH);

        // Border
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (bounds, 1.0f);

        animPhase += 0.03f;
        if (animPhase > juce::MathConstants<float>::twoPi)
            animPhase -= juce::MathConstants<float>::twoPi;
    }

    void timerCallback() override { repaint(); }

private:
    DoublerProcessor& doublerProc;
    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADTGraphComponent)
};

// ==============================================================================
//  Main Doubler Panel
// ==============================================================================
class DoublerPanel : public juce::Component
{
public:
    DoublerPanel (DoublerProcessor& proc, PresetManager& /*presets*/)
        : doublerProc (proc)
    {
        goldenLAF = std::make_unique<GoldenSliderLookAndFeel>();

        // Toggle
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState (!doublerProc.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]()
        {
            doublerProc.setBypassed (!toggleButton->getToggleState());
        };
        addAndMakeVisible (toggleButton.get());

        // Title
        titleLabel.setText ("DOUBLER", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (titleLabel);

        auto p = doublerProc.getParams();

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

        make (headroomSlider, "Headroom", 0.0, 1.0, p.headroom);
        make (delayASlider,   "A Delay",  0.0, 1.0, p.delayA);
        make (levelASlider,   "A Level",  0.0, 1.0, p.levelA);
        make (delayBSlider,   "B Delay",  0.0, 1.0, p.delayB);
        make (levelBSlider,   "B Level",  0.0, 1.0, p.levelB);
        make (outputSlider,   "Output",   0.0, 1.0, p.output);

        // Graph
        graphComponent = std::make_unique<ADTGraphComponent> (doublerProc);
        addAndMakeVisible (graphComponent.get());

        setSize (620, 280);
    }

    ~DoublerPanel() override
    {
        headroomSlider->getSlider().setLookAndFeel (nullptr);
        delayASlider->getSlider().setLookAndFeel (nullptr);
        levelASlider->getSlider().setLookAndFeel (nullptr);
        delayBSlider->getSlider().setLookAndFeel (nullptr);
        levelBSlider->getSlider().setLookAndFeel (nullptr);
        outputSlider->getSlider().setLookAndFeel (nullptr);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);

        // Title row
        auto titleRow = area.removeFromTop (24);
        toggleButton->setBounds (titleRow.removeFromRight (40).withSizeKeepingCentre (40, 40));
        titleLabel.setBounds (titleRow.removeFromLeft (120));
        area.removeFromTop (4);

        // Sliders on left, graph on right
        constexpr int n  = 6;
        constexpr int sw = 56;
        constexpr int sp = 8;
        int slidersWidth = n * sw + (n - 1) * sp;

        auto sliderArea = area.removeFromLeft (slidersWidth);
        area.removeFromLeft (12);
        graphComponent->setBounds (area);

        // Place sliders
        auto sa = sliderArea;
        auto place = [&](std::unique_ptr<VerticalSlider>& s)
        {
            s->setBounds (sa.removeFromLeft (sw));
            sa.removeFromLeft (sp);
        };

        place (headroomSlider);
        place (delayASlider);
        place (levelASlider);
        place (delayBSlider);
        place (levelBSlider);
        place (outputSlider);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1E1E1E));
        g.setColour (juce::Colour (0xFFD4AF37).withAlpha (0.3f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2), 6.0f, 1.5f);
    }

    void updateFromPreset()
    {
        auto p = doublerProc.getParams();
        toggleButton->setToggleState (!doublerProc.isBypassed(), juce::dontSendNotification);
        headroomSlider->setValue (p.headroom, juce::dontSendNotification);
        delayASlider->setValue   (p.delayA,   juce::dontSendNotification);
        levelASlider->setValue   (p.levelA,   juce::dontSendNotification);
        delayBSlider->setValue   (p.delayB,   juce::dontSendNotification);
        levelBSlider->setValue   (p.levelB,   juce::dontSendNotification);
        outputSlider->setValue   (p.output,   juce::dontSendNotification);
    }

private:
    void pushToProcessor()
    {
        DoublerProcessor::Params p;
        p.headroom = (float) headroomSlider->getValue();
        p.delayA   = (float) delayASlider->getValue();
        p.levelA   = (float) levelASlider->getValue();
        p.delayB   = (float) delayBSlider->getValue();
        p.levelB   = (float) levelBSlider->getValue();
        p.output   = (float) outputSlider->getValue();
        doublerProc.setParams (p);
    }

    DoublerProcessor& doublerProc;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLAF;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;

    std::unique_ptr<VerticalSlider> headroomSlider, delayASlider, levelASlider;
    std::unique_ptr<VerticalSlider> delayBSlider, levelBSlider, outputSlider;

    std::unique_ptr<ADTGraphComponent> graphComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoublerPanel)
};