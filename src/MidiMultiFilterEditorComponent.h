// MidiMultiFilterEditorComponent.h
// MIDI Multi Filter Editor - tabbed interface for comprehensive MIDI filtering
// Tabs: Messages, Channels, Notes, Delay, Velocity
// Each tab has enable/disable toggle
// Wrapped in AudioProcessorEditor for proper JUCE integration

#pragma once

#include <JuceHeader.h>
#include "MidiMultiFilterProcessor.h"

// =============================================================================
// Main Editor Component (the actual UI)
// =============================================================================
class MidiMultiFilterEditorContent : public juce::Component,
                                      public juce::Button::Listener,
                                      public juce::TextEditor::Listener,
                                      public juce::Timer
{
public:
    MidiMultiFilterEditorContent(MidiMultiFilterProcessor& processor)
        : proc(processor)
    {
        // Tab buttons
        setupTabButton(tabMessages, "Messages", 0);
        setupTabButton(tabChannels, "Channels", 1);
        setupTabButton(tabNotes, "Notes", 2);
        setupTabButton(tabDelay, "Delay", 3);
        setupTabButton(tabVelocity, "Velocity", 4);
        
        // =====================================================================
        // Tab 1: Messages
        // =====================================================================
        setupToggleButton(msgNoteOn, "Note On", proc.passNoteOn);
        setupToggleButton(msgNoteOff, "Note Off", proc.passNoteOff);
        setupToggleButton(msgPolyPressure, "Poly Key Pressure", proc.passPolyPressure);
        setupToggleButton(msgCC, "CCs", proc.passCC);
        setupToggleButton(msgProgramChange, "Program Changes", proc.passProgramChange);
        setupToggleButton(msgChannelPressure, "Channel Pressure", proc.passChannelPressure);
        setupToggleButton(msgPitchBend, "Pitch Bend", proc.passPitchBend);
        setupToggleButton(msgSysex, "Misc Sysex", proc.passSysex);
        
        msgEnableToggle.setButtonText("Enable Message Filter");
        msgEnableToggle.setToggleState(proc.messageFilterEnabled, juce::dontSendNotification);
        msgEnableToggle.addListener(this);
        msgEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgreen);
        addChildComponent(msgEnableToggle);
        
        // =====================================================================
        // Tab 2: Channels
        // =====================================================================
        for (int i = 0; i < 16; ++i)
        {
            auto& btn = chButtons[i];
            btn.setButtonText("CH " + juce::String(i + 1));
            btn.setToggleState(proc.channelPass[i], juce::dontSendNotification);
            btn.addListener(this);
            btn.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
            addChildComponent(btn);
        }
        
        chEnableToggle.setButtonText("Enable Channel Filter");
        chEnableToggle.setToggleState(proc.channelFilterEnabled, juce::dontSendNotification);
        chEnableToggle.addListener(this);
        chEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgreen);
        addChildComponent(chEnableToggle);
        
        chAllBtn.setButtonText("All");
        chAllBtn.addListener(this);
        addChildComponent(chAllBtn);
        
        chNoneBtn.setButtonText("None");
        chNoneBtn.addListener(this);
        addChildComponent(chNoneBtn);
        
        // =====================================================================
        // Tab 3: Notes
        // =====================================================================
        notePassOnlyBtn.setButtonText("Pass Only");
        notePassOnlyBtn.setRadioGroupId(201);
        notePassOnlyBtn.setToggleState(proc.noteFilterPassOnly, juce::dontSendNotification);
        notePassOnlyBtn.addListener(this);
        notePassOnlyBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::cyan);
        addChildComponent(notePassOnlyBtn);
        
        noteFilterOutBtn.setButtonText("Filter Out");
        noteFilterOutBtn.setRadioGroupId(201);
        noteFilterOutBtn.setToggleState(!proc.noteFilterPassOnly, juce::dontSendNotification);
        noteFilterOutBtn.addListener(this);
        noteFilterOutBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::orange);
        addChildComponent(noteFilterOutBtn);
        
        noteTextEditor.setMultiLine(false);
        noteTextEditor.setText(proc.noteFilterText);
        noteTextEditor.addListener(this);
        noteTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        noteTextEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        noteTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
        addChildComponent(noteTextEditor);
        
        noteHelpLabel.setText(
            "Enter note numbers or ranges separated by commas.\n"
            "Examples: 60, 72-84, 36, 48-60\n"
            "Range 23-35 = notes 23,24,25...34,35 (13 notes)\n"
            "MIDI note 60 = Middle C",
            juce::dontSendNotification);
        noteHelpLabel.setFont(juce::Font(11.0f));
        noteHelpLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        noteHelpLabel.setJustificationType(juce::Justification::topLeft);
        addChildComponent(noteHelpLabel);
        
        noteEnableToggle.setButtonText("Enable Note Filter");
        noteEnableToggle.setToggleState(proc.noteFilterEnabled, juce::dontSendNotification);
        noteEnableToggle.addListener(this);
        noteEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgreen);
        addChildComponent(noteEnableToggle);
        
        // =====================================================================
        // Tab 4: Delay
        // =====================================================================
        delayLabel.setText("Note Off Delay (ms):", juce::dontSendNotification);
        delayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(delayLabel);
        
        delayEditor.setMultiLine(false);
        delayEditor.setText(juce::String(proc.delayMs));
        delayEditor.addListener(this);
        delayEditor.setInputRestrictions(6, "0123456789");
        delayEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        delayEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        delayEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
        addChildComponent(delayEditor);
        
        delayHelpLabel.setText(
            "Delays the Note Off message by specified milliseconds.\n"
            "Note On passes through immediately.\n"
            "Original Note Off is blocked; a new one is generated after delay.\n"
            "Example: 500 = half second, 1000 = 1 second, 5000 = 5 seconds",
            juce::dontSendNotification);
        delayHelpLabel.setFont(juce::Font(11.0f));
        delayHelpLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        delayHelpLabel.setJustificationType(juce::Justification::topLeft);
        addChildComponent(delayHelpLabel);
        
        delayEnableToggle.setButtonText("Enable Delay");
        delayEnableToggle.setToggleState(proc.delayEnabled, juce::dontSendNotification);
        delayEnableToggle.addListener(this);
        delayEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgreen);
        addChildComponent(delayEnableToggle);
        
        // =====================================================================
        // Tab 5: Velocity
        // =====================================================================
        velMinMaxBtn.setButtonText("Min/Max Limiter");
        velMinMaxBtn.setRadioGroupId(202);
        velMinMaxBtn.setToggleState(!proc.velocityFixedMode, juce::dontSendNotification);
        velMinMaxBtn.addListener(this);
        velMinMaxBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::cyan);
        addChildComponent(velMinMaxBtn);
        
        velFixedBtn.setButtonText("Fixed Velocity");
        velFixedBtn.setRadioGroupId(202);
        velFixedBtn.setToggleState(proc.velocityFixedMode, juce::dontSendNotification);
        velFixedBtn.addListener(this);
        velFixedBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::orange);
        addChildComponent(velFixedBtn);
        
        velMinLabel.setText("Min:", juce::dontSendNotification);
        velMinLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(velMinLabel);
        
        velMinEditor.setMultiLine(false);
        velMinEditor.setText(juce::String(proc.velocityMin));
        velMinEditor.addListener(this);
        velMinEditor.setInputRestrictions(3, "0123456789");
        velMinEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        velMinEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        addChildComponent(velMinEditor);
        
        velMaxLabel.setText("Max:", juce::dontSendNotification);
        velMaxLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(velMaxLabel);
        
        velMaxEditor.setMultiLine(false);
        velMaxEditor.setText(juce::String(proc.velocityMax));
        velMaxEditor.addListener(this);
        velMaxEditor.setInputRestrictions(3, "0123456789");
        velMaxEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        velMaxEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        addChildComponent(velMaxEditor);
        
        velFixedLabel.setText("Fixed:", juce::dontSendNotification);
        velFixedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(velFixedLabel);
        
        velFixedEditor.setMultiLine(false);
        velFixedEditor.setText(juce::String(proc.velocityFixed));
        velFixedEditor.addListener(this);
        velFixedEditor.setInputRestrictions(3, "0123456789");
        velFixedEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        velFixedEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        addChildComponent(velFixedEditor);
        
        velHelpLabel.setText(
            "Min/Max mode: Clamps velocity to range.\n"
            "  - Values below Min become Min\n"
            "  - Values above Max become Max\n"
            "Fixed mode: All notes play at fixed velocity.",
            juce::dontSendNotification);
        velHelpLabel.setFont(juce::Font(11.0f));
        velHelpLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        velHelpLabel.setJustificationType(juce::Justification::topLeft);
        addChildComponent(velHelpLabel);
        
        velEnableToggle.setButtonText("Enable Velocity");
        velEnableToggle.setToggleState(proc.velocityEnabled, juce::dontSendNotification);
        velEnableToggle.addListener(this);
        velEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgreen);
        addChildComponent(velEnableToggle);
        
        // Start on Messages tab
        currentTab = 0;
        showTab(0);
        
        startTimerHz(10);
    }

    ~MidiMultiFilterEditorContent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff252525));
        
        // Tab bar background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(0, 0, getWidth(), 40);
        
        // Content area border
        g.setColour(juce::Colour(0xff444444));
        g.drawRect(10, 50, getWidth() - 20, getHeight() - 60, 1);
    }

    void resized() override
    {
        int tabWidth = (getWidth() - 20) / 5;
        int y = 5;
        
        tabMessages.setBounds(10, y, tabWidth, 30);
        tabChannels.setBounds(10 + tabWidth, y, tabWidth, 30);
        tabNotes.setBounds(10 + tabWidth * 2, y, tabWidth, 30);
        tabDelay.setBounds(10 + tabWidth * 3, y, tabWidth, 30);
        tabVelocity.setBounds(10 + tabWidth * 4, y, tabWidth, 30);
        
        auto contentArea = juce::Rectangle<int>(15, 55, getWidth() - 30, getHeight() - 70);
        
        // Tab 1: Messages layout
        {
            auto area = contentArea;
            msgEnableToggle.setBounds(area.removeFromTop(25));
            area.removeFromTop(10);
            
            int btnW = 150, btnH = 28, gap = 5;
            int col1X = area.getX();
            int col2X = area.getX() + btnW + 20;
            int rowY = area.getY();
            
            msgNoteOn.setBounds(col1X, rowY, btnW, btnH); rowY += btnH + gap;
            msgNoteOff.setBounds(col1X, rowY, btnW, btnH); rowY += btnH + gap;
            msgPolyPressure.setBounds(col1X, rowY, btnW, btnH); rowY += btnH + gap;
            msgCC.setBounds(col1X, rowY, btnW, btnH);
            
            rowY = area.getY();
            msgProgramChange.setBounds(col2X, rowY, btnW, btnH); rowY += btnH + gap;
            msgChannelPressure.setBounds(col2X, rowY, btnW, btnH); rowY += btnH + gap;
            msgPitchBend.setBounds(col2X, rowY, btnW, btnH); rowY += btnH + gap;
            msgSysex.setBounds(col2X, rowY, btnW, btnH);
        }
        
        // Tab 2: Channels layout
        {
            auto area = contentArea;
            chEnableToggle.setBounds(area.removeFromTop(25));
            area.removeFromTop(5);
            
            auto btnRow = area.removeFromTop(25);
            chAllBtn.setBounds(btnRow.removeFromLeft(60));
            btnRow.removeFromLeft(5);
            chNoneBtn.setBounds(btnRow.removeFromLeft(60));
            
            area.removeFromTop(10);
            
            int btnW = 70, btnH = 28, gap = 5;
            int cols = 4;
            
            for (int i = 0; i < 16; ++i)
            {
                int col = i % cols;
                int row = i / cols;
                chButtons[i].setBounds(area.getX() + col * (btnW + gap),
                                       area.getY() + row * (btnH + gap),
                                       btnW, btnH);
            }
        }
        
        // Tab 3: Notes layout
        {
            auto area = contentArea;
            noteEnableToggle.setBounds(area.removeFromTop(25));
            area.removeFromTop(10);
            
            auto modeRow = area.removeFromTop(25);
            notePassOnlyBtn.setBounds(modeRow.removeFromLeft(120));
            modeRow.removeFromLeft(10);
            noteFilterOutBtn.setBounds(modeRow.removeFromLeft(120));
            
            area.removeFromTop(10);
            
            noteTextEditor.setBounds(area.removeFromTop(28));
            area.removeFromTop(10);
            noteHelpLabel.setBounds(area);
        }
        
        // Tab 4: Delay layout
        {
            auto area = contentArea;
            delayEnableToggle.setBounds(area.removeFromTop(25));
            area.removeFromTop(15);
            
            auto row = area.removeFromTop(28);
            delayLabel.setBounds(row.removeFromLeft(150));
            row.removeFromLeft(5);
            delayEditor.setBounds(row.removeFromLeft(80));
            
            area.removeFromTop(15);
            delayHelpLabel.setBounds(area);
        }
        
        // Tab 5: Velocity layout
        {
            auto area = contentArea;
            velEnableToggle.setBounds(area.removeFromTop(25));
            area.removeFromTop(10);
            
            auto modeRow = area.removeFromTop(25);
            velMinMaxBtn.setBounds(modeRow.removeFromLeft(140));
            modeRow.removeFromLeft(10);
            velFixedBtn.setBounds(modeRow.removeFromLeft(140));
            
            area.removeFromTop(15);
            
            auto minMaxRow = area.removeFromTop(28);
            velMinLabel.setBounds(minMaxRow.removeFromLeft(40));
            velMinEditor.setBounds(minMaxRow.removeFromLeft(60));
            minMaxRow.removeFromLeft(20);
            velMaxLabel.setBounds(minMaxRow.removeFromLeft(40));
            velMaxEditor.setBounds(minMaxRow.removeFromLeft(60));
            
            area.removeFromTop(10);
            
            auto fixedRow = area.removeFromTop(28);
            velFixedLabel.setBounds(fixedRow.removeFromLeft(50));
            velFixedEditor.setBounds(fixedRow.removeFromLeft(60));
            
            area.removeFromTop(15);
            velHelpLabel.setBounds(area);
        }
    }

    void buttonClicked(juce::Button* btn) override
    {
        // Tab buttons
        if (btn == &tabMessages) { showTab(0); return; }
        if (btn == &tabChannels) { showTab(1); return; }
        if (btn == &tabNotes) { showTab(2); return; }
        if (btn == &tabDelay) { showTab(3); return; }
        if (btn == &tabVelocity) { showTab(4); return; }
        
        // Enable toggles
        if (btn == &msgEnableToggle) { proc.messageFilterEnabled = btn->getToggleState(); return; }
        if (btn == &chEnableToggle) { proc.channelFilterEnabled = btn->getToggleState(); return; }
        if (btn == &noteEnableToggle) { proc.noteFilterEnabled = btn->getToggleState(); return; }
        if (btn == &delayEnableToggle) { proc.delayEnabled = btn->getToggleState(); return; }
        if (btn == &velEnableToggle) { proc.velocityEnabled = btn->getToggleState(); return; }
        
        // Message filter toggles
        if (btn == &msgNoteOn) { proc.passNoteOn = btn->getToggleState(); return; }
        if (btn == &msgNoteOff) { proc.passNoteOff = btn->getToggleState(); return; }
        if (btn == &msgPolyPressure) { proc.passPolyPressure = btn->getToggleState(); return; }
        if (btn == &msgCC) { proc.passCC = btn->getToggleState(); return; }
        if (btn == &msgProgramChange) { proc.passProgramChange = btn->getToggleState(); return; }
        if (btn == &msgChannelPressure) { proc.passChannelPressure = btn->getToggleState(); return; }
        if (btn == &msgPitchBend) { proc.passPitchBend = btn->getToggleState(); return; }
        if (btn == &msgSysex) { proc.passSysex = btn->getToggleState(); return; }
        
        // Channel buttons
        for (int i = 0; i < 16; ++i)
        {
            if (btn == &chButtons[i])
            {
                proc.channelPass[i] = btn->getToggleState();
                return;
            }
        }
        
        // Channel All/None
        if (btn == &chAllBtn)
        {
            for (int i = 0; i < 16; ++i)
            {
                proc.channelPass[i] = true;
                chButtons[i].setToggleState(true, juce::dontSendNotification);
            }
            return;
        }
        if (btn == &chNoneBtn)
        {
            for (int i = 0; i < 16; ++i)
            {
                proc.channelPass[i] = false;
                chButtons[i].setToggleState(false, juce::dontSendNotification);
            }
            return;
        }
        
        // Note filter mode
        if (btn == &notePassOnlyBtn) { proc.noteFilterPassOnly = true; return; }
        if (btn == &noteFilterOutBtn) { proc.noteFilterPassOnly = false; return; }
        
        // Velocity mode
        if (btn == &velMinMaxBtn) { proc.velocityFixedMode = false; return; }
        if (btn == &velFixedBtn) { proc.velocityFixedMode = true; return; }
    }

    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (&editor == &noteTextEditor)
        {
            proc.noteFilterText = editor.getText();
            proc.parseNoteFilterText();
        }
        else if (&editor == &delayEditor)
        {
            proc.delayMs = juce::jmax(0, editor.getText().getIntValue());
        }
        else if (&editor == &velMinEditor)
        {
            proc.velocityMin = juce::jlimit(1, 127, editor.getText().getIntValue());
        }
        else if (&editor == &velMaxEditor)
        {
            proc.velocityMax = juce::jlimit(1, 127, editor.getText().getIntValue());
        }
        else if (&editor == &velFixedEditor)
        {
            proc.velocityFixed = juce::jlimit(1, 127, editor.getText().getIntValue());
        }
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    MidiMultiFilterProcessor& proc;
    int currentTab = 0;
    
    // Tab buttons
    juce::TextButton tabMessages, tabChannels, tabNotes, tabDelay, tabVelocity;
    
    // Tab 1: Messages
    juce::ToggleButton msgEnableToggle;
    juce::ToggleButton msgNoteOn, msgNoteOff, msgPolyPressure, msgCC;
    juce::ToggleButton msgProgramChange, msgChannelPressure, msgPitchBend, msgSysex;
    
    // Tab 2: Channels
    juce::ToggleButton chEnableToggle;
    std::array<juce::ToggleButton, 16> chButtons;
    juce::TextButton chAllBtn, chNoneBtn;
    
    // Tab 3: Notes
    juce::ToggleButton noteEnableToggle;
    juce::ToggleButton notePassOnlyBtn, noteFilterOutBtn;
    juce::TextEditor noteTextEditor;
    juce::Label noteHelpLabel;
    
    // Tab 4: Delay
    juce::ToggleButton delayEnableToggle;
    juce::Label delayLabel;
    juce::TextEditor delayEditor;
    juce::Label delayHelpLabel;
    
    // Tab 5: Velocity
    juce::ToggleButton velEnableToggle;
    juce::ToggleButton velMinMaxBtn, velFixedBtn;
    juce::Label velMinLabel, velMaxLabel, velFixedLabel;
    juce::TextEditor velMinEditor, velMaxEditor, velFixedEditor;
    juce::Label velHelpLabel;
    
    void setupTabButton(juce::TextButton& btn, const juce::String& text, int index)
    {
        btn.setButtonText(text);
        btn.addListener(this);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(100);
        btn.setToggleState(index == 0, juce::dontSendNotification);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff505080));
        addAndMakeVisible(btn);
    }
    
    void setupToggleButton(juce::ToggleButton& btn, const juce::String& text, bool initialState)
    {
        btn.setButtonText(text);
        btn.setToggleState(initialState, juce::dontSendNotification);
        btn.addListener(this);
        btn.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        addChildComponent(btn);
    }
    
    void showTab(int tabIndex)
    {
        currentTab = tabIndex;
        hideAllTabs();
        
        switch (tabIndex)
        {
            case 0: // Messages
                msgEnableToggle.setVisible(true);
                msgNoteOn.setVisible(true);
                msgNoteOff.setVisible(true);
                msgPolyPressure.setVisible(true);
                msgCC.setVisible(true);
                msgProgramChange.setVisible(true);
                msgChannelPressure.setVisible(true);
                msgPitchBend.setVisible(true);
                msgSysex.setVisible(true);
                break;
                
            case 1: // Channels
                chEnableToggle.setVisible(true);
                chAllBtn.setVisible(true);
                chNoneBtn.setVisible(true);
                for (auto& btn : chButtons)
                    btn.setVisible(true);
                break;
                
            case 2: // Notes
                noteEnableToggle.setVisible(true);
                notePassOnlyBtn.setVisible(true);
                noteFilterOutBtn.setVisible(true);
                noteTextEditor.setVisible(true);
                noteHelpLabel.setVisible(true);
                break;
                
            case 3: // Delay
                delayEnableToggle.setVisible(true);
                delayLabel.setVisible(true);
                delayEditor.setVisible(true);
                delayHelpLabel.setVisible(true);
                break;
                
            case 4: // Velocity
                velEnableToggle.setVisible(true);
                velMinMaxBtn.setVisible(true);
                velFixedBtn.setVisible(true);
                velMinLabel.setVisible(true);
                velMinEditor.setVisible(true);
                velMaxLabel.setVisible(true);
                velMaxEditor.setVisible(true);
                velFixedLabel.setVisible(true);
                velFixedEditor.setVisible(true);
                velHelpLabel.setVisible(true);
                break;
        }
        
        tabMessages.setToggleState(tabIndex == 0, juce::dontSendNotification);
        tabChannels.setToggleState(tabIndex == 1, juce::dontSendNotification);
        tabNotes.setToggleState(tabIndex == 2, juce::dontSendNotification);
        tabDelay.setToggleState(tabIndex == 3, juce::dontSendNotification);
        tabVelocity.setToggleState(tabIndex == 4, juce::dontSendNotification);
    }
    
    void hideAllTabs()
    {
        msgEnableToggle.setVisible(false);
        msgNoteOn.setVisible(false);
        msgNoteOff.setVisible(false);
        msgPolyPressure.setVisible(false);
        msgCC.setVisible(false);
        msgProgramChange.setVisible(false);
        msgChannelPressure.setVisible(false);
        msgPitchBend.setVisible(false);
        msgSysex.setVisible(false);
        
        chEnableToggle.setVisible(false);
        chAllBtn.setVisible(false);
        chNoneBtn.setVisible(false);
        for (auto& btn : chButtons)
            btn.setVisible(false);
        
        noteEnableToggle.setVisible(false);
        notePassOnlyBtn.setVisible(false);
        noteFilterOutBtn.setVisible(false);
        noteTextEditor.setVisible(false);
        noteHelpLabel.setVisible(false);
        
        delayEnableToggle.setVisible(false);
        delayLabel.setVisible(false);
        delayEditor.setVisible(false);
        delayHelpLabel.setVisible(false);
        
        velEnableToggle.setVisible(false);
        velMinMaxBtn.setVisible(false);
        velFixedBtn.setVisible(false);
        velMinLabel.setVisible(false);
        velMinEditor.setVisible(false);
        velMaxLabel.setVisible(false);
        velMaxEditor.setVisible(false);
        velFixedLabel.setVisible(false);
        velFixedEditor.setVisible(false);
        velHelpLabel.setVisible(false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMultiFilterEditorContent)
};

// =============================================================================
// AudioProcessorEditor wrapper
// =============================================================================
class MidiMultiFilterEditor : public juce::AudioProcessorEditor
{
public:
    MidiMultiFilterEditor(MidiMultiFilterProcessor& p)
        : AudioProcessorEditor(p), content(p)
    {
        setSize(520, 380);
        addAndMakeVisible(content);
    }

    void resized() override
    {
        content.setBounds(getLocalBounds());
    }

private:
    MidiMultiFilterEditorContent content;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMultiFilterEditor)
};
