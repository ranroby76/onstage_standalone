#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

// Interactive draggable node for pitch/pan control
class HarmonyNode : public juce::Component
{
public:
    HarmonyNode(int voiceIndex) : voiceIdx(voiceIndex)
    {
        setSize(40, 40);
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw circle
        if (isMouseOverOrDragging())
            g.setColour(juce::Colour(0xFFFFFFFF));
        else
            g.setColour(juce::Colour(0xFFD4AF37));
            
        g.fillEllipse(bounds.reduced(2));
        
        // Draw border
        g.setColour(juce::Colours::black);
        g.drawEllipse(bounds.reduced(2), 2.0f);
        
        // Draw voice number
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(juce::String(voiceIdx + 1), bounds, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragger.startDraggingComponent(this, e);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        dragger.dragComponent(this, e, nullptr);
        if (onPositionChanged)
            onPositionChanged();
    }
    
    std::function<void()> onPositionChanged;
    
private:
    int voiceIdx;
    juce::ComponentDragger dragger;
};

// Canvas for visualizing pitch/pan
class HarmonyCanvas : public juce::Component, private juce::Timer
{
public:
    HarmonyCanvas(AudioEngine& engine) : audioEngine(engine)
    {
        // Create 4 draggable nodes
        for (int i = 0; i < 4; ++i)
        {
            nodes[i] = std::make_unique<HarmonyNode>(i);
            nodes[i]->onPositionChanged = [this, i]() { updateVoiceFromNode(i); };
            addAndMakeVisible(nodes[i].get());
        }
        
        startTimerHz(30);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Center line (0 semitones)
        g.setColour(juce::Colour(0xFF404040));
        float centerY = bounds.getHeight() / 2.0f;
        g.drawHorizontalLine((int)centerY, 0.0f, bounds.getWidth());
        
        // Center line (0 pan)
        float centerX = bounds.getWidth() / 2.0f;
        g.drawVerticalLine((int)centerX, 0.0f, bounds.getHeight());
        
        // Draw grid lines for semitones
        g.setColour(juce::Colour(0xFF2A2A2A));
        for (int semitone = -12; semitone <= 12; semitone += 3)
        {
            if (semitone == 0) continue;
            float y = semitonesToY(semitone);
            g.drawHorizontalLine((int)y, 0.0f, bounds.getWidth());
            
            // Label
            g.setColour(juce::Colour(0xFF606060));
            g.setFont(10.0f);
            g.drawText((semitone > 0 ? "+" : "") + juce::String(semitone), 
                       5, (int)y - 10, 40, 20, juce::Justification::centredLeft);
        }
        
        // Draw main vocal icon (center)
        g.setColour(juce::Colour(0xFFD4AF37));
        juce::Rectangle<float> micBounds(centerX - 20, centerY - 30, 40, 60);
        
        // Simple microphone shape
        g.fillEllipse(micBounds.removeFromTop(30));
        g.fillRect(micBounds.withSizeKeepingCentre(8, 30));
        
        // Draw "MAIN" label
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("MAIN", centerX - 30, centerY + 35, 60, 20, juce::Justification::centred);
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void resized() override
    {
        updateNodePositions();
    }
    
    void timerCallback() override
    {
        updateNodePositions();
    }
    
private:
    void updateNodePositions()
    {
        auto& harmonizer = audioEngine.getHarmonizerProcessor();
        auto params = harmonizer.getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            float semitones = params.voices[i].semitones;
            float pan = params.voices[i].pan;
            
            float x = panToX(pan) - 20;  // Center the 40px node
            float y = semitonesToY(semitones) - 20;
            
            nodes[i]->setTopLeftPosition((int)x, (int)y);
            nodes[i]->setVisible(params.voices[i].enabled);
        }
    }
    
    void updateVoiceFromNode(int voiceIndex)
    {
        auto& harmonizer = audioEngine.getHarmonizerProcessor();
        auto params = harmonizer.getParams();
        
        // Get node center position
        float nodeX = nodes[voiceIndex]->getX() + 20;
        float nodeY = nodes[voiceIndex]->getY() + 20;
        
        // Convert to pan and semitones
        params.voices[voiceIndex].pan = xToPan(nodeX);
        params.voices[voiceIndex].semitones = yToSemitones(nodeY);
        
        // Clamp values
        params.voices[voiceIndex].pan = juce::jlimit(-1.0f, 1.0f, params.voices[voiceIndex].pan);
        params.voices[voiceIndex].semitones = juce::jlimit(-12.0f, 12.0f, params.voices[voiceIndex].semitones);
        
        harmonizer.setParams(params);
    }
    
    float panToX(float pan) const
    {
        // -1.0 (left) to +1.0 (right) → 0 to width
        return juce::jmap(pan, -1.0f, 1.0f, 0.0f, (float)getWidth());
    }
    
    float xToPan(float x) const
    {
        return juce::jmap(x, 0.0f, (float)getWidth(), -1.0f, 1.0f);
    }
    
    float semitonesToY(float semitones) const
    {
        // +12 (top) to -12 (bottom)
        return juce::jmap(semitones, 12.0f, -12.0f, 0.0f, (float)getHeight());
    }
    
    float yToSemitones(float y) const
    {
        return juce::jmap(y, 0.0f, (float)getHeight(), 12.0f, -12.0f);
    }
    
    AudioEngine& audioEngine;
    std::unique_ptr<HarmonyNode> nodes[4];
};

class HarmonizerPanel : public juce::Component, private juce::Timer
{
public:
    HarmonizerPanel(AudioEngine& engine) : audioEngine(engine)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& harmonizer = audioEngine.getHarmonizerProcessor();
        auto params = harmonizer.getParams();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 50");
        toggleButton->setToggleState(!harmonizer.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() {
            audioEngine.getHarmonizerProcessor().setBypassed(!toggleButton->getToggleState());
        };
        addAndMakeVisible(toggleButton.get());

        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Harmonizer", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Canvas
        canvas = std::make_unique<HarmonyCanvas>(audioEngine);
        addAndMakeVisible(canvas.get());

        // Voice enable buttons and sliders for 4 voices
        for (int i = 0; i < 4; ++i)
        {
            // Enable button
            voiceEnableButtons[i] = std::make_unique<juce::ToggleButton>();
            voiceEnableButtons[i]->setButtonText("V" + juce::String(i + 1));
            voiceEnableButtons[i]->setToggleState(params.voices[i].enabled, juce::dontSendNotification);
            voiceEnableButtons[i]->onClick = [this]() { updateProcessor(); };
            addAndMakeVisible(voiceEnableButtons[i].get());
            
            // Volume slider
            volumeSliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            volumeSliders[i]->setRange(-60.0, 0.0, 0.1);
            volumeSliders[i]->setValue(params.voices[i].gainDb, juce::dontSendNotification);
            volumeSliders[i]->setTextValueSuffix(" dB");
            volumeSliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            volumeSliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(volumeSliders[i].get());
            
            // Volume label
            volumeLabels[i] = std::make_unique<juce::Label>();
            volumeLabels[i]->setText("Vol", juce::dontSendNotification);
            volumeLabels[i]->setFont(juce::Font(10.0f));
            volumeLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
            volumeLabels[i]->setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(volumeLabels[i].get());
            
            // Delay slider
            delaySliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            delaySliders[i]->setRange(0.0, 200.0, 2.0);  // 0-200ms in 2ms steps (101 steps)
            delaySliders[i]->setValue(params.voices[i].delayMs, juce::dontSendNotification);
            delaySliders[i]->setTextValueSuffix(" ms");
            delaySliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            delaySliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(delaySliders[i].get());
            
            // Delay label
            delayLabels[i] = std::make_unique<juce::Label>();
            delayLabels[i]->setText("Delay", juce::dontSendNotification);
            delayLabels[i]->setFont(juce::Font(10.0f));
            delayLabels[i]->setColour(juce::Label::textColourId, juce::Colours::white);
            delayLabels[i]->setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(delayLabels[i].get());
        }

        // Mix slider (vertical)
        mixSlider = std::make_unique<VerticalSlider>();
        mixSlider->setLabelText("MIX");
        mixSlider->setMidiInfo("MIDI: CC 59");
        mixSlider->setRange(-60.0, 0.0, 0.1);
        mixSlider->setValue(params.wetDb);
        mixSlider->setTextValueSuffix(" dB");
        mixSlider->getSlider().setLookAndFeel(goldenLookAndFeel.get());
        mixSlider->getSlider().onValueChange = [this]() { updateProcessor(); };
        addAndMakeVisible(mixSlider.get());

        startTimerHz(15);
    }

    ~HarmonizerPanel() override
    {
        stopTimer();
        for (int i = 0; i < 4; ++i)
        {
            volumeSliders[i]->setLookAndFeel(nullptr);
            delaySliders[i]->setLookAndFeel(nullptr);
        }
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
        
        // Top row: toggle + title
        auto topRow = area.removeFromTop(30);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        
        area.removeFromTop(10);
        
        // Reserve right side for MIX slider
        auto rightColumn = area.removeFromRight(80);
        
        // Upper half: Canvas
        auto upperHalf = area.removeFromTop(area.getHeight() / 2);
        canvas->setBounds(upperHalf);
        
        area.removeFromTop(10);
        
        // Lower half: 4 columns of voice controls
        int voiceWidth = area.getWidth() / 4;
        
        for (int i = 0; i < 4; ++i)
        {
            auto voiceColumn = area.removeFromLeft(voiceWidth).reduced(5, 0);
            
            // Enable button
            voiceEnableButtons[i]->setBounds(voiceColumn.removeFromTop(25));
            voiceColumn.removeFromTop(5);
            
            // Volume label + slider
            auto volRow = voiceColumn.removeFromTop(30);
            volumeLabels[i]->setBounds(volRow.removeFromLeft(35));
            volumeSliders[i]->setBounds(volRow);
            
            voiceColumn.removeFromTop(5);
            
            // Delay label + slider
            auto delayRow = voiceColumn.removeFromTop(30);
            delayLabels[i]->setBounds(delayRow.removeFromLeft(35));
            delaySliders[i]->setBounds(delayRow);
        }
        
        // Mix slider on the right
        mixSlider->setBounds(rightColumn);
    }

    void updateFromPreset()
    {
        auto params = audioEngine.getHarmonizerProcessor().getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            voiceEnableButtons[i]->setToggleState(params.voices[i].enabled, juce::dontSendNotification);
            volumeSliders[i]->setValue(params.voices[i].gainDb, juce::dontSendNotification);
            delaySliders[i]->setValue(params.voices[i].delayMs, juce::dontSendNotification);
        }
        mixSlider->setValue(params.wetDb, juce::dontSendNotification);
        toggleButton->setToggleState(!audioEngine.getHarmonizerProcessor().isBypassed(), juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto params = audioEngine.getHarmonizerProcessor().getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            if (!volumeSliders[i]->isMouseOverOrDragging())
                volumeSliders[i]->setValue(params.voices[i].gainDb, juce::dontSendNotification);
            if (!delaySliders[i]->isMouseOverOrDragging())
                delaySliders[i]->setValue(params.voices[i].delayMs, juce::dontSendNotification);
        }
        
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(params.wetDb, juce::dontSendNotification);
        
        bool shouldBeOn = !audioEngine.getHarmonizerProcessor().isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        HarmonizerProcessor::Params p = audioEngine.getHarmonizerProcessor().getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            p.voices[i].enabled = voiceEnableButtons[i]->getToggleState();
            p.voices[i].gainDb = (float)volumeSliders[i]->getValue();
            p.voices[i].delayMs = (float)delaySliders[i]->getValue();
        }
        
        p.wetDb = (float)mixSlider->getValue();
        audioEngine.getHarmonizerProcessor().setParams(p);
    }

    AudioEngine& audioEngine;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<HarmonyCanvas> canvas;
    
    std::unique_ptr<juce::ToggleButton> voiceEnableButtons[4];
    std::unique_ptr<StyledSlider> volumeSliders[4];
    std::unique_ptr<StyledSlider> delaySliders[4];
    std::unique_ptr<juce::Label> volumeLabels[4];
    std::unique_ptr<juce::Label> delayLabels[4];
    std::unique_ptr<VerticalSlider> mixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonizerPanel)
};