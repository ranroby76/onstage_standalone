
// #D:\Workspace\Subterraneum_plugins_daw\src\PluginBrowserPanel.cpp
// FIX: Removed vendor from items, removed favorites, added title,
// collapsible vendor/folder groups, custom drag image for visibility
// FIX: Added Recorder to System Tools
// NEW: Hidden plugins (eye toggle) are filtered out
// FIX: A-Z sorting in getFilteredPlugins()
// FIX: Smart ByFolder grouping — skips generic VST3/VST folder names
// FIX: Sort within groups alphabetically
// FIX: Added MidiMultiFilter to System Tools

#include "PluginBrowserPanel.h"
#include "ContainerProcessor.h"

// =============================================================================
// HELPER: Generic folder names that are NOT useful for ByFolder grouping
// These are standard system/VST host paths — NOT vendor names
// =============================================================================
static bool isGenericFolderName(const juce::String& name)
{
    static const juce::StringArray genericFolders = {
        // System paths
        "VST3", "VST", "VST2", "Components", "VSTPlugins",
        "Common Files", "Plug-Ins", "Plug-ins", "Audio",
        "Program Files", "Program Files (x86)", "Library",
        "usr", "lib", "local", "vst", "vst3", ".vst", ".vst3",
        "Steinberg", "Contents", "MacOS", "Resources",
        "x86_64-win", "x86_64-linux", "x86-win",
        // Category-style subfolder names (e.g. MeldaProduction\Stereo\)
        "Delay", "Distortion", "Dynamics", "EQ", "Equalizer",
        "Filter", "Modulation", "Reverb", "Stereo", "Time",
        "Pitch Shift", "Tools", "Effects",
        "Compressor", "Limiter", "Chorus", "Flanger", "Phaser",
        "Generators", "Analyzers", "Meters", "Mastering",
        "Mixing", "Utilities", "Channel Strip"
    };
    if (name.isEmpty()) return true;
    // Drive letters like "C:", "D:"
    if (name.length() <= 2 && name.endsWithChar(':')) return true;
    return genericFolders.contains(name, true);
}

// =============================================================================
// HELPER: Get a meaningful folder group key for a plugin path
// Walks up from parent dir, skipping generic folders
// Falls back to manufacturerName, then "[General]"
// =============================================================================
static juce::String getSmartFolderGroup(const juce::PluginDescription& desc)
{
    juce::File f(desc.fileOrIdentifier);
    juce::File current = f.getParentDirectory();
    
    // Walk up to 4 levels looking for a non-generic folder name
    for (int depth = 0; depth < 4 && current.exists(); ++depth)
    {
        juce::String name = current.getFileName();
        
        // Stop at filesystem/drive roots
        if (name.isEmpty() || current == current.getParentDirectory())
            break;
        if (name.length() <= 2 && name.endsWithChar(':'))
            break;
        
        if (!isGenericFolderName(name))
            return name;
        current = current.getParentDirectory();
    }
    
    // All ancestors are generic — use vendor metadata as fallback
    if (desc.manufacturerName.isNotEmpty()
        && !desc.manufacturerName.equalsIgnoreCase("Unknown"))
        return desc.manufacturerName;
    
    return "[General]";
}

// =============================================================================
// FavoritePatchItem - .ons patch file entry in Favorites mode
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
    
    // Patch name (without .ons extension)
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(patchFile.getFileNameWithoutExtension(), 28, 0, getWidth() - 36, getHeight(), 
               juce::Justification::centredLeft, true);
}

void FavoritePatchItem::mouseDoubleClick(const juce::MouseEvent&) {
    if (onPatchDoubleClick) onPatchDoubleClick(patchFile);
}

// =============================================================================
// ContainerPresetItem - .onsc preset file entry in Containers tab
// =============================================================================
ContainerPresetItem::ContainerPresetItem(const juce::File& file) : presetFile(file) {
    setSize(200, 36);
}

void ContainerPresetItem::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    if (hovered) {
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
    
    // Purple container badge
    float x = 6.0f;
    g.setColour(juce::Colour(80, 40, 100));
    g.fillRoundedRectangle(x, (getHeight() - 18) / 2.0f, 36, 18, 3.0f);
    g.setColour(juce::Colour(200, 160, 255));
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText("CONT", (int)x, 0, 36, getHeight(), juce::Justification::centred);
    x += 42.0f;
    
    // Preset name (without .container extension)
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(presetFile.getFileNameWithoutExtension(), (int)x, 0, getWidth() - (int)x - 8, getHeight(),
               juce::Justification::centredLeft, true);
}

void ContainerPresetItem::mouseDrag(const juce::MouseEvent& e) {
    if (e.getDistanceFromDragStart() > 5) {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            juce::String dragId = "CONTAINERPRESET:" + presetFile.getFullPathName();
            juce::String displayName = presetFile.getFileNameWithoutExtension();
            
            // Custom drag image with purple theme
            int imgWidth = 180;
            int imgHeight = 28;
            juce::Image dragImage(juce::Image::ARGB, imgWidth, imgHeight, true);
            juce::Graphics g(dragImage);
            
            g.setColour(juce::Colour(0xDD2A1A30));
            g.fillRoundedRectangle(0, 0, (float)imgWidth, (float)imgHeight, 4.0f);
            g.setColour(juce::Colour(200, 160, 255));
            g.drawRoundedRectangle(0.5f, 0.5f, imgWidth - 1.0f, imgHeight - 1.0f, 4.0f, 1.5f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText(displayName, 8, 0, imgWidth - 16, imgHeight, juce::Justification::centredLeft, true);
            
            container->startDragging(dragId, this, juce::ScaledImage(dragImage), true);
        }
    }
}

void ContainerPresetItem::mouseDoubleClick(const juce::MouseEvent&) {
    if (onPresetDoubleClick) onPresetDoubleClick(presetFile);
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
    
    // FX badge for plugins (OnStage: effects only, no instruments)
    if (!isSystemTool) {
        g.setColour(juce::Colour(0xFF87CEEB));
        g.fillRoundedRectangle(x, (getHeight() - 18) / 2.0f, 28, 18, 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText("FX", (int)x, 0, 28, getHeight(), juce::Justification::centred);
        x += 34.0f;
    }
    
    // Name only (no vendor)
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    juce::String name;
    if (isSystemTool) {
        switch (toolType) {
            case SystemToolType::Connector:          name = "Connector/Amp"; break;
            case SystemToolType::StereoMeter:        name = "Stereo Meter"; break;
            case SystemToolType::Recorder:           name = "Recorder"; break;
            case SystemToolType::TransientSplitter:  name = "Transient Splitter"; break;
            case SystemToolType::Container:          name = "Container"; break;
            case SystemToolType::VST2Plugin:         name = "VST2 Plugin..."; break;
            case SystemToolType::VST3Plugin:         name = "VST3 Plugin..."; break;
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
                        displayName = "Connector/Amp";
                        break;
                    case SystemToolType::StereoMeter: 
                        dragId = "TOOL:StereoMeter"; 
                        displayName = "Stereo Meter";
                        break;
                    case SystemToolType::Recorder:
                        dragId = "TOOL:Recorder"; 
                        displayName = "Recorder";
                        break;
                    case SystemToolType::TransientSplitter:
                        dragId = "TOOL:TransientSplitter"; 
                        displayName = "Transient Splitter";
                        break;
                    case SystemToolType::Container:
                        dragId = "TOOL:Container";
                        displayName = "Container";
                        break;
                    case SystemToolType::VST2Plugin:
                        dragId = "TOOL:VST2Plugin"; 
                        displayName = "VST2 Plugin...";
                        break;
                    case SystemToolType::VST3Plugin:
                        dragId = "TOOL:VST3Plugin"; 
                        displayName = "VST3 Plugin...";
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
    favoriteItems.clear();
    containerPresetItems.clear();
    
    currentGroupByVendor = groupByVendor;
    currentGroupByFolder = groupByFolder;
    currentGroupByFormat = groupByFormat;
    
    int y = 5;
    
    if (!groupByVendor && !groupByFolder && !groupByFormat) {
        // FLAT MODE - Simple A-Z list (already sorted by getFilteredPlugins)
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
                // ==========================================================
                // FIX: Smart folder grouping — skip generic folders like
                // "VST3", "Common Files", etc. Find the actual vendor/product
                // subfolder. Falls back to vendor metadata or "[General]".
                // ==========================================================
                groupKey = getSmartFolderGroup(desc);
            } else if (groupByFormat) {
                groupKey = desc.pluginFormatName;
            }
            
            groups[groupKey].add(desc);
        }
        
        // ============================================================
        // FIX: Sort plugins A-Z within each group
        // ============================================================
        for (auto& [key, groupPlugins] : groups) {
            std::sort(groupPlugins.begin(), groupPlugins.end(),
                [](const juce::PluginDescription& a, const juce::PluginDescription& b) {
                    return a.name.compareIgnoreCase(b.name) < 0;
                });
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
    favoriteItems.clear();
    containerPresetItems.clear();
    
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
    
    auto* r = items.add(new PluginBrowserItem(SystemToolType::Recorder));
    r->setBounds(0, y, getWidth(), 32);
    r->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(r);
    y += 34;
    
    auto* ts = items.add(new PluginBrowserItem(SystemToolType::TransientSplitter));
    ts->setBounds(0, y, getWidth(), 32);
    ts->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(ts);
    y += 34;
    
    auto* cont = items.add(new PluginBrowserItem(SystemToolType::Container));
    cont->setBounds(0, y, getWidth(), 32);
    cont->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(cont);
    y += 34;
    
    auto* v2 = items.add(new PluginBrowserItem(SystemToolType::VST2Plugin));
    v2->setBounds(0, y, getWidth(), 32);
    v2->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(v2);
    y += 34;
    
    auto* v3 = items.add(new PluginBrowserItem(SystemToolType::VST3Plugin));
    v3->setBounds(0, y, getWidth(), 32);
    v3->onToolDoubleClick = onToolDoubleClick;
    addAndMakeVisible(v3);
    y += 34;
    
    setSize(getWidth(), y + 10);
}

void PluginBrowserList::setFavorites(const juce::Array<juce::File>& patchFiles) {
    items.clear();
    headers.clear();
    flatHeaders.clear();
    groupedPlugins.clear();
    favoriteItems.clear();
    containerPresetItems.clear();
    removeAllChildren();
    
    int y = 5;
    
    if (patchFiles.isEmpty()) {
        auto* h = flatHeaders.add(new juce::Label());
        h->setText("No .ons patches found", juce::dontSendNotification);
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

void PluginBrowserList::setContainerPresets(const juce::Array<juce::File>& presetFiles) {
    items.clear();
    headers.clear();
    flatHeaders.clear();
    groupedPlugins.clear();
    favoriteItems.clear();
    containerPresetItems.clear();
    removeAllChildren();
    
    int y = 5;
    
    if (presetFiles.isEmpty()) {
        auto* h = flatHeaders.add(new juce::Label());
        h->setText("No .onsc presets found", juce::dontSendNotification);
        h->setFont(juce::Font(juce::FontOptions(12.0f)));
        h->setColour(juce::Label::textColourId, juce::Colours::grey);
        h->setBounds(5, y, getWidth() - 10, 24);
        addAndMakeVisible(h);
        y += 26;
        
        auto* hint = flatHeaders.add(new juce::Label());
        hint->setText("Save containers via right-click > Save Container Preset", juce::dontSendNotification);
        hint->setFont(juce::Font(juce::FontOptions(10.0f)));
        hint->setColour(juce::Label::textColourId, juce::Colour(120, 120, 120));
        hint->setBounds(5, y, getWidth() - 10, 20);
        addAndMakeVisible(hint);
        y += 22;
    } else {
        auto* h = flatHeaders.add(new juce::Label());
        h->setText("Container Presets", juce::dontSendNotification);
        h->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        h->setColour(juce::Label::textColourId, juce::Colour(200, 160, 255));
        h->setBounds(5, y, getWidth() - 10, 24);
        addAndMakeVisible(h);
        y += 26;
        
        for (const auto& file : presetFiles) {
            auto* item = containerPresetItems.add(new ContainerPresetItem(file));
            item->setBounds(0, y, getWidth(), 36);
            item->onPresetDoubleClick = onContainerPresetDoubleClick;
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
    // Mode selector buttons: Add Plugins | Favorites | Containers
    addPluginsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::cyan.darker());
    addPluginsBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addPluginsBtn.addListener(this);
    addAndMakeVisible(addPluginsBtn);
    
    favoritesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    favoritesBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    favoritesBtn.addListener(this);
    addAndMakeVisible(favoritesBtn);
    
    containersBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    containersBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    containersBtn.addListener(this);
    addAndMakeVisible(containersBtn);
    
    // Search box
    searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colours::grey);
    searchBox.addListener(this);
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A2A));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF4A4A4A));
    addAndMakeVisible(searchBox);
    
    // Type filter buttons (FX | Tools) — OnStage: no instruments, no "All"
    effectsBtn.setButtonText("FX");
    toolsBtn.setButtonText("Tools");
    
    for (auto* btn : { &effectsBtn, &toolsBtn }) {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        btn->addListener(this);
        addAndMakeVisible(btn);
    }
    effectsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::skyblue.darker());
    
    // View mode buttons (Flat | Vendor | Folder | Format)
    flatBtn.setButtonText("A-Z");
    vendorBtn.setButtonText("Vendor");
    folderBtn.setButtonText("Folder");
    formatBtn.setButtonText("Format");
    
    for (auto* btn : { &flatBtn, &vendorBtn, &folderBtn, &formatBtn }) {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        btn->addListener(this);
        addAndMakeVisible(btn);
    }
    flatBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4A4A4A));
    
    // Plugin count label
    countLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    countLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    countLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(countLabel);
    
    // Favorites folder controls (hidden unless in favorites mode)
    setFavFolderBtn.setButtonText("Set Folder");
    setFavFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A3A3A));
    setFavFolderBtn.onClick = [this]() { selectFavoritesFolder(); };
    addChildComponent(setFavFolderBtn);
    
    favFolderLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    favFolderLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    favFolderLabel.setJustificationType(juce::Justification::centredLeft);
    addChildComponent(favFolderLabel);
    
    // Plugin list in viewport
    pluginList = std::make_unique<PluginBrowserList>();
    pluginList->onPluginDoubleClick = [this](const juce::PluginDescription& desc) {
        // Double-click adds to graph center
        if (onPluginDropped)
            onPluginDropped(desc, juce::Point<int>(400, 300));
    };
    pluginList->onToolDoubleClick = [this](SystemToolType type) {
        if (onToolDropped)
            onToolDropped(type, juce::Point<int>(400, 300));
    };
    pluginList->onPatchDoubleClick = [this](const juce::File& file) {
        showWorkspaceSelector(file);
    };
    pluginList->onContainerPresetDoubleClick = [this](const juce::File& file) {
        if (onContainerPresetLoad)
            onContainerPresetLoad(file, juce::Point<int>(400, 300));
    };
    
    viewport.setViewedComponent(pluginList.get(), false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    
    // Listen for plugin list changes
    processor.knownPluginList.addChangeListener(this);
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
    auto area = getLocalBounds().reduced(6);
    
    // Mode selector row: [Add Plugins] [Favorites] [Containers]
    auto modeRow = area.removeFromTop(24);
    int tabW = modeRow.getWidth() / 3;
    addPluginsBtn.setBounds(modeRow.removeFromLeft(tabW - 1));
    modeRow.removeFromLeft(2);
    favoritesBtn.setBounds(modeRow.removeFromLeft(tabW - 1));
    modeRow.removeFromLeft(2);
    containersBtn.setBounds(modeRow);
    area.removeFromTop(6);
    
    if (favoritesMode) {
        // Favorites folder controls
        auto folderRow = area.removeFromTop(22);
        setFavFolderBtn.setBounds(folderRow.removeFromLeft(70));
        folderRow.removeFromLeft(6);
        favFolderLabel.setBounds(folderRow);
        area.removeFromTop(4);
        
        // Update folder label
        auto folder = getFavoritesFolder();
        if (folder.exists())
            favFolderLabel.setText(folder.getFileName(), juce::dontSendNotification);
        else
            favFolderLabel.setText("(not set)", juce::dontSendNotification);
    }
    
    // Search box
    searchBox.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    
    // Type filter row (only in plugin mode)
    if (!favoritesMode && !containersMode) {
        auto typeRow = area.removeFromTop(22);
        int btnW = typeRow.getWidth() / 2;
        effectsBtn.setBounds(typeRow.removeFromLeft(btnW));
        toolsBtn.setBounds(typeRow);
        area.removeFromTop(4);
        
        // View mode row
        auto viewRow = area.removeFromTop(22);
        btnW = viewRow.getWidth() / 4;
        flatBtn.setBounds(viewRow.removeFromLeft(btnW));
        vendorBtn.setBounds(viewRow.removeFromLeft(btnW));
        folderBtn.setBounds(viewRow.removeFromLeft(btnW));
        formatBtn.setBounds(viewRow);
        area.removeFromTop(4);
    }
    
    // Count label
    countLabel.setBounds(area.removeFromBottom(16));
    area.removeFromBottom(2);
    
    // Plugin list viewport
    viewport.setBounds(area);
    
    // Resize plugin list to fit viewport width
    if (pluginList) {
        pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
    }
    
    // Show/hide favorites controls
    setFavFolderBtn.setVisible(favoritesMode);
    favFolderLabel.setVisible(favoritesMode);
    
    // Show/hide type/view buttons (hidden in favorites and containers modes)
    bool showPluginControls = !favoritesMode && !containersMode;
    effectsBtn.setVisible(showPluginControls);
    toolsBtn.setVisible(showPluginControls);
    flatBtn.setVisible(showPluginControls);
    vendorBtn.setVisible(showPluginControls);
    folderBtn.setVisible(showPluginControls);
    formatBtn.setVisible(showPluginControls);
}

void PluginBrowserPanel::visibilityChanged() {
    if (isVisible()) { refresh(); searchBox.grabKeyboardFocus(); }
}

bool PluginBrowserPanel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) { searchBox.clear(); applyFilters(); return true; }
    if (!searchBox.hasKeyboardFocus(true)) searchBox.grabKeyboardFocus();
    return false;
}

void PluginBrowserPanel::buttonClicked(juce::Button* btn) {
    if (btn == &addPluginsBtn) {
        favoritesMode = false;
        containersMode = false;
        updateButtons();
        applyFilters();
        resized();
    } else if (btn == &favoritesBtn) {
        favoritesMode = true;
        containersMode = false;
        updateButtons();
        loadFavoritesList();
        resized();
    } else if (btn == &containersBtn) {
        favoritesMode = false;
        containersMode = true;
        updateButtons();
        loadContainerPresetsList();
        resized();
    } else if (btn == &effectsBtn) {
        typeFilter = TypeFilter::Effects;
        updateButtons();
        applyFilters();
    } else if (btn == &toolsBtn) {
        typeFilter = TypeFilter::Tools;
        updateButtons();
        applyFilters();
    } else if (btn == &flatBtn) {
        viewMode = ViewMode::Flat;
        updateButtons();
        applyFilters();
    } else if (btn == &vendorBtn) {
        viewMode = ViewMode::ByVendor;
        updateButtons();
        applyFilters();
    } else if (btn == &folderBtn) {
        viewMode = ViewMode::ByFolder;
        updateButtons();
        applyFilters();
    } else if (btn == &formatBtn) {
        viewMode = ViewMode::ByFormat;
        updateButtons();
        applyFilters();
    }
    
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
    viewport.setViewPosition(0, 0);
}

void PluginBrowserPanel::textEditorTextChanged(juce::TextEditor&) {
    searchText = searchBox.getText();
    if (favoritesMode)
        loadFavoritesList();
    else if (containersMode)
        loadContainerPresetsList();
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
    else if (containersMode)
        loadContainerPresetsList();
    else
        applyFilters();
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
}

void PluginBrowserPanel::updateButtons() {
    // Mode selector highlights
    addPluginsBtn.setColour(juce::TextButton::buttonColourId, (!favoritesMode && !containersMode) ? juce::Colours::cyan.darker() : juce::Colour(0xFF2A2A2A));
    favoritesBtn.setColour(juce::TextButton::buttonColourId, favoritesMode ? juce::Colour(0xFFB8860B) : juce::Colour(0xFF2A2A2A));
    containersBtn.setColour(juce::TextButton::buttonColourId, containersMode ? juce::Colour(80, 40, 100) : juce::Colour(0xFF2A2A2A));
    containersBtn.setColour(juce::TextButton::textColourOffId, containersMode ? juce::Colour(200, 160, 255) : juce::Colours::lightgrey);
    
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
        countLabel.setText(juce::String(pluginList->getNumItems()) + " tools", juce::dontSendNotification);
        return;
    }
    
    auto filtered = getFilteredPlugins();
    pluginList->setPlugins(filtered,
        viewMode == ViewMode::ByVendor, viewMode == ViewMode::ByFolder, viewMode == ViewMode::ByFormat);
    countLabel.setText(juce::String(filtered.size()) + " plugin" + (filtered.size() != 1 ? "s" : ""), juce::dontSendNotification);
}

// NEW: Filters out hidden plugins (eye toggle in Plugin Manager tab)
// FIX: Sort results A-Z by name so flat mode and group contents are alphabetical
juce::Array<juce::PluginDescription> PluginBrowserPanel::getFilteredPlugins() {
    auto all = processor.knownPluginList.getTypes();
    juce::Array<juce::PluginDescription> result;
    
    auto* userSettings = processor.appProperties.getUserSettings();
    
    for (const auto& p : all) {
        // OnStage: Always skip instruments — effects only
        if (p.isInstrument) continue;
        
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
    
    // ================================================================
    // FIX: Sort A-Z by plugin name
    // Without this, flat mode shows plugins in scan order (random)
    // ================================================================
    std::sort(result.begin(), result.end(),
        [](const juce::PluginDescription& a, const juce::PluginDescription& b) {
            return a.name.compareIgnoreCase(b.name) < 0;
        });
    
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
        auto files = folder.findChildFiles(juce::File::findFiles, true, "*.ons");
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

void PluginBrowserPanel::loadContainerPresetsList() {
    auto folder = ContainerProcessor::getEffectiveDefaultFolder();
    juce::Array<juce::File> presets;
    
    if (folder.isDirectory()) {
        auto files = folder.findChildFiles(juce::File::findFiles, true, "*.onsc");
        files.sort();
        
        for (const auto& f : files) {
            if (searchText.isNotEmpty()) {
                if (!f.getFileNameWithoutExtension().containsIgnoreCase(searchText))
                    continue;
            }
            presets.add(f);
        }
    }
    
    pluginList->setContainerPresets(presets);
    countLabel.setText(juce::String(presets.size()) + " preset" + (presets.size() != 1 ? "s" : ""), juce::dontSendNotification);
    pluginList->setSize(viewport.getWidth() - viewport.getScrollBarThickness(), pluginList->getHeight());
    viewport.setViewPosition(0, 0);
}

void PluginBrowserPanel::showWorkspaceSelector(const juce::File& patchFile) {
    // Load patch directly
    processor.loadUserPreset(patchFile);
    
    if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getTopLevelComponent()))
        editor->resized();
}
