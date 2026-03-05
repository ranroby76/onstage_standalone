
// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab.h
// Plugin Manager Tab with scanning, tree view, and plugin info
// FIX: buildTree() preserves expand/collapse state
// NEW: Eye toggle - hide/show plugins from Add menus
// NEW: OOP scanning only — no moduleinfo.json, no skip-known shortcuts
// NEW: "Scan All Plugins" and "Rescan Existing" both use OOP deep scan
// FIX: scanDialog type matches CloseableDialogWindow (was juce::DialogWindow)
// NEW: Cancel button support for scan dialog

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OutOfProcessScanner.h"
#include "Style.h"
#include <map>
#include <set>

class SubterraneumAudioProcessor;
class AutoPluginScanner;

class PluginFolderRow : public juce::Component {
public:
    PluginFolderRow(const juce::String& path, std::function<void()> onRemove) 
        : folderPath(path), removeCallback(std::move(onRemove))
    {
        pathLabel.setText(path, juce::dontSendNotification);
        pathLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
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
    void closeButtonPressed() override {
        if (onCloseRequest)
            onCloseRequest();
        else
            setVisible(false);
    }
    std::function<void()> onCloseRequest;
};

// =============================================================================
// ScanProgressPanel — Simple VST3-only scanner (kept for legacy/simple scan)
// =============================================================================
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
    juce::File deadMansPedal;
    std::unique_ptr<OutOfProcessScanner> oopScanner;
    
    void timerCallback() override;
    void finishScan();
};

// =============================================================================
// AutoPluginScanner — Single-pass scanner UI with continuous visual feedback
// UI timer reads atomics from background OutOfProcessScanner — NEVER blocks
// =============================================================================
class AutoPluginScanner : public juce::Component, private juce::Timer {
public:
    AutoPluginScanner(SubterraneumAudioProcessor& processor, std::function<void()> onComplete);
    ~AutoPluginScanner() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void startScan();
    void startRescanExisting(const juce::Array<OutOfProcessScanner::PluginToScan>& plugins);
    void startQuickScan();
    void cancelScan();
    
    // Settings (kept for API compat with ScanOptionsPanel)
    void setTrustAllMode(bool) {}
    void setSkipVST2ShellPlugins(bool) {}
    void setVST2Timeout(int) {}
    void setSkipProblematicPlugins(bool) {}
    
private:
    SubterraneumAudioProcessor& processor;
    std::function<void()> onCompleteCallback;
    
    // UI labels — updated by timer reading atomics
    juce::Label titleLabel { "title", "Scanning Plugins..." };
    juce::Label phaseLabel { "phase", "" };
    juce::Label statusLabel { "status", "Preparing..." };
    juce::Label pluginLabel { "plugin", "" };
    juce::Label statsLabel { "stats", "" };
    juce::ProgressBar progressBar { progress };
    juce::TextButton cancelBtn { "Cancel" };
    double progress = 0.0;
    
    // The background scanner (does all work on its own thread)
    std::unique_ptr<OutOfProcessScanner> oopScanner;
    
    // Quick scan: stored for timestamp saving after scan completes
    juce::Array<OutOfProcessScanner::PluginToScan> quickScanAllFiles;
    
    void timerCallback() override;
    void collectPluginFiles();
    void finishScan();
    void removeDuplicatePlugins();
    void savePluginTimestamps(const juce::Array<OutOfProcessScanner::PluginToScan>& files);
    juce::FileSearchPath getSearchPathForFormat(const juce::String& formatName);
};

// =============================================================================
// ScanOptionsPanel — Scan options dialog
// =============================================================================
class ScanOptionsPanel : public juce::Component {
public:
    ScanOptionsPanel(SubterraneumAudioProcessor& p, std::function<void()> onComplete);
    ~ScanOptionsPanel() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    SubterraneumAudioProcessor& processor;
    std::function<void()> onCompleteCallback;
    
    juce::Label titleLabel { "title", "Plugin Scanner" };
    juce::Label formatInfoLabel { "formatInfo", "" };
    juce::Label statusLabel { "status", "" };
    juce::Label safeInfoLabel { "safeInfo", "" };
    
    juce::TextButton scanNewBtn { "Scan For New Plugins" };
    juce::TextButton rescanAllBtn { "Rescan All (Clear + Scan)" };
    juce::TextButton removeMissingBtn { "Remove Missing" };
    juce::TextButton removeAllBtn { "Remove All Plugins" };
    juce::TextButton cancelBtn { "Close" };
    
    void updateFormatInfo();
    void scanForNewPlugins(bool clearList);
    void launchScanner();
    void removeMissingPlugins();
    void removeAllPlugins();
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
    void runStartupQuickScan();
    
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
    
    juce::TextButton scanBtn { "Scan All Plugins" };
    juce::TextButton rescanExistingBtn { "Rescan Existing" };
    juce::Label rescanStatusLabel;
    juce::TextButton foldersBtn { "Plugin Folders..." };
    juce::TextButton clearBtn { "Clear All" };
    juce::TextButton resetBlacklistBtn { "Reset Blacklist" };
    juce::TextButton showBlacklistBtn { "Blacklist..." };
    
    std::unique_ptr<CloseableDialogWindow> foldersDialog;
    // FIX: Must be CloseableDialogWindow to match the make_unique<CloseableDialogWindow>() calls
    std::unique_ptr<CloseableDialogWindow> scanDialog;
    
    std::set<juce::String> expandedFolders;
    
    void buildTree();
    void showScanDialog();
    void showFoldersDialog();
    void showBlacklistDialog();
    void rescanExistingPlugins();
    void updateScanButtonStates();
    void expandAllItems();
    void collapseAllItems();
    
    void saveExpandedState();
    void restoreExpandedState();
    
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
        PluginTreeItem(const juce::String& itemName, bool isSortByVendor, bool isVendorFolder = false) 
            : name(itemName), isPlugin(false), showColumns(!isSortByVendor), isVendor(isVendorFolder) {}
        PluginTreeItem(const juce::String& itemName, const juce::PluginDescription& desc, bool isSortByVendor) 
            : name(itemName), description(desc), isPlugin(true), showColumns(!isSortByVendor), isVendor(false) {}
        
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
        bool hiddenFromMenus = false;
        
        PluginManagerTab* parentTab = nullptr;
    };
};
