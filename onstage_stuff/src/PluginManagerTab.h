#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "PluginProcessor.h"

// Forward declaration
class ScanDialogWindow;

// =============================================================================
// Plugin Folder Row Component
// =============================================================================
class PluginFolderRow : public juce::Component {
public:
    PluginFolderRow(const juce::String& path, std::function<void()> onRemove)
        : folderPath(path), removeCallback(onRemove)
    {
        pathLabel.setText(path, juce::dontSendNotification);
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

// =============================================================================
// Plugin Folder Section
// =============================================================================
class PluginFolderSection : public juce::Component {
public:
    PluginFolderSection(const juce::String& formatName, 
                        const juce::String& settingsKey,
                        juce::PropertiesFile* settings,
                        std::function<void()> onChanged);
    ~PluginFolderSection() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
    void addFolder(const juce::String& path);
    void removeFolder(const juce::String& path);
    juce::StringArray getFolders() const;
    void setFolders(const juce::StringArray& folders);
    
    int getPreferredHeight() const;
    void setExpanded(bool expanded);
    bool isExpanded() const { return expanded; }
    
private:
    juce::String formatName;
    juce::String settingsKey;
    juce::PropertiesFile* settings;
    std::function<void()> onChangedCallback;
    
    juce::Label headerLabel;
    juce::TextButton expandBtn;
    juce::TextButton addBtn { "Add Folder..." };
    juce::OwnedArray<PluginFolderRow> folderRows;
    
    bool expanded = true;
    
    void rebuildRows();
    void saveFolders();
    void loadFolders();
    void browseForFolder();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFolderSection)
};

// =============================================================================
// Plugin Folders Panel (NO VST2)
// =============================================================================
class PluginFoldersPanel : public juce::Component {
public:
    PluginFoldersPanel(SubterraneumAudioProcessor& processor);
    ~PluginFoldersPanel() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
    juce::StringArray getAllFolders() const;
    void applyToFormatManager();
    int getPreferredHeight() const;
    
private:
    SubterraneumAudioProcessor& processor;
    
    juce::Label titleLabel { "title", "Plugin Search Folders" };
    juce::TextButton collapseAllBtn { "Collapse All" };
    juce::TextButton addDefaultsBtn { "Add Defaults" };
    
    std::unique_ptr<PluginFolderSection> vst3Section;
    std::unique_ptr<PluginFolderSection> auSection;
    std::unique_ptr<PluginFolderSection> clapSection;
    std::unique_ptr<PluginFolderSection> ladspaSection;
    
    juce::Viewport viewport;
    juce::Component contentComponent;
    
    void addDefaultPaths();
    void updateLayout();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFoldersPanel)
};

// =============================================================================
// Auto Scanner - Scans all platform formats automatically
// =============================================================================
class AutoPluginScanner : public juce::Component, private juce::Timer {
public:
    AutoPluginScanner(SubterraneumAudioProcessor& processor, std::function<void()> onComplete);
    ~AutoPluginScanner() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
    void startScan();
    
private:
    SubterraneumAudioProcessor& processor;
    std::function<void()> onCompleteCallback;
    
    juce::Label titleLabel { "title", "Scanning Plugins..." };
    juce::Label formatLabel { "format", "" };
    juce::Label statusLabel { "status", "Initializing..." };
    juce::Label pluginLabel { "plugin", "" };
    juce::ProgressBar progressBar { progress };
    
    double progress = 0.0;
    
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    juce::StringArray filesToScan;
    int currentFormatIndex = 0;
    int totalPluginsFound = 0;
    juce::StringArray formatNames;
    
    juce::File deadMansPedal;
    
    void timerCallback() override;
    void scanNextFormat();
    void finishScan();
    juce::FileSearchPath getSearchPathForFormat(const juce::String& formatName);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoPluginScanner)
};

// =============================================================================
// Scan Options Panel - Manual scan options
// =============================================================================
class ScanOptionsPanel : public juce::Component {
public:
    ScanOptionsPanel(SubterraneumAudioProcessor& processor, std::function<void()> onComplete);
    ~ScanOptionsPanel() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
private:
    SubterraneumAudioProcessor& processor;
    std::function<void()> onCompleteCallback;
    
    juce::Label titleLabel { "title", "Plugin Scan Options" };
    juce::Label formatInfoLabel { "formats", "" };
    
    juce::TextButton scanNewBtn { "Scan for New Plugins" };
    juce::TextButton rescanAllBtn { "Rescan All Plugins" };
    juce::TextButton removeMissingBtn { "Remove Missing Plugins" };
    juce::TextButton removeAllBtn { "Remove All Plugins" };
    juce::TextButton cancelBtn { "Cancel" };
    
    juce::Label statusLabel { "status", "" };
    
    void scanForNewPlugins(bool clearList);
    void removeMissingPlugins();
    void removeAllPlugins();
    void updateFormatInfo();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScanOptionsPanel)
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
    
    juce::TextButton scanBtn { "Scan Plugins" };
    juce::TextButton resetBlacklistBtn { "Reset Blacklist" };
    juce::TextButton foldersBtn { "Plugin Folders..." };
    
    std::unique_ptr<juce::DialogWindow> foldersDialog;
    std::unique_ptr<juce::DialogWindow> scanDialog;
    
    void buildTree();
    void showScanDialog();
    void startAutoScan();
    void checkForCrashedScan();
    void showFoldersDialog();
    void expandAllItems();
    void collapseAllItems();
    
    juce::String savedDeviceName;
    bool wasDeviceOpen = false;
    void disableAudioForScan();
    void restoreAudioAfterScan();
    
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

        juce::String name;
        juce::PluginDescription description;
        bool isPlugin;
        bool showColumns;
        bool isVendor;
    };
};