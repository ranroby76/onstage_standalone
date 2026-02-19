#pragma once
// ==============================================================================
//  TransientSplitterPanel.h
//  OnStage — Transient Splitter UI Panel
//
//  Knobs: Sensitivity, Decay, Hold, Smoothing, HP Focus, LP Focus,
//         Transient Gain, Sustain Gain, Balance
//  Toggles: Stereo Link, Gate Mode, Invert
//  Meters: Transient/Sustain RMS, Activity indicator
// ==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/TransientSplitterProcessor.h"

// ==============================================================================
//  Activity Meter — shows transient detection level + T/S RMS bars
// ==============================================================================
class TransientSplitterMeter : public juce::Component, private juce::Timer
{
public:
    TransientSplitterMeter (TransientSplitterProcessor& p) : proc (p)
    {
        startTimerHz (30);
    }
    ~TransientSplitterMeter() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillRoundedRectangle (b, 4.0f);

        float barH = (b.getHeight() - 20.0f) / 3.0f;
        float y = b.getY() + 2.0f;
        float x = b.getX() + 4.0f;
        float w = b.getWidth() - 8.0f;

        // --- Activity bar ---
        g.setColour (juce::Colour (0xFF555555));
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText ("ACTIVITY", x, y, w, 10.0f, juce::Justification::centredLeft);
        y += 11.0f;

        float actW = w * juce::jlimit (0.0f, 1.0f, activity);
        g.setColour (juce::Colour (0xFF1A1A2E));
        g.fillRoundedRectangle (x, y, w, barH - 2, 2.0f);
        auto actCol = activity > 0.5f ? juce::Colour (0xFFDD8800) : juce::Colour (0xFF00AA55);
        g.setColour (actCol);
        g.fillRoundedRectangle (x, y, actW, barH - 2, 2.0f);
        y += barH + 2.0f;

        // --- Transient RMS ---
        g.setColour (juce::Colour (0xFF555555));
        g.drawText ("TRANSIENT", x, y, w, 10.0f, juce::Justification::centredLeft);
        y += 11.0f;

        float tRms = juce::jlimit (0.0f, 1.0f, (tL + tR) * 0.5f);
        g.setColour (juce::Colour (0xFF1A1A2E));
        g.fillRoundedRectangle (x, y, w, barH - 2, 2.0f);
        g.setColour (juce::Colour (0xFFD4AF37));
        g.fillRoundedRectangle (x, y, w * tRms, barH - 2, 2.0f);
        y += barH + 2.0f;

        // --- Sustain RMS ---
        g.setColour (juce::Colour (0xFF555555));
        g.drawText ("SUSTAIN", x, y, w, 10.0f, juce::Justification::centredLeft);
        y += 11.0f;

        float sRms = juce::jlimit (0.0f, 1.0f, (sL + sR) * 0.5f);
        g.setColour (juce::Colour (0xFF1A1A2E));
        g.fillRoundedRectangle (x, y, w, barH - 2, 2.0f);
        g.setColour (juce::Colour (0xFF4488CC));
        g.fillRoundedRectangle (x, y, w * sRms, barH - 2, 2.0f);

        // Border
        g.setColour (juce::Colour (0xFF333333));
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
    }

private:
    void timerCallback() override
    {
        tL = proc.transientRmsL.load();
        tR = proc.transientRmsR.load();
        sL = proc.sustainRmsL.load();
        sR = proc.sustainRmsR.load();
        activity = proc.transientActivity.load();
        repaint();
    }

    TransientSplitterProcessor& proc;
    float tL = 0, tR = 0, sL = 0, sR = 0, activity = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientSplitterMeter)
};

// ==============================================================================
//  Mode Toggle Button (golden on/off)
// ==============================================================================
class SplitterModeButton : public juce::Button
{
public:
    SplitterModeButton (const juce::String& text) : juce::Button (text)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool hover, bool /*down*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        bool isOn = getToggleState();

        g.setColour (isOn ? juce::Colour (0xFFD4AF37) : (hover ? juce::Colour (0xFF3A3A3A)
                                                                : juce::Colour (0xFF2A2A2A)));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        g.setColour (isOn ? juce::Colours::black : juce::Colours::white);
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (getButtonText(), bounds, juce::Justification::centred);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplitterModeButton)
};

// ==============================================================================
//  TransientSplitterPanel
// ==============================================================================
class TransientSplitterPanel : public juce::Component, private juce::Timer
{
public:
    TransientSplitterPanel (TransientSplitterProcessor& proc)
        : processor (proc), meter (proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (toggleButton.get());

        // Title
        titleLabel.setText ("Transient Splitter", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (titleLabel);

        // --- Knobs ---
        auto setupKnob = [this] (StyledSlider& s, juce::Label& l, const juce::String& name,
                                  double min, double max, double def, double step, const juce::String& suffix)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
            s.setRange (min, max, step);
            s.setValue (def, juce::dontSendNotification);
            s.setTextValueSuffix (suffix);
            s.setLookAndFeel (goldenLookAndFeel.get());
            addAndMakeVisible (s);

            l.setText (name, juce::dontSendNotification);
            l.setFont (juce::Font (9.5f, juce::Font::bold));
            l.setColour (juce::Label::textColourId, juce::Colour (0xFF999999));
            l.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (l);
        };

        setupKnob (sensitivityKnob, sensitivityLabel, "SENSITIVITY", 0.0, 1.0, 0.5, 0.01, "");
        setupKnob (decayKnob,       decayLabel,       "DECAY",       1.0, 500.0, 50.0, 1.0, " ms");
        setupKnob (holdKnob,        holdLabel,        "HOLD",        0.0, 100.0, 10.0, 0.5, " ms");
        setupKnob (smoothKnob,      smoothLabel,      "SMOOTH",      0.1, 50.0, 2.0, 0.1, " ms");
        setupKnob (hpFocusKnob,     hpFocusLabel,     "HP FOCUS",    20.0, 5000.0, 20.0, 1.0, " Hz");
        setupKnob (lpFocusKnob,     lpFocusLabel,     "LP FOCUS",    200.0, 20000.0, 20000.0, 1.0, " Hz");
        setupKnob (transGainKnob,   transGainLabel,   "T GAIN",      -60.0, 12.0, 0.0, 0.1, " dB");
        setupKnob (sustGainKnob,    sustGainLabel,    "S GAIN",      -60.0, 12.0, 0.0, 0.1, " dB");
        setupKnob (balanceKnob,     balanceLabel,     "BALANCE",     -1.0, 1.0, 0.0, 0.01, "");

        hpFocusKnob.setSkewFactorFromMidPoint (200.0);
        lpFocusKnob.setSkewFactorFromMidPoint (3000.0);

        // --- Mode Toggles ---
        stereoLinkBtn.setButtonText ("STEREO LINK");
        stereoLinkBtn.setToggleState (processor.stereoLinked.load(), juce::dontSendNotification);
        addAndMakeVisible (stereoLinkBtn);

        gateModeBtn.setButtonText ("GATE");
        gateModeBtn.setToggleState (processor.gateMode.load(), juce::dontSendNotification);
        addAndMakeVisible (gateModeBtn);

        invertBtn.setButtonText ("INVERT");
        invertBtn.setToggleState (processor.invertMode.load(), juce::dontSendNotification);
        addAndMakeVisible (invertBtn);

        // --- Meter ---
        addAndMakeVisible (meter);

        // Load current values
        sensitivityKnob.setValue (processor.sensitivity.load(), juce::dontSendNotification);
        decayKnob.setValue       (processor.decay.load(), juce::dontSendNotification);
        holdKnob.setValue        (processor.holdTime.load(), juce::dontSendNotification);
        smoothKnob.setValue      (processor.smoothing.load(), juce::dontSendNotification);
        hpFocusKnob.setValue     (processor.focusHPFreq.load(), juce::dontSendNotification);
        lpFocusKnob.setValue     (processor.focusLPFreq.load(), juce::dontSendNotification);
        transGainKnob.setValue   (processor.transientGainDb.load(), juce::dontSendNotification);
        sustGainKnob.setValue    (processor.sustainGainDb.load(), juce::dontSendNotification);
        balanceKnob.setValue     (processor.balance.load(), juce::dontSendNotification);

        // Callbacks
        sensitivityKnob.onValueChange = [this] { processor.sensitivity.store    ((float) sensitivityKnob.getValue()); };
        decayKnob.onValueChange       = [this] { processor.decay.store          ((float) decayKnob.getValue()); };
        holdKnob.onValueChange        = [this] { processor.holdTime.store       ((float) holdKnob.getValue()); };
        smoothKnob.onValueChange      = [this] { processor.smoothing.store      ((float) smoothKnob.getValue()); };
        hpFocusKnob.onValueChange     = [this] { processor.focusHPFreq.store    ((float) hpFocusKnob.getValue()); };
        lpFocusKnob.onValueChange     = [this] { processor.focusLPFreq.store    ((float) lpFocusKnob.getValue()); };
        transGainKnob.onValueChange   = [this] { processor.transientGainDb.store((float) transGainKnob.getValue()); };
        sustGainKnob.onValueChange    = [this] { processor.sustainGainDb.store  ((float) sustGainKnob.getValue()); };
        balanceKnob.onValueChange     = [this] { processor.balance.store        ((float) balanceKnob.getValue()); };

        stereoLinkBtn.onClick = [this] { processor.stereoLinked.store (stereoLinkBtn.getToggleState()); };
        gateModeBtn.onClick   = [this] { processor.gateMode.store    (gateModeBtn.getToggleState()); };
        invertBtn.onClick     = [this] { processor.invertMode.store  (invertBtn.getToggleState()); };

        startTimerHz (10);
    }

    ~TransientSplitterPanel() override
    {
        stopTimer();
        sensitivityKnob.setLookAndFeel (nullptr);
        decayKnob.setLookAndFeel (nullptr);
        holdKnob.setLookAndFeel (nullptr);
        smoothKnob.setLookAndFeel (nullptr);
        hpFocusKnob.setLookAndFeel (nullptr);
        lpFocusKnob.setLookAndFeel (nullptr);
        transGainKnob.setLookAndFeel (nullptr);
        sustGainKnob.setLookAndFeel (nullptr);
        balanceKnob.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF181820));

        // Section dividers
        auto b = getLocalBounds();
        int headerH = 30;
        int row1Bottom = headerH + knobH + 16;

        g.setColour (juce::Colour (0xFF2A2A30));
        g.drawHorizontalLine (row1Bottom, 8.0f, (float)(b.getWidth() - 8));
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);

        // Header: toggle + title
        auto header = b.removeFromTop (26);
        toggleButton->setBounds (header.removeFromLeft (40).reduced (2));
        titleLabel.setBounds (header);
        b.removeFromTop (4);

        // Row 1: Detection knobs (4) + meter
        auto row1 = b.removeFromTop (knobH);
        int meterW = 120;
        auto meterArea = row1.removeFromRight (meterW);
        meter.setBounds (meterArea);

        int kw = row1.getWidth() / 4;
        layoutKnob (sensitivityKnob, sensitivityLabel, row1.removeFromLeft (kw));
        layoutKnob (decayKnob,       decayLabel,       row1.removeFromLeft (kw));
        layoutKnob (holdKnob,        holdLabel,        row1.removeFromLeft (kw));
        layoutKnob (smoothKnob,      smoothLabel,      row1);

        b.removeFromTop (10);

        // Row 2: Focus + Output knobs (5) + toggles
        auto row2 = b.removeFromTop (knobH);
        int toggleW = 90;
        auto toggleArea = row2.removeFromRight (toggleW);

        int kw2 = row2.getWidth() / 5;
        layoutKnob (hpFocusKnob,   hpFocusLabel,   row2.removeFromLeft (kw2));
        layoutKnob (lpFocusKnob,   lpFocusLabel,   row2.removeFromLeft (kw2));
        layoutKnob (transGainKnob, transGainLabel, row2.removeFromLeft (kw2));
        layoutKnob (sustGainKnob,  sustGainLabel,  row2.removeFromLeft (kw2));
        layoutKnob (balanceKnob,   balanceLabel,   row2);

        // Toggles stacked vertically
        int btnH = 24;
        int btnGap = 4;
        stereoLinkBtn.setBounds (toggleArea.removeFromTop (btnH).reduced (2, 0));
        toggleArea.removeFromTop (btnGap);
        gateModeBtn.setBounds   (toggleArea.removeFromTop (btnH).reduced (2, 0));
        toggleArea.removeFromTop (btnGap);
        invertBtn.setBounds     (toggleArea.removeFromTop (btnH).reduced (2, 0));
    }

private:
    static constexpr int knobH = 90;

    void layoutKnob (juce::Slider& knob, juce::Label& label, juce::Rectangle<int> area)
    {
        label.setBounds (area.removeFromTop (14));
        knob.setBounds  (area);
    }

    void timerCallback() override
    {
        // Sync knobs from processor (in case loaded from state)
    }

    TransientSplitterProcessor& processor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;

    // Detection
    StyledSlider sensitivityKnob, decayKnob, holdKnob, smoothKnob;
    juce::Label  sensitivityLabel, decayLabel, holdLabel, smoothLabel;

    // Focus
    StyledSlider hpFocusKnob, lpFocusKnob;
    juce::Label  hpFocusLabel, lpFocusLabel;

    // Output
    StyledSlider transGainKnob, sustGainKnob, balanceKnob;
    juce::Label  transGainLabel, sustGainLabel, balanceLabel;

    // Mode toggles
    SplitterModeButton stereoLinkBtn { "STEREO LINK" };
    SplitterModeButton gateModeBtn   { "GATE" };
    SplitterModeButton invertBtn     { "INVERT" };

    // Meter
    TransientSplitterMeter meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientSplitterPanel)
};
