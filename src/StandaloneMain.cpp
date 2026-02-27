// #D:\Workspace\Subterraneum_plugins_daw\src\StandaloneMain.cpp
// FIX: Added --scan-plugin mode for out-of-process plugin scanning
// When launched with --scan-plugin <path> <format> <outputFile>,
// runs in headless mode: loads one plugin, writes metadata, exits.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginScanWorker.h"
#include "OutOfProcessScanner.h"

// =============================================================================
// Debug logging disabled - was writing colosseum_child_startup.txt to Desktop
// =============================================================================
static void logChild(const juce::String& /*message*/)
{
    // No-op for release
}

class SubterraneumApplication : public juce::JUCEApplication
{
public:
    SubterraneumApplication() {}

    const juce::String getApplicationName() override { return "Colosseum"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        logChild("=== CHILD STARTUP ===");
        logChild("Raw commandLine: " + commandLine);
        logChild("commandLine length: " + juce::String(commandLine.length()));
        logChild("Contains --scan-plugin: " + juce::String(commandLine.contains("--scan-plugin") ? "YES" : "NO"));
        
        // =================================================================
        // OUT-OF-PROCESS SCAN MODE
        // Usage: Colosseum --scan-plugin <pluginPath> <formatName> <outputFile>
        // Runs headless: loads one plugin, writes JSON metadata, exits.
        // =================================================================
        if (commandLine.contains("--scan-plugin"))
        {
            logChild("Entering scan mode");
            scanMode = true;
            
            // Parse arguments
            juce::StringArray args = juce::StringArray::fromTokens(commandLine, true);
            
            logChild("Parsed args count: " + juce::String(args.size()));
            for (int i = 0; i < args.size(); ++i)
                logChild("  arg[" + juce::String(i) + "]: " + args[i]);
            
            int scanIdx = args.indexOf("--scan-plugin");
            logChild("scanIdx: " + juce::String(scanIdx));
            logChild("Check: scanIdx + 3 < args.size() => " + juce::String(scanIdx + 3) + " < " + juce::String(args.size()));
            
            if (scanIdx >= 0 && scanIdx + 3 < args.size())
            {
                juce::String pluginPath = args[scanIdx + 1];
                juce::String formatName = args[scanIdx + 2];
                juce::String outputFile = args[scanIdx + 3];
                
                // Remove quotes if present
                pluginPath = pluginPath.unquoted();
                formatName = formatName.unquoted();
                outputFile = outputFile.unquoted();
                
                logChild("pluginPath: " + pluginPath);
                logChild("formatName: " + formatName);
                logChild("outputFile: " + outputFile);
                logChild("Calling PluginScanWorker::runScan()...");
                
                // Run scan worker (headless, no GUI)
                PluginScanWorker::runScan(pluginPath, formatName, outputFile);
                
                logChild("PluginScanWorker::runScan() returned");
            }
            else
            {
                logChild("ERROR: Not enough arguments!");
            }
            
            logChild("Calling quit()");
            // Exit immediately after scan
            quit();
            return;
        }
        
        logChild("Not scan mode - launching GUI");
        
        // =================================================================
        // NORMAL MODE — Launch GUI
        // =================================================================
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        logChild("shutdown() called, scanMode=" + juce::String(scanMode ? "true" : "false"));
        mainWindow = nullptr;
        OutOfProcessScanner::cleanupTempFiles();
        
        if (!scanMode)
            juce::Process::terminate();
    }

    void systemRequestedQuit() override
    {
        if (scanMode)
        {
            quit();
            return;
        }
        
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
            deviceManager->initialiseWithDefaultDevices(256, 256);
            
            // Set preferred audio device type based on platform
            #if JUCE_WINDOWS
                deviceManager->setCurrentAudioDeviceType("ASIO", true);
            #elif JUCE_MAC
                deviceManager->setCurrentAudioDeviceType("CoreAudio", true);
            #elif JUCE_LINUX
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
            
            // Close the device initially
            deviceManager->closeAudioDevice();
            
            SubterraneumAudioProcessor::standaloneDeviceManager = deviceManager.get();
            
            processor = std::make_unique<SubterraneumAudioProcessor>();
            
            deviceManager->addAudioCallback(&audioSourcePlayer);
            audioSourcePlayer.setProcessor(processor.get());
            deviceManager->addMidiInputDeviceCallback("", &audioSourcePlayer);
            
            auto* editor = new SubterraneumAudioProcessorEditor(*processor);
            setContentOwned(editor, true);

            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        ~MainWindow() override
        {
            audioSourcePlayer.setProcessor(nullptr);
            deviceManager->removeAudioCallback(&audioSourcePlayer);
            deviceManager->removeMidiInputDeviceCallback("", &audioSourcePlayer);
            deviceManager->closeAudioDevice();
            clearContentComponent();
            processor = nullptr;
            SubterraneumAudioProcessor::standaloneDeviceManager = nullptr;
            deviceManager = nullptr;
            juce::Thread::sleep(50);
        }

        void closeButtonPressed() override
        {
            confirmAndQuit();
        }
        
        void confirmAndQuit()
        {
            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Exit Colosseum",
                "Are you sure you want to exit?",
                "Yes",
                "No",
                this,
                juce::ModalCallbackFunction::create([](int result)
                {
                    if (result == 1)
                    {
                        if (auto* app = juce::JUCEApplicationBase::getInstance())
                        {
                            app->quit();
                        }
                    }
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
    bool scanMode = false;
};

START_JUCE_APPLICATION(SubterraneumApplication)
