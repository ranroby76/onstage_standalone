// VST3 Folder management - simplified (VST3 only)

#include "PluginManagerTab.h"

// =============================================================================
// VST3FolderSection Implementation
// =============================================================================
VST3FolderSection::VST3FolderSection(juce::PropertiesFile* s, std::function<void()> onChanged)
    : settings(s), onChangedCallback(onChanged)
{
    headerLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF50C878));  // VST3 green
    addAndMakeVisible(headerLabel);
    
    addBtn.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select VST3 Folder",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "",
            true
        );
        
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | 
                                  juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.exists() && result.isDirectory()) {
                    addFolder(result.getFullPathName());
                }
            }
        );
    };
    addAndMakeVisible(addBtn);
    
    defaultsBtn.onClick = [this]() { addDefaultPaths(); };
    addAndMakeVisible(defaultsBtn);
    
    loadFolders();
}

VST3FolderSection::~VST3FolderSection() {
    saveFolders();
}

void VST3FolderSection::paint(juce::Graphics& g) {
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
    
    g.setColour(juce::Colour(0xFF50C878).withAlpha(0.5f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
}

void VST3FolderSection::resized() {
    auto area = getLocalBounds().reduced(10);
    
    auto headerRow = area.removeFromTop(28);
    headerLabel.setBounds(headerRow.removeFromLeft(200));
    defaultsBtn.setBounds(headerRow.removeFromRight(100).reduced(2));
    addBtn.setBounds(headerRow.removeFromRight(100).reduced(2));
    
    area.removeFromTop(8);
    
    int y = area.getY();
    for (auto* row : folderRows) {
        row->setBounds(area.getX(), y, area.getWidth(), 24);
        y += 28;
    }
}

void VST3FolderSection::addFolder(const juce::String& path) {
    // Check for duplicates
    for (auto* row : folderRows) {
        if (row->getPath().equalsIgnoreCase(path)) return;
    }
    
    auto* row = new PluginFolderRow(path, [this, path]() { removeFolder(path); });
    folderRows.add(row);
    addAndMakeVisible(row);
    
    saveFolders();
    if (onChangedCallback) onChangedCallback();
    
    if (auto* parent = getParentComponent()) parent->resized();
}

void VST3FolderSection::removeFolder(const juce::String& path) {
    for (int i = 0; i < folderRows.size(); ++i) {
        if (folderRows[i]->getPath().equalsIgnoreCase(path)) {
            folderRows.remove(i);
            break;
        }
    }
    
    saveFolders();
    if (onChangedCallback) onChangedCallback();
    
    if (auto* parent = getParentComponent()) parent->resized();
}

juce::StringArray VST3FolderSection::getFolders() const {
    juce::StringArray folders;
    for (auto* row : folderRows) {
        folders.add(row->getPath());
    }
    return folders;
}

void VST3FolderSection::loadFolders() {
    folderRows.clear();
    
    if (!settings) return;
    
    juce::String paths = settings->getValue("VST3Folders", "");
    if (paths.isNotEmpty()) {
        juce::StringArray folders;
        folders.addTokens(paths, "|", "");
        for (const auto& f : folders) {
            if (f.isNotEmpty()) {
                auto* row = new PluginFolderRow(f, [this, f]() { removeFolder(f); });
                folderRows.add(row);
                addAndMakeVisible(row);
            }
        }
    }
}

void VST3FolderSection::saveFolders() {
    if (!settings) return;
    
    juce::StringArray folders = getFolders();
    settings->setValue("VST3Folders", folders.joinIntoString("|"));
    settings->saveIfNeeded();
}

void VST3FolderSection::addDefaultPaths() {
    #if JUCE_WINDOWS
    addFolder("C:\\Program Files\\Common Files\\VST3");
    addFolder("C:\\Program Files (x86)\\Common Files\\VST3");
    #elif JUCE_MAC
    addFolder("/Library/Audio/Plug-Ins/VST3");
    addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Audio/Plug-Ins/VST3").getFullPathName());
    #elif JUCE_LINUX
    addFolder("/usr/lib/vst3");
    addFolder("/usr/local/lib/vst3");
    addFolder(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".vst3").getFullPathName());
    #endif
}

// =============================================================================
// PluginFoldersPanel Implementation
// =============================================================================
PluginFoldersPanel::PluginFoldersPanel(SubterraneumAudioProcessor& p) : processor(p) {
    auto* settings = processor.appProperties.getUserSettings();
    
    vst3Section = std::make_unique<VST3FolderSection>(settings, [](){});
    addAndMakeVisible(vst3Section.get());
    
    setSize(500, 400);
}

PluginFoldersPanel::~PluginFoldersPanel() {}

void PluginFoldersPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1E1E1E));
}

void PluginFoldersPanel::resized() {
    auto area = getLocalBounds().reduced(15);
    
    // Calculate height based on number of folders
    int sectionHeight = 50 + (vst3Section ? vst3Section->getFolders().size() * 28 : 0) + 50;
    vst3Section->setBounds(area.removeFromTop(juce::jmax(150, sectionHeight)));
}

juce::StringArray PluginFoldersPanel::getAllFolders() const {
    if (vst3Section) return vst3Section->getFolders();
    return {};
}