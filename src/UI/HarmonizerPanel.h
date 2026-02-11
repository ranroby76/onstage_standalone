// ==============================================================================
//  HarmonizerPanel.h
//  OnStage - Harmonizer UI with draggable circles and formant control
//  
//  Voice colors: V1=Green, V2=Yellow, V3=Light Blue, V4=Light Purple
//  Circles: 25 vertical steps (-12 to +12), 101 horizontal steps (-50 to +50)
//  Window width: 2x wider
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/HarmonizerProcessor.h"

class PresetManager;

// ==============================================================================
// Voice colors
// ==============================================================================
namespace VoiceColors
{
    inline juce::Colour getColor(int voiceIndex)
    {
        switch (voiceIndex)
        {
            case 0: return juce::Colour(0xFF50C878);  // Green
            case 1: return juce::Colour(0xFFFFD700);  // Yellow/Gold
            case 2: return juce::Colour(0xFF87CEEB);  // Light Blue
            case 3: return juce::Colour(0xFFDDA0DD);  // Light Purple/Plum
            default: return juce::Colour(0xFFD4AF37);
        }
    }
}

// ==============================================================================
// Draggable Voice Node
// ==============================================================================
class VoiceNode : public juce::Component
{
public:
    VoiceNode(int voiceIndex) : voiceIdx(voiceIndex)
    {
        setSize(36, 36);
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::Colour voiceColor = VoiceColors::getColor(voiceIdx);
        
        // Outer glow
        if (isMouseOverOrDragging())
            g.setColour(voiceColor.withAlpha(0.4f));
        else
            g.setColour(voiceColor.withAlpha(0.2f));
        g.fillEllipse(bounds);
        
        // Main circle
        g.setColour(voiceColor.withAlpha(0.8f));
        g.fillEllipse(bounds.reduced(4));
        
        // Inner bright
        g.setColour(voiceColor);
        g.fillEllipse(bounds.reduced(8));
        
        // Voice number
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
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
    
    void mouseUp(const juce::MouseEvent&) override
    {
        // Snap to grid on release
        if (onPositionChanged)
            onPositionChanged();
    }
    
    std::function<void()> onPositionChanged;
    
private:
    int voiceIdx;
    juce::ComponentDragger dragger;
};

// ==============================================================================
// Harmony Canvas with Draggable Nodes and Floating Particles
// ==============================================================================
class HarmonyCanvas : public juce::Component, private juce::Timer
{
public:
    struct Particle
    {
        float x, y;
        float vx, vy;
        float age;
        int voiceIndex;
        float brightness;
    };
    
    HarmonyCanvas(HarmonizerProcessor& proc) : harmonizer(proc)
    {
        // Create draggable nodes
        for (int i = 0; i < 4; ++i)
        {
            nodes[i] = std::make_unique<VoiceNode>(i);
            nodes[i]->onPositionChanged = [this, i]() { updateVoiceFromNode(i); };
            addAndMakeVisible(nodes[i].get());
        }
        
        startTimerHz(60);
        frameCount = 0;
    }
    
    ~HarmonyCanvas() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto params = harmonizer.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        
        // Draw grid lines for semitones (25 steps: -12 to +12)
        g.setColour(juce::Colour(0xFF202020));
        for (int st = -12; st <= 12; ++st)
        {
            float y = semitonesToY((float)st);
            if (st == 0)
                g.setColour(juce::Colour(0xFF404040));
            else
                g.setColour(juce::Colour(0xFF202020));
            g.drawHorizontalLine((int)y, 0, (float)getWidth());
        }
        
        // Draw grid lines for pan (major lines at -50, -25, 0, +25, +50)
        for (int pan = -50; pan <= 50; pan += 25)
        {
            float x = panToX(pan / 50.0f);
            if (pan == 0)
                g.setColour(juce::Colour(0xFF404040));
            else
                g.setColour(juce::Colour(0xFF202020));
            g.drawVerticalLine((int)x, 0, (float)getHeight());
        }
        
        // Draw center point (main voice)
        g.setColour(juce::Colour(0xFFD4AF37));
        g.fillEllipse(centerX - 6, centerY - 6, 12, 12);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillEllipse(centerX - 3, centerY - 3, 6, 6);
        
        // Draw floating particles
        for (const auto& p : particles)
        {
            juce::Colour color = VoiceColors::getColor(p.voiceIndex);
            float alpha = p.brightness * (1.0f - p.age);
            
            float size = 3.0f + p.brightness * 3.0f;
            
            g.setColour(color.withAlpha(alpha * 0.3f));
            g.fillEllipse(p.x - size * 1.5f, p.y - size * 1.5f, size * 3.0f, size * 3.0f);
            
            g.setColour(color.withAlpha(alpha * 0.7f));
            g.fillEllipse(p.x - size, p.y - size, size * 2.0f, size * 2.0f);
        }
        
        // Labels
        g.setColour(juce::Colour(0xFF606060));
        g.setFont(10.0f);
        g.drawText("L50", 5, (int)centerY - 6, 25, 12, juce::Justification::left);
        g.drawText("R50", getWidth() - 28, (int)centerY - 6, 25, 12, juce::Justification::right);
        g.drawText("+12", (int)centerX - 12, 3, 24, 12, juce::Justification::centred);
        g.drawText("-12", (int)centerX - 12, getHeight() - 15, 24, 12, juce::Justification::centred);
        
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
        auto params = harmonizer.getParams();
        frameCount++;
        
        // Update node positions from processor (if changed externally)
        updateNodePositions();
        
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        
        // Spawn particles from enabled voices
        for (int v = 0; v < 4; ++v)
        {
            if (!params.voices[v].enabled) continue;
            
            if ((frameCount + v * 11) % 20 == 0)
            {
                float sourceX = panToX(params.voices[v].pan);
                float sourceY = semitonesToY(params.voices[v].semitones);
                
                int numParticles = 2 + juce::Random::getSystemRandom().nextInt(2);
                
                for (int p = 0; p < numParticles; ++p)
                {
                    Particle newP;
                    newP.x = sourceX;
                    newP.y = sourceY;
                    
                    float angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
                    float speed = 0.5f + juce::Random::getSystemRandom().nextFloat() * 1.0f;
                    
                    newP.vx = std::cos(angle) * speed;
                    newP.vy = std::sin(angle) * speed;
                    newP.age = 0.0f;
                    newP.voiceIndex = v;
                    newP.brightness = 0.5f + juce::Random::getSystemRandom().nextFloat() * 0.5f;
                    
                    particles.push_back(newP);
                }
            }
        }
        
        // Update particles
        for (int i = (int)particles.size() - 1; i >= 0; --i)
        {
            auto& p = particles[(size_t)i];
            
            p.x += p.vx;
            p.y += p.vy;
            
            // Gravity toward center
            float dx = centerX - p.x;
            float dy = centerY - p.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 1.0f)
            {
                p.vx += dx / dist * 0.015f;
                p.vy += dy / dist * 0.015f;
            }
            
            p.vx *= 0.98f;
            p.vy *= 0.98f;
            p.age += 0.015f;
            
            if (p.age >= 1.0f || p.x < -10 || p.x > getWidth() + 10 ||
                p.y < -10 || p.y > getHeight() + 10)
            {
                particles.erase(particles.begin() + i);
            }
        }
        
        if (particles.size() > 150)
            particles.erase(particles.begin(), particles.begin() + 25);
        
        repaint();
    }
    
    // Get formatted string for display: [pitch, pan]
    static juce::String getVoiceValueString(float semitones, float pan)
    {
        // Format pitch
        juce::String pitchStr;
        int pitchInt = (int)std::round(semitones);
        if (pitchInt > 0)
            pitchStr = "+" + juce::String(pitchInt);
        else
            pitchStr = juce::String(pitchInt);
        
        // Format pan (-1 to +1) -> (-50 to +50)
        int panInt = (int)std::round(pan * 50.0f);
        juce::String panStr;
        if (panInt < 0)
            panStr = "L" + juce::String(-panInt);
        else if (panInt > 0)
            panStr = "R" + juce::String(panInt);
        else
            panStr = "C";
        
        return "[" + pitchStr + ", " + panStr + "]";
    }
    
private:
    void updateNodePositions()
    {
        auto params = harmonizer.getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            float x = panToX(params.voices[i].pan) - 18;  // Center the 36px node
            float y = semitonesToY(params.voices[i].semitones) - 18;
            
            // Only update if not being dragged
            if (!nodes[i]->isMouseButtonDown())
                nodes[i]->setTopLeftPosition((int)x, (int)y);
            
            nodes[i]->setVisible(params.voices[i].enabled);
        }
    }
    
    void updateVoiceFromNode(int voiceIndex)
    {
        auto params = harmonizer.getParams();
        
        float nodeX = nodes[voiceIndex]->getX() + 18;
        float nodeY = nodes[voiceIndex]->getY() + 18;
        
        // Convert to pan and semitones with snapping
        float rawPan = xToPan(nodeX);
        float rawSemitones = yToSemitones(nodeY);
        
        // Snap pan to 101 steps (-50 to +50 -> -1.0 to +1.0)
        // Each step is 0.02 (1/50)
        float snappedPan = std::round(rawPan * 50.0f) / 50.0f;
        snappedPan = juce::jlimit(-1.0f, 1.0f, snappedPan);
        
        // Snap semitones to 25 steps (-12 to +12)
        // Each step is 1 semitone
        float snappedSemitones = std::round(rawSemitones);
        snappedSemitones = juce::jlimit(-12.0f, 12.0f, snappedSemitones);
        
        params.voices[voiceIndex].pan = snappedPan;
        params.voices[voiceIndex].semitones = snappedSemitones;
        
        harmonizer.setParams(params);
    }
    
    float panToX(float pan) const
    {
        // -1.0 to +1.0 -> margin to (width - margin)
        float margin = 30.0f;
        return juce::jmap(pan, -1.0f, 1.0f, margin, (float)getWidth() - margin);
    }
    
    float xToPan(float x) const
    {
        float margin = 30.0f;
        return juce::jmap(x, margin, (float)getWidth() - margin, -1.0f, 1.0f);
    }
    
    float semitonesToY(float semitones) const
    {
        // +12 (top) to -12 (bottom)
        float margin = 20.0f;
        return juce::jmap(semitones, 12.0f, -12.0f, margin, (float)getHeight() - margin);
    }
    
    float yToSemitones(float y) const
    {
        float margin = 20.0f;
        return juce::jmap(y, margin, (float)getHeight() - margin, 12.0f, -12.0f);
    }
    
    HarmonizerProcessor& harmonizer;
    std::unique_ptr<VoiceNode> nodes[4];
    std::vector<Particle> particles;
    int frameCount;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonyCanvas)
};

// ==============================================================================
// Main Harmonizer Panel - 2x wider with formant controls
// ==============================================================================
class HarmonizerPanel : public juce::Component, private juce::Timer
{
public:
    HarmonizerPanel(HarmonizerProcessor& proc, PresetManager& /*presets*/) : harmonizer(proc)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto params = harmonizer.getParams();

        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 50");
        toggleButton->setToggleState(!harmonizer.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() {
            harmonizer.setBypassed(!toggleButton->getToggleState());
        };
        addAndMakeVisible(toggleButton.get());

        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Harmonizer", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Canvas with draggable nodes
        canvas = std::make_unique<HarmonyCanvas>(harmonizer);
        addAndMakeVisible(canvas.get());

        // Create voice controls
        for (int i = 0; i < 4; ++i)
        {
            juce::Colour voiceColor = VoiceColors::getColor(i);
            
            // Enable button
            voiceEnableButtons[i] = std::make_unique<juce::ToggleButton>();
            voiceEnableButtons[i]->setButtonText("V" + juce::String(i + 1));
            voiceEnableButtons[i]->setToggleState(params.voices[i].enabled, juce::dontSendNotification);
            voiceEnableButtons[i]->setColour(juce::ToggleButton::textColourId, voiceColor);
            voiceEnableButtons[i]->setColour(juce::ToggleButton::tickColourId, voiceColor);
            voiceEnableButtons[i]->onClick = [this]() { updateProcessor(); };
            addAndMakeVisible(voiceEnableButtons[i].get());
            
            // Value label showing [pitch, pan]
            voiceValueLabels[i] = std::make_unique<juce::Label>();
            voiceValueLabels[i]->setFont(juce::Font(11.0f, juce::Font::bold));
            voiceValueLabels[i]->setColour(juce::Label::textColourId, voiceColor);
            voiceValueLabels[i]->setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(voiceValueLabels[i].get());
            
            // Pitch slider
            pitchSliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            pitchSliders[i]->setRange(-12.0, 12.0, 1.0);  // 25 steps
            pitchSliders[i]->setValue(params.voices[i].semitones, juce::dontSendNotification);
            pitchSliders[i]->setTextValueSuffix(" st");
            pitchSliders[i]->setColour(juce::Slider::thumbColourId, voiceColor);
            pitchSliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            pitchSliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(pitchSliders[i].get());
            
            pitchLabels[i] = std::make_unique<juce::Label>();
            pitchLabels[i]->setText("Pitch", juce::dontSendNotification);
            pitchLabels[i]->setFont(juce::Font(10.0f));
            pitchLabels[i]->setColour(juce::Label::textColourId, voiceColor);
            addAndMakeVisible(pitchLabels[i].get());
            
            // Pan slider
            panSliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            panSliders[i]->setRange(-50.0, 50.0, 1.0);  // 101 steps, display as -50 to +50
            panSliders[i]->setValue(params.voices[i].pan * 50.0, juce::dontSendNotification);
            panSliders[i]->setColour(juce::Slider::thumbColourId, voiceColor);
            panSliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            panSliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(panSliders[i].get());
            
            panLabels[i] = std::make_unique<juce::Label>();
            panLabels[i]->setText("Pan", juce::dontSendNotification);
            panLabels[i]->setFont(juce::Font(10.0f));
            panLabels[i]->setColour(juce::Label::textColourId, voiceColor);
            addAndMakeVisible(panLabels[i].get());
            
            // Formant slider
            formantSliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            formantSliders[i]->setRange(-12.0, 12.0, 0.5);
            formantSliders[i]->setValue(params.voices[i].formant, juce::dontSendNotification);
            formantSliders[i]->setTextValueSuffix(" st");
            formantSliders[i]->setColour(juce::Slider::thumbColourId, voiceColor);
            formantSliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            formantSliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(formantSliders[i].get());
            
            formantLabels[i] = std::make_unique<juce::Label>();
            formantLabels[i]->setText("Formant", juce::dontSendNotification);
            formantLabels[i]->setFont(juce::Font(10.0f));
            formantLabels[i]->setColour(juce::Label::textColourId, voiceColor);
            addAndMakeVisible(formantLabels[i].get());
            
            // Volume slider
            volumeSliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            volumeSliders[i]->setRange(-60.0, 0.0, 0.1);
            volumeSliders[i]->setValue(params.voices[i].gainDb, juce::dontSendNotification);
            volumeSliders[i]->setTextValueSuffix(" dB");
            volumeSliders[i]->setColour(juce::Slider::thumbColourId, voiceColor);
            volumeSliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            volumeSliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(volumeSliders[i].get());
            
            volumeLabels[i] = std::make_unique<juce::Label>();
            volumeLabels[i]->setText("Vol", juce::dontSendNotification);
            volumeLabels[i]->setFont(juce::Font(10.0f));
            volumeLabels[i]->setColour(juce::Label::textColourId, voiceColor);
            addAndMakeVisible(volumeLabels[i].get());
            
            // Delay slider
            delaySliders[i] = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            delaySliders[i]->setRange(0.0, 200.0, 2.0);
            delaySliders[i]->setValue(params.voices[i].delayMs, juce::dontSendNotification);
            delaySliders[i]->setTextValueSuffix(" ms");
            delaySliders[i]->setColour(juce::Slider::thumbColourId, voiceColor);
            delaySliders[i]->setLookAndFeel(goldenLookAndFeel.get());
            delaySliders[i]->onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(delaySliders[i].get());
            
            delayLabels[i] = std::make_unique<juce::Label>();
            delayLabels[i]->setText("Delay", juce::dontSendNotification);
            delayLabels[i]->setFont(juce::Font(10.0f));
            delayLabels[i]->setColour(juce::Label::textColourId, voiceColor);
            addAndMakeVisible(delayLabels[i].get());
        }

        // Mix slider
        mixSlider = std::make_unique<VerticalSlider>();
        mixSlider->setLabelText("MIX");
        mixSlider->setMidiInfo("MIDI: CC 59");
        mixSlider->setRange(-60.0, 0.0, 0.1);
        mixSlider->setValue(params.wetDb);
        mixSlider->setTextValueSuffix(" dB");
        mixSlider->getSlider().setLookAndFeel(goldenLookAndFeel.get());
        mixSlider->getSlider().onValueChange = [this]() { updateProcessor(); };
        addAndMakeVisible(mixSlider.get());

        // Glide slider
        glideSlider = std::make_unique<VerticalSlider>();
        glideSlider->setLabelText("GLIDE");
        glideSlider->setRange(1.0, 500.0, 1.0);
        glideSlider->setValue(params.glideMs);
        glideSlider->setTextValueSuffix(" ms");
        glideSlider->getSlider().setLookAndFeel(goldenLookAndFeel.get());
        glideSlider->getSlider().onValueChange = [this]() { updateProcessor(); };
        addAndMakeVisible(glideSlider.get());

        startTimerHz(15);
    }

    ~HarmonizerPanel() override
    {
        stopTimer();
        for (int i = 0; i < 4; ++i)
        {
            pitchSliders[i]->setLookAndFeel(nullptr);
            panSliders[i]->setLookAndFeel(nullptr);
            formantSliders[i]->setLookAndFeel(nullptr);
            volumeSliders[i]->setLookAndFeel(nullptr);
            delaySliders[i]->setLookAndFeel(nullptr);
        }
        mixSlider->getSlider().setLookAndFeel(nullptr);
        glideSlider->getSlider().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
        
        // Draw voice section separators
        auto area = getLocalBounds().reduced(15);
        area.removeFromTop(50);
        area.removeFromRight(180);
        area.removeFromTop((int)(area.getHeight() * 0.4f) + 15);
        
        int voiceWidth = area.getWidth() / 4;
        g.setColour(juce::Colour(0xFF353535));
        for (int i = 1; i < 4; ++i)
        {
            int x = area.getX() + i * voiceWidth;
            g.drawVerticalLine(x, (float)area.getY(), (float)area.getBottom());
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Top row
        auto topRow = area.removeFromTop(30);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        
        area.removeFromTop(10);
        
        // Right column
        auto rightColumn = area.removeFromRight(180);
        mixSlider->setBounds(rightColumn.removeFromLeft(80));
        rightColumn.removeFromLeft(20);
        glideSlider->setBounds(rightColumn.removeFromLeft(80));
        
        // Canvas (40% height)
        int canvasHeight = (int)(area.getHeight() * 0.4f);
        canvas->setBounds(area.removeFromTop(canvasHeight));
        
        area.removeFromTop(15);
        
        // Voice controls
        int voiceWidth = area.getWidth() / 4;
        int sliderHeight = 24;
        int labelWidth = 50;
        int gap = 3;
        
        for (int i = 0; i < 4; ++i)
        {
            auto voiceColumn = area.removeFromLeft(voiceWidth).reduced(8, 0);
            
            // Enable button + value label on same row
            auto headerRow = voiceColumn.removeFromTop(26);
            voiceEnableButtons[i]->setBounds(headerRow.removeFromLeft(50));
            voiceValueLabels[i]->setBounds(headerRow);
            voiceColumn.removeFromTop(gap);
            
            // Pitch
            auto pitchRow = voiceColumn.removeFromTop(sliderHeight);
            pitchLabels[i]->setBounds(pitchRow.removeFromLeft(labelWidth));
            pitchSliders[i]->setBounds(pitchRow);
            voiceColumn.removeFromTop(gap);
            
            // Pan
            auto panRow = voiceColumn.removeFromTop(sliderHeight);
            panLabels[i]->setBounds(panRow.removeFromLeft(labelWidth));
            panSliders[i]->setBounds(panRow);
            voiceColumn.removeFromTop(gap);
            
            // Formant
            auto formantRow = voiceColumn.removeFromTop(sliderHeight);
            formantLabels[i]->setBounds(formantRow.removeFromLeft(labelWidth));
            formantSliders[i]->setBounds(formantRow);
            voiceColumn.removeFromTop(gap);
            
            // Volume
            auto volRow = voiceColumn.removeFromTop(sliderHeight);
            volumeLabels[i]->setBounds(volRow.removeFromLeft(labelWidth));
            volumeSliders[i]->setBounds(volRow);
            voiceColumn.removeFromTop(gap);
            
            // Delay
            auto delayRow = voiceColumn.removeFromTop(sliderHeight);
            delayLabels[i]->setBounds(delayRow.removeFromLeft(labelWidth));
            delaySliders[i]->setBounds(delayRow);
        }
    }

    void updateFromPreset()
    {
        auto params = harmonizer.getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            voiceEnableButtons[i]->setToggleState(params.voices[i].enabled, juce::dontSendNotification);
            pitchSliders[i]->setValue(params.voices[i].semitones, juce::dontSendNotification);
            panSliders[i]->setValue(params.voices[i].pan * 50.0, juce::dontSendNotification);
            formantSliders[i]->setValue(params.voices[i].formant, juce::dontSendNotification);
            volumeSliders[i]->setValue(params.voices[i].gainDb, juce::dontSendNotification);
            delaySliders[i]->setValue(params.voices[i].delayMs, juce::dontSendNotification);
        }
        mixSlider->setValue(params.wetDb, juce::dontSendNotification);
        glideSlider->setValue(params.glideMs, juce::dontSendNotification);
        toggleButton->setToggleState(!harmonizer.isBypassed(), juce::dontSendNotification);
    }

private:
    void timerCallback() override
    {
        auto params = harmonizer.getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            // Update value labels
            juce::String valueStr = HarmonyCanvas::getVoiceValueString(
                params.voices[i].semitones, params.voices[i].pan);
            voiceValueLabels[i]->setText(valueStr, juce::dontSendNotification);
            
            if (!pitchSliders[i]->isMouseOverOrDragging())
                pitchSliders[i]->setValue(params.voices[i].semitones, juce::dontSendNotification);
            if (!panSliders[i]->isMouseOverOrDragging())
                panSliders[i]->setValue(params.voices[i].pan * 50.0, juce::dontSendNotification);
            if (!formantSliders[i]->isMouseOverOrDragging())
                formantSliders[i]->setValue(params.voices[i].formant, juce::dontSendNotification);
            if (!volumeSliders[i]->isMouseOverOrDragging())
                volumeSliders[i]->setValue(params.voices[i].gainDb, juce::dontSendNotification);
            if (!delaySliders[i]->isMouseOverOrDragging())
                delaySliders[i]->setValue(params.voices[i].delayMs, juce::dontSendNotification);
        }
        
        if (!mixSlider->getSlider().isMouseOverOrDragging())
            mixSlider->setValue(params.wetDb, juce::dontSendNotification);
        if (!glideSlider->getSlider().isMouseOverOrDragging())
            glideSlider->setValue(params.glideMs, juce::dontSendNotification);
        
        bool shouldBeOn = !harmonizer.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }

    void updateProcessor()
    {
        HarmonizerProcessor::Params p = harmonizer.getParams();
        
        for (int i = 0; i < 4; ++i)
        {
            p.voices[i].enabled = voiceEnableButtons[i]->getToggleState();
            p.voices[i].semitones = (float)pitchSliders[i]->getValue();
            p.voices[i].pan = (float)(panSliders[i]->getValue() / 50.0);  // Convert -50..+50 to -1..+1
            p.voices[i].formant = (float)formantSliders[i]->getValue();
            p.voices[i].gainDb = (float)volumeSliders[i]->getValue();
            p.voices[i].delayMs = (float)delaySliders[i]->getValue();
        }
        
        p.wetDb = (float)mixSlider->getValue();
        p.glideMs = (float)glideSlider->getValue();
        harmonizer.setParams(p);
    }

    HarmonizerProcessor& harmonizer;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    std::unique_ptr<HarmonyCanvas> canvas;
    
    std::unique_ptr<juce::ToggleButton> voiceEnableButtons[4];
    std::unique_ptr<juce::Label> voiceValueLabels[4];  // Shows [pitch, pan]
    std::unique_ptr<StyledSlider> pitchSliders[4];
    std::unique_ptr<StyledSlider> panSliders[4];
    std::unique_ptr<StyledSlider> formantSliders[4];
    std::unique_ptr<StyledSlider> volumeSliders[4];
    std::unique_ptr<StyledSlider> delaySliders[4];
    std::unique_ptr<juce::Label> pitchLabels[4];
    std::unique_ptr<juce::Label> panLabels[4];
    std::unique_ptr<juce::Label> formantLabels[4];
    std::unique_ptr<juce::Label> volumeLabels[4];
    std::unique_ptr<juce::Label> delayLabels[4];
    
    std::unique_ptr<VerticalSlider> mixSlider;
    std::unique_ptr<VerticalSlider> glideSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonizerPanel)
};
