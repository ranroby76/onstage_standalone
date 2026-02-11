// ==============================================================================
//  DelayPanel.h
//  OnStage - Delay UI with type selector and floating dots animation
//
//  Models: Oxide (Tape), Warp (Pitch), Crystal (Pure Echo), Drift (Doubler)
//  Based on Airwindows open source code (MIT license) by Chris Johnson
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/DelayProcessor.h"

class PresetManager;

// ==============================================================================
// Delay Type Button (matches Compressor style)
// ==============================================================================
class DelayTypeButton : public juce::Component
{
public:
    DelayTypeButton(const juce::String& label) : buttonLabel(label)
    {
        setRepaintsOnMouseActivity(true);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        juce::Colour bgColor;
        if (isSelected)
            bgColor = juce::Colour(0xFFD4AF37);
        else if (isMouseOver())
            bgColor = juce::Colour(0xFF3A3A3A);
        else
            bgColor = juce::Colour(0xFF2A2A2A);
        
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        g.setColour(isSelected ? juce::Colours::black : juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(buttonLabel, bounds, juce::Justification::centred);
    }
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && onClick)
            onClick();
    }
    
    void setSelected(bool shouldBeSelected)
    {
        if (isSelected != shouldBeSelected)
        {
            isSelected = shouldBeSelected;
            repaint();
        }
    }
    
    bool getSelected() const { return isSelected; }
    
    std::function<void()> onClick;
    
private:
    juce::String buttonLabel;
    bool isSelected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayTypeButton)
};

// ==============================================================================
// Delay Floating Dots Animation
// ==============================================================================
class DelayGraphComponent : public juce::Component, private juce::Timer
{
public:
    struct Dot
    {
        float x, y;
        float vx, vy;
        float age;
        float brightness;
        int generation;
        float angle;
    };
    
    DelayGraphComponent(DelayProcessor& processor) 
        : delayProcessor(processor) 
    {
        startTimerHz(60);
        frameCount = 0;
        lastFireTime = 0;
    }
    
    ~DelayGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto params = delayProcessor.getParams();
        
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        
        g.setColour(juce::Colour(0xFF505050));
        g.fillEllipse(centerX - 4, centerY - 4, 8, 8);
        
        switch (params.type)
        {
            case DelayProcessor::Type::Warp:
            {
                for (int i = 1; i <= 4; ++i)
                {
                    float radius = bounds.getWidth() * 0.1f * i;
                    float rotation = (float)frameCount * 0.01f * (5 - i);
                    g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.1f + 0.05f * i));
                    
                    juce::Path arc;
                    arc.addCentredArc(centerX, centerY, radius, radius, rotation,
                                      0, juce::MathConstants<float>::pi * 1.5f, true);
                    g.strokePath(arc, juce::PathStrokeType(2.0f));
                }
                break;
            }
            case DelayProcessor::Type::Oxide:
            {
                float reelRadius = bounds.getHeight() * 0.25f;
                float rotation = (float)frameCount * 0.02f;
                
                g.setColour(juce::Colour(0xFF404040));
                g.fillEllipse(centerX - bounds.getWidth() * 0.25f - reelRadius,
                              centerY - reelRadius, reelRadius * 2, reelRadius * 2);
                g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.3f));
                for (int spoke = 0; spoke < 3; ++spoke)
                {
                    float angle = rotation + spoke * juce::MathConstants<float>::twoPi / 3.0f;
                    float x1 = centerX - bounds.getWidth() * 0.25f;
                    float y1 = centerY;
                    float x2 = x1 + std::cos(angle) * reelRadius * 0.8f;
                    float y2 = y1 + std::sin(angle) * reelRadius * 0.8f;
                    g.drawLine(x1, y1, x2, y2, 2.0f);
                }
                
                g.setColour(juce::Colour(0xFF404040));
                g.fillEllipse(centerX + bounds.getWidth() * 0.25f - reelRadius,
                              centerY - reelRadius, reelRadius * 2, reelRadius * 2);
                g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.3f));
                for (int spoke = 0; spoke < 3; ++spoke)
                {
                    float angle = -rotation + spoke * juce::MathConstants<float>::twoPi / 3.0f;
                    float x1 = centerX + bounds.getWidth() * 0.25f;
                    float y1 = centerY;
                    float x2 = x1 + std::cos(angle) * reelRadius * 0.8f;
                    float y2 = y1 + std::sin(angle) * reelRadius * 0.8f;
                    g.drawLine(x1, y1, x2, y2, 2.0f);
                }
                
                g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.5f));
                g.drawLine(centerX - bounds.getWidth() * 0.25f + reelRadius, centerY,
                           centerX + bounds.getWidth() * 0.25f - reelRadius, centerY, 3.0f);
                break;
            }
            case DelayProcessor::Type::Drift:
            {
                float barWidth = bounds.getWidth() * 0.15f;
                float barHeight = bounds.getHeight() * 0.6f;
                float spacing = bounds.getWidth() * 0.2f;
                
                float lHeight = barHeight * params.p[1];
                g.setColour(juce::Colour(0xFF404040));
                g.fillRect(centerX - spacing - barWidth / 2, centerY - barHeight / 2,
                           barWidth, barHeight);
                g.setColour(juce::Colour(0xFFD4AF37));
                g.fillRect(centerX - spacing - barWidth / 2, centerY + barHeight / 2 - lHeight,
                           barWidth, lHeight);
                g.setColour(juce::Colours::white);
                g.setFont(11.0f);
                g.drawText("L", centerX - spacing - barWidth / 2, centerY + barHeight / 2 + 5,
                           barWidth, 15, juce::Justification::centred);
                
                float rHeight = barHeight * params.p[2];
                g.setColour(juce::Colour(0xFF404040));
                g.fillRect(centerX + spacing - barWidth / 2, centerY - barHeight / 2,
                           barWidth, barHeight);
                g.setColour(juce::Colour(0xFFD4AF37));
                g.fillRect(centerX + spacing - barWidth / 2, centerY + barHeight / 2 - rHeight,
                           barWidth, rHeight);
                g.setColour(juce::Colours::white);
                g.drawText("R", centerX + spacing - barWidth / 2, centerY + barHeight / 2 + 5,
                           barWidth, 15, juce::Justification::centred);
                break;
            }
            default: // Crystal
            {
                int numRings = 4;
                float maxRadius = juce::jmin(getWidth(), getHeight()) * 0.45f;
                
                for (int i = 1; i <= numRings; ++i)
                {
                    float radius = maxRadius * ((float)i / (float)numRings);
                    float alpha = 0.15f * (1.0f - (float)i / (float)(numRings + 1));
                    g.setColour(juce::Colour(0xFFD4AF37).withAlpha(alpha));
                    g.drawEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2, 1.0f);
                }
                break;
            }
        }
        
        // Draw dots
        for (const auto& dot : dots)
        {
            float size = 3.0f + (dot.brightness * 4.0f);
            float alpha = dot.brightness * (1.0f - dot.age * 0.7f);
            
            float genFade = 1.0f - (dot.generation * 0.15f);
            juce::Colour dotColor = juce::Colour(0xFFD4AF37).interpolatedWith(
                juce::Colour(0xFF3A3000), 1.0f - genFade);
            
            g.setColour(dotColor.withAlpha(alpha * 0.3f));
            g.fillEllipse(dot.x - size * 1.5f, dot.y - size * 1.5f, size * 3.0f, size * 3.0f);
            
            g.setColour(dotColor.withAlpha(alpha * 0.8f));
            g.fillEllipse(dot.x - size, dot.y - size, size * 2.0f, size * 2.0f);
            
            g.setColour(juce::Colours::white.withAlpha(alpha * 0.5f));
            g.fillEllipse(dot.x - size * 0.3f, dot.y - size * 0.3f, size * 0.6f, size * 0.6f);
        }
        
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override
    {
        auto params = delayProcessor.getParams();
        frameCount++;
        
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        
        // Use first param as a proxy for animation speed
        float animParam = juce::jlimit(0.05f, 1.0f, params.p[0]);
        float fireIntervalFrames = juce::jmap(animParam, 0.05f, 1.0f, 10.0f, 120.0f);
        fireIntervalFrames = juce::jlimit(10.0f, 120.0f, fireIntervalFrames);
        
        if (frameCount - lastFireTime >= (int)fireIntervalFrames)
        {
            lastFireTime = frameCount;
            
            int numDots = 3 + juce::Random::getSystemRandom().nextInt(3);
            
            for (int i = 0; i < numDots; ++i)
            {
                Dot newDot;
                newDot.angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
                
                float baseSpeed = juce::jmap(animParam, 0.05f, 1.0f, 3.0f, 1.0f);
                float speed = baseSpeed * (0.8f + juce::Random::getSystemRandom().nextFloat() * 0.4f);
                
                newDot.vx = std::cos(newDot.angle) * speed;
                newDot.vy = std::sin(newDot.angle) * speed;
                newDot.x = centerX;
                newDot.y = centerY;
                newDot.age = 0.0f;
                newDot.brightness = 0.7f + juce::Random::getSystemRandom().nextFloat() * 0.3f;
                newDot.generation = 0;
                
                dots.push_back(newDot);
            }
        }
        
        // Update dots
        float maxRadius = juce::jmin(getWidth(), getHeight()) * 0.45f;
        int numStages = 4;
        float agingRate = 0.015f;
        float echoRatio = juce::jlimit(0.1f, 1.0f, params.p[1]);
        
        for (int i = (int)dots.size() - 1; i >= 0; --i)
        {
            auto& dot = dots[(size_t)i];
            
            dot.x += dot.vx;
            dot.y += dot.vy;
            dot.vx *= 0.995f;
            dot.vy *= 0.995f;
            dot.age += agingRate;
            
            float dx = dot.x - centerX;
            float dy = dot.y - centerY;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            float echoRadius = maxRadius / (float)numStages;
            int currentZone = (int)(dist / echoRadius);
            
            if (currentZone > dot.generation && 
                dot.generation < numStages && 
                dot.brightness > 0.2f)
            {
                Dot echo;
                echo.x = dot.x;
                echo.y = dot.y;
                
                float angleVariation = (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.5f;
                echo.angle = dot.angle + angleVariation;
                
                float echoSpeed = std::sqrt(dot.vx * dot.vx + dot.vy * dot.vy) * echoRatio;
                echo.vx = std::cos(echo.angle) * echoSpeed;
                echo.vy = std::sin(echo.angle) * echoSpeed;
                
                echo.age = dot.age * 0.3f;
                echo.brightness = dot.brightness * echoRatio * 0.8f;
                echo.generation = dot.generation + 1;
                
                if (echo.brightness > 0.1f)
                    dots.push_back(echo);
                
                dot.generation = currentZone;
            }
            
            if (dot.age >= 1.0f || 
                dot.brightness < 0.05f ||
                dot.x < -20 || dot.x > getWidth() + 20 ||
                dot.y < -20 || dot.y > getHeight() + 20)
            {
                dots.erase(dots.begin() + i);
            }
        }
        
        if (dots.size() > 300)
            dots.erase(dots.begin(), dots.begin() + 50);
        
        repaint();
    }
    
private:
    DelayProcessor& delayProcessor;
    std::vector<Dot> dots;
    int frameCount;
    int lastFireTime;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayGraphComponent)
};

// ==============================================================================
// Main Delay Panel
// ==============================================================================
class DelayPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int MAX_SLIDERS = 8;  // 2 (Dry+Wet) + up to 6 model params
    
    DelayPanel(DelayProcessor& processor, PresetManager& /*presets*/) : delayProcessor(processor)
    {
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        
        // Toggle button
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!delayProcessor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            delayProcessor.setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Delay", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        
        // Type selector buttons
        auto createTypeButton = [this](std::unique_ptr<DelayTypeButton>& btn, 
                                        const juce::String& name, DelayProcessor::Type type)
        {
            btn = std::make_unique<DelayTypeButton>(name);
            btn->onClick = [this, type]() { selectType(type); };
            addAndMakeVisible(btn.get());
        };
        
        createTypeButton(oxideButton,   DelayProcessor::getTypeName(DelayProcessor::Type::Oxide),   DelayProcessor::Type::Oxide);
        createTypeButton(warpButton,    DelayProcessor::getTypeName(DelayProcessor::Type::Warp),    DelayProcessor::Type::Warp);
        createTypeButton(crystalButton, DelayProcessor::getTypeName(DelayProcessor::Type::Crystal), DelayProcessor::Type::Crystal);
        createTypeButton(driftButton,   DelayProcessor::getTypeName(DelayProcessor::Type::Drift),   DelayProcessor::Type::Drift);
        
        updateTypeButtons();
        
        // Create 6 generic sliders (max any model uses)
        static const char* midiCCs[] = { "CC 30", "CC 31", "CC 32", "CC 33", "CC 34", "CC 35", "CC 36", "CC 37" };
        
        for (int i = 0; i < MAX_SLIDERS; ++i)
        {
            sliders[i] = std::make_unique<VerticalSlider>();
            sliders[i]->setMidiInfo(midiCCs[i]);
            sliders[i]->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            sliders[i]->getSlider().onValueChange = [this]() { updateProcessor(); };
            addAndMakeVisible(sliders[i].get());
        }
        
        // Graph component
        graphComponent = std::make_unique<DelayGraphComponent>(delayProcessor);
        addAndMakeVisible(graphComponent.get());
        
        // Configure sliders for current type and load defaults
        rebuildSliders();
        
        startTimerHz(15);
    }
    
    ~DelayPanel() override
    {
        stopTimer();
        for (int i = 0; i < MAX_SLIDERS; ++i)
            sliders[i]->getSlider().setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
        
        auto area = getLocalBounds().reduced(15);
        area.removeFromTop(40);
        g.setColour(juce::Colour(0xFF888888));
        g.setFont(11.0f);
        g.drawText("TYPE", 15, area.getY() + 2, 40, 16, juce::Justification::centredLeft);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Title row
        auto titleRow = area.removeFromTop(35);
        toggleButton->setBounds(titleRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(titleRow);
        
        // Type selector row
        auto typeRow = area.removeFromTop(32);
        typeRow.removeFromLeft(50);  // Space for "TYPE" label
        
        int buttonWidth = 70;
        int buttonSpacing = 8;
        oxideButton->setBounds(typeRow.removeFromLeft(buttonWidth));
        typeRow.removeFromLeft(buttonSpacing);
        warpButton->setBounds(typeRow.removeFromLeft(buttonWidth));
        typeRow.removeFromLeft(buttonSpacing);
        crystalButton->setBounds(typeRow.removeFromLeft(buttonWidth));
        typeRow.removeFromLeft(buttonSpacing);
        driftButton->setBounds(typeRow.removeFromLeft(buttonWidth));
        
        area.removeFromTop(15);
        
        // Controls area
        auto currentType = delayProcessor.getParams().type;
        int numParams = DelayProcessor::getNumParams(currentType);
        
        int sliderWidth = 60;
        int spacing = 12;
        int controlAreaWidth = numParams * sliderWidth + (numParams - 1) * spacing;
        auto controlArea = area.removeFromLeft(controlAreaWidth);
        area.removeFromLeft(20);
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout visible sliders
        for (int i = 0; i < MAX_SLIDERS; ++i)
        {
            if (i < numParams)
            {
                sliders[i]->setVisible(true);
                sliders[i]->setBounds(controlArea.removeFromLeft(sliderWidth));
                if (i < numParams - 1)
                    controlArea.removeFromLeft(spacing);
            }
            else
            {
                sliders[i]->setVisible(false);
            }
        }
    }
    
    void updateFromPreset()
    {
        auto p = delayProcessor.getParams();
        toggleButton->setToggleState(!delayProcessor.isBypassed(), juce::dontSendNotification);
        
        int numParams = DelayProcessor::getNumParams(p.type);
        for (int i = 0; i < numParams && i < MAX_SLIDERS; ++i)
            sliders[i]->setValue(p.p[i], juce::dontSendNotification);
        
        updateTypeButtons();
        rebuildSliders();
    }

private:
    void timerCallback() override
    {
        auto p = delayProcessor.getParams();
        int numParams = DelayProcessor::getNumParams(p.type);
        
        for (int i = 0; i < numParams && i < MAX_SLIDERS; ++i)
        {
            if (!sliders[i]->getSlider().isMouseOverOrDragging())
                sliders[i]->setValue(p.p[i], juce::dontSendNotification);
        }
        
        bool shouldBeOn = !delayProcessor.isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void selectType(DelayProcessor::Type type)
    {
        auto p = delayProcessor.getParams();
        if (p.type != type)
        {
            p.type = type;
            // Load defaults for the new type
            int numParams = DelayProcessor::getNumParams(type);
            for (int i = 0; i < 6; ++i)
                p.p[i] = (i < numParams) ? DelayProcessor::getDefaultValue(type, i) : 0.0f;
            
            delayProcessor.setParams(p);
            updateTypeButtons();
            rebuildSliders();
            resized();
            repaint();
        }
    }
    
    void updateTypeButtons()
    {
        auto type = delayProcessor.getParams().type;
        oxideButton->setSelected(type == DelayProcessor::Type::Oxide);
        warpButton->setSelected(type == DelayProcessor::Type::Warp);
        crystalButton->setSelected(type == DelayProcessor::Type::Crystal);
        driftButton->setSelected(type == DelayProcessor::Type::Drift);
    }
    
    void rebuildSliders()
    {
        auto currentType = delayProcessor.getParams().type;
        int numParams = DelayProcessor::getNumParams(currentType);
        auto p = delayProcessor.getParams();
        
        for (int i = 0; i < MAX_SLIDERS; ++i)
        {
            if (i < numParams)
            {
                double min, max, step;
                DelayProcessor::getParamRange(currentType, i, min, max, step);
                
                sliders[i]->setLabelText(DelayProcessor::getParamName(currentType, i));
                sliders[i]->setRange(min, max, step);
                sliders[i]->setValue(p.p[i], juce::dontSendNotification);
                sliders[i]->setTextValueSuffix(DelayProcessor::getParamSuffix(currentType, i));
                sliders[i]->setVisible(true);
            }
            else
            {
                sliders[i]->setVisible(false);
            }
        }
    }
    
    void updateProcessor()
    {
        DelayProcessor::Params p = delayProcessor.getParams();
        int numParams = DelayProcessor::getNumParams(p.type);
        
        for (int i = 0; i < numParams && i < MAX_SLIDERS; ++i)
            p.p[i] = (float)sliders[i]->getValue();
        
        delayProcessor.setParams(p);
    }
    
    DelayProcessor& delayProcessor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    
    // Type selector buttons
    std::unique_ptr<DelayTypeButton> oxideButton;
    std::unique_ptr<DelayTypeButton> warpButton;
    std::unique_ptr<DelayTypeButton> crystalButton;
    std::unique_ptr<DelayTypeButton> driftButton;
    
    // Dynamic sliders (up to 6)
    std::unique_ptr<VerticalSlider> sliders[MAX_SLIDERS];
    
    std::unique_ptr<DelayGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayPanel)
};
