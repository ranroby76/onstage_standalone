// D:\Workspace\Subterraneum_plugins_daw\src\GraphCanvas_PluginWindow.cpp
// CRITICAL FIX: Proper editor lifecycle management for Flowstone and other plugins
// FREEZE FIX: Add timeout protection for plugins that block during createEditor()
// 
// THE REAL PROBLEM:
// JUCE's AudioPluginInstance only allows ONE editor at a time. When createEditor() 
// is called, JUCE automatically deletes any existing editor first. However, our
// window still holds a pointer to that deleted editor, causing crashes or black windows.
//
// SOLUTION:
// 1. Check if window exists in our map - if yes, bring to front
// 2. Check if editor exists WITHOUT a valid window - delete the orphaned editor
// 3. Always create fresh editor and window together
// 4. Store the window properly so we can track its lifecycle
// 5. FREEZE FIX: Use async timer to detect if createEditor() hangs

#include "GraphCanvas.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

// =============================================================================
// THREAD-SAFE DEBUG LOGGER - DISABLED FOR RELEASE
// =============================================================================
/*
class PluginWindowLogger {
public:
    static PluginWindowLogger& getInstance() {
        static PluginWindowLogger instance;
        return instance;
    }
    
    void log(const juce::String& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        if (!logFile.is_open()) return;
        
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        char timeBuffer[100];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", std::localtime(&time_t_now));
        
        auto threadId = std::this_thread::get_id();
        std::stringstream ss;
        ss << threadId;
        
        logFile << "[" << timeBuffer << "." << std::setfill('0') << std::setw(3) << ms.count() 
                << "][Thread:" << ss.str().substr(ss.str().length() - 4) << "] " 
                << message.toStdString() << std::endl;
        logFile.flush();
    }
    
    ~PluginWindowLogger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }
    
private:
    PluginWindowLogger() {
        auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        auto logPath = exePath.getParentDirectory().getChildFile("plugin_load_debug.log");
        
        logFile.open(logPath.getFullPathName().toStdString(), std::ios::out | std::ios::app);
    }
    
    std::ofstream logFile;
    std::mutex logMutex;
};

#define WLOG(msg) PluginWindowLogger::getInstance().log(msg)
*/

// Debug logging disabled - no-op macro
#define WLOG(msg) ((void)0)

// =============================================================================
// FLOWSTONE FIX: Custom PluginWindow with proper editor cleanup
// =============================================================================
// FlowStone and some other plugins show black windows after close/reopen
// because they don't properly handle editor destruction.
// This class ensures proper cleanup by:
// 1. Notifying the editor it's being deleted (editorBeingDeleted)
// 2. Explicitly clearing the content BEFORE window destruction
// 3. Giving the plugin a chance to clean up properly
// =============================================================================
class FlowStonePluginWindow : public juce::DocumentWindow {
public:
    FlowStonePluginWindow(const juce::String& name, juce::Colour backgroundColour, int buttonsNeeded,
                      GraphCanvas* canvas, juce::AudioProcessorGraph::NodeID nodeID)
        : juce::DocumentWindow(name, backgroundColour, buttonsNeeded),
          graphCanvas(canvas),
          nodeID(nodeID)
    {
        WLOG("FlowStonePluginWindow created for node " + juce::String(nodeID.uid));
    }
    
    ~FlowStonePluginWindow() override
    {
        WLOG("FlowStonePluginWindow destructor START for node " + juce::String(nodeID.uid));
        
        // CRITICAL: Properly destroy editor BEFORE window destruction
        cleanupEditor();
        
        WLOG("FlowStonePluginWindow destructor COMPLETE");
    }
    
    void closeButtonPressed() override
    {
        WLOG("FlowStonePluginWindow close button pressed for node " + juce::String(nodeID.uid));
        
        // Cleanup editor first
        cleanupEditor();
        
        // Remove from active windows map
        if (graphCanvas) {
            graphCanvas->activePluginWindows.erase(nodeID);
        }
        
        WLOG("FlowStonePluginWindow closed and removed from map");
    }
    
private:
    void cleanupEditor()
    {
        WLOG("  Cleaning up editor...");
        
        auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getContentComponent());
        if (editor)
        {
            WLOG("    Editor found - notifying plugin...");
            
            // CRITICAL FOR FLOWSTONE: Tell the editor it's being deleted
            // This gives the plugin a chance to clean up properly
            if (auto* processor = editor->getAudioProcessor())
            {
                WLOG("    Calling editorBeingDeleted()...");
                processor->editorBeingDeleted(editor);
            }
            
            // Clear content - since we used setContentOwned(editor, true),
            // the window OWNS the editor and will delete it automatically here
            WLOG("    Clearing window content (editor will be deleted automatically)...");
            clearContentComponent();
            
            // DO NOT manually delete editor - it's already deleted by clearContentComponent()!
            // The old code had: delete editor; which caused a DOUBLE-DELETE CRASH
            
            WLOG("    Editor cleanup COMPLETE");
        }
        else
        {
            WLOG("    No editor found");
        }
    }
    
    GraphCanvas* graphCanvas;
    juce::AudioProcessorGraph::NodeID nodeID;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlowStonePluginWindow)
};

void GraphCanvas::openPluginWindow(juce::AudioProcessorGraph::Node* node)
{
    WLOG(">>> openPluginWindow() START");
    
    if (!node) {
        WLOG("  ERROR: node is nullptr");
        return;
    }

    WLOG("  Checking for MeteringProcessor...");
    auto* cache = getCachedNodeType(node->nodeID);
    MeteringProcessor* meteringProc = cache ? cache->meteringProc : dynamic_cast<MeteringProcessor*>(node->getProcessor());
    if (!meteringProc || !meteringProc->getInnerPlugin()) {
        WLOG("  ERROR: No MeteringProcessor or innerPlugin");
        return;
    }

    auto nodeID = node->nodeID;
    auto* innerPlugin = meteringProc->getInnerPlugin();
    
    // FREEZE FIX: Use cached name instead of calling getName()
    juce::String pluginName = meteringProc->getCachedName();
    
    WLOG("  Plugin: " + pluginName + " (ID: " + juce::String(nodeID.uid) + ")");

    // =================================================================
    // STEP 1: Check if we already have a valid window for this plugin
    // =================================================================
    WLOG("  STEP 1: Checking for existing window...");
    auto it = activePluginWindows.find(nodeID);
    if (it != activePluginWindows.end() && it->second)
    {
        // Window exists in our map - verify it's still valid
        if (it->second->isVisible() && it->second->getPeer() != nullptr)
        {
            WLOG("  Found valid window - bringing to front");
            it->second->toFront(true);
            WLOG("<<< openPluginWindow() COMPLETE (reused existing)");
            return;
        }
        else
        {
            WLOG("  Found invalid window - removing from map");
            activePluginWindows.erase(it);
        }
    }
    else
    {
        WLOG("  No existing window found");
    }

    // =================================================================
    // STEP 2: Handle any orphaned editor
    // =================================================================
    WLOG("  STEP 2: Checking for orphaned editor...");
    auto* existingEditor = innerPlugin->getActiveEditor();
    if (existingEditor != nullptr)
    {
        WLOG("  Found orphaned editor - deleting it");
        delete existingEditor;
        
        WLOG("  Sleeping 50ms for cleanup...");
        juce::Thread::sleep(50);
        WLOG("  Sleep complete");
    }
    else
    {
        WLOG("  No orphaned editor found");
    }

    // =================================================================
    // STEP 3: Create fresh editor and window WITH TIMEOUT PROTECTION
    // =================================================================
    WLOG("  STEP 3: Checking if plugin has editor...");
    if (!innerPlugin->hasEditor())
    {
        WLOG("  Plugin has no editor - aborting");
        return;
    }
    
    WLOG("  Plugin has editor - preparing to create...");
    
    // FREEZE PROTECTION: Use async approach with progress dialog
    // Some plugins block forever in createEditor() - we need to show user progress
    // and allow cancellation
    
    struct EditorCreationState {
        juce::AudioPluginInstance* plugin;
        juce::Component* editor = nullptr;
        std::atomic<bool> completed{false};
        std::atomic<bool> cancelled{false};
        std::atomic<bool> timedOut{false};
    };
    
    auto state = std::make_shared<EditorCreationState>();
    state->plugin = innerPlugin;
    
    WLOG("  Creating loading dialog...");
    
    // Show a modal-ish dialog that doesn't block
    auto* loadingDialog = new juce::Component();
    loadingDialog->setSize(300, 100);
    
    // Create and show as a floating window
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Loading Plugin Editor";
    opts.content.setOwned(loadingDialog);
    opts.componentToCentreAround = this;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    
    auto* dialogWindow = opts.launchAsync();
    
    WLOG("  Starting editor creation timer...");
    
    // Use a timer to attempt editor creation without blocking
    class EditorCreationTimer : public juce::Timer {
    public:
        EditorCreationTimer(std::shared_ptr<EditorCreationState> s, 
                           juce::DialogWindow* dlg,
                           GraphCanvas* canvas,
                           juce::AudioProcessorGraph::NodeID nid,
                           juce::String pName)
            : state(s), dialog(dlg), graphCanvas(canvas), nodeID(nid), pluginName(pName)
        {
            WLOG("    Timer created - attempting editor creation");
            attempts = 0;
            startTime = juce::Time::getMillisecondCounterHiRes();
        }
        
        void timerCallback() override {
            attempts++;
            double elapsed = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
            
            WLOG("    Timer callback #" + juce::String(attempts) + " (elapsed: " + 
                 juce::String(elapsed, 2) + "s)");
            
            // Check for timeout (5 seconds)
            if (elapsed > 5.0) {
                WLOG("    TIMEOUT! Plugin took too long to create editor");
                state->timedOut = true;
                state->cancelled = true;
                
                if (dialog) {
                    WLOG("    Closing dialog");
                    dialog->exitModalState(0);
                    dialog->setVisible(false);
                }
                
                WLOG("    Showing error message");
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Plugin Editor Timeout",
                    "The plugin '" + pluginName + 
                    "' took too long to open its editor.\n\n"
                    "This plugin may have compatibility issues.",
                    "OK");
                
                stopTimer();
                delete this;
                return;
            }
            
            // Check if dialog was closed (user cancelled)
            if (dialog && !dialog->isVisible()) {
                WLOG("    User cancelled - dialog closed");
                state->cancelled = true;
                stopTimer();
                delete this;
                return;
            }
            
            // Attempt to create editor on first callback
            if (attempts == 1 && !state->completed && !state->cancelled) {
                WLOG("    ATTEMPTING: innerPlugin->createEditor()...");
                
                try {
                    state->editor = state->plugin->createEditor();
                    
                    if (state->editor) {
                        WLOG("    SUCCESS: Editor created!");
                        state->completed = true;
                    } else {
                        WLOG("    FAILED: createEditor() returned nullptr");
                        state->cancelled = true;
                    }
                } catch (const std::exception& e) {
                    WLOG("    EXCEPTION during createEditor(): " + juce::String(e.what()));
                    state->cancelled = true;
                } catch (...) {
                    WLOG("    UNKNOWN EXCEPTION during createEditor()");
                    state->cancelled = true;
                }
            }
            
            // If completed successfully, create the window
            if (state->completed && state->editor && !state->cancelled) {
                WLOG("    Creating plugin window...");
                
                if (dialog) {
                    dialog->exitModalState(0);
                    dialog->setVisible(false);
                }
                
                // Create window with GraphCanvas reference for proper cleanup
                std::unique_ptr<juce::DocumentWindow> window(
                    new FlowStonePluginWindow(
                        pluginName,
                        juce::Colours::darkgrey,
                        juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton,
                        graphCanvas,
                        nodeID));
                
                WLOG("    Setting editor as window content...");
                window->setContentOwned(state->editor, true);
                window->setResizable(true, false);
                
                // Size and position the window
                int editorWidth = state->editor->getWidth();
                int editorHeight = state->editor->getHeight();
                
                WLOG("    Editor size: " + juce::String(editorWidth) + "x" + juce::String(editorHeight));
                
                // Ensure minimum size
                if (editorWidth < 100) editorWidth = 400;
                if (editorHeight < 100) editorHeight = 300;
                
                window->centreWithSize(editorWidth, editorHeight);
                
                WLOG("    Making window visible...");
                window->setVisible(true);
                
                // Store the window in our map
                WLOG("    Storing window in map...");
                graphCanvas->activePluginWindows[nodeID] = std::move(window);
                
                WLOG("    Plugin window creation COMPLETE");
                WLOG("<<< openPluginWindow() COMPLETE (success)");
                
                stopTimer();
                delete this;
                return;
            }
            
            // If cancelled or failed
            if (state->cancelled) {
                WLOG("    Creation cancelled or failed");
                
                if (dialog) {
                    dialog->exitModalState(0);
                    dialog->setVisible(false);
                }
                
                if (!state->timedOut && state->editor == nullptr) {
                    WLOG("    Showing error message");
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Plugin Editor Error",
                        "Failed to create editor for plugin '" + pluginName + "'",
                        "OK");
                }
                
                WLOG("<<< openPluginWindow() COMPLETE (failed)");
                
                stopTimer();
                delete this;
                return;
            }
        }
        
    private:
        std::shared_ptr<EditorCreationState> state;
        juce::DialogWindow* dialog;
        GraphCanvas* graphCanvas;
        juce::AudioProcessorGraph::NodeID nodeID;
        juce::String pluginName;
        int attempts;
        double startTime;
    };
    
    // Start the timer (will be deleted automatically when done)
    auto* timer = new EditorCreationTimer(state, dialogWindow, this, nodeID, pluginName);
    timer->startTimer(100); // Check every 100ms
    
    WLOG("  Timer started - openPluginWindow() will complete async");
}


