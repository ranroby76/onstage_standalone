// #D:\Workspace\Subterraneum_plugins_daw\src\PluginBrowserPanel.cpp
// FIX: Removed vendor from items, removed favorites, added title,
// collapsible vendor/folder groups, custom drag image for visibility
// FIX: Added Recorder to System Tools
// NEW: Hidden plugins (eye toggle) are filtered out

#include "PluginBrowserPanel.h"

// =============================================================================
// FavoritePatchItem - .subt patch file entry in Favorites mode
// =============================================================================
FavoritePatchItem::FavoritePatchItem(const juce::File& file) : patchFile(file) {
    setSize(200, 36);
}

void FavoritePatchItem::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    if (hovered) {
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
    
    // Star icon
    g.setColour(juce::Colour(0xFFFFD700));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(juce::String::charToString(0x2605), 6, 0, 20, getHeight(), juce::Justification::centred);
    
    // Patch name (without .subt extension)
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(patchFile.getFileNameWithoutExtension(), 28, 0, getWidth() - 36, getHeight(), 
               juce::Justification::centredLeft, true);
}

void FavoritePatchItem::mouseDoubleClick(const juce::MouseEvent&) {
    if (onPatchDoubleClick) onPatchDoubleClick(patchFile);
}

// =============================================================================
// PluginBrowserItem - Plugin entry (no favorites, no vendor display)
// =============================================================================
PluginBrowserItem::PluginBrowserItem(const juce::PluginDescription& desc)
    : description(desc), isSystemTool(false) {
    setSize(200, 32);
}

PluginBrowserItem::PluginBrowserItem(SystemToolType type)
    : toolType(type), isSystemTool(true) {
    setSize(200, 32);
}

void PluginBrowserItem::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    if (hovered) {
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
    
    float x = 8.0f;
    
    // Format/Type badge
    juce::String badge;
    juce::Colour badgeColor;
    
    if (isSystemTool) {
        badge = "SYS";
        badgeColor = juce::Colour(0xFFFF9800);
    } else {
        if (description.pluginFormatName.containsIgnoreCase("VST3")) {
            badge = "VST3";
            badgeColor = juce::Colour(0xFF50C878);
        } else if (description.pluginFormatName.containsIgnoreCase("VST")) {
            badge = "VST2";
            badgeColor = juce::Colour(0xFF4A90D9);
        } else if (description.pluginFormatName.containsIgnoreCase("AU")) {
            badge = "AU";
            badgeColor = juce::Colour(0xFFFF6B6B);
        } else {
            badge = description.pluginFormatName.substring(0, 4);
            badgeColor = juce::Colours::grey;
        }
    }
    
    g.setColour(badgeColor);
    g.fillRoundedRectangle(x, (getHeight() - 18) / 2.0f, 36, 18, 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(badge, (int)x, 0, 36, getHeight(), juce::Justification::centred);
    x += 42.0f;
    
    // INST/FX badge for plugins
    if (!isSystemTool) {
        bool isInstr = description.isInstrument;
        g.setColour(isInstr ? juce::Colour(0xFFFFD700) : juce::Colour(0xFF87CEEB));
        g.fillRoundedRectangle(x, (getHeight() - 18) / 2.0f, 28, 18, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(isInstr ? "INST" : "FX", (int)x, 0, 28, getHeight(), juce::Justification::centred);
        x += 34.0f;
    }
    
    // Name only (no vendor)
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    juce::String name;
    if (isSystemTool) {
        switch (toolType) {
            case SystemToolType::Connector:   name = "Connector"; break;
            case SystemToolType::StereoMeter: name = "Stereo Meter"; break;
            case SystemToolType::MidiMonitor: name = "MIDI Monitor"; break;
            case SystemToolType::Recorder:       name = "Recorder"; break;
            case SystemToolType::ManualSampler:  name = "Manual Sampling"; break;
            case SystemToolType::AutoSampler:    name = "Auto Sampling"; break;
            case SystemToolType::MidiPlayer:     name = "MIDI Player"; break;
            case SystemToolType::StepSeq:        name = "Step Seq"; break;
            case SystemToolType::TransientSplitter: name = "Transient Splitter"; break;
            case SystemToolType::Latcher:            name = "Latcher"; break;
            case SystemToolType::VST2Plugin:     name = "VST2 Plugin..."; break;
            default: name = "Unknown";
        }
    } else {
        name = description.name;
    }
    g.drawText(name, (int)x, 0, getWidth() - (int)x - 8, getHeight(), juce::Justification::centredLeft, true);
}

void PluginBrowserItem::mouseDown(const juce::MouseEvent& /*e*/) {
    // No favorites toggle anymore
}

void PluginBrowserItem::mouseDrag(const juce::MouseEvent& e) {
    if (e.getDistanceFromDragStart() > 5) {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            juce::String dragId;
            juce::String displayName;
            
            if (isSystemTool) {
                switch (toolType) {
                    case SystemToolType::Connector:   
                        dragId = "TOOL:Connector"; 
                        displayName = "Connector";
                        break;
                    case SystemToolType::StereoMeter: 
                        dragId = "TOOL:StereoMeter"; 
                        displayName = "Stereo Meter";
                        break;
                    case SystemToolType::MidiMonitor: 
                        dragId = "TOOL:MidiMonitor"; 
                        displayName = "MIDI Monitor";
                        break;
                    case SystemToolType::Recorder:
                        dragId = "TOOL:Recorder"; 
                        displayName = "Recorder";
                        break;
                    case SystemToolType::ManualSampler:
                        dragId = "TOOL:ManualSampler"; 
                        displayName = "Manual Sampling";
                        break;
                    case SystemToolType::AutoSampler:
                        dragId = "TOOL:AutoSampler"; 
                        displayName = "Auto Sampling";
                        break;
                    case SystemToolType::MidiPlayer:
                        dragId = "TOOL:MidiPlayer"; 
                        displayName = "MIDI Player";
                        break;
                    case SystemToolType::StepSeq:
                        dragId = "TOOL:StepSeq"; 
                        displayName = "Step Seq";
                        break;
                    case SystemToolType::TransientSplitter:
                        dragId = "TOOL:TransientSplitter"; 
                        displayName = "Transient Splitter";
                        break;
                    case SystemToolType::Latcher:
                        dragId = "TOOL:Latcher";
                        displayName = "Latcher";
                        break;
                    case SystemToolType::VST2Plugin:
                        dragId = "TOOL:VST2Plugin"; 
                        displayName = "VST2 Plugin...";
                        break;
                    default: 
                        dragId = "TOOL:Unknown";
                        displayName = "Unknown";
                }
            } else {
                dragId = description.createIdentifierString();
                displayName = description.name;
            }
            
            // FIX: Create custom drag image that will be visible above OpenGL
            int imgWidth = 180;
            int imgHeight = 28;
            juce::Image dragImage(juce::Image::ARGB, imgWidth, imgHeight, true);
            juce::Graphics g(dragImage);
            
            // Semi-transparent background
            g.setColour(juce::Colour(0xDD333333));
            g.fillRoundedRectangle(0, 0, (float)imgWidth, (float)imgHeight, 4.0f);
            
            // Border
            g.setColour(juce::Colours::cyan);
            g.drawRoundedRectangle(0.5f, 0.5f, imgWidth - 1.0f, imgHeight - 1.0f, 4.0f, 1.5f);
            
            // Text
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText(displayName, 8, 0, imgWidth - 16, imgHeight, juce::Justification::centredLeft, true);
            
            container->startDragging(dragId, this, juce::ScaledImage(dragImage), true);
        }
    }
}

void PluginBrowserItem::mouseDoubleClick(const juce::MouseEvent&) {
    if (isSystemTool) {
        if (onToolDoubleClick) onToolDoubleClick(toolType);
    } else {
        if (onPluginDoubleClick) onPluginDoubleClick(description);
    }
}

// =============================================================================
// PluginGroupHeader - Collapsible group header
// =============================================================================
PluginGroupHeader::PluginGroupHeader(const juce::String& name, int count)
    : groupName(name), pluginCount(count) {
    setSize(200, 28);
}

void PluginGroupHeader::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    if (hovered) {
        g.setColour(juce::Colour(0xFF3A3A3A));
    } else {
        g.setColour(juce::Colour(0xFF2A2A2A));
    }
    g.fillRoundedRectangle(bounds.reduced(2, 1), 3.0f);
    
    // Arrow indicator
    g.setColour(juce::Colours::orange);
    float arrowX = 12.0f;
    float arrowY = getHeight() / 2.0f;
    float arrowSize = 5.0f;
    
    juce::Path arrow;
    if (expanded) {
        // Down arrow
        arrow.addTriangle(arrowX - arrowSize, arrowY - arrowSize/2,
                          arrowX + arrowSize, arrowY - arrowSize/2,
                          arrowX, arrowY + arrowSize/2);
    } else {
        // Right arrow
        arrow.addTriangle(arrowX - arrowSize/2, arrowY - arrowSize,
                          arrowX + arrowSize/2, arrowY,
                          arrowX - arrowSize/2, arrowY + arrowSize);
    }
    g.fillPath(arrow);
    
    // Group name
    g.setColour(juce::Colours::cyan);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(groupName, 28, 0, getWidth() - 70, getHeight(), juce::Justification::centredLeft, true);
    
    // Plugin count
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("(" + juce::String(pluginCount) + ")", getWidth() - 45, 0, 40, getHeight(), juce::Justification::centredRight);
}

void PluginGroupHeader::mouseDown(const juce::MouseEvent&) {
    if (onToggle) onToggle(groupName);
}

// =============================================================================
// PluginBrowserList
// =============================================================================
void PluginBrowserList::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF252525));
}

void PluginBrowserList::toggleGroup(const juce::String& groupName) {
    if (expandedGroups.count(groupName))
        expandedGroups.erase(groupName);
    else
        expandedGroups.insert(groupName);
    rebuildLayout();
}

void PluginBrowserList::rebuildLayout() {
    // Clear items only
    items.clear();
    
    int y = 5;
    
    // Rebuild based on current grouping and expanded state
    for (auto* header : headers) {
        header->setBounds(0, y, getWidth(), 28);
        header->setExpanded(expandedGroups.count(header->getName()) > 0);
        y += 30;
        
        // Add items only if expanded
        if (header->isExpanded() && groupedPlugins.count(header->getName())) {
            for (const auto& desc : groupedPlugins[header->getName()]) {
                auto* item = items.add(new PluginBrowserItem(desc));
                item->setBounds(0, y, getWidth(), 32);
                item->onPluginDoubleClick = onPluginDoubleClick;
                addAndMakeVisible(item);
                y += 34;
            }
        }
    }
    
    setSize(getWidth(), y + 10);
}

void PluginBrowserList::setPlugins(const juce::Array<juce::PluginDescription>& plugins,
                                    bool groupByVendor, bool groupByFolder, bool groupByFormat) {
    items.clear();
    headers.clear();
    flatHeaders.clear();
    groupedPlugins.clear();
    
    currentGroupByVendor = groupByVendor;
    currentGroupByFolder = groupByFolder;
    currentGroupByFormat = groupByFormat;
    
    int y = 5;
    
    if (!groupByVendor && !groupByFolder && !groupByFormat) {
        // FLAT MODE - Simple A-Z list
        for (const auto& desc : plugins) {
            auto* item = items.add(new PluginBrowserItem(desc));
            item->setBounds(0, y, getWidth(), 32);
            item->onPluginDoubleClick = onPluginDoubleClick;
            addAndMakeVisible(item);
            y += 34;
        }
    } else {
        // GROUPED MODE - By vendor, folder, or format
        std::map<juce::String, juce::Array<juce::PluginDescription>> groups;
        
        for (const auto& desc : plugins) {
            juce::String groupKey;
            if (groupByVendor) {
                groupKey = desc.manufacturerName.isEmpty() ? "Unknown" : desc.manufacturerName;
            } else if (groupByFolder) {
                juce::File f(desc.fileOrIdentifier);
                groupKey = f.getParentDirectory().getFileName();
                if (groupKey.isEmpty()) groupKey = "Root";
            } else if (groupByFormat) {
                groupKey = desc.pluginFormatName;
            }
            groups[groupKey].add(desc);
        }
        
        // Store for rebuild
        groupedPlugins = groups;
        
        // Create headers and items
        for (auto& [groupName, groupPlugins] : groups) {
            auto* header = headers.add(new PluginGroupHeader(groupName, groupPlugins.size()));
            header->setBounds(0, y, getWidth(), 28);
            header->onToggle = [this](const juce::String& name) { toggleGroup(name); };
            header->setExpanded(expandedGroups.count(groupName) > 0);
            addAndMakeVisible(header);
            y += 30;
            
            // Only show items if expanded
            if (header->isExpanded()) {
                for (const auto& desc : groupPlugins) {
                    auto* item = items.add(new PluginBrowserItem(desc));
                    item->setBounds(0, y, getWidth(), 32);
                    item->onPluginDoubleClick = onPluginDoubleClick;
                    addAndMakeVisible(item);
                    y += 34;
                }
            }
        }
    }
    
    setSize(getWidth(), y + 10);
}

void PluginBrowserList::setSystemTools() {
    items.clear();
    headers.clear();
    flatHeaders.clear();
    groupedPlugins.clear();
    
    int y = 5;
    
    auto* h = flatHeaders.add(new juce::Label());
    h->setText("System Tools", juce::dontSendNotification);
    h->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    h->setColour(juce::Label::textColourId, juce::Colours::orange);
    h->setBounds(5, y, getWidth() - 10, 24);
    addAndMakeVisible(h);
    y += 26;
    
    auto* c = items.add(new PluginBrowserItem(SystemToolType::Connector));
    c->setBounds(0, y, getWidth(), 32);
    c->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(c);
    y += 34;
    
    auto* s = items.add(new PluginBrowserItem(SystemToolType::StereoMeter));
    s->setBounds(0, y, getWidth(), 32);
    s->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(s);
    y += 34;
    
    auto* m = items.add(new PluginBrowserItem(SystemToolType::MidiMonitor));
    m->setBounds(0, y, getWidth(), 32);
    m->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(m);
    y += 34;
    
    // NEW: Add Recorder
    auto* r = items.add(new PluginBrowserItem(SystemToolType::Recorder));
    r->setBounds(0, y, getWidth(), 32);
    r->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(r);
    y += 34;
    
    auto* ms = items.add(new PluginBrowserItem(SystemToolType::ManualSampler));
    ms->setBounds(0, y, getWidth(), 32);
    ms->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(ms);
    y += 34;
    
    auto* as = items.add(new PluginBrowserItem(SystemToolType::AutoSampler));
    as->setBounds(0, y, getWidth(), 32);
    as->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(as);
    y += 34;
    
    auto* mp = items.add(new PluginBrowserItem(SystemToolType::MidiPlayer));
    mp->setBounds(0, y, getWidth(), 32);
    mp->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(mp);
    y += 34;
    
    auto* ss = items.add(new PluginBrowserItem(SystemToolType::StepSeq));
    ss->setBounds(0, y, getWidth(), 32);
    ss->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(ss);
    y += 34;
    
    auto* ts = items.add(new PluginBrowserItem(SystemToolType::TransientSplitter));
    ts->setBounds(0, y, getWidth(), 32);
    ts->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(ts);
    y += 34;
    
    auto* latch = items.add(new PluginBrowserItem(SystemToolType::Latcher));
    latch->setBounds(0, y, getWidth(), 32);
    latch->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(latch);
    y += 34;
    
    auto* v2 = items.add(new PluginBrowserItem(SystemToolType::VST2Plugin));
    v2->setBounds(0, y, getWidth(), 32);
    v2->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(v2);
    y += 34;
    
    setSize(getWidth(), y + 10);
}

void PluginBrowserList::setFavorites(const juce::Array<juce::File>& patchFiles) {
    items.clear();
    headers.clear();
    flatHeaders.clear();
    groupedPlugins.clear();
    favoriteItems.clear();
    removeAllChildren();
    
    int y = 5;
    
    if (patchFiles.isEmpty()) {
        auto* h = flatHeaders.add(new juce::Label());
        h->setText("No .subt patches found", juce::dontSendNotification);
        h->setFont(juce::Font(juce::FontOptions(12.0f)));
        h->setColour(juce::Label::textColourId, juce::Colours::grey);
        h->setBounds(5, y, getWidth() - 10, 24);
        addAndMakeVisible(h);
        y += 26;
    } else {
        auto* h = flatHeaders.add(new juce::Label());
        h->setText("Favorite Patches", juce::dontSendNotification);
        h->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        h->setColour(juce::Label::textColourId, juce::Colour(0xFFFFD700));
        h->setBounds(5, y, getWidth() - 10, 24);
        addAndMakeVisible(h);
        y += 26;
        
        for (const auto& file : patchFiles) {
            auto* item = favoriteItems.add(new FavoritePatchItem(file));
            item->setBounds(0, y, getWidth(), 36);
            item->onPatchDoubleClick = onPatchDoubleClick;
            addAndMakeVisible(item);
            y += 38;
        }
    }
    
    setSize(getWidth(), y + 10);
}

// =============================================================================
// PluginBrowserPanel
// =============================================================================
PluginBrowserPanel::PluginBrowserPanel(SubterraneumAudioProcessor& p) : processor(p) {
    // Mode selector buttons: Add Plugins | Favorites
    addPluginsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::cyan.darker());
    addPluginsBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addPluginsBtn.addListener(this);
    addAndMakeVisible(addPluginsBtn);
    
    favoritesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    favoritesBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    favoritesBtn.addListener(this);
    addAndMakeVisible(favoritesBtn);
    
    searchBox.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
    searchBox.addListener(this);
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A2A));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(searchBox);
    
    // Only 4 filter buttons now (removed favorites)
    for (auto* b : { &allBtn, &instrumentsBtn, &effectsBtn, &toolsBtn,
                     &flatBtn, &vendorBtn, &folderBtn, &formatBtn }) {
        b->addListener(this);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        addAndMakeVisible(b);
    }
    
    // Favorites mode controls
    setFavFolderBtn.addListener(this);
    setFavFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2a5a2a));
    setFavFolderBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    addAndMakeVisible(setFavFolderBtn);
    setFavFolderBtn.setVisible(false);
    
    favFolderLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    favFolderLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    favFolderLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(favFolderLabel);
    favFolderLabel.setVisible(false);
    
    pluginList = std::make_unique<PluginBrowserList>();
    pluginList->onPluginDoubleClick = [this](const auto& d) { if (onPluginDropped) onPluginDropped(d, {300,300}); };
    pluginList->onToolDoubleClick = [this](auto t) { if (onToolDropped) onToolDropped(t, {300,300}); };
    pluginList->onPatchDoubleClick = [this](const juce::File& f) { showWorkspaceSelector(f); };
    viewport.setViewedComponent(pluginList.get(), false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    
    countLabel.setFont(11.0f);
    countLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    countLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(countLabel);
    
    // Load saved favorites folder path
    if (auto* settings = processor.appProperties.getUserSettings()) {
        auto path = settings->getValue("FavoritesPatchFolder", "");
        if (path.isNotEmpty()) {
            juce::File dir(path);
            if (dir.isDirectory())
                favFolderLabel.setText(dir.getFileName(), juce::dontSendNotification);
        }
    }
    
    processor.knownPluginList.addChangeListener(this);
    setWantsKeyboardFocus(true);
}

PluginBrowserPanel::~PluginBrowserPanel() {
    processor.knownPluginList.removeChangeListener(this);
}

void PluginBrowserPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF252525));
    g.setColour(juce::Colour(0xFF4A4A4A));
    g.drawRect(getLocalBounds(), 1);
}

void PluginBrowserPanel::resized() {
    auto area = getLocalBounds().reduced(8);
    
    // Mode selector row: [Add Plugins] [Favorites]
    auto modeRow = area.removeFromTop(28);
    int halfW = modeRow.getWidth() / 2;
    addPluginsBtn.setBounds(modeRow.removeFromLeft(halfW).reduced(1, 0));
    favoritesBtn.setBounds(modeRow.reduced(1, 0));
    area.removeFromTop(4);
    
    if (favoritesMode) {
        // Favorites mode layout
        searchBox.setVisible(true);
        searchBox.setBounds(area.removeFromTop(28));
        searchBox.setTextToShowWhenEmpty("Search patches...", juce::Colours::grey);
        area.removeFromTop(6);
        
        // Set Folder button + label
        setFavFolderBtn.setVisible(true);
        setFavFolderBtn.setBounds(area.removeFromTop(26).reduced(2, 0));
        area.removeFromTop(2);
        favFolderLabel.setVisible(true);
        favFolderLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(4);
        
        // Hide plugin filter/view buttons
        allBtn.setVisible(false);
        instrumentsBtn.setVisible(false);
        effectsBtn.setVisible(false);
        toolsBtn.setVisible(false);
        flatBtn.setVisible(false);
        vendorBtn.setVisible(false);
        folderBtn.setVisible(false);
        formatBtn.setVisible(false);
    } else {
        // Plugin browser mode layout
        setFavFolderBtn.setVisible(false);
        favFolderLabel.setVisible(false);
        
        searchBox.setVisible(true);
        searchBox.setBounds(area.removeFromTop(28));
        searchBox.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
        area.removeFromTop(6);
        
        // Type filter row
        auto row1 = area.removeFromTop(26);
        int w1 = row1.getWidth() / 4;
        allBtn.setVisible(true);
        instrumentsBtn.setVisible(true);
        effectsBtn.setVisible(true);
        toolsBtn.setVisible(true);
        allBtn.setBounds(row1.removeFromLeft(w1).reduced(2, 0));
        instrumentsBtn.setBounds(row1.removeFromLeft(w1).reduced(2, 0));
        effectsBtn.setBounds(row1.removeFromLeft(w1).reduced(2, 0));
        toolsBtn.setBounds(row1.reduced(2, 0));
        area.removeFromTop(4);
        
        // View mode row (hidden for Tools)
        auto row2 = area.removeFromTop(26);
        bool showViewMode = (typeFilter != TypeFilter::Tools);
        flatBtn.setVisible(showViewMode);
        vendorBtn.setVisible(showViewMode);
        folderBtn.setVisible(showViewMode);
        formatBtn.setVisible(showViewMode);
        if (showViewMode) {
            int w2 = row2.getWidth() / 4;
            flatBtn.setBounds(row2.removeFromLeft(w2).reduced(2, 0));
            vendorBtn.setBounds(row2.removeFromLeft(w2).reduced(2, 0));
            folderBtn.setBounds(row2.removeFromLeft(w2).reduced(2, 0));
            formatBtn.setBounds(row2.reduced(2, 0));
        }
        area.removeFromTop(4);
    }
    
    countLabel.setBounds(area.removeFromBottom(20));
    area.removeFromBottom(4);
    
    viewport.setBounds(area);
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
}

void PluginBrowserPanel::visibilityChanged() {
    if (isVisible()) { refresh(); searchBox.grabKeyboardFocus(); }
}

bool PluginBrowserPanel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) { searchBox.clear(); applyFilters(); return true; }
    if (!searchBox.hasKeyboardFocus(true)) searchBox.grabKeyboardFocus();
    return false;
}

void PluginBrowserPanel::buttonClicked(juce::Button* b) {
    if (b == &addPluginsBtn) {
        if (!favoritesMode) return;  // already in this mode
        favoritesMode = false;
        searchBox.clear();
        updateButtons();
        applyFilters();
        resized();
        return;
    }
    if (b == &favoritesBtn) {
        if (favoritesMode) return;  // already in this mode
        favoritesMode = true;
        searchBox.clear();
        updateButtons();
        loadFavoritesList();
        resized();
        return;
    }
    if (b == &setFavFolderBtn) {
        selectFavoritesFolder();
        return;
    }
    
    if (b == &allBtn) typeFilter = TypeFilter::All;
    else if (b == &instrumentsBtn) typeFilter = TypeFilter::Instruments;
    else if (b == &effectsBtn) typeFilter = TypeFilter::Effects;
    else if (b == &toolsBtn) typeFilter = TypeFilter::Tools;
    else if (b == &flatBtn) viewMode = ViewMode::Flat;
    else if (b == &vendorBtn) viewMode = ViewMode::ByVendor;
    else if (b == &folderBtn) viewMode = ViewMode::ByFolder;
    else if (b == &formatBtn) viewMode = ViewMode::ByFormat;
    
    updateButtons();
    applyFilters();
    resized();
}

void PluginBrowserPanel::textEditorTextChanged(juce::TextEditor&) {
    searchText = searchBox.getText();
    if (favoritesMode)
        loadFavoritesList();
    else
        applyFilters();
}

void PluginBrowserPanel::changeListenerCallback(juce::ChangeBroadcaster*) {
    applyFilters();
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
    viewport.setViewPosition(0, 0);
    repaint();
}

void PluginBrowserPanel::refresh() {
    updateButtons();
    if (favoritesMode)
        loadFavoritesList();
    else
        applyFilters();
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
}

void PluginBrowserPanel::updateButtons() {
    // Mode selector highlights
    addPluginsBtn.setColour(juce::TextButton::buttonColourId, !favoritesMode ? juce::Colours::cyan.darker() : juce::Colour(0xFF2A2A2A));
    favoritesBtn.setColour(juce::TextButton::buttonColourId, favoritesMode ? juce::Colour(0xFFB8860B) : juce::Colour(0xFF2A2A2A));
    
    allBtn.setColour(juce::TextButton::buttonColourId, typeFilter == TypeFilter::All ? juce::Colours::cyan.darker() : juce::Colour(0xFF2A2A2A));
    instrumentsBtn.setColour(juce::TextButton::buttonColourId, typeFilter == TypeFilter::Instruments ? juce::Colours::gold.darker() : juce::Colour(0xFF2A2A2A));
    effectsBtn.setColour(juce::TextButton::buttonColourId, typeFilter == TypeFilter::Effects ? juce::Colours::skyblue.darker() : juce::Colour(0xFF2A2A2A));
    toolsBtn.setColour(juce::TextButton::buttonColourId, typeFilter == TypeFilter::Tools ? juce::Colours::orange.darker() : juce::Colour(0xFF2A2A2A));
    
    flatBtn.setColour(juce::TextButton::buttonColourId, viewMode == ViewMode::Flat ? juce::Colour(0xFF4A4A4A) : juce::Colour(0xFF2A2A2A));
    vendorBtn.setColour(juce::TextButton::buttonColourId, viewMode == ViewMode::ByVendor ? juce::Colour(0xFF4A4A4A) : juce::Colour(0xFF2A2A2A));
    folderBtn.setColour(juce::TextButton::buttonColourId, viewMode == ViewMode::ByFolder ? juce::Colour(0xFF4A4A4A) : juce::Colour(0xFF2A2A2A));
    formatBtn.setColour(juce::TextButton::buttonColourId, viewMode == ViewMode::ByFormat ? juce::Colour(0xFF4A4A4A) : juce::Colour(0xFF2A2A2A));
}

void PluginBrowserPanel::applyFilters() {
    if (typeFilter == TypeFilter::Tools) {
        pluginList->setSystemTools();
        countLabel.setText("4 tools", juce::dontSendNotification);
        return;
    }
    
    auto filtered = getFilteredPlugins();
    pluginList->setPlugins(filtered,
        viewMode == ViewMode::ByVendor, viewMode == ViewMode::ByFolder, viewMode == ViewMode::ByFormat);
    countLabel.setText(juce::String(filtered.size()) + " plugin" + (filtered.size() != 1 ? "s" : ""), juce::dontSendNotification);
}

// NEW: Filters out hidden plugins (eye toggle in Plugin Manager tab)
juce::Array<juce::PluginDescription> PluginBrowserPanel::getFilteredPlugins() {
    auto all = processor.knownPluginList.getTypes();
    juce::Array<juce::PluginDescription> result;
    
    auto* userSettings = processor.appProperties.getUserSettings();
    
    for (const auto& p : all) {
        if (typeFilter == TypeFilter::Instruments && !p.isInstrument) continue;
        if (typeFilter == TypeFilter::Effects && p.isInstrument) continue;
        
        // NEW: Skip hidden plugins (eye toggle)
        if (userSettings) {
            juce::String key = "PluginHidden_" + p.fileOrIdentifier.replaceCharacters(" :/\\.", "_____")
                               + "_" + juce::String(p.uniqueId);
            if (userSettings->getBoolValue(key, false)) continue;
        }
        
        if (searchText.isNotEmpty()) {
            juce::String s = searchText.toLowerCase();
            if (!p.name.toLowerCase().contains(s) && 
                !p.manufacturerName.toLowerCase().contains(s)) continue;
        }
        result.add(p);
    }
    return result;
}

// =============================================================================
// Favorites Mode Methods
// =============================================================================
juce::File PluginBrowserPanel::getFavoritesFolder() const {
    if (auto* settings = processor.appProperties.getUserSettings()) {
        auto path = settings->getValue("FavoritesPatchFolder", "");
        if (path.isNotEmpty()) {
            juce::File dir(path);
            if (dir.isDirectory()) return dir;
        }
    }
    return {};
}

void PluginBrowserPanel::selectFavoritesFolder() {
    favFolderChooser = std::make_shared<juce::FileChooser>(
        "Select Favorites Patch Folder",
        getFavoritesFolder().exists() ? getFavoritesFolder() : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "",
        true);
    
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    
    favFolderChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser) {
        auto result = chooser.getResult();
        if (result.isDirectory()) {
            if (auto* settings = processor.appProperties.getUserSettings()) {
                settings->setValue("FavoritesPatchFolder", result.getFullPathName());
                settings->saveIfNeeded();
            }
            favFolderLabel.setText(result.getFileName(), juce::dontSendNotification);
            loadFavoritesList();
        }
    });
}

void PluginBrowserPanel::loadFavoritesList() {
    auto folder = getFavoritesFolder();
    juce::Array<juce::File> patches;
    
    if (folder.isDirectory()) {
        auto files = folder.findChildFiles(juce::File::findFiles, true, "*.subt");
        files.sort();
        
        for (const auto& f : files) {
            if (searchText.isNotEmpty()) {
                if (!f.getFileNameWithoutExtension().containsIgnoreCase(searchText))
                    continue;
            }
            patches.add(f);
        }
    }
    
    pluginList->setFavorites(patches);
    countLabel.setText(juce::String(patches.size()) + " patch" + (patches.size() != 1 ? "es" : ""), juce::dontSendNotification);
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
    viewport.setViewPosition(0, 0);
}

void PluginBrowserPanel::showWorkspaceSelector(const juce::File& patchFile) {
    juce::PopupMenu menu;
    menu.addSectionHeader("Load into workspace:");
    
    for (int i = 0; i < 16; ++i) {
        menu.addItem(i + 1, "Workspace " + juce::String(i + 1));
    }
    
    menu.addSeparator();
    menu.addItem(100, "Load into CURRENT workspace");
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(getScreenBounds().reduced(20)),
        [this, patchFile](int result) {
            if (result == 0) return;  // dismissed
            
            if (result == 100) {
                // Load into current workspace
                processor.loadUserPreset(patchFile);
                
                // Refresh editor
                if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getTopLevelComponent()))
                    editor->resized();
            } else {
                int wsIndex = result - 1;
                // Switch to target workspace first, then load
                processor.switchWorkspace(wsIndex);
                processor.loadUserPreset(patchFile);
                
                if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getTopLevelComponent()))
                    editor->resized();
            }
        });
}









