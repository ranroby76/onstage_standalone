// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab.h
// Plugin Manager Tab with scanning, tree view, and plugin info
// FIX: buildTree() preserves expand/collapse state
// NEW: Eye toggle - hide/show plugins from Add menus

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Style.h"
#include <map>
#include <set>

class SubterraneumAudioProcessor;
class AutoPluginScanner;

class PluginFolderRow : public juce::Component {
public:
    PluginFolderRow(const juce::String& path, std::function<void()> removeCallback) 
        : folderPath(path), removeCallback(removeCallback)
    {
        pathLabel.setText(path, juce::dontSendNotification);
        pathLabel.setFont(juce::Font(12.0f));
        pathLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        pathLabel.setTooltip(path);
        addAndMakeVisible(pathLabel);
        removeBtn.setButtonText("X");
        removeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        removeBtn.onClick = removeCallback;
        addAndMakeVisible(removeBtn);
        juce::File folder(path);
        if (!folder.exists()) {
            pathLabel.setColour(juce::Label::textColourId, juce::Colours::red);
            pathLabel.setTooltip(path + " (NOT FOUND)");
        }
    }
    void resized() override {
        auto area = getLocalBounds();
        removeBtn.setBounds(area.removeFromRight(24).reduced(2));
        area.removeFromRight(4);
        pathLabel.setBounds(area);
    }
    juce::String getPath() const { return folderPath; }
private:
    juce::String folderPath;
    juce::Label pathLabel;
    juce::TextButton removeBtn;
    std::function<void()> removeCallback;
};

class PluginFolderSection : public juce::Component {
public:
    PluginFolderSection(const juce::String& formatName, const juce::String& settingsKey,
                        juce::PropertiesFile* settings, std::function<void()> onChanged);
    ~PluginFolderSection() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setExpanded(bool exp);
    bool isExpanded() const { return expanded; }
    int getPreferredHeight() const;
    void addFolder(const juce::String& path);
    void removeFolder(const juce::String& path);
    juce::StringArray getFolders() const;
    void setFolders(const juce::StringArray& folders);
private:
    juce::String formatName;
    juce::String settingsKey;
    juce::PropertiesFile* settings;
    std::function<void()> onChangedCallback;
    juce::Label headerLabel;
    juce::TextButton expandBtn { "-" };
    juce::TextButton addBtn { "Add..." };
    juce::OwnedArray<PluginFolderRow> folderRows;
    bool expanded = true;
    void rebuildRows();
    void saveFolders();
    void loadFolders();
    void browseForFolder();
    std::unique_ptr<juce::FileChooser> fileChooser;
};

class VST3FolderSection : public juce::Component {
public:
    VST3FolderSection(juce::PropertiesFile* settings, std::function<void()> onChanged);
    ~VST3FolderSection() override;
    void resized() override;
    void paint(juce::Graphics& g) override;
    void addFolder(const juce::String& path);
    void removeFolder(const juce::String& path);
    juce::StringArray getFolders() const;
    void addDefaultPaths();
private:
    juce::PropertiesFile* settings;
    std::function<void()> onChangedCallback;
    juce::Label headerLabel { "header", "VST3 Plugin Folders" };
    juce::TextButton addBtn { "Add Folder..." };
    juce::TextButton defaultsBtn { "Add Defaults" };
    juce::OwnedArray<PluginFolderRow> folderRows;
    void rebuildRows();
    void saveFolders();
    void loadFolders();
    std::unique_ptr<juce::FileChooser> fileChooser;
};

class PluginFoldersPanel : public juce::Component {
public:
    PluginFoldersPanel(SubterraneumAudioProcessor& p);
    ~PluginFoldersPanel() override;
    void resized() override;
    void paint(juce::Graphics& g) override;
    juce::StringArray getAllFolders() const;
    std::function<void()> onCloseRequest;
private:
    SubterraneumAudioProcessor& processor;
    juce::Label titleLabel { "title", "Plugin Folders" };
    juce::TextButton collapseAllBtn { "Collapse All" };
    juce::TextButton addDefaultsBtn { "Add Defaults" };
    juce::TextButton closeBtn { "Close" };
    juce::Viewport viewport;
    juce::Component contentComponent;
    std::unique_ptr<PluginFolderSection> vst2Section;
    std::unique_ptr<PluginFolderSection> vst3Section;
    std::unique_ptr<PluginFolderSection> auSection;
    std::unique_ptr<PluginFolderSection> clapSection;
    std::unique_ptr<PluginFolderSection> ladspaSection;
    void updateLayout();
    void applyToFormatManager();
    void addDefaultPaths();
    int getPreferredHeight() const;
};

class CloseableDialogWindow : public juce::DialogWindow {
public:
    CloseableDialogWindow(const juce::String& title, juce::Colour bg, bool escapeCloses)
        : DialogWindow(title, bg, escapeCloses, true) { setUsingNativeTitleBar(true); }
    void closeButtonPressed() override { setVisible(false); }
};

class ScanProgressPanel : public juce::Component, private juce::Timer {
public:
    ScanProgressPanel(SubterraneumAudioProcessor& p, std::function<void()> onComplete);
    ~ScanProgressPanel() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void startScan();
private:
    SubterraneumAudioProcessor& processor;
    std::function<void()> onCompleteCallback;
    juce::Label titleLabel { "title", "Scanning VST3 Plugins..." };
    juce::Label statusLabel { "status", "Preparing..." };
    juce::Label pluginLabel { "plugin", "" };
    juce::ProgressBar progressBar { progress };
    double progress = 0.0;
    bool scanning = false;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    juce::File deadMansPedal;
    void timerCallback() override;
    void finishScan();
};

class AutoPluginScanner : public juce::Component, private juce::Timer {
public:
    AutoPluginScanner(SubterraneumAudioProcessor& processor, std::function<void()> onComplete);
    ~AutoPluginScanner() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void startScan();
private:
    SubterraneumAudioProcessor& processor;
    std::function<void()> onCompleteCallback;
    juce::Label titleLabel { "title", "Scanning Plugins..." };
    juce::Label formatLabel { "format", "" };
    juce::Label statusLabel { "status", "Preparing..." };
    juce::Label pluginLabel { "plugin", "" };
    juce::ProgressBar progressBar { progress };
    double progress = 0.0;
    int currentFormatIndex = 0;
    int totalPluginsFound = 0;
    juce::StringArray formatNames;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    juce::File deadMansPedal;
    void timerCallback() override;
    void startNextFormat();
    void finishScan();
};

// =============================================================================
// Main Plugin Manager Tab
// =============================================================================
class PluginManagerTab : public juce::Component, 
                         public juce::ComboBox::Listener, 
                         public juce::Button::Listener,
                         public juce::ChangeListener,
                         private juce::Timer {
public:
    PluginManagerTab(SubterraneumAudioProcessor& p);
    ~PluginManagerTab() override;
    void paint(juce::Graphics& g) override; 
    void resized() override;
    void comboBoxChanged(juce::ComboBox* cb) override;
    void buttonClicked(juce::Button* b) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;
    
    void removePluginFromList(const juce::PluginDescription& desc);
    void checkForCrashedScan();
    
    // NEW: Plugin visibility (eye toggle) - hide plugins from Add menus
    bool isPluginHidden(const juce::PluginDescription& desc);
    void setPluginHidden(const juce::PluginDescription& desc, bool hidden);
    void togglePluginVisibility(const juce::PluginDescription& desc);
    
private:
    SubterraneumAudioProcessor& processor;
    
    juce::Label sortLabel { "sort", "Sort By:" };
    juce::ComboBox sortCombo;
    juce::ToggleButton instBtn { "INSTRUMENTS" };
    juce::ToggleButton effectBtn { "EFFECTS" };
    juce::ToggleButton allBtn { "ALL" };
    juce::TextButton expandAllBtn { "+" };
    juce::TextButton collapseAllBtn { "-" };
    
    juce::TreeView pluginTree;
    juce::TableHeaderComponent tableHeader;
    
    juce::TextEditor infoPanel;
    
    juce::TextButton scanBtn { "Scan Plugins" };
    juce::TextButton rescanExistingBtn { "Rescan Existing" };
    juce::Label rescanStatusLabel;
    juce::TextButton foldersBtn { "Plugin Folders..." };
    juce::TextButton clearBtn { "Clear All" };
    juce::TextButton resetBlacklistBtn { "Reset Blacklist" };
    
    std::unique_ptr<CloseableDialogWindow> foldersDialog;
    std::unique_ptr<juce::DialogWindow> scanDialog;
    
    // FIX: Track which folders are expanded across tree rebuilds
    std::set<juce::String> expandedFolders;
    
    void buildTree();
    void showScanDialog();
    void showFoldersDialog();
    void rescanExistingPlugins();
    void expandAllItems();
    void collapseAllItems();
    
    // FIX: Save/restore expand state across buildTree() calls
    void saveExpandedState();
    void restoreExpandedState();
    
    // NEW: Visibility key helper
    juce::String getPluginVisibilityKey(const juce::PluginDescription& desc);
    
    class TreeLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area,
                                      juce::Colour, bool isOpen, bool isMouseOver) override {
            auto boxSize = std::min(area.getWidth(), area.getHeight()) * 0.7f;
            auto box = area.withSizeKeepingCentre(boxSize, boxSize);
            g.setColour(juce::Colours::white.withAlpha(isMouseOver ? 1.0f : 0.7f));
            g.drawRect(box, 1.0f);
            auto lineThickness = 2.0f;
            auto center = box.getCentre();
            g.fillRect(box.getX() + 3, center.y - lineThickness/2, box.getWidth() - 6, lineThickness);
            if (!isOpen)
                g.fillRect(center.x - lineThickness/2, box.getY() + 3, lineThickness, box.getHeight() - 6);
        }
    };
    TreeLookAndFeel treeLAF;

    class PluginTreeItem : public juce::TreeViewItem {
    public:
        PluginTreeItem(const juce::String& name, bool isSortByVendor, bool isVendorFolder = false) 
            : name(name), isPlugin(false), showColumns(!isSortByVendor), isVendor(isVendorFolder) {}
        PluginTreeItem(const juce::String& name, const juce::PluginDescription& desc, bool isSortByVendor) 
            : name(name), description(desc), isPlugin(true), showColumns(!isSortByVendor), isVendor(false) {}
        
        bool mightContainSubItems() override { return !isPlugin; }
        void paintItem(juce::Graphics& g, int w, int h) override;
        void itemClicked(const juce::MouseEvent& e) override;
        juce::var getDragSourceDescription() override;
        
        bool getDefaultOpenness() const { return isVendor ? false : true; }
        
        juce::String getFormatBadge() const;
        juce::Colour getFormatColor() const;

        juce::String name;
        juce::PluginDescription description;
        bool isPlugin;
        bool showColumns;
        bool isVendor;
        bool hiddenFromMenus = false;  // NEW: Eye toggle state
        
        PluginManagerTab* parentTab = nullptr;
    };
};
