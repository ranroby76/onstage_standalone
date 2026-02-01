// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_Main.cpp
// Main Plugin Manager Tab implementation
// FIX: Added "Rescan Existing" button for plugin version updates
// FIX: Folder dialog now closes properly with X button
// FIX: checkForCrashedScan included
// FIX: Rescan status label for visual feedback
// FIX: Dark orange-purple button color

#include "PluginManagerTab.h"
#include "BackgroundPluginScanner.h"

// =============================================================================
// PluginManagerTab Constructor
// =============================================================================
PluginManagerTab::PluginManagerTab(SubterraneumAudioProcessor& p)
    : processor(p)
{
    addAndMakeVisible(sortLabel);
    addAndMakeVisible(sortCombo);
    sortCombo.addItem("Type", 1);
    sortCombo.addItem("Vendor", 2);
    sortCombo.setSelectedId(processor.sortPluginsByVendor ? 2 : 1, juce::dontSendNotification);
    sortCombo.addListener(this);

    instBtn.setRadioGroupId(101);
    effectBtn.setRadioGroupId(101);
    allBtn.setRadioGroupId(101);
    allBtn.setToggleState(true, juce::dontSendNotification);
    instBtn.addListener(this);
    effectBtn.addListener(this);
    allBtn.addListener(this);
    addAndMakeVisible(instBtn);
    addAndMakeVisible(effectBtn);
    addAndMakeVisible(allBtn);
    
    expandAllBtn.setTooltip("Expand All");
    expandAllBtn.addListener(this);
    addAndMakeVisible(expandAllBtn);
    
    collapseAllBtn.setTooltip("Collapse All");
    collapseAllBtn.addListener(this);
    addAndMakeVisible(collapseAllBtn);

    pluginTree.setLookAndFeel(&treeLAF);
    pluginTree.setDefaultOpenness(false);
    pluginTree.setMultiSelectEnabled(false);
    pluginTree.setRootItemVisible(false);
    addAndMakeVisible(pluginTree);
    
    addAndMakeVisible(tableHeader);
    tableHeader.addColumn("Plugin Name", 1, 250);
    tableHeader.addColumn("Vendor", 2, 150);
    tableHeader.addColumn("Format", 3, 80);
    tableHeader.addColumn("Category", 4, 100);
    tableHeader.addColumn("Version", 5, 80);
    tableHeader.setVisible(!processor.sortPluginsByVendor);

    // Info panel
    infoPanel.setMultiLine(true);
    infoPanel.setReadOnly(true);
    infoPanel.setScrollbarsShown(true);
    infoPanel.setCaretVisible(false);
    infoPanel.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    infoPanel.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
    infoPanel.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
    infoPanel.setFont(juce::Font(14.0f));
    
    juce::String infoText = 
        "PLUGIN MANAGER\n"
        "=============================================\n\n"
        "SCANNING FOR PLUGINS\n"
        "---------------------------------------------\n"
        "1. Click 'Scan Plugins' button\n"
        "2. Wait for scan to complete\n"
        "3. Plugins appear in the list when done\n\n"
        "RESCAN EXISTING\n"
        "---------------------------------------------\n"
        "Use when you've updated a plugin:\n"
        "1. Click 'Rescan Existing' button\n"
        "2. Plugins are re-scanned by file path\n"
        "3. Duplicates are removed automatically\n\n"
        "CRASH DURING SCAN?\n"
        "---------------------------------------------\n"
        "If Colosseum crashes during a plugin scan:\n"
        "1. Restart Colosseum\n"
        "2. You'll be prompted to blacklist the\n"
        "   problematic plugin\n"
        "3. Click 'Yes' to blacklist it\n"
        "4. Run the scan again\n"
        "5. The crashed plugin will be skipped\n\n"
        "PLUGIN FOLDERS\n"
        "---------------------------------------------\n"
        "Click 'Plugin Folders...' to configure:\n"
        "- VST3 search paths\n"
        "- Use 'Add Defaults' for standard locations\n\n"
        "FILTERS\n"
        "---------------------------------------------\n"
        "- INSTRUMENTS: Show only VSTi plugins\n"
        "- EFFECTS: Show only effect plugins\n"
        "- ALL: Show everything\n"
        "- Sort By: Organize by Type or Vendor\n\n"
        "USING PLUGINS\n"
        "---------------------------------------------\n"
        "1. Go to the Rack tab\n"
        "2. Use the Add Plugins panel on the right\n"
        "3. Drag a plugin onto the rack\n"
        "4. Connect it with audio/MIDI cables\n\n"
        "RESET BLACKLIST\n"
        "---------------------------------------------\n"
        "Use if you've fixed a previously problematic\n"
        "plugin or want to try scanning it again.\n";
    
    infoPanel.setText(infoText);
    addAndMakeVisible(infoPanel);
    
    // Buttons
    scanBtn.addListener(this);
    scanBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    addAndMakeVisible(scanBtn);
    
    // Rescan Existing button - dark orange-purple
    rescanExistingBtn.addListener(this);
    rescanExistingBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF6B3FA0));
    rescanExistingBtn.setTooltip("Re-scan plugins already in list to detect version updates");
    addAndMakeVisible(rescanExistingBtn);
    
    // Rescan status label (hidden by default)
    rescanStatusLabel.setText("", juce::dontSendNotification);
    rescanStatusLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    rescanStatusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    rescanStatusLabel.setJustificationType(juce::Justification::centred);
    rescanStatusLabel.setVisible(false);
    addAndMakeVisible(rescanStatusLabel);
    
    foldersBtn.addListener(this);
    addAndMakeVisible(foldersBtn);
    
    clearBtn.addListener(this);
    clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    addAndMakeVisible(clearBtn);
    
    resetBlacklistBtn.addListener(this);
    addAndMakeVisible(resetBlacklistBtn);
    
    processor.knownPluginList.addChangeListener(this);
    
    buildTree();
    checkForCrashedScan();
    
    // Auto-scan on first run if no plugins found
    if (processor.knownPluginList.getNumTypes() == 0) {
        startTimer(500);
    }
}

PluginManagerTab::~PluginManagerTab() {
    stopTimer();
    processor.knownPluginList.removeChangeListener(this);
    pluginTree.setLookAndFeel(nullptr);
}

void PluginManagerTab::timerCallback() {
    stopTimer();
    showScanDialog();
}

void PluginManagerTab::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground);
}

void PluginManagerTab::resized() {
    auto area = getLocalBounds().reduced(10);
    
    // Top row - filters and sort
    auto topRow = area.removeFromTop(30);
    sortLabel.setBounds(topRow.removeFromLeft(60));
    sortCombo.setBounds(topRow.removeFromLeft(100));
    topRow.removeFromLeft(20);
    instBtn.setBounds(topRow.removeFromLeft(110));
    effectBtn.setBounds(topRow.removeFromLeft(80));
    allBtn.setBounds(topRow.removeFromLeft(50));
    topRow.removeFromLeft(20);
    expandAllBtn.setBounds(topRow.removeFromLeft(30));
    collapseAllBtn.setBounds(topRow.removeFromLeft(30));
    
    area.removeFromTop(10);
    
    // Bottom row - buttons
    auto bottomRow = area.removeFromBottom(35);
    scanBtn.setBounds(bottomRow.removeFromLeft(110));
    bottomRow.removeFromLeft(8);
    rescanExistingBtn.setBounds(bottomRow.removeFromLeft(120));
    bottomRow.removeFromLeft(8);
    foldersBtn.setBounds(bottomRow.removeFromLeft(120));
    bottomRow.removeFromLeft(8);
    clearBtn.setBounds(bottomRow.removeFromLeft(90));
    bottomRow.removeFromLeft(8);
    resetBlacklistBtn.setBounds(bottomRow.removeFromLeft(110));
    
    // Center the rescan status label in remaining space
    if (bottomRow.getWidth() > 0)
        rescanStatusLabel.setBounds(bottomRow);
    
    area.removeFromBottom(10);
    
    // Right side - info panel
    auto rightPanel = area.removeFromRight(350);
    infoPanel.setBounds(rightPanel);
    
    area.removeFromRight(10);
    
    // Table header (only in Type view)
    if (!processor.sortPluginsByVendor) {
        tableHeader.setBounds(area.removeFromTop(25));
    }
    
    // Plugin tree
    pluginTree.setBounds(area);
}

void PluginManagerTab::comboBoxChanged(juce::ComboBox* cb) {
    if (cb == &sortCombo) {
        processor.sortPluginsByVendor = (sortCombo.getSelectedId() == 2);
        tableHeader.setVisible(!processor.sortPluginsByVendor);
        buildTree();
        resized();
    }
}

void PluginManagerTab::buttonClicked(juce::Button* b) {
    if (b == &instBtn || b == &effectBtn || b == &allBtn) {
        buildTree();
    } else if (b == &expandAllBtn) {
        expandAllItems();
    } else if (b == &collapseAllBtn) {
        collapseAllItems();
    } else if (b == &scanBtn) {
        showScanDialog();
    } else if (b == &rescanExistingBtn) {
        rescanExistingPlugins();
    } else if (b == &foldersBtn) {
        showFoldersDialog();
    } else if (b == &resetBlacklistBtn) {
        processor.knownPluginList.clearBlacklistedFiles();
        if (auto* userSettings = processor.appProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml())
                userSettings->setValue("KnownPlugins", xml.get());
            userSettings->saveIfNeeded();
        }
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Blacklist Reset", "Blacklisted plugins cleared. Please rescan.");
    } else if (b == &clearBtn) {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Clear Plugin List",
            "Remove all plugins from the list?\n\nYou can scan again to re-add them.",
            "Clear",
            "Cancel",
            nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1) {
                    processor.knownPluginList.clear();
                    if (auto* userSettings = processor.appProperties.getUserSettings()) {
                        userSettings->removeValue("KnownPlugins");
                        userSettings->saveIfNeeded();
                    }
                    buildTree();
                }
            }));
    }
}

void PluginManagerTab::changeListenerCallback(juce::ChangeBroadcaster*) {
    buildTree();
}

// =============================================================================
// Rescan Existing Plugins - with visual feedback
// =============================================================================
void PluginManagerTab::rescanExistingPlugins() {
    auto types = processor.knownPluginList.getTypes();
    
    if (types.isEmpty()) {
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "No Plugins",
            "No plugins in list to rescan. Use 'Scan Plugins' first.");
        return;
    }
    
    // Show status label
    rescanStatusLabel.setText("Rescanning plugins...", juce::dontSendNotification);
    rescanStatusLabel.setVisible(true);
    rescanExistingBtn.setEnabled(false);
    repaint();
    
    // Use callAsync so the UI updates before the blocking scan
    juce::MessageManager::callAsync([this]() {
        auto types = processor.knownPluginList.getTypes();
        
        // Collect unique file paths
        juce::StringArray filePaths;
        for (const auto& desc : types) {
            if (!filePaths.contains(desc.fileOrIdentifier)) {
                filePaths.add(desc.fileOrIdentifier);
            }
        }
        
        // Clear the list
        processor.knownPluginList.clear();
        
        // Re-scan each file path
        juce::File deadMansPedal = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                       .getChildFile("Colosseum")
                                       .getChildFile("RescanDeadMan.txt");
        
        for (const auto& path : filePaths) {
            juce::File pluginFile(path);
            if (!pluginFile.exists()) continue;
            
            deadMansPedal.replaceWithText(path);
            
            for (int i = 0; i < processor.formatManager.getNumFormats(); ++i) {
                auto* format = processor.formatManager.getFormat(i);
                if (format->fileMightContainThisPluginType(path)) {
                    juce::OwnedArray<juce::PluginDescription> results;
                    format->findAllTypesForFile(results, path);
                    
                    for (auto* desc : results) {
                        processor.knownPluginList.addType(*desc);
                    }
                    break;
                }
            }
        }
        
        deadMansPedal.deleteFile();
        
        // Save the updated list
        if (auto* userSettings = processor.appProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml()) {
                userSettings->setValue("KnownPlugins", xml.get());
                userSettings->saveIfNeeded();
            }
        }
        
        buildTree();
        
        // Hide status label
        rescanStatusLabel.setVisible(false);
        rescanExistingBtn.setEnabled(true);
        
        int newCount = processor.knownPluginList.getNumTypes();
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Rescan Complete",
            "Rescanned " + juce::String(filePaths.size()) + " plugin files.\n" +
            "Found " + juce::String(newCount) + " plugins.");
    });
}

// =============================================================================
// Build Tree View
// =============================================================================
void PluginManagerTab::buildTree() {
    pluginTree.setRootItem(nullptr);
    
    auto types = processor.knownPluginList.getTypes();
    
    juce::Array<juce::PluginDescription> filtered;
    for (const auto& p : types) {
        if (instBtn.getToggleState() && !p.isInstrument) continue;
        if (effectBtn.getToggleState() && p.isInstrument) continue;
        filtered.add(p);
    }
    
    std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    
    auto* root = new PluginTreeItem("Root", processor.sortPluginsByVendor);
    
    if (processor.sortPluginsByVendor) {
        std::map<juce::String, juce::Array<juce::PluginDescription>> byVendor;
        for (const auto& p : filtered) {
            juce::String vendor = p.manufacturerName.isEmpty() ? "Unknown" : p.manufacturerName;
            byVendor[vendor].add(p);
        }
        
        for (auto& pair : byVendor) {
            auto* vendorItem = new PluginTreeItem(pair.first + " (" + juce::String(pair.second.size()) + ")", 
                                                   true, true);
            vendorItem->parentTab = this;
            
            std::sort(pair.second.begin(), pair.second.end(), 
                [](const auto& a, const auto& b) { return a.name.compareIgnoreCase(b.name) < 0; });
            
            for (const auto& p : pair.second) {
                auto* item = new PluginTreeItem(p.name, p, true);
                item->parentTab = this;
                vendorItem->addSubItem(item);
            }
            root->addSubItem(vendorItem);
        }
    } else {
        auto* instrFolder = new PluginTreeItem("Instruments", false);
        auto* effectFolder = new PluginTreeItem("Effects", false);
        instrFolder->parentTab = this;
        effectFolder->parentTab = this;
        
        for (const auto& p : filtered) {
            auto* item = new PluginTreeItem(p.name, p, false);
            item->parentTab = this;
            if (p.isInstrument)
                instrFolder->addSubItem(item);
            else
                effectFolder->addSubItem(item);
        }
        
        if (instrFolder->getNumSubItems() > 0) root->addSubItem(instrFolder);
        else delete instrFolder;
        
        if (effectFolder->getNumSubItems() > 0) root->addSubItem(effectFolder);
        else delete effectFolder;
    }
    
    pluginTree.setRootItem(root);
}

void PluginManagerTab::showScanDialog() {
    auto* panel = new ScanProgressPanel(processor, [this]() {
        if (scanDialog) scanDialog->setVisible(false);
        buildTree();
    });
    
    scanDialog = std::make_unique<juce::DialogWindow>(
        "Scan VST3 Plugins", 
        juce::Colours::darkgrey, 
        true, 
        true
    );
    scanDialog->setContentOwned(panel, true);
    scanDialog->setUsingNativeTitleBar(true);
    scanDialog->setResizable(false, false);
    scanDialog->centreWithSize(400, 180);
    scanDialog->setVisible(true);
    
    panel->startScan();
}

// =============================================================================
// FIX: Folder dialog now uses CloseableDialogWindow for proper close handling
// =============================================================================
void PluginManagerTab::showFoldersDialog() {
    auto* panel = new PluginFoldersPanel(processor);
    
    foldersDialog = std::make_unique<CloseableDialogWindow>(
        "Plugin Folders",
        juce::Colours::darkgrey,
        true
    );
    
    panel->onCloseRequest = [this]() {
        if (foldersDialog) {
            foldersDialog->setVisible(false);
        }
    };
    
    foldersDialog->setContentOwned(panel, true);
    foldersDialog->setResizable(true, true);
    foldersDialog->centreWithSize(550, 450);
    foldersDialog->setVisible(true);
}

void PluginManagerTab::expandAllItems() {
    if (auto* root = pluginTree.getRootItem()) {
        for (int i = 0; i < root->getNumSubItems(); ++i) {
            root->getSubItem(i)->setOpen(true);
        }
    }
}

void PluginManagerTab::collapseAllItems() {
    if (auto* root = pluginTree.getRootItem()) {
        for (int i = 0; i < root->getNumSubItems(); ++i) {
            root->getSubItem(i)->setOpen(false);
        }
    }
}

void PluginManagerTab::removePluginFromList(const juce::PluginDescription& desc) {
    processor.knownPluginList.removeType(desc);
    
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPlugins", xml.get());
        userSettings->saveIfNeeded();
    }
    
    buildTree();
}

// =============================================================================
// Check for crashed scan on startup
// =============================================================================
void PluginManagerTab::checkForCrashedScan() {
    juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Colosseum");
    juce::File deadMansPedal = dataDir.getChildFile("PluginScanDeadMan.txt");
    
    if (deadMansPedal.existsAsFile()) {
        juce::String crashedPlugin = deadMansPedal.loadFileAsString().trim();
        
        if (crashedPlugin.isNotEmpty()) {
            auto safeThis = juce::Component::SafePointer<PluginManagerTab>(this);
            
            juce::MessageManager::callAsync([safeThis, crashedPlugin, deadMansPedal]() {
                if (safeThis == nullptr) return;
                
                auto* alertWindow = new juce::AlertWindow(
                    "Plugin Scan Crash Detected",
                    "The last scan crashed while loading:\n\n" + crashedPlugin + 
                    "\n\nWhat would you like to do?",
                    juce::MessageBoxIconType::WarningIcon);
                
                alertWindow->addButton("Blacklist Plugin", 1);
                alertWindow->addButton("Allow Anyway", 2);
                alertWindow->addButton("Try Again", 3);
                alertWindow->addButton("Ignore", 0);
                
                alertWindow->enterModalState(true,
                    juce::ModalCallbackFunction::create([safeThis, crashedPlugin, deadMansPedal, alertWindow](int result) {
                        if (safeThis == nullptr) { delete alertWindow; return; }
                        
                        switch (result) {
                            case 1:  // Blacklist
                            {
                                safeThis->processor.knownPluginList.addToBlacklist(crashedPlugin);
                                deadMansPedal.deleteFile();
                                
                                if (auto* settings = safeThis->processor.appProperties.getUserSettings()) {
                                    if (auto xml = safeThis->processor.knownPluginList.createXml())
                                        settings->setValue("KnownPlugins", xml.get());
                                    settings->saveIfNeeded();
                                }
                                
                                juce::NativeMessageBox::showMessageBoxAsync(
                                    juce::MessageBoxIconType::InfoIcon,
                                    "Plugin Blacklisted",
                                    "The plugin has been blacklisted and will be skipped in future scans.");
                                break;
                            }
                            
                            case 2:  // Allow Anyway
                            {
                                deadMansPedal.deleteFile();
                                
                                auto* settings = safeThis->processor.appProperties.getUserSettings();
                                if (settings) {
                                    juce::String approved = settings->getValue("ApprovedPlugins", "");
                                    if (approved.isNotEmpty()) approved += "|";
                                    approved += crashedPlugin;
                                    settings->setValue("ApprovedPlugins", approved);
                                    settings->saveIfNeeded();
                                }
                                
                                juce::NativeMessageBox::showMessageBoxAsync(
                                    juce::MessageBoxIconType::InfoIcon,
                                    "Plugin Approved",
                                    "The plugin has been added to the approved list.");
                                break;
                            }
                            
                            case 3:  // Try Again
                                deadMansPedal.deleteFile();
                                break;
                            
                            case 0:  // Ignore
                            default:
                                break;
                        }
                        
                        delete alertWindow;
                    }), true);
            });
        } else {
            deadMansPedal.deleteFile();
        }
    }
}

// =============================================================================
// PluginTreeItem Implementation
// =============================================================================
juce::String PluginManagerTab::PluginTreeItem::getFormatBadge() const {
    if (!isPlugin) return "";
    return SubterraneumAudioProcessor::getShortFormatName(description.pluginFormatName);
}

juce::Colour PluginManagerTab::PluginTreeItem::getFormatColor() const {
    if (!isPlugin) return juce::Colours::grey;
    return SubterraneumAudioProcessor::getFormatColor(description.pluginFormatName);
}

void PluginManagerTab::PluginTreeItem::paintItem(juce::Graphics& g, int w, int h) {
    if (!isPlugin) { 
        g.setColour(juce::Colours::grey);
        g.drawRect(0, 0, w, h, 1); 
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(name, 25, 0, w-25, h, juce::Justification::centredLeft);
    } else { 
        g.setColour(isSelected() ? juce::Colours::blue.withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillRect(0, 0, w, h);
        
        juce::String formatBadge = getFormatBadge();
        juce::Colour formatColor = getFormatColor();
        
        int badgeWidth = 40;
        int badgeHeight = h - 4;
        int badgeX = 5;
        int badgeY = 2;
        
        g.setColour(formatColor);
        g.fillRoundedRectangle((float)badgeX, (float)badgeY, (float)badgeWidth, (float)badgeHeight, 3.0f);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(formatBadge, badgeX, badgeY, badgeWidth, badgeHeight, juce::Justification::centred);
        
        int deleteBtnWidth = 20;
        int deleteBtnHeight = h - 6;
        int deleteBtnX = w - deleteBtnWidth - 5;
        int deleteBtnY = 3;
        
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle((float)deleteBtnX, (float)deleteBtnY, (float)deleteBtnWidth, (float)deleteBtnHeight, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("X", deleteBtnX, deleteBtnY, deleteBtnWidth, deleteBtnHeight, juce::Justification::centred);
        
        g.setColour(juce::Colours::lightgrey);
        g.setFont(14.0f);
        int textStartX = badgeX + badgeWidth + 8;
        int textEndX = deleteBtnX - 8;
        
        if (showColumns) {
            const int COL_NAME = 200, COL_VENDOR = 150, COL_CAT = 100, COL_VER = 80;
            int x = textStartX;
            
            g.drawText(name, x, 0, COL_NAME - 5, h, juce::Justification::centredLeft);
            x += COL_NAME;
            
            g.setColour(juce::Colours::grey);
            g.drawText(description.manufacturerName, x, 0, COL_VENDOR - 5, h, juce::Justification::centredLeft);
            x += COL_VENDOR;
            
            g.drawText(description.category, x, 0, COL_CAT - 5, h, juce::Justification::centredLeft);
            x += COL_CAT;
            
            g.drawText(description.version, x, 0, COL_VER - 5, h, juce::Justification::centredLeft);
        } else {
            juce::String displayText = name;
            if (description.manufacturerName.isNotEmpty()) {
                displayText += " (" + description.manufacturerName + ")";
            }
            g.drawText(displayText, textStartX, 0, textEndX - textStartX, h, juce::Justification::centredLeft);
        }
    }
}

void PluginManagerTab::PluginTreeItem::itemClicked(const juce::MouseEvent& e) {
    if (!isPlugin) {
        setOpen(!isOpen());
        return;
    }
    
    auto itemBounds = getItemPosition(true);
    int w = itemBounds.getWidth();
    
    int deleteBtnWidth = 20;
    int deleteBtnX = w - deleteBtnWidth - 5;
    
    auto localPos = e.getEventRelativeTo(getOwnerView()).getPosition();
    localPos.x -= itemBounds.getX();
    
    if (localPos.x >= deleteBtnX && localPos.x <= deleteBtnX + deleteBtnWidth) {
        if (parentTab != nullptr) {
            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Remove Plugin",
                "Remove \"" + name + "\" from the plugin list?\n\nThe plugin file will NOT be deleted.",
                "Remove",
                "Cancel",
                nullptr,
                juce::ModalCallbackFunction::create([this](int result) {
                    if (result == 1 && parentTab != nullptr) {
                        parentTab->removePluginFromList(description);
                    }
                }));
        }
    }
}

juce::var PluginManagerTab::PluginTreeItem::getDragSourceDescription() {
    if (!isPlugin) return {};
    
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("pluginName", description.name);
    obj->setProperty("pluginIdentifier", description.fileOrIdentifier);
    obj->setProperty("pluginFormat", description.pluginFormatName);
    obj->setProperty("uniqueId", description.uniqueId);
    obj->setProperty("isInstrument", description.isInstrument);
    
    return juce::var(obj.get());
}
