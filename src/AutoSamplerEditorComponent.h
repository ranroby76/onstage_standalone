// #D:\Workspace\Subterraneum_plugins_daw\src\AutoSamplerEditorComponent.h
// AUTO SAMPLER EDITOR - Popup for E button
// Text editor for note/velocity syntax with Load/Save preset support
// Settings: silence threshold, silence duration, note hold time

#pragma once

#include <JuceHeader.h>
#include "AutoSamplerProcessor.h"

class AutoSamplerEditorComponent : public juce::Component {
public:
    AutoSamplerEditorComponent(AutoSamplerProcessor* proc)
        : processor(proc)
    {
        setSize(420, 600);
        
        // =====================================================================
        // Title
        // =====================================================================
        titleLabel.setText("Auto Sampler Editor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // =====================================================================
        // Syntax Help Label
        // =====================================================================
        helpLabel.setText(
            "PROGRAMMING NOTES\n"
            "  [notes], [velocities]              per line\n"
            "  [notes], [velocities], [durations]  with hold time (sec)\n"
            "  Range:  [48-72], [127]             25 notes @ vel 127\n"
            "  List:   [60,64,67], [80,100,120]   zip = 3 files\n"
            "  Cross:  [48-72], [64,96,127]       range x list = 75 files\n"
            "  Lines starting with // or # are comments\n\n"
            "SFZ INSTRUMENT\n"
            "  Create SFZ: auto-generates .sfz after recording\n"
            "  Fill-Gap: maps sampled notes across all 128 keys\n"
            "    using midpoints between notes as split boundaries\n"
            "  Files: RecFolder/Family.sfz + Family/samples/*.wav",
            juce::dontSendNotification);
        helpLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::plain));
        helpLabel.setColour(juce::Label::textColourId, juce::Colour(160, 160, 180));
        helpLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(helpLabel);
        
        // =====================================================================
        // Text Editor
        // =====================================================================
        textEditor.setMultiLine(true, true);
        textEditor.setReturnKeyStartsNewLine(true);
        textEditor.setScrollbarsShown(true);
        textEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
        textEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(30, 30, 35));
        textEditor.setColour(juce::TextEditor::textColourId, juce::Colour(220, 220, 240));
        textEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(70, 70, 80));
        textEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0, 180, 255));
        textEditor.setText(processor->getEditorText());
        addAndMakeVisible(textEditor);
        
        // =====================================================================
        // Load / Save Buttons
        // =====================================================================
        loadBtn.setButtonText("Load");
        loadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(50, 80, 120));
        loadBtn.onClick = [this]() { loadPreset(); };
        addAndMakeVisible(loadBtn);
        
        saveBtn.setButtonText("Save");
        saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(50, 100, 70));
        saveBtn.onClick = [this]() { savePreset(); };
        addAndMakeVisible(saveBtn);
        
        // =====================================================================
        // Parse Count Label (shows how many files will be generated)
        // =====================================================================
        parseCountLabel.setFont(juce::Font(12.0f));
        parseCountLabel.setColour(juce::Label::textColourId, juce::Colour(100, 200, 100));
        parseCountLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(parseCountLabel);
        
        textEditor.onTextChange = [this]() { updateParseCount(); };
        
        // =====================================================================
        // Settings: Silence Threshold
        // =====================================================================
        threshLabel.setText("Silence Threshold (dB):", juce::dontSendNotification);
        threshLabel.setFont(juce::Font(12.0f));
        threshLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(threshLabel);
        
        threshSlider.setRange(-90.0, -20.0, 1.0);
        threshSlider.setValue(processor->getSilenceThresholdDb());
        threshSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        threshSlider.setColour(juce::Slider::trackColourId, juce::Colour(80, 80, 100));
        threshSlider.onValueChange = [this]() {
            processor->setSilenceThresholdDb((float)threshSlider.getValue());
        };
        addAndMakeVisible(threshSlider);
        
        // =====================================================================
        // Settings: Silence Duration
        // =====================================================================
        durLabel.setText("Silence Duration (ms):", juce::dontSendNotification);
        durLabel.setFont(juce::Font(12.0f));
        durLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(durLabel);
        
        durSlider.setRange(100.0, 5000.0, 50.0);
        durSlider.setValue(processor->getSilenceDurationMs());
        durSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        durSlider.setColour(juce::Slider::trackColourId, juce::Colour(80, 80, 100));
        durSlider.onValueChange = [this]() {
            processor->setSilenceDurationMs((float)durSlider.getValue());
        };
        addAndMakeVisible(durSlider);
        
        // =====================================================================
        // Settings: Note Hold Time
        // =====================================================================
        holdLabel.setText("Note Hold Time (ms):", juce::dontSendNotification);
        holdLabel.setFont(juce::Font(12.0f));
        holdLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(holdLabel);
        
        holdSlider.setRange(200.0, 10000.0, 100.0);
        holdSlider.setValue(processor->getNoteHoldMs());
        holdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        holdSlider.setColour(juce::Slider::trackColourId, juce::Colour(80, 80, 100));
        holdSlider.onValueChange = [this]() {
            processor->setNoteHoldMs((float)holdSlider.getValue());
        };
        addAndMakeVisible(holdSlider);
        
        // =====================================================================
        // SFZ Generation Toggle
        // =====================================================================
        sfzToggle.setButtonText("Create SFZ from samples");
        sfzToggle.setToggleState(processor->getCreateSfz(), juce::dontSendNotification);
        sfzToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
        sfzToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0, 180, 255));
        sfzToggle.onClick = [this]() {
            processor->setCreateSfz(sfzToggle.getToggleState());
            fillGapToggle.setVisible(sfzToggle.getToggleState());
        };
        addAndMakeVisible(sfzToggle);
        
        // =====================================================================
        // Fill-Gap Toggle (visible only when SFZ is on)
        // =====================================================================
        fillGapToggle.setButtonText("Fill-Gap (full 128-key range)");
        fillGapToggle.setToggleState(processor->getFillGap(), juce::dontSendNotification);
        fillGapToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
        fillGapToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0, 180, 255));
        fillGapToggle.onClick = [this]() {
            processor->setFillGap(fillGapToggle.getToggleState());
        };
        fillGapToggle.setVisible(processor->getCreateSfz());
        addAndMakeVisible(fillGapToggle);
        
        updateParseCount();
    }
    
    ~AutoSamplerEditorComponent() override
    {
        // Save text back to processor when closing
        processor->setEditorText(textEditor.getText());
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        
        titleLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
        
        helpLabel.setBounds(area.removeFromTop(155));
        area.removeFromTop(4);
        
        // Text editor takes bulk of space
        auto editorArea = area.removeFromTop(180);
        textEditor.setBounds(editorArea);
        area.removeFromTop(4);
        
        // Load / Save / Parse count row
        auto btnRow = area.removeFromTop(28);
        loadBtn.setBounds(btnRow.removeFromLeft(70));
        btnRow.removeFromLeft(8);
        saveBtn.setBounds(btnRow.removeFromLeft(70));
        btnRow.removeFromLeft(12);
        parseCountLabel.setBounds(btnRow);
        area.removeFromTop(10);
        
        // Settings
        auto threshRow = area.removeFromTop(24);
        threshLabel.setBounds(threshRow.removeFromLeft(170));
        threshSlider.setBounds(threshRow);
        area.removeFromTop(4);
        
        auto durRow = area.removeFromTop(24);
        durLabel.setBounds(durRow.removeFromLeft(170));
        durSlider.setBounds(durRow);
        area.removeFromTop(4);
        
        auto holdRow = area.removeFromTop(24);
        holdLabel.setBounds(holdRow.removeFromLeft(170));
        holdSlider.setBounds(holdRow);
        area.removeFromTop(8);
        
        // SFZ toggles
        sfzToggle.setBounds(area.removeFromTop(24));
        area.removeFromTop(2);
        fillGapToggle.setBounds(area.removeFromTop(24).withTrimmedLeft(20));
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(35, 35, 40));
    }

private:
    void updateParseCount()
    {
        auto pairs = AutoSamplerProcessor::parseNoteList(textEditor.getText());
        if (pairs.empty()) {
            parseCountLabel.setText("No valid entries", juce::dontSendNotification);
            parseCountLabel.setColour(juce::Label::textColourId, juce::Colour(200, 100, 100));
        } else {
            parseCountLabel.setText(juce::String((int)pairs.size()) + " files will be generated",
                                   juce::dontSendNotification);
            parseCountLabel.setColour(juce::Label::textColourId, juce::Colour(100, 200, 100));
        }
    }
    
    void loadPreset()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load Auto Sampler Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.txt;*.csv");
        
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.exists()) {
                    auto content = result.loadFileAsString();
                    textEditor.setText(content);
                    processor->setEditorText(content);
                    updateParseCount();
                }
            });
    }
    
    void savePreset()
    {
        // Save text to processor first
        processor->setEditorText(textEditor.getText());
        
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save Auto Sampler Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.txt");
        
        fileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result != juce::File()) {
                    auto targetFile = result.withFileExtension("txt");
                    targetFile.replaceWithText(textEditor.getText());
                }
            });
    }
    
    AutoSamplerProcessor* processor;
    
    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::TextEditor textEditor;
    juce::TextButton loadBtn, saveBtn;
    juce::Label parseCountLabel;
    
    juce::Label threshLabel, durLabel, holdLabel;
    juce::Slider threshSlider, durSlider, holdSlider;
    
    juce::ToggleButton sfzToggle, fillGapToggle;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoSamplerEditorComponent)
};