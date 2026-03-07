// #D:\Workspace\onstage_colosseum_upgrade\src\OnStageDialog.h
// Custom styled dialog for OnStage with Fanan logo and white/orange theme

#pragma once

#include <JuceHeader.h>

class OnStageDialog : public juce::Component
{
public:
    enum class DialogType
    {
        OkCancel,
        YesNoCancel,
        Ok
    };

    // Static helper to show Ok/Cancel dialog
    static void showOkCancel(const juce::String& title,
                             const juce::String& message,
                             const juce::String& okText,
                             const juce::String& cancelText,
                             juce::Component* parent,
                             std::function<void(bool)> callback)
    {
        auto* dialog = new OnStageDialog(title, message, DialogType::OkCancel,
                                         okText, cancelText, "", callback);
        dialog->launchDialog(parent);
    }

    // Static helper to show Yes/No/Cancel dialog
    static void showYesNoCancel(const juce::String& title,
                                const juce::String& message,
                                const juce::String& yesText,
                                const juce::String& noText,
                                const juce::String& cancelText,
                                juce::Component* parent,
                                std::function<void(int)> callback)  // 1=yes, 2=no, 0=cancel
    {
        auto* dialog = new OnStageDialog(title, message, DialogType::YesNoCancel,
                                         yesText, noText, cancelText,
                                         [callback](bool) {}, callback);
        dialog->launchDialog(parent);
    }

    // Static helper to show Ok-only dialog
    static void showMessage(const juce::String& title,
                            const juce::String& message,
                            juce::Component* parent,
                            std::function<void()> callback = nullptr)
    {
        auto* dialog = new OnStageDialog(title, message, DialogType::Ok,
                                         "OK", "", "",
                                         [callback](bool) { if (callback) callback(); });
        dialog->launchDialog(parent);
    }

    OnStageDialog(const juce::String& title,
                  const juce::String& message,
                  DialogType type,
                  const juce::String& btn1Text,
                  const juce::String& btn2Text,
                  const juce::String& btn3Text,
                  std::function<void(bool)> okCancelCallback,
                  std::function<void(int)> yesNoCancelCallback = nullptr)
        : dialogTitle(title),
          dialogMessage(message),
          dialogType(type),
          button1Text(btn1Text),
          button2Text(btn2Text),
          button3Text(btn3Text),
          onOkCancel(okCancelCallback),
          onYesNoCancel(yesNoCancelCallback)
    {
        // Load logo
        loadLogo();

        // Create buttons based on type
        if (dialogType == DialogType::OkCancel)
        {
            button1.setButtonText(button1Text);
            button2.setButtonText(button2Text);
            addAndMakeVisible(button1);
            addAndMakeVisible(button2);

            button1.onClick = [this]() { closeWithResult(1); };
            button2.onClick = [this]() { closeWithResult(0); };
        }
        else if (dialogType == DialogType::YesNoCancel)
        {
            button1.setButtonText(button1Text);
            button2.setButtonText(button2Text);
            button3.setButtonText(button3Text);
            addAndMakeVisible(button1);
            addAndMakeVisible(button2);
            addAndMakeVisible(button3);

            button1.onClick = [this]() { closeWithResult(1); };
            button2.onClick = [this]() { closeWithResult(2); };
            button3.onClick = [this]() { closeWithResult(0); };
        }
        else // Ok only
        {
            button1.setButtonText(button1Text);
            addAndMakeVisible(button1);
            button1.onClick = [this]() { closeWithResult(1); };
        }

        // Style buttons
        styleButton(button1, true);   // Primary button (orange)
        styleButton(button2, false);  // Secondary button (white outline)
        styleButton(button3, false);  // Tertiary button (white outline)

        setSize(450, 220);
    }

    void paint(juce::Graphics& g) override
    {
        // Dark background with rounded corners
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);

        // Orange accent border
        g.setColour(juce::Colour(0xFFFF8C00));  // Dark orange
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 12.0f, 2.0f);

        // Draw logo at top left
        if (logo.isValid())
        {
            int logoHeight = 40;
            float aspectRatio = (float)logo.getWidth() / (float)logo.getHeight();
            int logoWidth = (int)(logoHeight * aspectRatio);
            g.drawImage(logo, 16, 12, logoWidth, logoHeight,
                        0, 0, logo.getWidth(), logo.getHeight());
        }

        // Title - white text, right of logo or at top
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        int titleX = logo.isValid() ? 130 : 20;
        g.drawText(dialogTitle, titleX, 20, getWidth() - titleX - 20, 30,
                   juce::Justification::centredLeft, true);

        // Orange separator line
        g.setColour(juce::Colour(0xFFFF8C00));
        g.fillRect(20, 60, getWidth() - 40, 2);

        // Message - light gray text
        g.setColour(juce::Colour(0xFFE0E0E0));
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        
        juce::Rectangle<int> messageArea(20, 75, getWidth() - 40, 80);
        g.drawFittedText(dialogMessage, messageArea, juce::Justification::centredLeft, 4);
    }

    void resized() override
    {
        int btnWidth = 100;
        int btnHeight = 36;
        int btnSpacing = 12;
        int bottomMargin = 20;
        int y = getHeight() - bottomMargin - btnHeight;

        if (dialogType == DialogType::OkCancel)
        {
            int totalWidth = btnWidth * 2 + btnSpacing;
            int startX = (getWidth() - totalWidth) / 2;
            button1.setBounds(startX, y, btnWidth, btnHeight);
            button2.setBounds(startX + btnWidth + btnSpacing, y, btnWidth, btnHeight);
        }
        else if (dialogType == DialogType::YesNoCancel)
        {
            int totalWidth = btnWidth * 3 + btnSpacing * 2;
            int startX = (getWidth() - totalWidth) / 2;
            button1.setBounds(startX, y, btnWidth, btnHeight);
            button2.setBounds(startX + btnWidth + btnSpacing, y, btnWidth, btnHeight);
            button3.setBounds(startX + (btnWidth + btnSpacing) * 2, y, btnWidth, btnHeight);
        }
        else
        {
            button1.setBounds((getWidth() - btnWidth) / 2, y, btnWidth, btnHeight);
        }
    }

private:
    juce::String dialogTitle;
    juce::String dialogMessage;
    DialogType dialogType;
    juce::String button1Text, button2Text, button3Text;

    juce::TextButton button1, button2, button3;
    juce::Image logo;

    std::function<void(bool)> onOkCancel;
    std::function<void(int)> onYesNoCancel;

    juce::Component::SafePointer<juce::DialogWindow> dialogWindow;

    void loadLogo()
    {
        // Try multiple paths for logo
        juce::File logoFile;

        auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        auto exeDir = exePath.getParentDirectory();
        auto assetsDir = exeDir.getParentDirectory().getChildFile("assets");

        logoFile = assetsDir.getChildFile("fanan_logo.png");
        if (!logoFile.existsAsFile())
        {
            logoFile = exeDir.getChildFile("fanan_logo.png");
        }
        if (!logoFile.existsAsFile())
        {
            logoFile = juce::File("D:/Workspace/onstage_colosseum_upgrade/assets/fanan_logo.png");
        }

        if (logoFile.existsAsFile())
        {
            logo = juce::ImageFileFormat::loadFrom(logoFile);
        }
    }

    void styleButton(juce::TextButton& btn, bool isPrimary)
    {
        if (isPrimary)
        {
            // Orange filled button
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF8C00));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFE07800));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }
        else
        {
            // White outline button
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF3A3A3A));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }
    }

    void launchDialog(juce::Component* parent)
    {
        juce::DialogWindow::LaunchOptions launchOpts;
        launchOpts.dialogTitle = "";
        launchOpts.dialogBackgroundColour = juce::Colours::transparentBlack;
        launchOpts.content.setOwned(this);
        launchOpts.componentToCentreAround = parent;
        launchOpts.escapeKeyTriggersCloseButton = true;
        launchOpts.useNativeTitleBar = false;
        launchOpts.resizable = false;
        launchOpts.useBottomRightCornerResizer = false;

        dialogWindow = launchOpts.launchAsync();

        if (dialogWindow != nullptr)
        {
            // Remove title bar and make borderless
            dialogWindow->setTitleBarHeight(0);
            dialogWindow->setDropShadowEnabled(true);
        }
    }

    void closeWithResult(int result)
    {
        if (dialogType == DialogType::YesNoCancel && onYesNoCancel)
        {
            onYesNoCancel(result);
        }
        else if (onOkCancel)
        {
            onOkCancel(result == 1);
        }

        if (dialogWindow != nullptr)
        {
            dialogWindow->exitModalState(result);
            dialogWindow.getComponent()->setVisible(false);
            juce::MessageManager::callAsync([safeWindow = dialogWindow]() {
                if (safeWindow != nullptr)
                    delete safeWindow.getComponent();
            });
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnStageDialog)
};
