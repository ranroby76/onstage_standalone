// #D:\Workspace\Subterraneum_plugins_daw\src\LatcherEditorComponent.h
// THE LATCHER EDITOR - CallOutBox popup editor for LatcherProcessor
// Layout:
//   TOP:    Title bar with "The Latcher" + All Notes Off button
//   CENTER: 4x4 pad grid (clickable toggle pads with latch state)
//   BOTTOM: Selected pad settings (Trigger Note, Output Note, Velocity, Channel)

#pragma once

#include <JuceHeader.h>
#include "LatcherProcessor.h"

class LatcherEditorComponent : public juce::Component,
                                public juce::Timer {
public:
    LatcherEditorComponent(LatcherProcessor* processor)
        : proc(processor)
    {
        setSize(520, 480);
        startTimerHz(20);
    }

    ~LatcherEditorComponent() override { stopTimer(); }

    // =========================================================================
    // Paint
    // =========================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(35, 35, 40));
        if (!proc) return;

        auto bounds = getLocalBounds();

        // --- TITLE BAR ---
        auto titleBar = bounds.removeFromTop(40);
        g.setColour(juce::Colour(50, 50, 58));
        g.fillRect(titleBar);
        g.setColour(juce::Colour(70, 70, 80));
        g.drawLine((float)titleBar.getX(), (float)titleBar.getBottom(),
                   (float)titleBar.getRight(), (float)titleBar.getBottom(), 1.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.drawText("The Latcher", titleBar.reduced(12, 0), juce::Justification::centredLeft);

        // All Notes Off button
        allNotesOffRect = titleBar.removeFromRight(120).reduced(8, 6).toFloat();
        g.setColour(juce::Colour(120, 40, 40));
        g.fillRoundedRectangle(allNotesOffRect, 5.0f);
        g.setColour(juce::Colour(180, 60, 60));
        g.drawRoundedRectangle(allNotesOffRect, 5.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("ALL OFF", allNotesOffRect, juce::Justification::centred);

        // --- BOTTOM CONTROLS BAR (selected pad settings) ---
        auto bottomBar = bounds.removeFromBottom(100);
        g.setColour(juce::Colour(42, 42, 48));
        g.fillRect(bottomBar);
        g.setColour(juce::Colour(70, 70, 80));
        g.drawLine((float)bottomBar.getX(), (float)bottomBar.getY(),
                   (float)bottomBar.getRight(), (float)bottomBar.getY(), 1.0f);
        drawPadSettings(g, bottomBar);

        // --- CENTER: 4x4 PAD GRID ---
        padGridArea = bounds.reduced(12, 12).toFloat();
        drawPadGrid(g, padGridArea);
    }

    // =========================================================================
    // Draw 4x4 pad grid
    // =========================================================================
    void drawPadGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float padW = area.getWidth() / LatcherProcessor::GridCols;
        float padH = area.getHeight() / LatcherProcessor::GridRows;
        float gap = 4.0f;

        for (int row = 0; row < LatcherProcessor::GridRows; row++)
        {
            for (int col = 0; col < LatcherProcessor::GridCols; col++)
            {
                int padIndex = row * LatcherProcessor::GridCols + col;
                auto padRect = juce::Rectangle<float>(
                    area.getX() + col * padW + gap / 2,
                    area.getY() + row * padH + gap / 2,
                    padW - gap,
                    padH - gap);

                bool latched = proc->isPadLatched(padIndex);
                bool selected = (padIndex == selectedPad);

                // Pad background
                if (latched)
                {
                    g.setColour(juce::Colour(30, 120, 200));  // Blue when latched
                    g.fillRoundedRectangle(padRect, 6.0f);
                    // Glow effect
                    g.setColour(juce::Colour(60, 160, 255).withAlpha(0.2f));
                    g.fillRoundedRectangle(padRect.expanded(2), 8.0f);
                }
                else
                {
                    g.setColour(juce::Colour(55, 55, 65));
                    g.fillRoundedRectangle(padRect, 6.0f);
                }

                // Selection border
                if (selected)
                {
                    g.setColour(juce::Colours::orange);
                    g.drawRoundedRectangle(padRect, 6.0f, 2.5f);
                }
                else
                {
                    g.setColour(latched ? juce::Colour(80, 160, 240) : juce::Colour(80, 80, 90));
                    g.drawRoundedRectangle(padRect, 6.0f, 1.0f);
                }

                // Pad number (top-left)
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.drawText(juce::String(padIndex + 1),
                           padRect.reduced(6, 4), juce::Justification::topLeft);

                // Output note name (center)
                const auto& pad = proc->getPad(padIndex);
                juce::String noteName = juce::MidiMessage::getMidiNoteName(pad.outputNote, true, true, 4);
                g.setColour(latched ? juce::Colours::white : juce::Colours::white.withAlpha(0.7f));
                g.setFont(juce::Font(juce::FontOptions(latched ? 16.0f : 14.0f, juce::Font::bold)));
                g.drawText(noteName, padRect, juce::Justification::centred);

                // Channel (bottom-right)
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.drawText("Ch" + juce::String(pad.midiChannel),
                           padRect.reduced(6, 4), juce::Justification::bottomRight);

                // Velocity bar (bottom, thin)
                float velFrac = (float)pad.velocity / 127.0f;
                auto velBar = padRect.removeFromBottom(3).reduced(8, 0);
                g.setColour(juce::Colour(60, 60, 70));
                g.fillRoundedRectangle(velBar, 1.0f);
                g.setColour(latched ? juce::Colour(100, 200, 255) : juce::Colour(100, 100, 120));
                g.fillRoundedRectangle(velBar.withWidth(velBar.getWidth() * velFrac), 1.0f);
            }
        }
    }

    // =========================================================================
    // Draw selected pad settings (bottom bar)
    // =========================================================================
    void drawPadSettings(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto& pad = proc->getPad(selectedPad);
        auto content = area.reduced(12, 8);

        // Title
        g.setColour(juce::Colours::orange);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("Pad " + juce::String(selectedPad + 1) + " Settings",
                   content.removeFromTop(20), juce::Justification::centredLeft);

        auto row = content.toFloat();
        float boxW = (row.getWidth() - 30) / 4.0f;

        // Helper to draw a labeled value box
        auto drawValueBox = [&](juce::Rectangle<float> rect, const juce::String& label,
                                const juce::String& value, const juce::String& sublabel = "")
        {
            // Background
            g.setColour(juce::Colour(30, 30, 35));
            g.fillRoundedRectangle(rect, 4.0f);
            g.setColour(juce::Colour(60, 60, 70));
            g.drawRoundedRectangle(rect, 4.0f, 1.0f);

            // Label (top)
            g.setColour(juce::Colour(160, 160, 180));
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.drawText(label, rect.reduced(4, 2), juce::Justification::topLeft);

            // Value (center)
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
            g.drawText(value, rect, juce::Justification::centred);

            // Sublabel (bottom)
            if (sublabel.isNotEmpty())
            {
                g.setColour(juce::Colour(120, 120, 140));
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.drawText(sublabel, rect.reduced(4, 2), juce::Justification::centredBottom);
            }
        };

        // Store rects for mouse interaction
        triggerNoteRect = juce::Rectangle<float>(row.getX(), row.getY(), boxW, row.getHeight()).reduced(2);
        outputNoteRect = juce::Rectangle<float>(row.getX() + boxW + 10, row.getY(), boxW, row.getHeight()).reduced(2);
        velocityRect = juce::Rectangle<float>(row.getX() + (boxW + 10) * 2, row.getY(), boxW, row.getHeight()).reduced(2);
        channelRect = juce::Rectangle<float>(row.getX() + (boxW + 10) * 3, row.getY(), boxW, row.getHeight()).reduced(2);

        juce::String trigNoteName = juce::MidiMessage::getMidiNoteName(pad.triggerNote, true, true, 4);
        juce::String outNoteName = juce::MidiMessage::getMidiNoteName(pad.outputNote, true, true, 4);

        drawValueBox(triggerNoteRect, "TRIGGER", trigNoteName, "Drag \xE2\x86\x95 (Note " + juce::String(pad.triggerNote) + ")");
        drawValueBox(outputNoteRect,  "OUTPUT",  outNoteName,  "Drag \xE2\x86\x95 (Note " + juce::String(pad.outputNote) + ")");
        drawValueBox(velocityRect,    "VELOCITY", juce::String(pad.velocity), "Drag \xE2\x86\x95");
        drawValueBox(channelRect,     "CHANNEL",  juce::String(pad.midiChannel), "Drag \xE2\x86\x95 (1-16)");
    }

    // =========================================================================
    // Mouse
    // =========================================================================
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!proc) return;
        auto pos = e.position;

        // All Notes Off button
        if (allNotesOffRect.contains(pos))
        {
            proc->allNotesOff();
            repaint();
            return;
        }

        // Check pad grid
        if (padGridArea.contains(pos))
        {
            float padW = padGridArea.getWidth() / LatcherProcessor::GridCols;
            float padH = padGridArea.getHeight() / LatcherProcessor::GridRows;

            int col = (int)((pos.x - padGridArea.getX()) / padW);
            int row = (int)((pos.y - padGridArea.getY()) / padH);

            col = juce::jlimit(0, LatcherProcessor::GridCols - 1, col);
            row = juce::jlimit(0, LatcherProcessor::GridRows - 1, row);

            int padIndex = row * LatcherProcessor::GridCols + col;

            if (e.mods.isRightButtonDown())
            {
                // Right-click: select pad for editing
                selectedPad = padIndex;
            }
            else
            {
                // Left-click: toggle latch AND select
                selectedPad = padIndex;
                proc->togglePadFromUI(padIndex);
            }

            repaint();
            return;
        }

        // Check bottom value boxes for drag start
        if (triggerNoteRect.contains(pos))
        {
            dragTarget = DragTarget::TriggerNote;
            dragStartY = pos.y;
            dragStartValue = proc->getPad(selectedPad).triggerNote;
            return;
        }
        if (outputNoteRect.contains(pos))
        {
            dragTarget = DragTarget::OutputNote;
            dragStartY = pos.y;
            dragStartValue = proc->getPad(selectedPad).outputNote;
            return;
        }
        if (velocityRect.contains(pos))
        {
            dragTarget = DragTarget::Velocity;
            dragStartY = pos.y;
            dragStartValue = proc->getPad(selectedPad).velocity;
            return;
        }
        if (channelRect.contains(pos))
        {
            dragTarget = DragTarget::Channel;
            dragStartY = pos.y;
            dragStartValue = proc->getPad(selectedPad).midiChannel;
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!proc || dragTarget == DragTarget::None) return;

        float deltaY = dragStartY - e.position.y;
        auto& pad = proc->getPad(selectedPad);

        switch (dragTarget)
        {
            case DragTarget::TriggerNote:
                pad.triggerNote = juce::jlimit(0, 127, (int)(dragStartValue + deltaY * 0.3));
                break;
            case DragTarget::OutputNote:
                pad.outputNote = juce::jlimit(0, 127, (int)(dragStartValue + deltaY * 0.3));
                break;
            case DragTarget::Velocity:
                pad.velocity = juce::jlimit(1, 127, (int)(dragStartValue + deltaY * 0.5));
                break;
            case DragTarget::Channel:
                pad.midiChannel = juce::jlimit(1, 16, (int)(dragStartValue + deltaY * 0.1));
                break;
            default:
                break;
        }

        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragTarget = DragTarget::None;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!proc) return;

        // Double-click on value boxes: reset to default
        if (triggerNoteRect.contains(e.position))
        {
            proc->getPad(selectedPad).triggerNote = 60 + selectedPad;
            repaint();
            return;
        }
        if (outputNoteRect.contains(e.position))
        {
            proc->getPad(selectedPad).outputNote = 60 + selectedPad;
            repaint();
            return;
        }
        if (velocityRect.contains(e.position))
        {
            proc->getPad(selectedPad).velocity = 100;
            repaint();
            return;
        }
        if (channelRect.contains(e.position))
        {
            proc->getPad(selectedPad).midiChannel = 1;
            repaint();
            return;
        }
    }

    // =========================================================================
    // Timer
    // =========================================================================
    void timerCallback() override
    {
        repaint();
    }

private:
    LatcherProcessor* proc = nullptr;
    int selectedPad = 0;

    // Layout rects (updated in paint)
    juce::Rectangle<float> padGridArea;
    juce::Rectangle<float> allNotesOffRect;
    juce::Rectangle<float> triggerNoteRect;
    juce::Rectangle<float> outputNoteRect;
    juce::Rectangle<float> velocityRect;
    juce::Rectangle<float> channelRect;

    // Drag state for value editing
    enum class DragTarget { None, TriggerNote, OutputNote, Velocity, Channel };
    DragTarget dragTarget = DragTarget::None;
    float dragStartY = 0.0f;
    float dragStartValue = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LatcherEditorComponent)
};
