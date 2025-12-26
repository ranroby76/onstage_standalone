#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../AudioEngine.h"

// Reverb Particle Cloud Animation
class ReverbGraphComponent : public juce::Component, private juce::Timer {
public:
    struct Particle {
        float x, y;           // Position
        float vx, vy;         // Velocity
        float age;            // 0.0 to 1.0 (lifetime)
        float brightness;     // Brightness based on frequency
        float rotationAngle;  // For swirl effect
    };
    
    ReverbGraphComponent(AudioEngine& engine) 
        : audioEngine(engine) 
    {
        startTimerHz(60); // Smooth particle motion
        frameCount = 0;
    }
    
    ~ReverbGraphComponent() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        auto& reverb = audioEngine.getReverbProcessor();
        auto params = reverb.getParams();
        
        // Background
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);
        
        // Draw center point (sound source)
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        
        g.setColour(juce::Colour(0xFF404040));
        g.fillEllipse(centerX - 3, centerY - 3, 6, 6);
        
        // Draw particles
        for (const auto& particle : particles) {
            float size = 2.0f + (particle.brightness * 3.0f);
            float alpha = (1.0f - particle.age) * particle.brightness;
            
            // Color gradient from golden to darker (high freq fades faster)
            juce::Colour color = juce::Colour(0xFFD4AF37).interpolatedWith(
                juce::Colour(0xFF3A3000), 
                particle.age
            );
            
            g.setColour(color.withAlpha(alpha * 0.6f));
            g.fillEllipse(particle.x - size, particle.y - size, size * 2, size * 2);
        }
        
        // Border
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }
    
    void timerCallback() override {
        auto& reverb = audioEngine.getReverbProcessor();
        auto params = reverb.getParams();
        frameCount++;
        
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        
        // Spawn new particles continuously (breathing effect)
        int spawnRate = juce::jlimit(1, 5, (int)(params.wetGain * 5.0f));
        
        for (int i = 0; i < spawnRate; ++i) {
            Particle newParticle;
            
            // Random angle for radial explosion
            float angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            
            // Velocity based on wetGain (room size simulation)
            float speed = 1.0f + (params.wetGain * 3.0f);
            newParticle.vx = std::cos(angle) * speed;
            newParticle.vy = std::sin(angle) * speed;
            
            // Start from center
            newParticle.x = centerX;
            newParticle.y = centerY;
            
            newParticle.age = 0.0f;
            
            // High frequencies are brighter (will fade faster)
            newParticle.brightness = 0.4f + (juce::Random::getSystemRandom().nextFloat() * 0.6f);
            
            newParticle.rotationAngle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            
            particles.push_back(newParticle);
        }
        
        // Update existing particles
        float dampingFactor = juce::jmap(params.highCutHz, 2000.0f, 20000.0f, 0.98f, 0.95f);
        float agingRate = 0.015f; // Base aging rate
        
        for (int i = particles.size() - 1; i >= 0; --i) {
            auto& particle = particles[i];
            
            // Update position
            particle.x += particle.vx;
            particle.y += particle.vy;
            
            // Apply damping (slowdown over time)
            particle.vx *= dampingFactor;
            particle.vy *= dampingFactor;
            
            // Swirl effect (slight rotation)
            float swirl = 0.02f;
            float dx = particle.x - centerX;
            float dy = particle.y - centerY;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist > 1.0f) {
                float perpX = -dy / dist;
                float perpY = dx / dist;
                particle.vx += perpX * swirl;
                particle.vy += perpY * swirl;
            }
            
            // Age particle (high freq particles age faster)
            float brightnessAgingFactor = (2.0f - particle.brightness); // Brighter = faster aging
            particle.age += agingRate * brightnessAgingFactor;
            
            // Remove dead or out-of-bounds particles
            if (particle.age >= 1.0f || 
                particle.x < 0 || particle.x > getWidth() ||
                particle.y < 0 || particle.y > getHeight()) {
                particles.erase(particles.begin() + i);
            }
        }
        
        // Limit particle count for performance
        if (particles.size() > 500) {
            particles.erase(particles.begin(), particles.begin() + 100);
        }
        
        repaint();
    }
    
private:
    AudioEngine& audioEngine;
    std::vector<Particle> particles;
    int frameCount;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbGraphComponent)
};

class ReverbPanel : public juce::Component, private juce::Timer {
public:
    ReverbPanel(AudioEngine& engine) : audioEngine(engine) {
        lastIrDirectory = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        auto& r = audioEngine.getReverbProcessor();
        auto params = r.getParams();
        
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setMidiInfo("MIDI: Note 26"); 
        toggleButton->setToggleState(!r.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { 
            audioEngine.getReverbProcessor().setBypassed(!toggleButton->getToggleState()); 
        };
        addAndMakeVisible(toggleButton.get());
        
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Convolution Reverb", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        
        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load IR File");
        loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF404040));
        loadButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFD4AF37));
        loadButton.onClick = [this]() { openIrFile(); };
        
        addAndMakeVisible(irNameLabel);
        irNameLabel.setText(r.getCurrentIrName(), juce::dontSendNotification);
        irNameLabel.setJustificationType(juce::Justification::centred);
        irNameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        irNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF202020));
        irNameLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xFF404040));
        
        auto cS = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& n, int cc, 
                      double min, double max, double v, const juce::String& suf) {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(n);
            s->setMidiInfo("MIDI: CC " + juce::String(cc));
            s->setRange(min, max, (max-min)/100.0);
            s->setValue(v);
            s->setTextValueSuffix(suf);
            s->getSlider().setLookAndFeel(goldenLookAndFeel.get());
            s->getSlider().onValueChange = [this]() { updateReverb(); };
            addAndMakeVisible(s.get());
        };
        
        cS(wetSlider, "Wet Level", 28, 0.0, 10.0, params.wetGain, "");
        cS(lowCutSlider, "Low Cut", 37, 20.0, 1000.0, params.lowCutHz, " Hz");
        cS(highCutSlider, "High Cut", 38, 1000.0, 20000.0, params.highCutHz, " Hz");
        
        // Add graph component
        graphComponent = std::make_unique<ReverbGraphComponent>(audioEngine);
        addAndMakeVisible(graphComponent.get());
        
        startTimerHz(15);
    }
    
    ~ReverbPanel() override {
        stopTimer();
        wetSlider->getSlider().setLookAndFeel(nullptr);
        lowCutSlider->getSlider().setLookAndFeel(nullptr);
        highCutSlider->getSlider().setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(getLocalBounds().reduced(10));
    }
    
    void resized() override {
        auto area = getLocalBounds().reduced(15);
        auto topRow = area.removeFromTop(40);
        toggleButton->setBounds(topRow.removeFromRight(40).withSizeKeepingCentre(40, 40));
        titleLabel.setBounds(topRow);
        area.removeFromTop(10);
        
        // Left-aligned controls
        int controlAreaWidth = 380;
        auto controlArea = area.removeFromLeft(controlAreaWidth);
        area.removeFromLeft(20); // Gap
        
        // Graph fills remaining space
        graphComponent->setBounds(area);
        
        // Layout IR controls and sliders
        auto irArea = controlArea.removeFromLeft(140);
        loadButton.setBounds(irArea.removeFromTop(30).reduced(5));
        irArea.removeFromTop(5);
        irNameLabel.setBounds(irArea.removeFromTop(30).reduced(5));
        
        int numSliders = 3;
        int sliderWidth = 60;
        int spacing = 30;
        int totalW = (numSliders * sliderWidth) + ((numSliders - 1) * spacing);
        int startX = controlArea.getX() + (controlArea.getWidth() - totalW) / 2;
        auto sArea = controlArea.withX(startX).withWidth(totalW);
        
        wetSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        lowCutSlider->setBounds(sArea.removeFromLeft(sliderWidth)); sArea.removeFromLeft(spacing);
        highCutSlider->setBounds(sArea.removeFromLeft(sliderWidth));
    }
    
    void updateFromPreset() {
        auto& r = audioEngine.getReverbProcessor();
        auto p = r.getParams();
        toggleButton->setToggleState(!r.isBypassed(), juce::dontSendNotification);
        wetSlider->setValue(p.wetGain, juce::dontSendNotification);
        lowCutSlider->setValue(p.lowCutHz, juce::dontSendNotification);
        highCutSlider->setValue(p.highCutHz, juce::dontSendNotification);
        irNameLabel.setText(r.getCurrentIrName(), juce::dontSendNotification);
    }
    
private:
    void timerCallback() override {
        auto p = audioEngine.getReverbProcessor().getParams();
        
        if (!wetSlider->getSlider().isMouseOverOrDragging())
            wetSlider->setValue(p.wetGain, juce::dontSendNotification);
        if (!lowCutSlider->getSlider().isMouseOverOrDragging())
            lowCutSlider->setValue(p.lowCutHz, juce::dontSendNotification);
        if (!highCutSlider->getSlider().isMouseOverOrDragging())
            highCutSlider->setValue(p.highCutHz, juce::dontSendNotification);
        
        bool shouldBeOn = !audioEngine.getReverbProcessor().isBypassed();
        if (toggleButton->getToggleState() != shouldBeOn)
            toggleButton->setToggleState(shouldBeOn, juce::dontSendNotification);
    }
    
    void updateReverb() {
        ReverbProcessor::Params p = audioEngine.getReverbProcessor().getParams();
        p.wetGain = wetSlider->getValue();
        p.lowCutHz = lowCutSlider->getValue();
        p.highCutHz = highCutSlider->getValue();
        audioEngine.getReverbProcessor().setParams(p);
    }
    
    void openIrFile() {
        auto chooser = std::make_shared<juce::FileChooser>("Load Impulse Response", lastIrDirectory, "*.wav;*.aiff;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, 
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    lastIrDirectory = file.getParentDirectory();
                    auto p = audioEngine.getReverbProcessor().getParams();
                    p.irFilePath = file.getFullPathName();
                    audioEngine.getReverbProcessor().setParams(p);
                    irNameLabel.setText(file.getFileNameWithoutExtension(), juce::dontSendNotification);
                }
            }
        );
    }
    
    AudioEngine& audioEngine;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;
    juce::TextButton loadButton;
    juce::Label irNameLabel;
    juce::File lastIrDirectory;
    std::unique_ptr<VerticalSlider> wetSlider, lowCutSlider, highCutSlider;
    std::unique_ptr<ReverbGraphComponent> graphComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbPanel)
};
