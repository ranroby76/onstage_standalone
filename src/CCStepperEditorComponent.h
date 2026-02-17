// #D:\Workspace\Subterraneum_plugins_daw\src\CCStepperEditorComponent.h
// CC STEPPER EDITOR - CallOutBox popup editor for CCStepperProcessor
// Layout:
//   TOP:    Title bar with enable toggle + play icon + slot name
//   LEFT:   16 slot selector buttons
//   CENTER: Step bar grid (draggable CC values)
//   RIGHT:  Shift/utility buttons
//   BOTTOM: Controls (Ch, Steps, CC#, Rate, Type, Order, Speed, Swing, Smooth, T.Sig, Sync)

#pragma once

#include <JuceHeader.h>
#include "CCStepperProcessor.h"

class CCStepperEditorComponent : public juce::Component,
                                  public juce::Timer {
public:
    CCStepperEditorComponent(CCStepperProcessor* processor)
        : proc(processor)
    {
        setSize(900, 520);
        startTimerHz(20);
    }

    ~CCStepperEditorComponent() override { stopTimer(); }

    // =========================================================================
    // Paint
    // =========================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(35, 35, 40));
        if (!proc) return;

        auto bounds = getLocalBounds();

        // --- TITLE BAR ---
        auto titleBar = bounds.removeFromTop(36);
        g.setColour(juce::Colour(50, 50, 58));
        g.fillRect(titleBar);
        g.setColour(juce::Colour(70, 70, 80));
        g.drawLine((float)titleBar.getX(), (float)titleBar.getBottom(),
                    (float)titleBar.getRight(), (float)titleBar.getBottom(), 1.0f);

        auto& slot = proc->getSlot(selectedSlot);

        // Enable toggle circle with play triangle icon
        auto toggleRect = juce::Rectangle<float>(8.0f, 8.0f, 20.0f, 20.0f);
        g.setColour(slot.enabled ? juce::Colours::limegreen : juce::Colour(80, 80, 80));
        g.fillEllipse(toggleRect);
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawEllipse(toggleRect, 1.0f);

        // Play triangle inside the circle
        {
            auto cx = toggleRect.getCentreX();
            auto cy = toggleRect.getCentreY();
            float triSize = 6.0f;
            juce::Path playTri;
            playTri.addTriangle(
                cx - triSize * 0.35f, cy - triSize * 0.5f,
                cx - triSize * 0.35f, cy + triSize * 0.5f,
                cx + triSize * 0.55f, cy);
            g.setColour(slot.enabled ? juce::Colours::black : juce::Colour(50, 50, 50));
            g.fillPath(playTri);
        }

        // Slot name
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText("Sequence #" + juce::String(selectedSlot + 1) +
                   "  (Ch " + juce::String(slot.midiChannel) + ")",
                    titleBar.withTrimmedLeft(36).reduced(8, 0), juce::Justification::centredLeft);

        // --- BOTTOM CONTROLS BAR ---
        auto bottomBar = bounds.removeFromBottom(56);
        g.setColour(juce::Colour(42, 42, 48));
        g.fillRect(bottomBar);
        g.setColour(juce::Colour(70, 70, 80));
        g.drawLine((float)bottomBar.getX(), (float)bottomBar.getY(),
                    (float)bottomBar.getRight(), (float)bottomBar.getY(), 1.0f);
        drawBottomControls(g, bottomBar);

        // --- LEFT SLOT SELECTOR ---
        auto slotStrip = bounds.removeFromLeft(40);
        g.setColour(juce::Colour(42, 42, 48));
        g.fillRect(slotStrip);
        drawSlotSelector(g, slotStrip);

        // --- RIGHT UTILITY BUTTONS ---
        auto rightStrip = bounds.removeFromRight(36);
        g.setColour(juce::Colour(42, 42, 48));
        g.fillRect(rightStrip);
        drawUtilityButtons(g, rightStrip);

        // --- CENTER STEP GRID ---
        gridArea = bounds.reduced(2, 2).toFloat();
        drawStepGrid(g, gridArea);
    }

    // =========================================================================
    // Mouse
    // =========================================================================
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!proc) return;
        auto pos = e.position;
        auto bounds = getLocalBounds();

        auto titleBar = bounds.removeFromTop(36);
        // Toggle enabled
        auto toggleRect = juce::Rectangle<float>(8.0f, 8.0f, 20.0f, 20.0f);
        if (toggleRect.contains(pos))
        {
            auto& slot = proc->getSlot(selectedSlot);
            slot.enabled = !slot.enabled;
            repaint();
            return;
        }

        auto bottomBar = bounds.removeFromBottom(56);
        if (bottomBar.toFloat().contains(pos))
        {
            handleBottomBarClick(pos, bottomBar);
            return;
        }

        auto slotStrip = bounds.removeFromLeft(40);
        if (slotStrip.toFloat().contains(pos))
        {
            int slotIdx = (int)((pos.y - slotStrip.getY()) / (slotStrip.getHeight() / 16.0f));
            selectedSlot = juce::jlimit(0, 15, slotIdx);
            repaint();
            return;
        }

        auto rightStrip = bounds.removeFromRight(36);
        if (rightStrip.toFloat().contains(pos))
        {
            handleUtilityClick(pos, rightStrip);
            return;
        }

        gridArea = bounds.reduced(2, 2).toFloat();
        if (gridArea.contains(pos))
        {
            if (e.mods.isRightButtonDown()) { handleGridRightClick(pos); return; }
            editStepAtPosition(pos);
            isDraggingGrid = true;
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!proc || !isDraggingGrid) return;
        if (gridArea.contains(e.position)) editStepAtPosition(e.position);
    }

    void mouseUp(const juce::MouseEvent&) override { isDraggingGrid = false; }

    void timerCallback() override
    {
        if (!proc) return;
        int newStep = proc->getSlotCurrentStep(selectedSlot);
        if (newStep != lastDisplayedStep) { lastDisplayedStep = newStep; repaint(); }
    }

private:
    // =========================================================================
    // Draw: Step Grid
    // =========================================================================
    void drawStepGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto& slot = proc->getSlot(selectedSlot);
        int count = slot.stepCount;
        float barWidth = area.getWidth() / (float)count;
        float barMaxHeight = area.getHeight();

        g.setColour(juce::Colour(25, 25, 30));
        g.fillRect(area);

        // Grid lines every 4 steps
        g.setColour(juce::Colour(45, 45, 55));
        for (int i = 4; i < count; i += 4)
        {
            float x = area.getX() + i * barWidth;
            g.drawVerticalLine((int)x, area.getY(), area.getBottom());
        }

        // Horizontal guides at 25%, 50%, 75%
        g.setColour(juce::Colour(40, 40, 50));
        for (float frac : { 0.25f, 0.5f, 0.75f })
        {
            float y = area.getBottom() - frac * barMaxHeight;
            g.drawHorizontalLine((int)y, area.getX(), area.getRight());
        }

        // Active step highlight
        int activeStep = proc->getSlotCurrentStep(selectedSlot);
        if (proc->isPlaying() && slot.enabled && activeStep >= 0 && activeStep < count)
        {
            float x = area.getX() + activeStep * barWidth;
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRect(x, area.getY(), barWidth, barMaxHeight);
        }

        // Step bars
        for (int i = 0; i < count; i++)
        {
            float val = (float)slot.steps[i] / 127.0f;
            float barH = val * barMaxHeight;
            float x = area.getX() + i * barWidth;
            float y = area.getBottom() - barH;

            if (barH > 0.5f)
            {
                juce::Colour barColor;
                if (val < 0.33f)       barColor = juce::Colour(40, 120, 140);
                else if (val < 0.66f)  barColor = juce::Colour(60, 160, 80);
                else                   barColor = juce::Colour(140, 200, 60);

                if (i == activeStep && proc->isPlaying() && slot.enabled)
                    barColor = barColor.brighter(0.4f);

                g.setColour(barColor);
                g.fillRect(juce::Rectangle<float>(x + 1, y, barWidth - 2, barH));
                g.setColour(barColor.brighter(0.3f));
                g.fillRect(x + 1, y, barWidth - 2, 2.0f);
            }

            g.setColour(juce::Colour(55, 55, 65));
            g.drawRect(x, area.getY(), barWidth, barMaxHeight, 0.5f);
        }

        g.setColour(juce::Colour(70, 70, 80));
        g.drawRect(area, 1.0f);
    }

    // =========================================================================
    // Draw: Slot Selector
    // =========================================================================
    void drawSlotSelector(juce::Graphics& g, juce::Rectangle<int> strip)
    {
        float btnH = strip.getHeight() / 16.0f;
        for (int i = 0; i < 16; i++)
        {
            auto btnArea = juce::Rectangle<float>(
                (float)strip.getX(), strip.getY() + i * btnH, (float)strip.getWidth(), btnH);

            bool isSelected = (i == selectedSlot);
            bool hasData = proc->slotHasData(i);
            bool isEnabled = proc->getSlot(i).enabled;

            if (isSelected)          g.setColour(juce::Colour(60, 90, 140));
            else if (isEnabled)      g.setColour(juce::Colour(50, 60, 70));
            else                     g.setColour(juce::Colour(42, 42, 48));
            g.fillRect(btnArea.reduced(1));

            g.setColour(isSelected ? juce::Colours::white
                                    : (hasData ? juce::Colour(180, 180, 200) : juce::Colour(90, 90, 100)));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(juce::String(i + 1), btnArea, juce::Justification::centred);

            if (isEnabled && proc->isPlaying())
            {
                g.setColour(juce::Colours::limegreen);
                g.fillEllipse(btnArea.getX() + 2, btnArea.getY() + 2, 4, 4);
            }
        }
    }

    // =========================================================================
    // Draw: Utility Buttons
    // =========================================================================
    void drawUtilityButtons(juce::Graphics& g, juce::Rectangle<int> strip)
    {
        struct BtnInfo { const char* label; juce::Colour col; };
        BtnInfo buttons[] = {
            { "=",  juce::Colour(100, 100, 120) },
            { "^",  juce::Colour(80, 130, 180) },
            { "v",  juce::Colour(80, 130, 180) },
            { "<",  juce::Colour(80, 130, 180) },
            { ">",  juce::Colour(80, 130, 180) },
            { "C",  juce::Colour(180, 80, 80) },
        };

        float btnH = 30.0f;
        float startY = strip.getY() + 4.0f;

        for (int i = 0; i < 6; i++)
        {
            auto btnArea = juce::Rectangle<float>(
                strip.getX() + 3.0f, startY + i * (btnH + 3.0f),
                strip.getWidth() - 6.0f, btnH);

            g.setColour(buttons[i].col.darker(0.3f));
            g.fillRoundedRectangle(btnArea, 3.0f);
            g.setColour(buttons[i].col);
            g.drawRoundedRectangle(btnArea, 3.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText(buttons[i].label, btnArea, juce::Justification::centred);
        }
    }

    // =========================================================================
    // Draw: Bottom Controls
    // =========================================================================
    void drawBottomControls(juce::Graphics& g, juce::Rectangle<int> bar)
    {
        auto& slot = proc->getSlot(selectedSlot);
        auto area = bar.reduced(6, 4);

        // Time sig display
        juce::String tsSyncLabel = proc->isSyncToMasterTimeSig() ? "Master" :
            (juce::String(proc->getTimeSigNumerator()) + "/" + juce::String(proc->getTimeSigDenominator()));

        struct CtrlInfo { const char* label; juce::String value; float width; };
        CtrlInfo controls[] = {
            { "Ch",     juce::String(slot.midiChannel),                                  45.0f },
            { "Steps",  juce::String(slot.stepCount),                                    55.0f },
            { "CC#",    juce::String(slot.ccNumber),                                     50.0f },
            { "Rate",   CCStepperProcessor::getRateName(slot.rate),                      55.0f },
            { "Type",   CCStepperProcessor::getTypeName(slot.type),                      65.0f },
            { "Order",  CCStepperProcessor::getOrderName(slot.order),                    75.0f },
            { "Speed",  CCStepperProcessor::getSpeedName(slot.speed),                    60.0f },
            { "Swing",  juce::String((int)slot.swing) + "%",                             55.0f },
            { "Smooth", slot.smooth ? "On" : "Off",                                      55.0f },
            { "T.Sig",  tsSyncLabel,                                                     60.0f },
            { "SyncTS", proc->isSyncToMasterTimeSig() ? "On" : "Off",                    50.0f },
        };

        float x = (float)area.getX();
        float labelY = (float)area.getY();
        float valueY = labelY + 18.0f;

        for (int i = 0; i < 11; i++)
        {
            // Label
            g.setColour(juce::Colour(140, 140, 160));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            auto labelArea = juce::Rectangle<float>(x, labelY, controls[i].width, 16);
            g.drawText(controls[i].label, labelArea, juce::Justification::centred);

            // Value box
            auto valueArea = juce::Rectangle<float>(x + 2, valueY, controls[i].width - 4, 22.0f);

            // SyncTS uses toggle color
            if (i == 10)
            {
                bool syncOn = proc->isSyncToMasterTimeSig();
                g.setColour(syncOn ? juce::Colour(30, 70, 30) : juce::Colour(30, 30, 38));
            }
            else
                g.setColour(juce::Colour(30, 30, 38));

            g.fillRoundedRectangle(valueArea, 3.0f);
            g.setColour(juce::Colour(70, 70, 85));
            g.drawRoundedRectangle(valueArea, 3.0f, 1.0f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(controls[i].value, valueArea, juce::Justification::centred);

            x += controls[i].width + 6.0f;
        }
    }

    // =========================================================================
    // Click handlers
    // =========================================================================
    void editStepAtPosition(juce::Point<float> pos)
    {
        auto& slot = proc->getSlot(selectedSlot);
        int count = slot.stepCount;
        float barWidth = gridArea.getWidth() / (float)count;
        int stepIdx = juce::jlimit(0, count - 1, (int)((pos.x - gridArea.getX()) / barWidth));
        float normalY = 1.0f - (pos.y - gridArea.getY()) / gridArea.getHeight();
        slot.steps[stepIdx] = juce::jlimit(0, 127, (int)(normalY * 127.0f));
        repaint();
    }

    void handleBottomBarClick(juce::Point<float> pos, juce::Rectangle<int> bar)
    {
        auto& slot = proc->getSlot(selectedSlot);
        auto area = bar.reduced(6, 4);
        float x = (float)area.getX();
        float valueY = (float)area.getY() + 18.0f;

        float widths[] = { 45, 55, 50, 55, 65, 75, 60, 55, 55, 60, 50 };

        for (int i = 0; i < 11; i++)
        {
            auto valueArea = juce::Rectangle<float>(x + 2, valueY, widths[i] - 4, 22.0f);
            if (valueArea.contains(pos))
            {
                handleControlClick(i, slot);
                return;
            }
            x += widths[i] + 6.0f;
        }
    }

    void handleControlClick(int ci, CCStepperProcessor::Slot& slot)
    {
        juce::PopupMenu menu;

        switch (ci)
        {
            case 0: // Ch (MIDI Channel)
            {
                for (int ch = 1; ch <= 16; ch++)
                    menu.addItem(ch, "Ch " + juce::String(ch), true, ch == slot.midiChannel);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.midiChannel = r; repaint(); }
                });
                break;
            }
            case 1: // Steps
            {
                for (int i = 0; i < CCStepperProcessor::NumAllowedStepCounts; i++)
                {
                    int sc = CCStepperProcessor::AllowedStepCounts[i];
                    menu.addItem(i + 1, juce::String(sc), true, sc == slot.stepCount);
                }
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.stepCount = CCStepperProcessor::AllowedStepCounts[r - 1]; repaint(); }
                });
                break;
            }
            case 2: // CC#
            {
                menu.addSectionHeader("Common CCs");
                struct CCN { int n; const char* s; };
                CCN common[] = {
                    {1,"1-Mod"},{7,"7-Vol"},{10,"10-Pan"},{11,"11-Expr"},
                    {64,"64-Sustain"},{71,"71-Res"},{74,"74-Cut"},{91,"91-Rev"},{93,"93-Cho"}
                };
                for (auto& cc : common) menu.addItem(cc.n + 1, cc.s, true, cc.n == slot.ccNumber);
                menu.addSeparator();
                for (int cc = 0; cc < 128; cc++)
                    menu.addItem(cc + 1, "CC " + juce::String(cc), true, cc == slot.ccNumber);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.ccNumber = r - 1; repaint(); }
                });
                break;
            }
            case 3: // Rate
            {
                for (int r = 0; r < CCStepperProcessor::NumRates; r++)
                    menu.addItem(r + 1, CCStepperProcessor::getRateName((CCStepperProcessor::RateDiv)r),
                                  true, r == (int)slot.rate);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.rate = (CCStepperProcessor::RateDiv)(r - 1); repaint(); }
                });
                break;
            }
            case 4: // Type
            {
                for (int t = 0; t < CCStepperProcessor::NumRateTypes; t++)
                    menu.addItem(t + 1, CCStepperProcessor::getTypeName((CCStepperProcessor::RateType)t),
                                  true, t == (int)slot.type);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.type = (CCStepperProcessor::RateType)(r - 1); repaint(); }
                });
                break;
            }
            case 5: // Order
            {
                for (int o = 0; o < CCStepperProcessor::NumOrders; o++)
                    menu.addItem(o + 1, CCStepperProcessor::getOrderName((CCStepperProcessor::StepOrder)o),
                                  true, o == (int)slot.order);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.order = (CCStepperProcessor::StepOrder)(r - 1); repaint(); }
                });
                break;
            }
            case 6: // Speed
            {
                for (int s = 0; s < CCStepperProcessor::NumSpeedModes; s++)
                    menu.addItem(s + 1, CCStepperProcessor::getSpeedName((CCStepperProcessor::SpeedMode)s),
                                  true, s == (int)slot.speed);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) { slot.speed = (CCStepperProcessor::SpeedMode)(r - 1); repaint(); }
                });
                break;
            }
            case 7: // Swing (0-100)
            {
                int swingValues[] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                for (int i = 0; i < 11; i++)
                    menu.addItem(i + 1, juce::String(swingValues[i]) + "%",
                                  true, (int)slot.swing == swingValues[i]);
                menu.showMenuAsync({}, [this, &slot](int r) {
                    if (r > 0) {
                        int vals[] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                        slot.swing = (float)vals[r - 1]; repaint();
                    }
                });
                break;
            }
            case 8: // Smooth
            {
                slot.smooth = !slot.smooth;
                repaint();
                break;
            }
            case 9: // T.Sig (numerator/denominator popup)
            {
                if (proc->isSyncToMasterTimeSig())
                {
                    // Can't edit when synced - show info
                    menu.addItem(1, "Synced to Master (disable Sync first)", false);
                    menu.showMenuAsync({}, [](int) {});
                }
                else
                {
                    menu.addSectionHeader("Numerator");
                    for (int n = 1; n <= 12; n++)
                        menu.addItem(n, juce::String(n), true, n == proc->getTimeSigNumerator());
                    menu.addSeparator();
                    menu.addSectionHeader("Denominator");
                    int denoms[] = { 2, 4, 8, 16 };
                    for (int i = 0; i < 4; i++)
                        menu.addItem(100 + i, juce::String(denoms[i]), true, denoms[i] == proc->getTimeSigDenominator());

                    menu.showMenuAsync({}, [this](int r) {
                        if (r >= 1 && r <= 12) { proc->setTimeSigNumerator(r); repaint(); }
                        else if (r >= 100 && r <= 103) {
                            int denoms[] = { 2, 4, 8, 16 };
                            proc->setTimeSigDenominator(denoms[r - 100]); repaint();
                        }
                    });
                }
                break;
            }
            case 10: // SyncTS toggle
            {
                proc->setSyncToMasterTimeSig(!proc->isSyncToMasterTimeSig());
                repaint();
                break;
            }
        }
    }

    void handleUtilityClick(juce::Point<float> pos, juce::Rectangle<int> strip)
    {
        float btnH = 30.0f;
        float startY = strip.getY() + 4.0f;
        int btnIdx = (int)((pos.y - startY) / (btnH + 3.0f));
        if (btnIdx < 0 || btnIdx > 5) return;

        auto& slot = proc->getSlot(selectedSlot);

        switch (btnIdx)
        {
            case 0: // Menu
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Copy Sequence");
                menu.addItem(2, "Paste Sequence", proc->hasClipboardData());
                menu.addSeparator();
                menu.addItem(3, "Randomize");
                menu.addItem(4, "Invert Values");
                menu.addItem(5, "Set All to 64");
                menu.addSeparator();
                menu.addItem(6, "Clear Sequence");
                menu.showMenuAsync({}, [this, &slot](int r) {
                    switch (r) {
                        case 1: proc->copySlotToClipboard(selectedSlot); break;
                        case 2: proc->pasteSlotFromClipboard(selectedSlot); repaint(); break;
                        case 3: { juce::Random rng; for (int s = 0; s < slot.stepCount; s++) slot.steps[s] = rng.nextInt(128); repaint(); break; }
                        case 4: { for (int s = 0; s < slot.stepCount; s++) slot.steps[s] = 127 - slot.steps[s]; repaint(); break; }
                        case 5: { for (int s = 0; s < slot.stepCount; s++) slot.steps[s] = 64; repaint(); break; }
                        case 6: proc->clearSlot(selectedSlot); repaint(); break;
                    }
                });
                break;
            }
            case 1: { for (int s = 0; s < slot.stepCount; s++) slot.steps[s] = juce::jmin(127, slot.steps[s] + 1); repaint(); break; }
            case 2: { for (int s = 0; s < slot.stepCount; s++) slot.steps[s] = juce::jmax(0, slot.steps[s] - 1); repaint(); break; }
            case 3: { int first = slot.steps[0]; for (int s = 0; s < slot.stepCount - 1; s++) slot.steps[s] = slot.steps[s + 1]; slot.steps[slot.stepCount - 1] = first; repaint(); break; }
            case 4: { int last = slot.steps[slot.stepCount - 1]; for (int s = slot.stepCount - 1; s > 0; s--) slot.steps[s] = slot.steps[s - 1]; slot.steps[0] = last; repaint(); break; }
            case 5: proc->clearSlot(selectedSlot); repaint(); break;
        }
    }

    void handleGridRightClick(juce::Point<float> pos)
    {
        auto& slot = proc->getSlot(selectedSlot);
        int count = slot.stepCount;
        float barWidth = gridArea.getWidth() / (float)count;
        int stepIdx = juce::jlimit(0, count - 1, (int)((pos.x - gridArea.getX()) / barWidth));

        juce::PopupMenu menu;
        menu.addItem(1, "Enter Value...");
        menu.addItem(2, "Set to 0");
        menu.addItem(3, "Set to 64");
        menu.addItem(4, "Set to 127");
        menu.showMenuAsync({}, [this, &slot, stepIdx](int r) {
            switch (r) {
                case 1: {
                    auto* dlg = new juce::AlertWindow("Enter CC Value",
                        "Step " + juce::String(stepIdx + 1) + " (0-127):",
                        juce::MessageBoxIconType::NoIcon);
                    dlg->addTextEditor("val", juce::String(slot.steps[stepIdx]));
                    dlg->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                    dlg->addButton("Cancel", 0);
                    dlg->enterModalState(true, juce::ModalCallbackFunction::create(
                        [this, &slot, stepIdx, dlg](int result) {
                            if (result == 1) {
                                slot.steps[stepIdx] = juce::jlimit(0, 127, dlg->getTextEditorContents("val").getIntValue());
                                repaint();
                            }
                            delete dlg;
                        }), false);
                    break;
                }
                case 2: slot.steps[stepIdx] = 0; repaint(); break;
                case 3: slot.steps[stepIdx] = 64; repaint(); break;
                case 4: slot.steps[stepIdx] = 127; repaint(); break;
            }
        });
    }

    CCStepperProcessor* proc = nullptr;
    int selectedSlot = 0;
    int lastDisplayedStep = -1;
    bool isDraggingGrid = false;
    juce::Rectangle<float> gridArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CCStepperEditorComponent)
};
