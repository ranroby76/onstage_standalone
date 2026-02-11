// ==============================================================================
//  PreAmpPanel.h
//  OnStage - Pre-Amplifier UI
//
//  Tall panel with vertical gain slider (0 to +30 dB) and gain meter
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/PreAmpProcessor.h"

class PresetManager;

// Vertical gain meter visualization
class PreAmpMeterComponent : public juce::Component, private juce::Timer
{
public:
    PreAmpMeterComponent (PreAmpProcessor& proc) : preampProcessor (proc)
    {
        startTimerHz (30);
    }

    ~PreAmpMeterComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillRect (bounds);

        float gainDb = preampProcessor.getGainDb();
        float gainNorm = gainDb / 30.0f;

        // Vertical bar area
        auto barBounds = bounds.reduced (6, 10);

        g.setColour (juce::Colour (0xFF1A1A1A));
        g.fillRoundedRectangle (barBounds, 4.0f);

        // Fill from bottom
        auto fillBounds = barBounds;
        float fillH = barBounds.getHeight() * gainNorm;
        fillBounds.setY (barBounds.getBottom() - fillH);
        fillBounds.setHeight (fillH);

        // Gradient from green to gold to red
        juce::Colour fillColor;
        if (gainNorm < 0.5f)
            fillColor = juce::Colour (0xFF00CC44).interpolatedWith (juce::Colour (0xFFD4AF37), gainNorm * 2.0f);
        else
            fillColor = juce::Colour (0xFFD4AF37).interpolatedWith (juce::Colour (0xFFCC4444), (gainNorm - 0.5f) * 2.0f);

        g.setColour (fillColor);
        g.fillRoundedRectangle (fillBounds, 4.0f);

        // dB markers (horizontal lines)
        g.setFont (9.0f);
        for (int db = 0; db <= 30; db += 5)
        {
            float y = barBounds.getBottom() - (db / 30.0f) * barBounds.getHeight();
            g.setColour (juce::Colour (0xFF404040));
            g.drawHorizontalLine ((int)y, barBounds.getX(), barBounds.getRight());

            if (db % 10 == 0)
            {
                g.setColour (juce::Colour (0xFF606060));
                g.drawText ("+" + juce::String (db),
                            (int)barBounds.getRight() + 2, (int)y - 6, 28, 12,
                            juce::Justification::centredLeft);
            }
        }

        // Current value display at top
        g.setColour (juce::Colour (0xFFD4AF37));
        g.setFont (juce::Font (18.0f, juce::Font::bold));
        juce::String valueText = (gainDb > 0 ? "+" : "") + juce::String (gainDb, 1) + " dB";
        g.drawText (valueText, bounds.removeFromTop (28), juce::Justification::centred);

        // Border
        g.setColour (juce::Colour (0xFF404040));
        g.drawRect (getLocalBounds().toFloat(), 1.0f);
    }

    void timerCallback() override { repaint(); }

private:
    PreAmpProcessor& preampProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreAmpMeterComponent)
};

class PreAmpPanel : public juce::Component, private juce::Timer
{
public:
    PreAmpPanel (PreAmpProcessor& proc, PresetManager& /*presets*/)
        : preampProcessor (proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState (!preampProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]()
        {
            preampProcessor.setBypassed (!toggleButton->getToggleState());
        };
        addAndMakeVisible (toggleButton.get());

        // Title
        titleLabel.setText ("Pre-Amp", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (titleLabel);

        // Vertical gain slider
        gainSlider = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
        gainSlider->setRange (0.0, 30.0, 0.1);
        gainSlider->setValue (preampProcessor.getGainDb());
        gainSlider->setTextValueSuffix (" dB");
        gainSlider->setLookAndFeel (goldenLookAndFeel.get());
        gainSlider->onValueChange = [this]()
        {
            preampProcessor.setGainDb ((float)gainSlider->getValue());
        };
        addAndMakeVisible (gainSlider.get());

        // Gain label
        gainLabel.setText ("GAIN", juce::dontSendNotification);
        gainLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        gainLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        gainLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (gainLabel);

        // Meter
        meterComponent = std::make_unique<PreAmpMeterComponent> (preampProcessor);
        addAndMakeVisible (meterComponent.get());

        setSize (200, 400);

        startTimerHz (15);
    }

    ~PreAmpPanel() override
    {
        stopTimer();
        gainSlider->setLookAndFeel (nullptr);
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

        // Title row
        auto titleRow = area.removeFromTop (24);
        toggleButton->setBounds (titleRow.removeFromRight (40).withSizeKeepingCentre (40, 40));
        titleLabel.setBounds (titleRow);
        area.removeFromTop (6);

        // Left side: gain label + vertical slider
        // Right side: meter
        auto leftArea = area.removeFromLeft (area.getWidth() / 2 - 4);
        area.removeFromLeft (8);
        auto rightArea = area;

        // Gain label at top of left
        gainLabel.setBounds (leftArea.removeFromTop (18));
        leftArea.removeFromTop (4);

        // Vertical slider fills remaining left area
        gainSlider->setBounds (leftArea);

        // Meter fills right area
        meterComponent->setBounds (rightArea);
    }

    void updateFromPreset()
    {
        gainSlider->setValue (preampProcessor.getGainDb(), juce::dontSendNotification);
        toggleButton->setToggleState (!preampProcessor.isBypassed(), juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        if (!gainSlider->isMouseOverOrDragging())
            gainSlider->setValue (preampProcessor.getGainDb(), juce::dontSendNotification);

        bool shouldBeOn = !preampProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState (shouldBeOn, juce::dontSendNotification);
    }

    PreAmpProcessor& preampProcessor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::Label gainLabel;
    std::unique_ptr<juce::Slider> gainSlider;
    std::unique_ptr<PreAmpMeterComponent> meterComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreAmpPanel)
};