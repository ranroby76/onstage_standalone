#pragma once
#include <JuceHeader.h>
#include "RegistrationManager.h"

// Info Button Helper
class InfoButton : public juce::Button
{
public:
    InfoButton() : juce::Button("Info") { setTooltip("Click for registration instructions"); }
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
        auto area = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillEllipse(area);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(area.getHeight() * 0.7f, juce::Font::bold));
        g.drawText("i", area, juce::Justification::centred, false);
    }
};

class RegistrationComponent : public juce::Component
{
public:
    RegistrationComponent()
    {
        addAndMakeVisible(titleLabel);
        titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(userIdLabel);
        userIdLabel.setText("USER ID", juce::dontSendNotification);
        userIdLabel.setFont(juce::Font(20.0f, juce::Font::bold));
        userIdLabel.setColour(juce::Label::textColourId, juce::Colours::black);
        userIdLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(userIdValue);
        userIdValue.setText(RegistrationManager::getInstance().getMachineIDString(), juce::dontSendNotification);
        userIdValue.setFont(juce::Font(18.0f, juce::Font::bold));
        userIdValue.setColour(juce::Label::textColourId, juce::Colours::black);
        userIdValue.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(infoButton);
        infoButton.onClick = [this] { showInstructions(); };

        addAndMakeVisible(instructionLabel);
        instructionLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        instructionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        instructionLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(serialEditor);
        serialEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFFFFFF00));
        serialEditor.setColour(juce::TextEditor::textColourId, juce::Colours::black);
        serialEditor.setFont(juce::Font(20.0f));
        serialEditor.setJustification(juce::Justification::centred);

        addAndMakeVisible(saveButton);
        saveButton.setButtonText("SAVE LICENSE FILE");
        saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
        saveButton.onClick = [this] { checkSerial(); };

        addAndMakeVisible(bottomStatusLabel);
        bottomStatusLabel.setFont(juce::Font(15.0f, juce::Font::bold));
        bottomStatusLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(registeredSerialValue);
        registeredSerialValue.setFont(juce::Font(18.0f, juce::Font::plain));
        registeredSerialValue.setColour(juce::Label::textColourId, juce::Colours::black);
        registeredSerialValue.setJustificationType(juce::Justification::centred);
        registeredSerialValue.setVisible(false);

        updateState();
        setSize(320, 300);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xFFE08020)); // Orange background
        g.setColour(juce::Colours::black);
        g.drawRect(getLocalBounds(), 2);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(15);
        titleLabel.setBounds(area.removeFromTop(30));
        area.removeFromTop(5);
        userIdLabel.setBounds(area.removeFromTop(20));
        auto idRow = area.removeFromTop(25);
        int idWidth = 100; int infoSize = 20;
        userIdValue.setBounds(idRow.withWidth(idWidth).withX((getWidth() - idWidth)/2));
        infoButton.setBounds(userIdValue.getRight() + 5, idRow.getY() + 2, infoSize, infoSize);
        area.removeFromTop(15);
        instructionLabel.setBounds(area.removeFromTop(40));
        area.removeFromTop(5);

        // FIX: Changed isProMode() to isRegistered() - isProMode() doesn't exist
        if (RegistrationManager::getInstance().isRegistered()) {
            serialEditor.setVisible(false);
            saveButton.setVisible(false);
            registeredSerialValue.setVisible(true);
            registeredSerialValue.setBounds(area.removeFromTop(30));
        } else {
            registeredSerialValue.setVisible(false);
            serialEditor.setVisible(true);
            saveButton.setVisible(true);
            serialEditor.setBounds(area.removeFromTop(35).reduced(20, 0));
            area.removeFromTop(15);
            saveButton.setBounds(area.removeFromTop(45).reduced(5, 0));
        }
        bottomStatusLabel.setBounds(0, getHeight() - 30, getWidth(), 25);
    }

private:
    juce::Label titleLabel, userIdLabel, userIdValue, instructionLabel, bottomStatusLabel, registeredSerialValue;
    InfoButton infoButton;
    juce::TextEditor serialEditor;
    juce::TextButton saveButton;

    void updateState() {
        // FIX: Changed isProMode() to isRegistered() - isProMode() doesn't exist
        bool isRegistered = RegistrationManager::getInstance().isRegistered();
        if (isRegistered) {
            titleLabel.setText("REGISTRATION COMPLETE", juce::dontSendNotification);
            instructionLabel.setText("SERIAL NUMBER:", juce::dontSendNotification);
            registeredSerialValue.setText("LICENSE ACTIVE", juce::dontSendNotification);
            bottomStatusLabel.setText("REGISTERED", juce::dontSendNotification);
            bottomStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        } else {
            titleLabel.setText("PLEASE REGISTER", juce::dontSendNotification);
            instructionLabel.setText("ENTER SERIAL AND SAVE", juce::dontSendNotification);
            bottomStatusLabel.setText("NOT REGISTERED", juce::dontSendNotification);
            bottomStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        resized();
    }

    void checkSerial() {
        if (RegistrationManager::getInstance().tryRegister(serialEditor.getText().trim())) {
            updateState();
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Success", "Registration Successful!");
        } else {
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Failed", "Invalid Serial Number.");
        }
    }

    void showInstructions() {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Instructions", 
            "1. Send your User ID to support.\n2. Receive Serial Number.\n3. Enter Serial and Save.");
    }
};

