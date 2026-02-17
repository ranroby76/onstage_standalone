// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_Folders.cpp
// PluginFolderSection, PluginFolderRow, and PluginFoldersPanel implementations
// FIX: Added Close button to PluginFoldersPanel

#include "PluginManagerTab.h"

// =============================================================================
// PluginFolderSection Implementation
// =============================================================================
PluginFolderSection::PluginFolderSection(const juce::String& formatName,
                                         const juce::String& settingsKey,
                                         juce::PropertiesFile* settings,
                                         std::function<void()> onChanged)
    : formatName(formatName), settingsKey(settingsKey), settings(settings), onChangedCallback(onChanged)
{
    headerLabel.setText(formatName, juce::dontSendNotification);
    headerLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);
    
    expandBtn.setButtonText("-");
    expandBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::grey.darker());
    expandBtn.onClick = [this]() { setExpanded(!expanded); };
    addAndMakeVisible(expandBtn);
    
    addBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::green.darker());
    addBtn.onClick = [this]() { browseForFolder(); };
    addAndMakeVisible(addBtn);
    
    loadFolders();
}

PluginFolderSection::~PluginFolderSection() { folderRows.clear(); }

void PluginFolderSection::paint(juce::Graphics& g) {
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.fillRect(0, 0, getWidth(), 28);
    g.setColour(juce::Colours::grey.darker());
    g.drawRect(getLocalBounds(), 1);
}

void PluginFolderSection::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(28).reduced(4, 2);
    expandBtn.setBounds(header.removeFromLeft(24));
    header.removeFromLeft(4);
    headerLabel.setBounds(header.removeFromLeft(200));
    addBtn.setBounds(header.removeFromRight(80));
    
    if (expanded) {
        area.removeFromTop(4);
        for (auto* row : folderRows) {
            row->setBounds(area.removeFromTop(24).reduced(28, 0));
            area.removeFromTop(2);
        }
    }
}

int PluginFolderSection::getPreferredHeight() const {
    if (!expanded) return 28;
    return 28 + 4 + (folderRows.size() * 26) + 4;
}

void PluginFolderSection::setExpanded(bool exp) {
    expanded = exp;
    expandBtn.setButtonText(expanded ? "-" : "+");
    for (auto* row : folderRows) row->setVisible(expanded);
    if (auto* parent = getParentComponent()) parent->resized();
}

void PluginFolderSection::addFolder(const juce::String& path) {
    for (auto* row : folderRows)
        if (row->getPath() == path) return;
    
    auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
    folderRows.add(row);
    addAndMakeVisible(row);
    row->setVisible(expanded);
    saveFolders();
    if (auto* parent = getParentComponent()) parent->resized();
    if (onChangedCallback) onChangedCallback();
}

void PluginFolderSection::removeFolder(const juce::String& path) {
    for (int i = 0; i < folderRows.size(); ++i) {
        if (folderRows[i]->getPath() == path) {
            folderRows.remove(i);
            break;
        }
    }
    saveFolders();
    if (auto* parent = getParentComponent()) parent->resized();
    if (onChangedCallback) onChangedCallback();
}

juce::StringArray PluginFolderSection::getFolders() const {
    juce::StringArray folders;
    for (auto* row : folderRows) folders.add(row->getPath());
    return folders;
}

void PluginFolderSection::setFolders(const juce::StringArray& folders) {
    folderRows.clear();
    for (const auto& path : folders) {
        if (path.isNotEmpty()) {
            auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
            folderRows.add(row);
            addAndMakeVisible(row);
            row->setVisible(expanded);
        }
    }
    saveFolders();
    resized();
}

void PluginFolderSection::rebuildRows() {
    auto folders = getFolders();
    folderRows.clear();
    for (const auto& path : folders) {
        auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
        folderRows.add(row);
        addAndMakeVisible(row);
        row->setVisible(expanded);
    }
    resized();
}

void PluginFolderSection::saveFolders() {
    if (!settings) return;
    juce::StringArray folders;
    for (auto* row : folderRows) folders.add(row->getPath());
    settings->setValue(settingsKey, folders.joinIntoString("|"));
    settings->saveIfNeeded();
}

void PluginFolderSection::loadFolders() {
    if (!settings) return;
    juce::String savedPaths = settings->getValue(settingsKey, "");
    if (savedPaths.isEmpty()) return;
    juce::StringArray folders = juce::StringArray::fromTokens(savedPaths, "|", "");
    for (const auto& path : folders) {
        if (path.isNotEmpty()) {
            auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
            folderRows.add(row);
            addAndMakeVisible(row);
        }
    }
}

void PluginFolderSection::browseForFolder() {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select " + formatName + " Plugin Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.exists() && result.isDirectory())
                addFolder(result.getFullPathName());
        });
}

// =============================================================================
// PluginFoldersPanel Implementation - FIX: Added Close button
// =============================================================================
PluginFoldersPanel::PluginFoldersPanel(SubterraneumAudioProcessor& p) : processor(p)
{
    auto* settings = processor.appProperties.getUserSettings();
    auto onChanged = [this]() { updateLayout(); applyToFormatManager(); };
    
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    collapseAllBtn.onClick = [this]() {
        if (vst2Section) vst2Section->setExpanded(false);
        if (vst3Section) vst3Section->setExpanded(false);
        if (auSection) auSection->setExpanded(false);
        if (clapSection) clapSection->setExpanded(false);
        if (ladspaSection) ladspaSection->setExpanded(false);
        updateLayout();
    };
    addAndMakeVisible(collapseAllBtn);
    
    addDefaultsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::blue.darker());
    addDefaultsBtn.onClick = [this]() { addDefaultPaths(); };
    addAndMakeVisible(addDefaultsBtn);
    
    // FIX: Close button
    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    closeBtn.onClick = [this]() {
        if (onCloseRequest) onCloseRequest();
    };
    addAndMakeVisible(closeBtn);
    
    // =========================================================================
    // VST2 Section (Windows/Mac only)
    // =========================================================================
    #if JUCE_PLUGINHOST_VST
    vst2Section = std::make_unique<PluginFolderSection>("VST2 Plugins", "VST2Folders", settings, onChanged);
    contentComponent.addAndMakeVisible(vst2Section.get());
    #endif
    
    #if JUCE_PLUGINHOST_VST3
    vst3Section = std::make_unique<PluginFolderSection>("VST3 Plugins", "VST3Folders", settings, onChanged);
    contentComponent.addAndMakeVisible(vst3Section.get());
    #endif
    
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    auSection = std::make_unique<PluginFolderSection>("AudioUnit Plugins", "AUFolders", settings, onChanged);
    contentComponent.addAndMakeVisible(auSection.get());
    #endif
    
    #if SUBTERRANEUM_CLAP_HOSTING
    clapSection = std::make_unique<PluginFolderSection>("CLAP Plugins", "CLAPFolders", settings, onChanged);
    contentComponent.addAndMakeVisible(clapSection.get());
    #endif
    
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    ladspaSection = std::make_unique<PluginFolderSection>("LADSPA Plugins", "LADSPAFolders", settings, onChanged);
    contentComponent.addAndMakeVisible(ladspaSection.get());
    #endif
    
    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    
    // Check if any paths exist
    bool hasAnyPaths = false;
    if (vst2Section && !vst2Section->getFolders().isEmpty()) hasAnyPaths = true;
    if (vst3Section && !vst3Section->getFolders().isEmpty()) hasAnyPaths = true;
    if (auSection && !auSection->getFolders().isEmpty()) hasAnyPaths = true;
    if (clapSection && !clapSection->getFolders().isEmpty()) hasAnyPaths = true;
    if (ladspaSection && !ladspaSection->getFolders().isEmpty()) hasAnyPaths = true;
    
    if (!hasAnyPaths) addDefaultPaths();
    applyToFormatManager();
}

PluginFoldersPanel::~PluginFoldersPanel() {}

void PluginFoldersPanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xFF1E1E1E)); }

void PluginFoldersPanel::resized() {
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(30);
    titleLabel.setBounds(header.removeFromLeft(150));
    closeBtn.setBounds(header.removeFromRight(80));    // FIX: Close button on right
    header.removeFromRight(10);
    addDefaultsBtn.setBounds(header.removeFromRight(100));
    header.removeFromRight(10);
    collapseAllBtn.setBounds(header.removeFromRight(90));
    area.removeFromTop(10);
    viewport.setBounds(area);
    updateLayout();
}

void PluginFoldersPanel::updateLayout() {
    int y = 0;
    int width = viewport.getWidth() - 20;
    
    if (vst2Section) { 
        vst2Section->setBounds(0, y, width, vst2Section->getPreferredHeight()); 
        y += vst2Section->getPreferredHeight() + 8; 
    }
    if (vst3Section) { 
        vst3Section->setBounds(0, y, width, vst3Section->getPreferredHeight()); 
        y += vst3Section->getPreferredHeight() + 8; 
    }
    if (auSection) { 
        auSection->setBounds(0, y, width, auSection->getPreferredHeight()); 
        y += auSection->getPreferredHeight() + 8; 
    }
    if (clapSection) { 
        clapSection->setBounds(0, y, width, clapSection->getPreferredHeight()); 
        y += clapSection->getPreferredHeight() + 8; 
    }
    if (ladspaSection) { 
        ladspaSection->setBounds(0, y, width, ladspaSection->getPreferredHeight()); 
        y += ladspaSection->getPreferredHeight() + 8; 
    }
    contentComponent.setSize(width, y);
}

int PluginFoldersPanel::getPreferredHeight() const {
    int height = 50;
    if (vst2Section) height += vst2Section->getPreferredHeight() + 8;
    if (vst3Section) height += vst3Section->getPreferredHeight() + 8;
    if (auSection) height += auSection->getPreferredHeight() + 8;
    if (clapSection) height += clapSection->getPreferredHeight() + 8;
    if (ladspaSection) height += ladspaSection->getPreferredHeight() + 8;
    return height + 20;
}

juce::StringArray PluginFoldersPanel::getAllFolders() const {
    juce::StringArray all;
    if (vst2Section) all.addArray(vst2Section->getFolders());
    if (vst3Section) all.addArray(vst3Section->getFolders());
    if (auSection) all.addArray(auSection->getFolders());
    if (clapSection) all.addArray(clapSection->getFolders());
    if (ladspaSection) all.addArray(ladspaSection->getFolders());
    return all;
}

void PluginFoldersPanel::applyToFormatManager() {
    // Apply VST2 paths
    if (vst2Section) {
        for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
            auto* format = processor.formatManager.getFormat(i);
            if (format->getName() == "VST") {
                auto paths = vst2Section->getFolders();
                juce::FileSearchPath searchPath;
                for (const auto& p : paths) searchPath.add(juce::File(p));
                format->searchPathsForPlugins(searchPath, true, false);
            }
        }
    }
    
    // Apply VST3 paths
    if (vst3Section) {
        for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
            auto* format = processor.formatManager.getFormat(i);
            if (format->getName() == "VST3") {
                auto paths = vst3Section->getFolders();
                juce::FileSearchPath searchPath;
                for (const auto& p : paths) searchPath.add(juce::File(p));
                format->searchPathsForPlugins(searchPath, true, false);
            }
        }
    }
}

void PluginFoldersPanel::addDefaultPaths() {
    #if JUCE_WINDOWS
    // VST2 Default Paths (Windows)
    if (vst2Section) {
        vst2Section->addFolder("C:\\Program Files\\VSTPlugins");
        vst2Section->addFolder("C:\\Program Files\\Steinberg\\VSTPlugins");
        vst2Section->addFolder("C:\\Program Files\\Common Files\\VST2");
        vst2Section->addFolder("C:\\Program Files (x86)\\VSTPlugins");
        vst2Section->addFolder("C:\\Program Files (x86)\\Steinberg\\VSTPlugins");
    }
    
    // VST3 Default Paths (Windows)
    if (vst3Section) {
        vst3Section->addFolder("C:\\Program Files\\Common Files\\VST3");
        vst3Section->addFolder("C:\\Program Files (x86)\\Common Files\\VST3");
    }
    #elif JUCE_MAC
    // VST2 Default Paths (macOS)
    if (vst2Section) {
        vst2Section->addFolder("/Library/Audio/Plug-Ins/VST");
        vst2Section->addFolder("~/Library/Audio/Plug-Ins/VST");
    }
    
    // VST3 Default Paths (macOS)
    if (vst3Section) {
        vst3Section->addFolder("/Library/Audio/Plug-Ins/VST3");
        vst3Section->addFolder("~/Library/Audio/Plug-Ins/VST3");
    }
    
    // AU Default Paths (macOS)
    if (auSection) {
        auSection->addFolder("/Library/Audio/Plug-Ins/Components");
        auSection->addFolder("~/Library/Audio/Plug-Ins/Components");
    }
    #elif JUCE_LINUX
    // VST3 Default Paths (Linux)
    if (vst3Section) {
        vst3Section->addFolder("/usr/lib/vst3");
        vst3Section->addFolder("/usr/local/lib/vst3");
        vst3Section->addFolder("~/.vst3");
    }
    
    // LADSPA Default Paths (Linux)
    if (ladspaSection) {
        ladspaSection->addFolder("/usr/lib/ladspa");
        ladspaSection->addFolder("/usr/local/lib/ladspa");
    }
    #endif
    
    updateLayout();
    applyToFormatManager();
}
