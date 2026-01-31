// #D:\Workspace\Subterraneum_plugins_daw\src\StandaloneMain.cpp
// FIX: Added exit confirmation dialog and proper process cleanup

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

class SubterraneumApplication : public juce::JUCEApplication
{
public:
    SubterraneumApplication() {}

    const juce::String getApplicationName() override { return "Colosseum"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        // FIX: Ensure proper cleanup - window destructor handles everything
        mainWindow = nullptr;
        
        // FIX: Extra cleanup to ensure no zombie processes
        juce::Process::terminate();
    }

    void systemRequestedQuit() override
    {
        // FIX: Show confirmation dialog before quitting
        if (mainWindow)
        {
            mainWindow->confirmAndQuit();
        }
        else
        {
            quit();
        }
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            
            // Create audio device manager
            deviceManager = std::make_unique<juce::AudioDeviceManager>();
            
            // Initialize with maximum channels (256 is JUCE's typical max)
            // This allows us to access ALL channels including virtual/loopback
            deviceManager->initialiseWithDefaultDevices(256, 256);
            
            // Set preferred audio device type based on platform
            #if JUCE_WINDOWS
                // Windows: Prefer ASIO for low latency
                deviceManager->setCurrentAudioDeviceType("ASIO", true);
            #elif JUCE_MAC
                // macOS: Use CoreAudio
                deviceManager->setCurrentAudioDeviceType("CoreAudio", true);
            #elif JUCE_LINUX
                // Linux: Prefer JACK if available, otherwise ALSA
                auto availableTypes = deviceManager->getAvailableDeviceTypes();
                bool hasJack = false;
                for (auto* type : availableTypes) {
                    if (type->getTypeName() == "JACK") {
                        hasJack = true;
                        break;
                    }
                }
                if (hasJack) {
                    deviceManager->setCurrentAudioDeviceType("JACK", true);
                } else {
                    deviceManager->setCurrentAudioDeviceType("ALSA", true);
                }
            #endif
            
            // Close the device initially (user will select manually or load from settings)
            deviceManager->closeAudioDevice();
            
            // Store reference for processor
            SubterraneumAudioProcessor::standaloneDeviceManager = deviceManager.get();
            
            // Create processor and editor
            processor = std::make_unique<SubterraneumAudioProcessor>();
            
            // Setup audio callback
            deviceManager->addAudioCallback(&audioSourcePlayer);
            audioSourcePlayer.setProcessor(processor.get());
            
            // Setup MIDI input callback - this routes MIDI from enabled inputs to the processor
            deviceManager->addMidiInputDeviceCallback("", &audioSourcePlayer);
            
            // Create editor
            auto* editor = new SubterraneumAudioProcessorEditor(*processor);
            setContentOwned(editor, true);

            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        ~MainWindow() override
        {
            // CRITICAL: Proper shutdown order to prevent crashes and zombie processes
            
            // 1. First disconnect audio callbacks
            audioSourcePlayer.setProcessor(nullptr);
            deviceManager->removeAudioCallback(&audioSourcePlayer);
            deviceManager->removeMidiInputDeviceCallback("", &audioSourcePlayer);
            
            // 2. Close the audio device completely
            deviceManager->closeAudioDevice();
            
            // 3. Clear the content (destroys the editor)
            clearContentComponent();
            
            // 4. Now safe to destroy processor
            processor = nullptr;
            
            // 5. Clear the static reference
            SubterraneumAudioProcessor::standaloneDeviceManager = nullptr;
            
            // 6. Finally destroy device manager
            deviceManager = nullptr;
            
            // FIX: Give time for all threads to finish
            juce::Thread::sleep(50);
        }

        void closeButtonPressed() override
        {
            // FIX: Show confirmation dialog instead of immediately quitting
            confirmAndQuit();
        }
        
        // FIX: New method to confirm and quit
        void confirmAndQuit()
        {
            // Show Yes/No confirmation dialog
            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Exit Colosseum",
                "Are you sure you want to exit?",
                "Yes",  // OK button text
                "No",   // Cancel button text
                this,
                juce::ModalCallbackFunction::create([](int result)
                {
                    if (result == 1)  // User clicked Yes
                    {
                        // Quit the application
                        if (auto* app = juce::JUCEApplicationBase::getInstance())
                        {
                            app->quit();
                        }
                    }
                    // If result == 0, user clicked No - do nothing
                })
            );
        }

    private:
        std::unique_ptr<juce::AudioDeviceManager> deviceManager;
        std::unique_ptr<SubterraneumAudioProcessor> processor;
        juce::AudioProcessorPlayer audioSourcePlayer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SubterraneumApplication)
