// #D:\Workspace\Subterraneum_plugins_daw\src\PluginManagerTab_Main.cpp
// Plugin Manager Tab — Main UI, tree view, plugin info, scan buttons
// FIX: buildTree() preserves expand/collapse state
// NEW: Eye toggle - hide/show plugins from Add menus
// NEW: "Scan All Plugins" — full disk scan (VST2+VST3+AU) via OOP scanner
// NEW: "Rescan Existing" — only re-scans registered plugin paths (fast)
// Main Plugin Manager Tab implementation
// FIX: Added "Rescan Existing" button for plugin version updates
// FIX: Folder dialog now closes properly with X button
// FIX: checkForCrashedScan — no more blacklist prompts, just cleanup
// FIX: Rescan status label for visual feedback
// FIX: Dark orange-purple button color
// FIX: X button click detection - use e.x directly (already in item-local coordinates)
// FIX: buildTree() preserves expand/collapse state across rebuilds
// FIX: Darker folder/vendor header rectangles
// NEW: Eye toggle - hide/show plugins from Add menus (persisted to settings)
// NEW: rescanExistingPlugins uses safe OOP scanner (no more message-thread freezes)
// FIX: scanDialog onCloseRequest → cancelScan(), dialog height 270 for cancel button

#include "PluginManagerTab.h"
#include "BackgroundPluginScanner.h"
#include "OutOfProcessScanner.h"

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

    // OnStage: Effects-only mode — no instrument/all toggles needed
    // effectBtn is always active (effects-only filter)
    effectBtn.setToggleState(true, juce::dontSendNotification);
    effectBtn.addListener(this);
    addAndMakeVisible(effectBtn);
    
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
        "2. Plugins are scanned in a safe child process\n"
        "3. If a plugin crashes, only the child dies\n"
        "4. Colosseum stays running — zero freezes!\n\n"
        "3-PHASE SAFE SCAN\n"
        "---------------------------------------------\n"
        "Phase 1: Skip already-known plugins\n"
        "Phase 2: Read metadata from JSON files\n"
        "Phase 3: Deep scan via child process\n\n"
        "RESCAN EXISTING\n"
        "---------------------------------------------\n"
        "Use when you've updated a plugin:\n"
        "1. Click 'Rescan Existing' button\n"
        "2. Plugins are safely re-scanned\n"
        "3. Fresh metadata is collected\n\n"
        "PLUGIN FOLDERS\n"
        "---------------------------------------------\n"
        "Click 'Plugin Folders...' to configure:\n"
        "- VST3 search paths\n"
        "- Use 'Add Defaults' for standard locations\n\n"
        "FILTERS\n"
        "---------------------------------------------\n"
        "OnStage shows only effect plugins.\n"
        "Sort By: Organize by Type or Vendor\n\n"
        "VISIBILITY (Eye Icon)\n"
        "---------------------------------------------\n"
        "Click the eye icon next to any plugin to\n"
        "hide/show it in the Add Plugins menus.\n"
        "Hidden plugins stay in your library but\n"
        "won't appear in right-click or browser.\n\n"
        "USING PLUGINS\n"
        "---------------------------------------------\n"
        "1. Go to the Rack tab\n"
        "2. Use the Add Plugins panel on the right\n"
        "3. Drag a plugin onto the rack\n"
        "4. Connect it with audio/MIDI cables\n";
    
    infoPanel.setText(infoText);
    addAndMakeVisible(infoPanel);
    
    // Buttons
    scanBtn.addListener(this);
    scanBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    scanBtn.setTooltip("Clear and rescan ALL plugin folders (VST2, VST3, AU)");
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
    
    showBlacklistBtn.addListener(this);
    showBlacklistBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4A4A60));
    addAndMakeVisible(showBlacklistBtn);
    
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
    // OnStage: Effects-only mode — show "Effects" label only
    effectBtn.setBounds(topRow.removeFromLeft(80));
    topRow.removeFromLeft(20);
    expandAllBtn.setBounds(topRow.removeFromLeft(30));
    collapseAllBtn.setBounds(topRow.removeFromLeft(30));
    
    area.removeFromTop(10);
    
    // Bottom row - buttons
    auto bottomRow = area.removeFromBottom(35);
    scanBtn.setBounds(bottomRow.removeFromLeft(130));
    bottomRow.removeFromLeft(8);
    rescanExistingBtn.setBounds(bottomRow.removeFromLeft(120));
    bottomRow.removeFromLeft(8);
    foldersBtn.setBounds(bottomRow.removeFromLeft(120));
    bottomRow.removeFromLeft(8);
    clearBtn.setBounds(bottomRow.removeFromLeft(90));
    bottomRow.removeFromLeft(8);
    resetBlacklistBtn.setBounds(bottomRow.removeFromLeft(110));
    bottomRow.removeFromLeft(8);
    showBlacklistBtn.setBounds(bottomRow.removeFromLeft(90));
    
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
    if (b == &effectBtn) {
        // OnStage: Effects-only mode — this is just a label, but rebuild if clicked
        buildTree();
    } else if (b == &expandAllBtn) {
        expandAllItems();
    } else if (b == &collapseAllBtn) {
        collapseAllItems();
    } else if (b == &scanBtn) {
        if (scanDialog && scanDialog->isVisible()) return;
        showScanDialog();
    } else if (b == &rescanExistingBtn) {
        if (scanDialog && scanDialog->isVisible()) return;
        rescanExistingPlugins();
    } else if (b == &foldersBtn) {
        showFoldersDialog();
    } else if (b == &resetBlacklistBtn) {
        processor.knownPluginList.clearBlacklistedFiles();
        if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
            if (auto xml = processor.knownPluginList.createXml())
                userSettings->setValue("KnownPluginsV2", xml.get());
            userSettings->saveIfNeeded();
        }
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Blacklist Reset", "Blacklisted plugins cleared. Please rescan.");
    } else if (b == &showBlacklistBtn) {
        showBlacklistDialog();
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
                    if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
                        userSettings->removeValue("KnownPluginsV2");
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
// FIX: Rescan Existing — uses safe OOP scanner instead of blocking findAllTypesForFile
// =============================================================================
void PluginManagerTab::rescanExistingPlugins() {
    auto types = processor.knownPluginList.getTypes();
    
    if (types.isEmpty()) {
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "No Plugins",
            "No plugins in list to rescan. Use 'Scan All Plugins' first.");
        return;
    }
    
    // Collect file paths from the EXISTING known plugin list
    juce::Array<OutOfProcessScanner::PluginToScan> existingPlugins;
    std::set<juce::String> seenPaths;
    
    for (const auto& desc : types) {
        juce::String pathKey = desc.fileOrIdentifier.toLowerCase() + "|" + desc.pluginFormatName;
        if (seenPaths.count(pathKey) > 0) continue;
        seenPaths.insert(pathKey);
        existingPlugins.add({ desc.fileOrIdentifier, desc.pluginFormatName });
    }
    
    // Clear existing list so rescan gets fresh metadata from the real DLL
    processor.knownPluginList.clear();
    
    if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPluginsV2", xml.get());
        userSettings->saveIfNeeded();
    }
    
    // Launch OOP scanner with ONLY the previously-registered plugin paths
    auto safeThis = juce::Component::SafePointer<PluginManagerTab>(this);
    
    auto* scanner = new AutoPluginScanner(processor, [safeThis]() {
        if (safeThis) {
            safeThis->buildTree();
            if (safeThis->scanDialog) safeThis->scanDialog->setVisible(false);
            safeThis->updateScanButtonStates();
            
            int newCount = safeThis->processor.knownPluginList.getNumTypes();
            juce::NativeMessageBox::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Rescan Complete",
                "Rescanned " + juce::String(newCount) + " registered plugins.");
        }
    });
    
    scanDialog = std::make_unique<CloseableDialogWindow>(
        "Rescanning Existing Plugins...",
        juce::Colour(0xFF1E1E1E),
        true
    );
    scanDialog->onCloseRequest = [scanner]() { scanner->cancelScan(); };
    scanDialog->setContentOwned(scanner, true);
    scanDialog->setUsingNativeTitleBar(true);
    scanDialog->setResizable(false, false);
    scanDialog->centreWithSize(500, 270);
    scanDialog->setVisible(true);
    
    updateScanButtonStates();
    scanner->startRescanExisting(existingPlugins);
}

// =============================================================================
// Startup Quick Scan — called on app launch, scans only new/updated plugins
// Shows a small dialog that auto-closes if nothing new is found
// =============================================================================
void PluginManagerTab::runStartupQuickScan() {
    auto safeThis = juce::Component::SafePointer<PluginManagerTab>(this);
    
    auto* scanner = new AutoPluginScanner(processor, [safeThis]() {
        if (safeThis) {
            safeThis->buildTree();
            if (safeThis->scanDialog)
                safeThis->scanDialog->setVisible(false);
            safeThis->updateScanButtonStates();
        }
    });
    
    scanDialog = std::make_unique<CloseableDialogWindow>(
        "Startup Plugin Check",
        juce::Colour(0xFF1E1E1E),
        true
    );
    scanDialog->onCloseRequest = [scanner]() { scanner->cancelScan(); };
    scanDialog->setContentOwned(scanner, true);
    scanDialog->setUsingNativeTitleBar(true);
    scanDialog->setResizable(false, false);
    scanDialog->centreWithSize(500, 270);
    scanDialog->setVisible(true);
    
    updateScanButtonStates();
    scanner->startQuickScan();
}

// =============================================================================
// FIX: Save/restore expanded state across tree rebuilds
// =============================================================================
void PluginManagerTab::saveExpandedState() {
    expandedFolders.clear();
    
    if (auto* root = pluginTree.getRootItem()) {
        for (int i = 0; i < root->getNumSubItems(); ++i) {
            auto* sub = root->getSubItem(i);
            if (sub && sub->isOpen()) {
                if (auto* treeItem = dynamic_cast<PluginTreeItem*>(sub)) {
                    expandedFolders.insert(treeItem->name);
                }
            }
        }
    }
}

void PluginManagerTab::restoreExpandedState() {
    if (expandedFolders.empty()) return;
    
    if (auto* root = pluginTree.getRootItem()) {
        for (int i = 0; i < root->getNumSubItems(); ++i) {
            if (auto* treeItem = dynamic_cast<PluginTreeItem*>(root->getSubItem(i))) {
                if (expandedFolders.count(treeItem->name) > 0) {
                    treeItem->setOpen(true);
                }
            }
        }
    }
}

// =============================================================================
// Build Tree View - FIX: Preserves expand/collapse state, loads hidden state
// =============================================================================
void PluginManagerTab::buildTree() {
    // FIX: Save which folders are expanded before destroying the tree
    saveExpandedState();
    
    pluginTree.setRootItem(nullptr);
    
    auto types = processor.knownPluginList.getTypes();
    
    // Helper lambda to aggressively detect instruments even when isInstrument is false
    auto looksLikeInstrument = [](const juce::PluginDescription& p) -> bool {
        juce::String catLower = p.category.toLowerCase();
        juce::String nameLower = p.name.toLowerCase();
        return catLower.contains("instrument") ||
               catLower.contains("synth") ||
               catLower.contains("sampler") ||
               catLower.contains("drum") ||
               catLower.contains("piano") ||
               catLower.contains("organ") ||
               catLower.contains("bass") ||
               catLower.contains("string") ||
               catLower.contains("brass") ||
               catLower.contains("wood") ||
               catLower.contains("choir") ||
               catLower.contains("vocal") ||
               catLower.contains("generator") ||
               catLower == "vsti" ||
               nameLower.contains("addictive") ||
               nameLower.contains("kontakt") ||
               nameLower.contains("omnisphere") ||
               nameLower.contains("serum") ||
               nameLower.contains("massive") ||
               nameLower.contains("sylenth") ||
               nameLower.contains("nexus") ||
               nameLower.contains("spitfire") ||
               nameLower.contains("native instruments") ||
               nameLower.contains("aas ") ||
               nameLower.contains("chromaphone") ||
               nameLower.contains("lounge lizard") ||
               nameLower.contains("strum") ||
               nameLower.contains("ultra analog") ||
               nameLower.contains("tassman") ||
               nameLower.contains("session ") ||
               nameLower.contains("drummic") ||
               nameLower.contains("ezdrummer") ||
               nameLower.contains("superior drummer") ||
               nameLower.contains("bfd") ||
               nameLower.contains("steven slate") ||
               nameLower.contains("get good drums");
    };
    
    juce::Array<juce::PluginDescription> filtered;
    for (const auto& p : types) {
        // OnStage: Always filter out instruments — effects only
        if (p.isInstrument) continue;
        // Aggressive filter for plugins that look like instruments
        if (looksLikeInstrument(p)) continue;
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
                item->hiddenFromMenus = isPluginHidden(p);
                vendorItem->addSubItem(item);
            }
            root->addSubItem(vendorItem);
        }
    } else {
        // OnStage: Effects-only — no instruments folder
        auto* effectFolder = new PluginTreeItem("Effects", false);
        effectFolder->parentTab = this;
        
        for (const auto& p : filtered) {
            auto* item = new PluginTreeItem(p.name, p, false);
            item->parentTab = this;
            item->hiddenFromMenus = isPluginHidden(p);
            effectFolder->addSubItem(item);
        }
        
        if (effectFolder->getNumSubItems() > 0) root->addSubItem(effectFolder);
        else delete effectFolder;
    }
    
    pluginTree.setRootItem(root);
    
    // FIX: Restore which folders were expanded
    restoreExpandedState();
}

void PluginManagerTab::showScanDialog() {
    // Clear existing list for a fresh full scan
    processor.knownPluginList.clear();
    
    if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPluginsV2", xml.get());
        userSettings->saveIfNeeded();
    }
    
    // Launch the OOP scanner for ALL formats (VST2 + VST3 + AU + LADSPA)
    auto safeThis = juce::Component::SafePointer<PluginManagerTab>(this);
    
    auto* scanner = new AutoPluginScanner(processor, [safeThis]() {
        if (safeThis) {
            safeThis->buildTree();
            if (safeThis->scanDialog) safeThis->scanDialog->setVisible(false);
            safeThis->updateScanButtonStates();
            
            int newCount = safeThis->processor.knownPluginList.getNumTypes();
            juce::NativeMessageBox::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Scan Complete",
                "Found " + juce::String(newCount) + " plugins (all formats).");
        }
    });
    
    scanDialog = std::make_unique<CloseableDialogWindow>(
        "Scanning All Plugins...",
        juce::Colour(0xFF1E1E1E),
        true
    );
    scanDialog->onCloseRequest = [scanner]() { scanner->cancelScan(); };
    scanDialog->setContentOwned(scanner, true);
    scanDialog->setUsingNativeTitleBar(true);
    scanDialog->setResizable(false, false);
    scanDialog->centreWithSize(500, 270);
    scanDialog->setVisible(true);
    
    updateScanButtonStates();
    scanner->startScan();
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

void PluginManagerTab::updateScanButtonStates() {
    bool active = scanDialog && scanDialog->isVisible();
    scanBtn.setEnabled(!active);
    rescanExistingBtn.setEnabled(!active);
    scanBtn.setColour(juce::TextButton::buttonColourId, active ? juce::Colours::darkgrey : juce::Colours::darkgreen);
    rescanExistingBtn.setColour(juce::TextButton::buttonColourId, active ? juce::Colours::darkgrey : juce::Colour(0xFF6B3FA0));
}

void PluginManagerTab::showBlacklistDialog() {
    auto blacklisted = processor.knownPluginList.getBlacklistedFiles();
    juce::String msg;
    if (blacklisted.isEmpty()) {
        msg = "No blacklisted plugins.\nAll scanned plugins are available.";
    } else {
        msg = "Blacklisted plugins:\n\n";
        for (int i = 0; i < blacklisted.size(); ++i) {
            juce::File f(blacklisted[i]);
            msg += juce::String(i + 1) + ". " + f.getFileNameWithoutExtension() + "\n   " + blacklisted[i] + "\n\n";
        }
        msg += "Use 'Reset Blacklist' to clear and rescan.";
    }
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Blacklisted Plugins (" + juce::String(blacklisted.size()) + ")", msg);
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
    
    if (auto* userSettings = processor.pluginProperties.getUserSettings()) {
        if (auto xml = processor.knownPluginList.createXml())
            userSettings->setValue("KnownPluginsV2", xml.get());
        userSettings->saveIfNeeded();
    }
    
    buildTree();
}

// =============================================================================
// NEW: Plugin Visibility (Eye Toggle) - Hide/show plugins from Add menus
// =============================================================================
juce::String PluginManagerTab::getPluginVisibilityKey(const juce::PluginDescription& desc) {
    return "PluginHidden_" + desc.fileOrIdentifier.replaceCharacters(" :/\\.", "_____") 
           + "_" + juce::String(desc.uniqueId);
}

bool PluginManagerTab::isPluginHidden(const juce::PluginDescription& desc) {
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        return userSettings->getBoolValue(getPluginVisibilityKey(desc), false);
    }
    return false;
}

void PluginManagerTab::setPluginHidden(const juce::PluginDescription& desc, bool hidden) {
    if (auto* userSettings = processor.appProperties.getUserSettings()) {
        if (hidden) {
            userSettings->setValue(getPluginVisibilityKey(desc), true);
        } else {
            userSettings->removeValue(getPluginVisibilityKey(desc));
        }
        userSettings->saveIfNeeded();
    }
}

void PluginManagerTab::togglePluginVisibility(const juce::PluginDescription& desc) {
    bool currentlyHidden = isPluginHidden(desc);
    setPluginHidden(desc, !currentlyHidden);
    buildTree();
}

// =============================================================================
// FIX: checkForCrashedScan — moved to AutoScanner file (safe cleanup only)
// This is now just a thin wrapper that delegates to the implementation
// in PluginManagerTab_AutoScanner.cpp
// =============================================================================
// NOTE: checkForCrashedScan() is implemented in PluginManagerTab_AutoScanner.cpp

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

// FIX: Darker folder/vendor headers, eye toggle icon, dimmed text when hidden
void PluginManagerTab::PluginTreeItem::paintItem(juce::Graphics& g, int w, int h) {
    if (!isPlugin) { 
        // FIX: Darker folder/vendor header - subtle dark background instead of bright outline
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRect(0, 0, w, h);
        g.setColour(juce::Colour(0xFF444444));
        g.drawRect(0, 0, w, h, 1);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(name, 25, 0, w-25, h, juce::Justification::centredLeft);
    } else { 
        g.setColour(isSelected() ? juce::Colours::blue.withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillRect(0, 0, w, h);
        
        juce::String formatBadge = getFormatBadge();
        juce::Colour formatColor = getFormatColor();
        
        // Format badge (left side)
        int badgeWidth = 40;
        int badgeHeight = h - 4;
        int badgeX = 5;
        int badgeY = 2;
        
        g.setColour(formatColor);
        g.fillRoundedRectangle((float)badgeX, (float)badgeY, (float)badgeWidth, (float)badgeHeight, 3.0f);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(formatBadge, badgeX, badgeY, badgeWidth, badgeHeight, juce::Justification::centred);
        
        // Delete X button (far right)
        int deleteBtnWidth = 20;
        int deleteBtnHeight = h - 6;
        int deleteBtnX = w - deleteBtnWidth - 5;
        int deleteBtnY = 3;
        
        g.setColour(juce::Colours::darkred);
        g.fillRoundedRectangle((float)deleteBtnX, (float)deleteBtnY, (float)deleteBtnWidth, (float)deleteBtnHeight, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("X", deleteBtnX, deleteBtnY, deleteBtnWidth, deleteBtnHeight, juce::Justification::centred);
        
        // NEW: Eye visibility toggle button (left of X button)
        int eyeBtnWidth = 24;
        int eyeBtnHeight = h - 6;
        int eyeBtnX = deleteBtnX - eyeBtnWidth - 4;
        int eyeBtnY = 3;
        
        if (hiddenFromMenus) {
            // Hidden state - dim, dark background
            g.setColour(juce::Colour(0xFF3A3A3A));
            g.fillRoundedRectangle((float)eyeBtnX, (float)eyeBtnY, (float)eyeBtnWidth, (float)eyeBtnHeight, 3.0f);
            g.setColour(juce::Colours::grey.darker());
            g.setFont(juce::Font(13.0f));
            g.drawText(juce::String::charToString(0x2014), eyeBtnX, eyeBtnY, eyeBtnWidth, eyeBtnHeight, juce::Justification::centred);
        } else {
            // Visible state - green background, bright icon
            g.setColour(juce::Colour(0xFF2A4A2A));
            g.fillRoundedRectangle((float)eyeBtnX, (float)eyeBtnY, (float)eyeBtnWidth, (float)eyeBtnHeight, 3.0f);
            g.setColour(juce::Colours::lightgreen);
            g.setFont(juce::Font(14.0f));
            g.drawText(juce::String::charToString(0x25C9), eyeBtnX, eyeBtnY, eyeBtnWidth, eyeBtnHeight, juce::Justification::centred);
        }
        
        // Plugin name text - dimmed if hidden
        g.setColour(hiddenFromMenus ? juce::Colours::grey : juce::Colours::lightgrey);
        g.setFont(14.0f);
        int textStartX = badgeX + badgeWidth + 8;
        int textEndX = eyeBtnX - 8;
        
        if (showColumns) {
            const int COL_NAME = 200, COL_VENDOR = 150, COL_CAT = 100, COL_VER = 80;
            int x = textStartX;
            
            g.drawText(name, x, 0, COL_NAME - 5, h, juce::Justification::centredLeft);
            x += COL_NAME;
            
            g.setColour(hiddenFromMenus ? juce::Colours::grey.darker() : juce::Colours::grey);
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

// FIX: e.x is already in item-local coordinates - no getEventRelativeTo needed
// NEW: Added eye toggle click handling
void PluginManagerTab::PluginTreeItem::itemClicked(const juce::MouseEvent& e) {
    if (!isPlugin) {
        setOpen(!isOpen());
        return;
    }
    
    auto itemBounds = getItemPosition(true);
    int w = itemBounds.getWidth();
    
    // FIX: e.x is already in item-local coordinates - no conversion needed
    int clickX = e.x;
    
    // Delete X button bounds
    int deleteBtnWidth = 20;
    int deleteBtnX = w - deleteBtnWidth - 5;
    
    // Eye toggle button bounds
    int eyeBtnWidth = 24;
    int eyeBtnX = deleteBtnX - eyeBtnWidth - 4;
    
    // Check X button click
    if (clickX >= deleteBtnX && clickX <= deleteBtnX + deleteBtnWidth) {
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
        return;
    }
    
    // NEW: Check eye toggle button click
    if (clickX >= eyeBtnX && clickX <= eyeBtnX + eyeBtnWidth) {
        if (parentTab != nullptr) {
            parentTab->togglePluginVisibility(description);
        }
        return;
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