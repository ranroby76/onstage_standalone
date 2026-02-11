// ==============================================================================
//  StudioReverbPanel.h
//  OnStage — Studio Reverb UI with per-model controls
//
//  Model selector: Room / Chamber / Space / Plate
//  Each model shows its own native Airwindows sliders.
//  Animated visualization adapts per model.
// ==============================================================================

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "StyledSlider.h"
#include "EffectToggleButton.h"
#include "../dsp/StudioReverbProcessor.h"

class PresetManager;

// ==============================================================================
//  Reverb Type Button (golden accent when selected)
// ==============================================================================
class ReverbTypeButton : public juce::Component
{
public:
    ReverbTypeButton(const juce::String& label) : buttonLabel(label)
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
        if (e.mouseWasClicked() && onClick) onClick();
    }

    void setSelected(bool s) { if (isSelected != s) { isSelected = s; repaint(); } }
    bool getSelected() const { return isSelected; }
    std::function<void()> onClick;

private:
    juce::String buttonLabel;
    bool isSelected = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbTypeButton)
};

// ==============================================================================
//  Per-model animated visualization
// ==============================================================================
class StudioReverbGraphComponent : public juce::Component, private juce::Timer
{
public:
    struct Particle { float x, y, vx, vy, age, brightness, radius; };

    StudioReverbGraphComponent(StudioReverbProcessor& proc) : processor(proc)
    {
        startTimerHz(60);
        frameCount = 0;
    }
    ~StudioReverbGraphComponent() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int model = processor.getModelIndex();

        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRect(bounds);

        float cx = getWidth() * 0.5f;
        float cy = getHeight() * 0.5f;

        g.setColour(juce::Colour(0xFF505050));
        g.fillEllipse(cx - 4, cy - 4, 8, 8);

        switch (model)
        {
            case 0: paintRoom(g, cx, cy);    break;
            case 1: paintChamber(g, cx, cy); break;
            case 2: paintSpace(g, cx, cy);   break;
            case 3: paintPlate(g, cx, cy);   break;
        }

        for (const auto& p : particles)
        {
            float sz = 2.0f + p.brightness * 3.0f;
            float alpha = p.brightness * (1.0f - p.age * 0.8f);
            auto col = juce::Colour(0xFFD4AF37).withAlpha(juce::jlimit(0.0f, 1.0f, alpha));
            g.setColour(col.withAlpha(alpha * 0.25f));
            g.fillEllipse(p.x - sz * 1.5f, p.y - sz * 1.5f, sz * 3, sz * 3);
            g.setColour(col.withAlpha(alpha * 0.7f));
            g.fillEllipse(p.x - sz, p.y - sz, sz * 2, sz * 2);
            g.setColour(juce::Colours::white.withAlpha(alpha * 0.4f));
            g.fillEllipse(p.x - sz * 0.3f, p.y - sz * 0.3f, sz * 0.6f, sz * 0.6f);
        }

        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(bounds, 1.0f);
    }

private:
    // Room: concentric ripples
    void paintRoom(juce::Graphics& g, float cx, float cy)
    {
        float maxR = juce::jmin(getWidth(), getHeight()) * 0.44f;
        for (int i = 1; i <= 5; ++i)
        {
            float phase = (float)frameCount * 0.015f - i * 0.4f;
            float r = maxR * (0.15f + 0.85f * (float)i / 5.0f);
            float wobble = std::sin(phase) * 3.0f;
            float alpha = 0.08f + 0.07f * (1.0f - (float)i / 6.0f);
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(alpha));
            g.drawEllipse(cx - r - wobble, cy - r + wobble, (r + wobble) * 2, (r - wobble) * 2, 1.5f);
        }
    }

    // Chamber: golden-ratio spiral
    void paintChamber(juce::Graphics& g, float cx, float cy)
    {
        float maxR = juce::jmin(getWidth(), getHeight()) * 0.42f;
        constexpr float phi = 0.618033988749895f;
        float rotation = (float)frameCount * 0.006f;
        juce::Path spiral;
        float r = 5.0f, angle = rotation;
        spiral.startNewSubPath(cx + std::cos(angle) * r, cy + std::sin(angle) * r);
        for (int i = 0; i < 80; ++i)
        {
            r += maxR * 0.012f; angle += phi * 0.8f;
            if (r > maxR) break;
            spiral.lineTo(cx + std::cos(angle) * r, cy + std::sin(angle) * r);
        }
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.15f));
        g.strokePath(spiral, juce::PathStrokeType(1.5f));

        r = 10.0f; angle = rotation;
        for (int i = 0; i < 12; ++i)
        {
            r *= (1.0f + phi * 0.4f); angle += phi;
            if (r > maxR) break;
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.15f * (1.0f - r / maxR)));
            g.fillEllipse(cx + std::cos(angle) * r - 3, cy + std::sin(angle) * r - 3, 6, 6);
        }
    }

    // Space: drifting haze
    void paintSpace(juce::Graphics& g, float cx, float cy)
    {
        for (int i = 0; i < 5; ++i)
        {
            float r = 15.0f + i * 18.0f;
            float phase = (float)frameCount * 0.008f + i * 1.2f;
            float ox = std::sin(phase) * 20.0f;
            float oy = std::cos(phase * 0.7f) * 15.0f;
            float alpha = 0.04f + 0.02f * (5 - i);
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(alpha));
            g.fillEllipse(cx + ox - r, cy + oy - r, r * 2, r * 2);
        }
    }

    // Plate: horizontal shimmer
    void paintPlate(juce::Graphics& g, float /*cx*/, float /*cy*/)
    {
        float w = (float)getWidth(), h = (float)getHeight();
        for (int i = 0; i < 8; ++i)
        {
            float yPos = h * 0.15f + (h * 0.7f) * ((float)i / 7.0f);
            float phase = (float)frameCount * 0.03f + i * 0.9f;
            float amp = 4.0f + std::sin(phase) * 5.0f;
            juce::Path wave;
            wave.startNewSubPath(10.0f, yPos);
            for (float x = 10.0f; x < w - 10.0f; x += 4.0f)
            {
                float t = (x / w) * juce::MathConstants<float>::twoPi * (2.0f + i * 0.3f);
                wave.lineTo(x, yPos + std::sin(t + phase) * amp);
            }
            g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.06f + 0.05f * (1.0f - (float)i / 8.0f)));
            g.strokePath(wave, juce::PathStrokeType(1.2f));
        }
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.08f));
        g.drawRoundedRectangle(20, 20, w - 40, h - 40, 6, 1);
    }

    void timerCallback() override
    {
        int model = processor.getModelIndex();
        frameCount++;

        float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f;

        // Emit particles
        if (frameCount % 12 == 0)
        {
            int n = 2 + juce::Random::getSystemRandom().nextInt(3);
            for (int i = 0; i < n; ++i)
            {
                Particle p;
                float angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
                switch (model)
                {
                    case 0: // Room: radial
                        p.x = cx; p.y = cy;
                        p.vx = std::cos(angle) * 1.5f; p.vy = std::sin(angle) * 1.5f;
                        break;
                    case 1: // Chamber: spiral
                    {
                        float r0 = 10.0f + juce::Random::getSystemRandom().nextFloat() * 20.0f;
                        p.x = cx + std::cos(angle) * r0; p.y = cy + std::sin(angle) * r0;
                        float tang = angle + 1.2f;
                        p.vx = std::cos(tang) * 1.2f; p.vy = std::sin(tang) * 1.2f;
                        break;
                    }
                    case 2: // Space: slow float
                        p.x = cx + (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 30.0f;
                        p.y = cy + (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 30.0f;
                        p.vx = std::cos(angle) * 0.6f; p.vy = std::sin(angle) * 0.6f - 0.3f;
                        break;
                    case 3: // Plate: horizontal
                        p.x = cx;
                        p.y = cy + (juce::Random::getSystemRandom().nextFloat() - 0.5f) * getHeight() * 0.5f;
                        p.vx = (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 3.0f;
                        p.vy = (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.5f;
                        break;
                }
                p.age = 0.0f;
                p.brightness = 0.6f + juce::Random::getSystemRandom().nextFloat() * 0.4f;
                p.radius = 2.0f;
                particles.push_back(p);
            }
        }

        for (int i = (int)particles.size() - 1; i >= 0; --i)
        {
            auto& p = particles[(size_t)i];
            p.x += p.vx; p.y += p.vy;
            p.vx *= 0.993f; p.vy *= 0.993f;
            p.age += 0.015f;
            if (p.age >= 1.0f || p.x < -20 || p.x > getWidth() + 20 ||
                p.y < -20 || p.y > getHeight() + 20)
                particles.erase(particles.begin() + i);
        }
        if (particles.size() > 250)
            particles.erase(particles.begin(), particles.begin() + 40);

        repaint();
    }

    StudioReverbProcessor& processor;
    std::vector<Particle> particles;
    int frameCount = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioReverbGraphComponent)
};

// ==============================================================================
//  Main Panel — per-model sliders
// ==============================================================================
class StudioReverbPanel : public juce::Component, private juce::Timer
{
public:
    StudioReverbPanel(StudioReverbProcessor& proc, PresetManager& /*presets*/)
        : processor(proc)
    {
        goldenLAF = std::make_unique<GoldenSliderLookAndFeel>();
        auto p = processor.getParams();

        // Toggle
        toggleButton = std::make_unique<EffectToggleButton>();
        toggleButton->setToggleState(!processor.isBypassed(), juce::dontSendNotification);
        toggleButton->onClick = [this]() { processor.setBypassed(!toggleButton->getToggleState()); };
        addAndMakeVisible(toggleButton.get());

        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Studio Reverb", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        // Model selector
        auto makeBtn = [this](std::unique_ptr<ReverbTypeButton>& btn, const juce::String& name, int idx)
        {
            btn = std::make_unique<ReverbTypeButton>(name);
            btn->onClick = [this, idx]() { selectModel(idx); };
            addAndMakeVisible(btn.get());
        };
        makeBtn(btnRoom,    "ROOM",    0);
        makeBtn(btnChamber, "CHAMBER", 1);
        makeBtn(btnSpace,   "SPACE",   2);
        makeBtn(btnPlate,   "PLATE",   3);
        updateTypeButtons();

        // Helper to make sliders
        auto makeSlider = [&](std::unique_ptr<VerticalSlider>& s, const juce::String& name,
                              const juce::String& midi, double lo, double hi, double val,
                              const juce::String& suffix)
        {
            s = std::make_unique<VerticalSlider>();
            s->setLabelText(name);
            s->setMidiInfo(midi);
            s->setRange(lo, hi, (hi - lo) / 100.0);
            s->setValue(val);
            s->setTextValueSuffix(suffix);
            s->getSlider().setLookAndFeel(goldenLAF.get());
            s->getSlider().onValueChange = [this]() { pushToProcessor(); };
            addChildComponent(s.get()); // hidden by default
        };

        // ---- Shared Dry/Wet sliders (always visible) ----
        makeSlider(drySlider, "Dry", "CC 38", 0.0, 1.0, p.dry, "");
        makeSlider(wetSlider, "Wet", "CC 39", 0.0, 1.0, p.wet, "");
        drySlider->setVisible(true);
        wetSlider->setVisible(true);

        // ---- Room sliders (3) ----
        makeSlider(roomSizeSlider,    "Rm Size",  "CC 40", 0.0, 1.0, p.roomSize,    "");
        makeSlider(roomSustainSlider, "Sustain",  "CC 41", 0.0, 1.0, p.roomSustain, "");
        makeSlider(roomMulchSlider,   "Mulch",    "CC 42", 0.0, 1.0, p.roomMulch,   "");

        // ---- Chamber sliders (3) ----
        makeSlider(chamberDelaySlider, "Delay",   "CC 40", 0.0, 1.0, p.chamberDelay, "");
        makeSlider(chamberRegenSlider, "Regen",   "CC 41", 0.0, 1.0, p.chamberRegen, "");
        makeSlider(chamberThickSlider, "Thick",   "CC 42", 0.0, 1.0, p.chamberThick, "");

        // ---- Space sliders (5) ----
        makeSlider(spaceReplaceSlider,    "Replace",    "CC 40", 0.0, 1.0, p.spaceReplace,    "");
        makeSlider(spaceBrightnessSlider, "Brightness", "CC 41", 0.0, 1.0, p.spaceBrightness, "");
        makeSlider(spaceDetuneSlider,     "Detune",     "CC 42", 0.0, 1.0, p.spaceDetune,     "");
        makeSlider(spaceDerezSlider,      "Derez",      "CC 43", 0.0, 1.0, p.spaceDerez,      "");
        makeSlider(spaceBignessSlider,    "Bigness",    "CC 44", 0.0, 1.0, p.spaceBigness,    "");

        // ---- Plate sliders (4) ----
        makeSlider(plateInputPadSlider, "Input Pad", "CC 40", 0.0, 1.0, p.plateInputPad, "");
        makeSlider(plateDampingSlider,  "Damping",   "CC 41", 0.0, 1.0, p.plateDamping,  "");
        makeSlider(plateLowCutSlider,   "Low Cut",   "CC 42", 0.0, 1.0, p.plateLowCut,   "");
        makeSlider(platePredelaySlider, "PreDelay",  "CC 43", 0.0, 1.0, p.platePredelay, "");

        // Graph
        graphComponent = std::make_unique<StudioReverbGraphComponent>(processor);
        addAndMakeVisible(graphComponent.get());

        updateSliderVisibility();
        startTimerHz(15);
    }

    ~StudioReverbPanel() override
    {
        stopTimer();
        auto clearLAF = [](std::unique_ptr<VerticalSlider>& s) {
            if (s) s->getSlider().setLookAndFeel(nullptr);
        };
        // Dry/Wet
        clearLAF(drySlider); clearLAF(wetSlider);
        // Room
        clearLAF(roomSizeSlider); clearLAF(roomSustainSlider);
        clearLAF(roomMulchSlider);
        // Chamber
        clearLAF(chamberDelaySlider); clearLAF(chamberRegenSlider);
        clearLAF(chamberThickSlider);
        // Space
        clearLAF(spaceReplaceSlider); clearLAF(spaceBrightnessSlider);
        clearLAF(spaceDetuneSlider); clearLAF(spaceDerezSlider);
        clearLAF(spaceBignessSlider);
        // Plate
        clearLAF(plateInputPadSlider); clearLAF(plateDampingSlider);
        clearLAF(plateLowCutSlider); clearLAF(platePredelaySlider);
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
        typeRow.removeFromLeft(50);
        int bw = 75, bs = 8;
        btnRoom->setBounds(typeRow.removeFromLeft(bw));    typeRow.removeFromLeft(bs);
        btnChamber->setBounds(typeRow.removeFromLeft(bw)); typeRow.removeFromLeft(bs);
        btnSpace->setBounds(typeRow.removeFromLeft(bw));   typeRow.removeFromLeft(bs);
        btnPlate->setBounds(typeRow.removeFromLeft(bw));

        area.removeFromTop(15);

        // Count visible sliders
        int sw = 55, sp = 8;
        int visCount = countVisibleSliders();
        int controlW = visCount * sw + juce::jmax(0, visCount - 1) * sp;
        int maxControlW = getWidth() - 180;
        controlW = juce::jmin(controlW, maxControlW);

        auto controlArea = area.removeFromLeft(controlW);
        area.removeFromLeft(20);
        graphComponent->setBounds(area);

        auto layoutSlider = [&](std::unique_ptr<VerticalSlider>& s)
        {
            if (s && s->isVisible())
            {
                s->setBounds(controlArea.removeFromLeft(sw));
                controlArea.removeFromLeft(sp);
            }
        };

        // Layout per model: Dry + Wet first, then model-specific
        int model = processor.getModelIndex();

        // Dry/Wet always visible
        layoutSlider(drySlider);
        layoutSlider(wetSlider);

        // Spacer between dry/wet and model params
        controlArea.removeFromLeft(sp);

        switch (model)
        {
            case 0:
                layoutSlider(roomSizeSlider); layoutSlider(roomSustainSlider);
                layoutSlider(roomMulchSlider);
                break;
            case 1:
                layoutSlider(chamberDelaySlider); layoutSlider(chamberRegenSlider);
                layoutSlider(chamberThickSlider);
                break;
            case 2:
                layoutSlider(spaceReplaceSlider); layoutSlider(spaceBrightnessSlider);
                layoutSlider(spaceDetuneSlider); layoutSlider(spaceDerezSlider);
                layoutSlider(spaceBignessSlider);
                break;
            case 3:
                layoutSlider(plateInputPadSlider); layoutSlider(plateDampingSlider);
                layoutSlider(plateLowCutSlider); layoutSlider(platePredelaySlider);
                break;
        }
    }

    void updateFromPreset()
    {
        auto p = processor.getParams();
        toggleButton->setToggleState(!processor.isBypassed(), juce::dontSendNotification);
        updateTypeButtons();
        updateSliderVisibility();

        // Dry/Wet
        drySlider->setValue(p.dry, juce::dontSendNotification);
        wetSlider->setValue(p.wet, juce::dontSendNotification);
        // Room
        roomSizeSlider->setValue(p.roomSize, juce::dontSendNotification);
        roomSustainSlider->setValue(p.roomSustain, juce::dontSendNotification);
        roomMulchSlider->setValue(p.roomMulch, juce::dontSendNotification);
        // Chamber
        chamberDelaySlider->setValue(p.chamberDelay, juce::dontSendNotification);
        chamberRegenSlider->setValue(p.chamberRegen, juce::dontSendNotification);
        chamberThickSlider->setValue(p.chamberThick, juce::dontSendNotification);
        // Space
        spaceReplaceSlider->setValue(p.spaceReplace, juce::dontSendNotification);
        spaceBrightnessSlider->setValue(p.spaceBrightness, juce::dontSendNotification);
        spaceDetuneSlider->setValue(p.spaceDetune, juce::dontSendNotification);
        spaceDerezSlider->setValue(p.spaceDerez, juce::dontSendNotification);
        spaceBignessSlider->setValue(p.spaceBigness, juce::dontSendNotification);
        // Plate
        plateInputPadSlider->setValue(p.plateInputPad, juce::dontSendNotification);
        plateDampingSlider->setValue(p.plateDamping, juce::dontSendNotification);
        plateLowCutSlider->setValue(p.plateLowCut, juce::dontSendNotification);
        platePredelaySlider->setValue(p.platePredelay, juce::dontSendNotification);

        resized();
    }

private:
    void timerCallback() override
    {
        auto p = processor.getParams();
        int model = processor.getModelIndex();

        auto sync = [](std::unique_ptr<VerticalSlider>& s, float val) {
            if (s && s->isVisible() && !s->getSlider().isMouseOverOrDragging())
                s->setValue((double)val, juce::dontSendNotification);
        };

        // Dry/Wet always synced
        sync(drySlider, p.dry);
        sync(wetSlider, p.wet);

        switch (model)
        {
            case 0:
                sync(roomSizeSlider, p.roomSize); sync(roomSustainSlider, p.roomSustain);
                sync(roomMulchSlider, p.roomMulch);
                break;
            case 1:
                sync(chamberDelaySlider, p.chamberDelay); sync(chamberRegenSlider, p.chamberRegen);
                sync(chamberThickSlider, p.chamberThick);
                break;
            case 2:
                sync(spaceReplaceSlider, p.spaceReplace); sync(spaceBrightnessSlider, p.spaceBrightness);
                sync(spaceDetuneSlider, p.spaceDetune); sync(spaceDerezSlider, p.spaceDerez);
                sync(spaceBignessSlider, p.spaceBigness);
                break;
            case 3:
                sync(plateInputPadSlider, p.plateInputPad); sync(plateDampingSlider, p.plateDamping);
                sync(plateLowCutSlider, p.plateLowCut); sync(platePredelaySlider, p.platePredelay);
                break;
        }

        bool on = !processor.isBypassed();
        if (toggleButton->getToggleState() != on)
            toggleButton->setToggleState(on, juce::dontSendNotification);
    }

    void selectModel(int idx)
    {
        if (processor.getModelIndex() != idx)
        {
            processor.setModel(idx);
            updateTypeButtons();
            updateSliderVisibility();
            resized();
            repaint();
        }
    }

    void updateTypeButtons()
    {
        int m = processor.getModelIndex();
        btnRoom->setSelected(m == 0);
        btnChamber->setSelected(m == 1);
        btnSpace->setSelected(m == 2);
        btnPlate->setSelected(m == 3);
    }

    void updateSliderVisibility()
    {
        int m = processor.getModelIndex();

        auto show = [](std::unique_ptr<VerticalSlider>& s, bool v) {
            if (s) s->setVisible(v);
        };

        // Dry/Wet always visible
        show(drySlider, true); show(wetSlider, true);
        // Room
        show(roomSizeSlider, m == 0); show(roomSustainSlider, m == 0);
        show(roomMulchSlider, m == 0);
        // Chamber
        show(chamberDelaySlider, m == 1); show(chamberRegenSlider, m == 1);
        show(chamberThickSlider, m == 1);
        // Space
        show(spaceReplaceSlider, m == 2); show(spaceBrightnessSlider, m == 2);
        show(spaceDetuneSlider, m == 2); show(spaceDerezSlider, m == 2);
        show(spaceBignessSlider, m == 2);
        // Plate
        show(plateInputPadSlider, m == 3); show(plateDampingSlider, m == 3);
        show(plateLowCutSlider, m == 3); show(platePredelaySlider, m == 3);
    }

    int countVisibleSliders() const
    {
        int base = 2; // Dry + Wet always visible
        switch (processor.getModelIndex())
        {
            case 0: return base + 3;  // Room
            case 1: return base + 3;  // Chamber
            case 2: return base + 5;  // Space
            case 3: return base + 4;  // Plate
            default: return base + 3;
        }
    }

    void pushToProcessor()
    {
        StudioReverbProcessor::Params p = processor.getParams();
        int model = processor.getModelIndex();

        // Dry/Wet always read
        p.dry = (float)drySlider->getValue();
        p.wet = (float)wetSlider->getValue();

        switch (model)
        {
            case 0:
                p.roomSize    = (float)roomSizeSlider->getValue();
                p.roomSustain = (float)roomSustainSlider->getValue();
                p.roomMulch   = (float)roomMulchSlider->getValue();
                break;
            case 1:
                p.chamberDelay = (float)chamberDelaySlider->getValue();
                p.chamberRegen = (float)chamberRegenSlider->getValue();
                p.chamberThick = (float)chamberThickSlider->getValue();
                break;
            case 2:
                p.spaceReplace    = (float)spaceReplaceSlider->getValue();
                p.spaceBrightness = (float)spaceBrightnessSlider->getValue();
                p.spaceDetune     = (float)spaceDetuneSlider->getValue();
                p.spaceDerez      = (float)spaceDerezSlider->getValue();
                p.spaceBigness    = (float)spaceBignessSlider->getValue();
                break;
            case 3:
                p.plateInputPad = (float)plateInputPadSlider->getValue();
                p.plateDamping  = (float)plateDampingSlider->getValue();
                p.plateLowCut   = (float)plateLowCutSlider->getValue();
                p.platePredelay = (float)platePredelaySlider->getValue();
                break;
        }
        processor.setParams(p);
    }

    // ==============================================================================
    StudioReverbProcessor& processor;
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLAF;
    std::unique_ptr<EffectToggleButton> toggleButton;
    juce::Label titleLabel;

    // Type buttons
    std::unique_ptr<ReverbTypeButton> btnRoom, btnChamber, btnSpace, btnPlate;

    // Shared Dry/Wet sliders
    std::unique_ptr<VerticalSlider> drySlider, wetSlider;

    // Room sliders
    std::unique_ptr<VerticalSlider> roomSizeSlider, roomSustainSlider, roomMulchSlider;

    // Chamber sliders
    std::unique_ptr<VerticalSlider> chamberDelaySlider, chamberRegenSlider, chamberThickSlider;

    // Space sliders
    std::unique_ptr<VerticalSlider> spaceReplaceSlider, spaceBrightnessSlider, spaceDetuneSlider;
    std::unique_ptr<VerticalSlider> spaceDerezSlider, spaceBignessSlider;

    // Plate sliders
    std::unique_ptr<VerticalSlider> plateInputPadSlider, plateDampingSlider, plateLowCutSlider;
    std::unique_ptr<VerticalSlider> platePredelaySlider;

    // Animation
    std::unique_ptr<StudioReverbGraphComponent> graphComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioReverbPanel)
};
