#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../RegistrationManager.h"

// ==============================================================================
// Info Button & Tooltip Helper
// ==============================================================================
class InfoButton : public juce::Button
{
public:
    InfoButton() : juce::Button("Info")
    {
        setTooltip("Click for registration instructions");
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
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
        // --- 1. Header Title ---
        addAndMakeVisible(titleLabel);
        titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);

        // --- 2. User ID Section ---
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

        // --- 3. Info Button ---
        addAndMakeVisible(infoButton);
        infoButton.onClick = [this] { showInstructions(); };

        // --- 4. Instructions / Status ---
        addAndMakeVisible(instructionLabel);
        instructionLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        instructionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        instructionLabel.setJustificationType(juce::Justification::centred);

        // --- 5. Serial Input (Yellow Box) ---
        addAndMakeVisible(serialEditor);
        serialEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFFFFFF00)); // Yellow
        serialEditor.setColour(juce::TextEditor::textColourId, juce::Colours::black);
        serialEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::black);
        serialEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
        serialEditor.setFont(juce::Font(20.0f));
        serialEditor.setJustification(juce::Justification::centred);

        // --- 6. Save Button (Dark Grey) ---
        addAndMakeVisible(saveButton);
        saveButton.setButtonText("SAVE LICENSE FILE");
        saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333)); // Dark Grey gradient base
        saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        saveButton.onClick = [this] { checkSerial(); };

        // --- 7. Bottom Status ---
        addAndMakeVisible(bottomStatusLabel);
        bottomStatusLabel.setFont(juce::Font(15.0f, juce::Font::bold));
        bottomStatusLabel.setJustificationType(juce::Justification::centred);

        // --- 8. Registered Serial Display (Hidden by default) ---
        addAndMakeVisible(registeredSerialValue);
        registeredSerialValue.setFont(juce::Font(18.0f, juce::Font::plain));
        registeredSerialValue.setColour(juce::Label::textColourId, juce::Colours::black);
        registeredSerialValue.setJustificationType(juce::Justification::centred);
        registeredSerialValue.setVisible(false);

        updateState();
        setSize(320, 300);
    }

    void paint(juce::Graphics& g) override
    {
        // Orange Background (matching screenshot)
        g.fillAll(juce::Colour(0xFFE08020));
        
        // Black border
        g.setColour(juce::Colours::black);
        g.drawRect(getLocalBounds(), 2);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Title
        titleLabel.setBounds(area.removeFromTop(30));
        area.removeFromTop(5);

        // User ID Label
        userIdLabel.setBounds(area.removeFromTop(20));
        
        // User ID Value + Info Button
        auto idRow = area.removeFromTop(25);
        int idWidth = 100; 
        int infoSize = 20;
        // Center the ID, place button to its right
        userIdValue.setBounds(idRow.withWidth(idWidth).withX((getWidth() - idWidth)/2));
        infoButton.setBounds(userIdValue.getRight() + 5, idRow.getY() + 2, infoSize, infoSize);

        area.removeFromTop(15);

        // Instruction Text
        instructionLabel.setBounds(area.removeFromTop(40));
        area.removeFromTop(5);

        if (RegistrationManager::getInstance().isProMode())
        {
            // REGISTERED LAYOUT
            serialEditor.setVisible(false);
            saveButton.setVisible(false);
            
            registeredSerialValue.setVisible(true);
            registeredSerialValue.setBounds(area.removeFromTop(30));
        }
        else
        {
            // UNREGISTERED LAYOUT
            registeredSerialValue.setVisible(false);
            serialEditor.setVisible(true);
            saveButton.setVisible(true);

            serialEditor.setBounds(area.removeFromTop(35).reduced(20, 0));
            area.removeFromTop(15);
            saveButton.setBounds(area.removeFromTop(45).reduced(5, 0));
        }

        // Bottom Status
        // Push to bottom
        bottomStatusLabel.setBounds(0, getHeight() - 30, getWidth(), 25);
    }

private:
    juce::Label titleLabel;
    juce::Label userIdLabel;
    juce::Label userIdValue;
    InfoButton infoButton;
    juce::Label instructionLabel;
    juce::TextEditor serialEditor;
    juce::TextButton saveButton;
    juce::Label bottomStatusLabel;
    juce::Label registeredSerialValue;

    void updateState()
    {
        bool isRegistered = RegistrationManager::getInstance().isProMode();

        if (isRegistered)
        {
            titleLabel.setText("REGISTRATION COMPLETE", juce::dontSendNotification);
            
            instructionLabel.setText("SERIAL NUMBER:", juce::dontSendNotification);
            
            // Try to load the serial from file to display it
            juce::File licenseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                     .getChildFile("OnStage").getChildFile("user_license.wav");
            // Since we can't easily decode it back to string without the logic here, 
            // we will just show "Active" or if you have the plain text serial stored somewhere.
            // For now, we'll show a placeholder or "LICENSE ACTIVE".
            registeredSerialValue.setText("LICENSE ACTIVE", juce::dontSendNotification);

            bottomStatusLabel.setText("REGISTERED", juce::dontSendNotification);
            bottomStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen); // Green text
        }
        else
        {
            titleLabel.setText("PLEASE REGISTER", juce::dontSendNotification);
            
            instructionLabel.setText("ENTER YOUR SERIAL HERE, AND\nTHEN SAVE AS LICENSE FILE", juce::dontSendNotification);
            
            bottomStatusLabel.setText("NOT REGISTERED", juce::dontSendNotification);
            bottomStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        
        resized(); // Re-layout based on state
    }

    void checkSerial()
    {
        juce::String input = serialEditor.getText().trim();
        bool success = RegistrationManager::getInstance().tryRegister(input);
        
        if (success)
        {
            updateState();
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, 
                "Success", "Registration Successful!\nThank you for supporting us.");
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, 
                "Registration Failed", "Invalid Serial Number.\nPlease check your ID and Serial.");
        }
    }

    void showInstructions()
    {
        juce::String text = 
            "Upgrading to PRO version:\n\n"
            "1. Copy Your Machine ID\n"
            "2. Complete Your Purchase: Return to purchase page and enter\n"
            "   your Machine ID into the text box above your chosen bundle.\n"
            "3. Click \"BUY NOW\" to complete the payment.\n\n"
            "Receive Your Serial Number:\n"
            "After a successful purchase, your serial number will instantly\n"
            "appear in the box above. It will also be sent to your email.\n\n"
            "Register Your Plugin:\n"
            "Copy the serial number, paste it into the registration window\n"
            "back in your DAW, and click \"Save license file\".\n"
            "4. Done";

        // Show a simple alert window as the tooltip (easiest way to show multiline text modally)
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Registration Instructions", text);
    }
};