// (This is the reference layout)

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

class HarmonizerPanel : public juce::Component, private juce::Timer {
public:
    HarmonizerPanel(AudioEngine& engine) : audioEngine(engine) {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& h = audioEngine.getHarmonizerProcessor(); auto params = h.getParams();

        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 23"); 
        toggleButton->setToggleState(!h.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { audioEngine.getHarmonizerProcessor().setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Harmonizer (Fixed Mode)", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>(); s->setLabelText(n); s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(min, max, (max-min)/100.0); s->setValue(v); s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateHarmonizer(); };
            addAndMakeVisible(s.get());
        };

        cS(voice1PitchSlider, "V1 Pitch", 55, -12.0, 12.0, params.voices[0].fixedSemitones, " st");
        cS(voice1GainSlider, "V1 Gain", 56, -24.0, 12.0, params.voices[0].gainDb, " dB");
        cS(voice2PitchSlider, "V2 Pitch", 57, -12.0, 12.0, params.voices[1].fixedSemitones, " st");
        cS(voice2GainSlider, "V2 Gain", 58, -24.0, 12.0, params.voices[1].gainDb, " dB");
        cS(wetSlider, "Wet", 30, -24.0, 12.0, params.wetDb, " dB");

        voice1EnableToggle = std::make_unique<EffectToggleButton>();
        voice1EnableToggle->setButtonText("V1"); voice1EnableToggle->setMidiInfo("MIDI: Note 24");
        voice1EnableToggle->setToggleState(params.voices[0].enabled, juce::dontSendNotification);
        voice1EnableToggle->onClick = [this]() { updateHarmonizer(); };
        addAndMakeVisible(voice1EnableToggle.get());

        voice2EnableToggle = std::make_unique<EffectToggleButton>();
        voice2EnableToggle->setButtonText("V2"); voice2EnableToggle->setMidiInfo("MIDI: Note 25");
        voice2EnableToggle->setToggleState(params.voices[1].enabled, juce::dontSendNotification);
        voice2EnableToggle->onClick = [this]() { updateHarmonizer(); };
        addAndMakeVisible(voice2EnableToggle.get());

        startTimerHz(15);
    }

    ~HarmonizerPanel() override { stopTimer();
        voice1PitchSlider->getSlider().setLookAndFeel(nullptr); voice1GainSlider->getSlider().setLookAndFeel(nullptr);
        voice2PitchSlider->getSlider().setLookAndFeel(nullptr); voice2GainSlider->getSlider().setLookAndFeel(nullptr);
        wetSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040)); g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A)); g.fillRect(getLocalBounds().reduced(10));
    }

    void resized() override {
        auto area = getLocalBounds().reduced(15); // Standardized Padding
        auto titleRow = area.removeFromTop(40);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);

        area.removeFromTop(10);
        auto toggleRow = area.removeFromTop(40); // Space for Voice toggles
        area.removeFromTop(5);

        int numSliders = 5; int sliderWidth = 60; int spacing = 20;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        auto sArea = area.withX(startX).withWidth(totalW);

        // Position toggles above their respective groups
        voice1EnableToggle->setBounds(startX + sliderWidth/2, toggleRow.getY(), 40, 40);
        voice2EnableToggle->setBounds(startX + (sliderWidth*2) + (spacing*2) + sliderWidth/2, toggleRow.getY(), 40, 40);

        voice1PitchSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        voice1GainSlider->setBounds(sArea.removeFromLeft(sliderWidth));  sArea.removeFromLeft(spacing);
        voice2PitchSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        voice2GainSlider->setBounds(sArea.removeFromLeft(sliderWidth));  sArea.removeFromLeft(spacing);
        wetSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }

    void updateFromPreset() {
        auto& h = audioEngine.getHarmonizerProcessor(); auto p = h.getParams();
        voice1PitchSlider->setValue(p.voices[0].fixedSemitones, juce::dontSendNotification);
        voice1GainSlider->setValue(p.voices[0].gainDb, juce::dontSendNotification);
        voice2PitchSlider->setValue(p.voices[1].fixedSemitones, juce::dontSendNotification);
        voice2GainSlider->setValue(p.voices[1].gainDb, juce::dontSendNotification);
        wetSlider->setValue(p.wetDb, juce::dontSendNotification);
        toggleButton->setToggleState(!h.isBypassed(), juce::dontSendNotification);
        voice1EnableToggle->setToggleState(p.voices[0].enabled, juce::dontSendNotification);
        voice2EnableToggle->setToggleState(p.voices[1].enabled, juce::dontSendNotification);
    }

private:
    void timerCallback() override {
        auto p = audioEngine.getHarmonizerProcessor().getParams();
        if (!voice1PitchSlider->getSlider().isMouseOverOrDragging()) voice1PitchSlider->setValue(p.voices[0].fixedSemitones, juce::dontSendNotification);
        if (!voice1GainSlider->getSlider().isMouseOverOrDragging()) voice1GainSlider->setValue(p.voices[0].gainDb, juce::dontSendNotification);
        if (!voice2PitchSlider->getSlider().isMouseOverOrDragging()) voice2PitchSlider->setValue(p.voices[1].fixedSemitones, juce::dontSendNotification);
        if (!voice2GainSlider->getSlider().isMouseOverOrDragging()) voice2GainSlider->setValue(p.voices[1].gainDb, juce::dontSendNotification);
        if (!wetSlider->getSlider().isMouseOverOrDragging()) wetSlider->setValue(p.wetDb, juce::dontSendNotification);
        bool shouldBeOn = !audioEngine.getHarmonizerProcessor().isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn) toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
        if (voice1EnableToggle->getToggleState() != p.voices[0].enabled) voice1EnableToggle->setToggleState(p.voices[0].enabled, juce::dontSendNotification);
        if (voice2EnableToggle->getToggleState() != p.voices[1].enabled) voice2EnableToggle->setToggleState(p.voices[1].enabled, juce::dontSendNotification);
    }
    AudioEngine& audioEngine;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton; juce::Label titleLabel;
    std::unique_ptr<VerticalSlider> voice1PitchSlider, voice1GainSlider, voice2PitchSlider, voice2GainSlider, wetSlider;
    std::unique_ptr<EffectToggleButton> voice1EnableToggle, voice2EnableToggle;
    void updateHarmonizer() {
        HarmonizerProcessor::Params p; p.enabled = true; p.useDiatonicMode = false; p.wetDb = wetSlider->getValue(); p.glideMs = 50.0f;
        p.voices[0].enabled = voice1EnableToggle->getToggleState(); p.voices[0].fixedSemitones = voice1PitchSlider->getValue(); p.voices[0].gainDb = voice1GainSlider->getValue();
        p.voices[1].enabled = voice2EnableToggle->getToggleState(); p.voices[1].fixedSemitones = voice2PitchSlider->getValue(); p.voices[1].gainDb = voice2GainSlider->getValue();
        audioEngine.getHarmonizerProcessor().setParams(p);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonizerPanel)
};