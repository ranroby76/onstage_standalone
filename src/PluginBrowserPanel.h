// Plugin Browser Panel - ALL plugin and tool selection happens here
// Filters: All | Instruments | Effects | Tools (removed Favorites)
// FIX: Collapsible vendor/folder groups, title header, no vendor in items
// FIX: Added Recorder to SystemToolType
// NEW: Favorites mode - toggle title to browse saved .subt patches

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Style.h"
#include <set>
#include <map>

// =============================================================================
// System Tool Type - FIXED: Added Recorder
// =============================================================================
enum class SystemToolType {
    None,
    Connector,
    StereoMeter,
    MidiMonitor,
    Recorder,
    ManualSampler,
    AutoSampler,
    MidiPlayer,
    StepSeq,
    TransientSplitter,
    Latcher,
    VST2Plugin
};

// =============================================================================
// Favorite Patch Item - displays a .subt file in Favorites mode
// =============================================================================
class FavoritePatchItem : public juce::Component {
public:
    FavoritePatchItem(const juce::File& file);
    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent&) override;
    
    const juce::File& getFile() const { return patchFile; }
    
    std::function<void(const juce::File&)> onPatchDoubleClick;
    
private:
    juce::File patchFile;
    bool hovered = false;
};

// =============================================================================
// Plugin Browser Item - Plugin or system tool entry
// =============================================================================
class PluginBrowserItem : public juce::Component {
public:
    PluginBrowserItem(const juce::PluginDescription& desc);
    PluginBrowserItem(SystemToolType toolType);
    
    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    
    bool isPlugin() const { return !isSystemTool; }
    SystemToolType getToolType() const { return toolType; }
    const juce::PluginDescription& getDescription() const { return description; }
    
    std::function<void(const juce::PluginDescription&)> onPluginDoubleClick;
    std::function<void(SystemToolType)> onToolDoubleClick;
    
private:
    juce::PluginDescription description;
    SystemToolType toolType = SystemToolType::None;
    bool isSystemTool = false;
    bool hovered = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrowserItem)
};

// =============================================================================
// Collapsible Group Header
// =============================================================================
class PluginGroupHeader : public juce::Component {
public:
    PluginGroupHeader(const juce::String& name, int count);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    
    bool isExpanded() const { return expanded; }
    void setExpanded(bool shouldExpand) { expanded = shouldExpand; repaint(); }
    juce::String getName() const { return groupName; }
    
    std::function<void(const juce::String&)> onToggle;
    
private:
    juce::String groupName;
    int pluginCount = 0;
    bool expanded = false;
    bool hovered = false;
    
    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovered = false; repaint(); }
};

// =============================================================================
// Plugin Browser List
// =============================================================================
class PluginBrowserList : public juce::Component {
public:
    void setPlugins(const juce::Array<juce::PluginDescription>& plugins,
                    bool groupByVendor, bool groupByFolder, bool groupByFormat);
    void setSystemTools();
    void setFavorites(const juce::Array<juce::File>& patchFiles);
    void paint(juce::Graphics& g) override;
    
    void toggleGroup(const juce::String& groupName);
    
    std::function<void(const juce::PluginDescription&)> onPluginDoubleClick;
    std::function<void(SystemToolType)> onToolDoubleClick;
    std::function<void(const juce::File&)> onPatchDoubleClick;
    
private:
    juce::OwnedArray<PluginBrowserItem> items;
    juce::OwnedArray<PluginGroupHeader> headers;
    juce::OwnedArray<juce::Label> flatHeaders;
    juce::OwnedArray<FavoritePatchItem> favoriteItems;
    
    std::set<juce::String> expandedGroups;
    
    // Store plugins per group for rebuilding on expand/collapse
    std::map<juce::String, juce::Array<juce::PluginDescription>> groupedPlugins;
    bool currentGroupByVendor = false;
    bool currentGroupByFolder = false;
    bool currentGroupByFormat = false;
    
    void rebuildLayout();
};

// =============================================================================
// Plugin Browser Panel
// =============================================================================
class PluginBrowserPanel : public juce::Component,
                           public juce::TextEditor::Listener,
                           public juce::Button::Listener,
                           public juce::ChangeListener {
public:
    PluginBrowserPanel(SubterraneumAudioProcessor& processor);
    ~PluginBrowserPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void buttonClicked(juce::Button* button) override;
    void textEditorTextChanged(juce::TextEditor&) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    
    void refresh();
    
    std::function<void(const juce::PluginDescription&, juce::Point<int>)> onPluginDropped;
    std::function<void(SystemToolType, juce::Point<int>)> onToolDropped;
    
    // Callback for loading a favorite patch into a workspace
    std::function<void(const juce::File&, int workspaceIndex)> onFavoritePatchLoad;
    
private:
    SubterraneumAudioProcessor& processor;
    
    // Title row: two mode selector buttons
    juce::TextButton addPluginsBtn { "Add Plugins" };
    juce::TextButton favoritesBtn { "Favorites" };
    bool favoritesMode = false;
    
    juce::TextEditor searchBox;
    
    // Filter row: All | Instruments | Effects | Tools (removed Favorites)
    juce::TextButton allBtn { "All" };
    juce::TextButton instrumentsBtn { "Instr" };
    juce::TextButton effectsBtn { "Effects" };
    juce::TextButton toolsBtn { "Tools" };
    
    // View mode row
    juce::TextButton flatBtn { "A-Z" };
    juce::TextButton vendorBtn { "Vendor" };
    juce::TextButton folderBtn { "Folder" };
    juce::TextButton formatBtn { "Format" };
    
    // Favorites mode controls
    juce::TextButton setFavFolderBtn { "Set Folder..." };
    juce::Label favFolderLabel { "favFolder", "No folder set" };
    
    juce::Viewport viewport;
    std::unique_ptr<PluginBrowserList> pluginList;
    juce::Label countLabel;
    
    // File chooser for favorites folder
    std::shared_ptr<juce::FileChooser> favFolderChooser;
    
    enum class TypeFilter { All, Instruments, Effects, Tools };
    enum class ViewMode { Flat, ByVendor, ByFolder, ByFormat };
    
    TypeFilter typeFilter = TypeFilter::All;
    ViewMode viewMode = ViewMode::Flat;
    juce::String searchText;
    
    void applyFilters();
    void updateButtons();
    juce::Array<juce::PluginDescription> getFilteredPlugins();
    
    // Favorites helpers
    void selectFavoritesFolder();
    void loadFavoritesList();
    void showWorkspaceSelector(const juce::File& patchFile);
    juce::File getFavoritesFolder() const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrowserPanel)
};






