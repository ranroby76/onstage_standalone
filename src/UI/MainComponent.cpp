#include "MainComponent.h"
#include "IOPage.h"
#include "VocalsPage.h"
#include "MediaPage.h"
#include "../AppLogger.h"
#include "../RegistrationManager.h"

MainComponent::MainComponent()
    : presetManager(audioEngine),
      header(audioEngine),
      masterVolumeSlider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow),
      masterMeter(audioEngine) 
{
    LOG_INFO("=== MainComponent Constructor START ===");
    try {
        // FIX: CHECK REGISTRATION ON STARTUP
        // This loads the license.key file (if it exists) and restores PRO mode immediately.
        LOG_INFO("Step 0a: Checking License...");
        RegistrationManager::getInstance().checkRegistration();
        if (RegistrationManager::getInstance().isProMode()) {
            LOG_INFO("License Status: REGISTERED (PRO MODE)");
        } else {
            LOG_INFO("License Status: DEMO MODE");
        }

        LOG_INFO("Step 0b: Allocating GoldenSliderLookAndFeel...");
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
        LOG_INFO("Step 0c: Setting LookAndFeel for Tabs...");
        if (goldenLookAndFeel) {
            mainTabs.setLookAndFeel(goldenLookAndFeel.get());
        } else {
            LOG_ERROR("CRITICAL: Failed to allocate GoldenSliderLookAndFeel!");
        }

        LOG_INFO("Step 0d: Loading IO Settings from file...");
        if (ioSettingsManager.loadSettings()) {
            LOG_INFO("Settings loaded successfully.");
        } else {
            LOG_INFO("No settings file found or failed to load.");
        }

        LOG_INFO("Step 1: Adding header");
        addAndMakeVisible(header);
        
        LOG_INFO("Step 2: Adding mainTabs");
        addAndMakeVisible(mainTabs);
        // FIX: Force removal of all separator lines
        mainTabs.setTabBarDepth(60);
        mainTabs.setOutline(0);
        mainTabs.setIndent(0);
        mainTabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
        LOG_INFO("Step 3: Creating IOPage");
        ioPage = std::make_unique<IOPage>(audioEngine, ioSettingsManager);
        
        LOG_INFO("Step 4: Creating VocalsPage");
        vocalsPage = std::make_unique<VocalsPage>(audioEngine, presetManager);
        
        LOG_INFO("Step 5: Creating MediaPage");
        mediaPage = std::make_unique<MediaPage>(audioEngine, ioSettingsManager);

        LOG_INFO("Step 6: Adding tabs");
        juce::Colour bgColour(0xFF202020);
        if (ioPage)     mainTabs.addTab("      I/O      ",    bgColour, ioPage.get(),     true);
        if (vocalsPage) mainTabs.addTab("      Vocals      ", bgColour, vocalsPage.get(), true);
        if (mediaPage)  mainTabs.addTab("      Media      ",  bgColour, mediaPage.get(),  true);
        LOG_INFO("Step 7: Setting up record button");
        addAndMakeVisible(recordButton);
        recordButton.setButtonText("RECORD");
        recordButton.setMidiInfo("MIDI: CC 26 (Future)"); 
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF8B0000));
        recordButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        recordButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        recordButton.onClick = [this] { handleRecordClick(); };

        LOG_INFO("Step 8: Setting up download button");
        addAndMakeVisible(downloadWavButton);
        downloadWavButton.setButtonText("Download WAV");
        downloadWavButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        downloadWavButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF00FF00));
        downloadWavButton.setVisible(false);
        downloadWavButton.onClick = [this] { downloadRecording(); };

        LOG_INFO("Step 9: Adding master meter");
        addAndMakeVisible(masterMeter);

        LOG_INFO("Step 10: Setting up master volume slider");
        addAndMakeVisible(masterVolumeSlider);
        masterVolumeSlider.setRange(0.0, 1.0, 0.01);
        masterVolumeSlider.setValue(0.5, juce::dontSendNotification);
        masterVolumeSlider.setMidiInfo("MIDI: CC 7"); 
        masterVolumeSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFD4AF37));
        masterVolumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF404040));
        masterVolumeSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF202020));
        masterVolumeSlider.onValueChange = [this]() { 
            audioEngine.setMasterVolume(sliderValueToDb(masterVolumeSlider.getValue())); 
        };
        LOG_INFO("Step 11: Adding master volume label");
        addAndMakeVisible(masterVolumeLabel);
        masterVolumeLabel.setText("MASTER", juce::dontSendNotification);
        masterVolumeLabel.setFont(juce::Font(12.0f, juce::Font::bold));
        masterVolumeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
        masterVolumeLabel.setJustificationType(juce::Justification::centred);
        masterVolumeLabel.setMidiInfo("MIDI: CC 7");
        
        LOG_INFO("Step 11a: Adding slogan label");
        addAndMakeVisible(sloganLabel);
        sloganLabel.setText("ADVANCED LIVE PERFORMANCE PLATFORM", juce::dontSendNotification);
        sloganLabel.setFont(juce::Font(14.0f, juce::Font::bold));
        sloganLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));  // Golden
        sloganLabel.setJustificationType(juce::Justification::centredRight);
        
        LOG_INFO("Step 12: Setting up header callbacks");
        header.onSavePreset = [this]() { savePreset(); };
        header.onLoadPreset = [this]() { loadPreset(); };
        LOG_INFO("Step 13: Setting window size");
        setSize(1280, 720);
        
        LOG_INFO("=== MainComponent Constructor COMPLETE ===");
    }
    catch (const std::exception& e) {
        LOG_ERROR("EXCEPTION in MainComponent constructor: " + juce::String(e.what()));
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Startup Error", 
            "Error in MainComponent: " + juce::String(e.what()));
    }
    catch (...) {
        LOG_ERROR("UNKNOWN EXCEPTION in MainComponent constructor");
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Startup Error", 
            "Unknown error in MainComponent constructor");
    }
}

MainComponent::~MainComponent() 
{ 
    LOG_INFO("MainComponent destructor called");
    stopTimer();
    mainTabs.setLookAndFeel(nullptr);
}

void MainComponent::restoreIOSettings() 
{ 
    LOG_INFO("=== MainComponent::restoreIOSettings START ===");
    try {
        if (ioPage) {
            ioPage->restoreSavedSettings();
            LOG_INFO("=== MainComponent::restoreIOSettings COMPLETE ===");
        } else {
            LOG_ERROR("IOPage is null in restoreIOSettings");
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("EXCEPTION in restoreIOSettings: " + juce::String(e.what()));
    }
    catch (...) {
        LOG_ERROR("UNKNOWN EXCEPTION in restoreIOSettings");
    }
}

void MainComponent::paint(juce::Graphics& g) 
{ 
    g.fillAll(juce::Colour(0xFF202020)); 
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    auto rightColumn = bounds.removeFromRight(60);
    auto headerHeight = 60;
    header.setBounds(bounds.removeFromTop(headerHeight));
    mainTabs.setBounds(bounds);

    int stripY = headerHeight + (60 - 24) / 2;
    int rightEdge = bounds.getWidth() - 10;
    recordButton.setBounds(rightEdge - 80, stripY, 80, 24);
    downloadWavButton.setBounds(rightEdge - 80 - 120 - 5, stripY, 120, 24);
    recordButton.toFront(true); 
    downloadWavButton.toFront(true);
    
    // Position slogan CENTERED between tabs and download button
    int tabsEndX = 450;
    int downloadStartX = rightEdge - 80 - 120 - 5;
    int availableWidth = downloadStartX - tabsEndX - 20;
    int sloganWidth = 350;
    int sloganX = tabsEndX + (availableWidth - sloganWidth) / 2;
    int sloganY = headerHeight + (60 - 20) / 2;
    
    sloganLabel.setBounds(sloganX, sloganY, sloganWidth, 20);
    sloganLabel.setJustificationType(juce::Justification::centred);
    sloganLabel.toFront(true);

    rightColumn.removeFromTop(headerHeight);
    masterVolumeLabel.setBounds(rightColumn.removeFromTop(20).reduced(2));
    masterMeter.setBounds(rightColumn.removeFromTop(160).reduced(8, 0));
    rightColumn.removeFromTop(10);
    masterVolumeSlider.setBounds(rightColumn.removeFromTop(250).withSizeKeepingCentre(40, 250));
}

void MainComponent::handleRecordClick()
{
    if (!RegistrationManager::getInstance().isProMode())
    {
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, 
            "Demo Mode", "Recording is disabled in Demo Mode.\nPlease register to unlock.");
        return;
    }

    if (isRecording) {
        juce::NativeMessageBox::showYesNoBox(juce::AlertWindow::QuestionIcon, "Stop Recording", "Stop recording?", this,
            juce::ModalCallbackFunction::create([this](int result) { if (result == 1) stopRecording(); }));
    } else {
        if (showDownloads) { showDownloads = false; downloadWavButton.setVisible(false);
        }
        juce::NativeMessageBox::showYesNoBox(juce::AlertWindow::QuestionIcon, "Start Recording", "Start recording?", this,
            juce::ModalCallbackFunction::create([this](int result) { if (result == 1) startRecording(); }));
    }
}

void MainComponent::startRecording() 
{ 
    if (audioEngine.startRecording()) { 
        isRecording = true;
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::white); 
        recordButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF8B0000)); 
        startTimer(500); 
    } else { 
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "Could not start recording.");
    } 
}

void MainComponent::stopRecording() 
{ 
    audioEngine.stopRecording(); 
    isRecording = false; 
    stopTimer(); 
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF8B0000)); 
    recordButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white); 
    showDownloads = true;
    downloadWavButton.setVisible(true); 
}

void MainComponent::timerCallback() 
{ 
    recordFlickerState = !recordFlickerState;
    if (recordFlickerState) 
        recordButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF8B0000));
    else 
        recordButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
}

void MainComponent::downloadRecording() 
{ 
    juce::File r = audioEngine.getLastRecordingFile(); 
    if (!r.existsAsFile()) return; 
    
    auto c = std::make_shared<juce::FileChooser>("Save", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav");
    c->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, 
        [this, r, c](const juce::FileChooser& fc) { 
            juce::File d = fc.getResult(); 
            if (d != juce::File{}) 
                exportAudioFile(r, d); 
        }
    );
}

void MainComponent::exportAudioFile(const juce::File& s, const juce::File& d) 
{ 
    auto& formats = audioEngine.getFormatManager(); 
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(s));
    if (!reader) return; 
    
    juce::WavAudioFormat df; 
    std::unique_ptr<juce::FileOutputStream> os(new juce::FileOutputStream(d)); 
    if(os->failedToOpen()) return; 
    
    // UPDATED: Writer now configured for 44.1kHz and 24-bit
    std::unique_ptr<juce::AudioFormatWriter> w(df.createWriterFor(os.get(), 44100.0, 2, 24, {}, 0)); 
    if(!w) return; 
    os.release();
    
    juce::AudioFormatReaderSource rs(reader.get(), false); 
    // Resample from Source Rate to 44100Hz
    juce::ResamplingAudioSource res(&rs, false, 2); 
    res.setResamplingRatio(reader->sampleRate / 44100.0); 
    res.prepareToPlay(512, 44100.0); 
    
    juce::AudioBuffer<float> b(2, 2048); 
    juce::AudioSourceChannelInfo i(&b, 0, 2048);
    juce::int64 total = (juce::int64)((double)reader->lengthInSamples / (reader->sampleRate/44100.0)); 
    juce::int64 done=0; 
    
    while(done<total){ 
        int n=juce::jmin((juce::int64)2048, total-done);
        i.numSamples=n; 
        b.clear(); 
        res.getNextAudioBlock(i); 
        if(!w->writeFromAudioSampleBuffer(b,0,n)) break; 
        done+=n; 
    } 
    
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Success", "Saved!");
}

float MainComponent::sliderValueToDb(double v) 
{ 
    return v <= 0.0 ? -100.0f : static_cast<float>((v - 0.5) * 44.0);
}

void MainComponent::savePreset() 
{ 
    auto c = std::make_shared<juce::FileChooser>("Save", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.onspreset", true);
    c->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, 
        [this, c](const juce::FileChooser& fc) { 
            auto f = fc.getResult(); 
            if (f != juce::File{}) 
                if (presetManager.savePreset(f)) 
                    header.setPresetName(presetManager.getCurrentPresetName()); 
        }
    );
}

void MainComponent::loadPreset() 
{ 
    auto c = std::make_shared<juce::FileChooser>("Load", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.onspreset", true);
    c->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, 
        [this, c](const juce::FileChooser& fc) { 
            auto f = fc.getResult(); 
            if (f != juce::File{}) 
                if (presetManager.loadPreset(f)) { 
                    header.setPresetName(presetManager.getCurrentPresetName()); 
            
                    if (vocalsPage) 
                        vocalsPage->updateAllControlsFromEngine(); 
                } 
        }
    );
}