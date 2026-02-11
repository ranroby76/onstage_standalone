// ==============================================================================
//  InternalPluginBrowser.h
//  OnStage — Right sidebar panel listing all built-in effects for drag-drop
//
//  Drag data string format:  "INTERNAL:<EffectType>"
//  e.g. "INTERNAL:EQ", "INTERNAL:Compressor", "INTERNAL:GuitarOverdrive"
//
//  Categories: Dynamics, Color, Time, Pitch, System, Guitar
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <vector>

// ==============================================================================
//  InternalEffectInfo — descriptor for one built-in effect
// ==============================================================================
struct InternalEffectInfo
{
    juce::String typeID;       // e.g. "EQ", "GuitarOverdrive" — matches OnStageGraph::addEffect()
    juce::String displayName;  // e.g. "EQ", "Overdrive"
    juce::String category;     // e.g. "Dynamics", "Guitar", "System"
};

// ==============================================================================
//  Master list of all available internal effects
// ==============================================================================
inline std::vector<InternalEffectInfo> getInternalEffects()
{
    return {
        // --- Studio Effects ---
        { "Gate",         "Gate",          "Dynamics" },
        { "EQ",           "EQ",            "Dynamics" },
        { "Compressor",   "Compressor",    "Dynamics" },
        { "DeEsser",      "De-Esser",      "Dynamics" },
        { "DynamicEQ",    "Dynamic EQ",    "Dynamics" },
        { "Master",       "Master",        "Dynamics" },
        { "Exciter",      "Exciter",       "Color"    },
        { "Sculpt",       "Sculpt",        "Color"    },
        { "Saturation",   "Saturation",    "Color"    },
        { "Doubler",      "Doubler",       "Color"    },
        { "Reverb",       "Convo. Reverb", "Time"     },
        { "StudioReverb", "Studio Reverb", "Time"     },
        { "Delay",        "Delay",         "Time"     },
        { "Harmonizer",   "Harmonizer",    "Pitch"    },

        // --- System Tools ---
        { "PreAmp",       "Pre-Amp",       "System"   },
        { "Recorder",     "Recorder",      "System"   },
        // { "Tuner",        "Tuner",         "System"   },  // DISABLED — needs pitch detection fixes

        // --- Guitar Effects ---
        { "GuitarOverdrive",  "Overdrive",            "Guitar" },
        { "GuitarDistortion", "Distortion",           "Guitar" },
        { "GuitarFuzz",       "Fuzz",                 "Guitar" },
        { "GuitarChorus",     "Chorus",               "Guitar" },
        { "GuitarFlanger",    "Flanger",              "Guitar" },
        { "GuitarPhaser",     "Phaser",               "Guitar" },
        { "GuitarTremolo",    "Tremolo",              "Guitar" },
        { "GuitarVibrato",    "Vibrato",              "Guitar" },
        { "GuitarTone",       "Tone",                 "Guitar" },
        { "GuitarRotary",     "Rotary Speaker",       "Guitar" },
        { "GuitarWah",        "Wah",                  "Guitar" },
        { "GuitarReverb",     "Reverb",               "Guitar" },
        { "GuitarNoiseGate",  "Noise Gate",           "Guitar" },
        { "GuitarToneStack",  "Tone Stack",           "Guitar" },
        { "GuitarCabSim",     "Cab Sim",              "Guitar" },
        { "GuitarCabIR",      "Cab IR (Convolution)", "Guitar" },
    };
}

// ==============================================================================
//  InternalPluginItem — one draggable row in the list
// ==============================================================================
class InternalPluginItem : public juce::Component
{
public:
    explicit InternalPluginItem (const InternalEffectInfo& info)
        : effectInfo (info)
    {
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
        bool hovered = isMouseOver();

        // Background
        g.setColour (hovered ? juce::Colour (0xFF3A3A3A) : juce::Colour (0xFF2D2D2D));
        g.fillRoundedRectangle (bounds, 3.0f);

        // Category color dot
        juce::Colour dotColour = juce::Colours::grey;
        if (effectInfo.category == "Dynamics") dotColour = juce::Colour (0xFF00CC66);
        if (effectInfo.category == "Color")    dotColour = juce::Colour (0xFFFF6B6B);
        if (effectInfo.category == "Time")     dotColour = juce::Colour (0xFFCC88FF);
        if (effectInfo.category == "Pitch")    dotColour = juce::Colour (0xFFFFAA00);
        if (effectInfo.category == "System")   dotColour = juce::Colour (0xFFDDCC00);
        if (effectInfo.category == "Guitar")   dotColour = juce::Colour (0xFF663399);

        g.setColour (dotColour);
        g.fillEllipse (bounds.getX() + 8, bounds.getCentreY() - 3, 6, 6);

        // Name
        g.setColour (juce::Colours::white);
        g.setFont (12.0f);
        g.drawText (effectInfo.displayName,
                    bounds.withTrimmedLeft (22).withTrimmedRight (4),
                    juce::Justification::centredLeft);

        // Category tag (right-aligned)
        g.setColour (juce::Colours::grey.withAlpha (0.6f));
        g.setFont (9.0f);
        g.drawText (effectInfo.category,
                    bounds.withTrimmedRight (6),
                    juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            dragStartPos = e.getPosition();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 5)
        {
            auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
            if (container != nullptr)
            {
                juce::String dragID = "INTERNAL:" + effectInfo.typeID;
                juce::Image dragImage (juce::Image::ARGB, 120, 24, true);
                juce::Graphics g (dragImage);
                g.setColour (juce::Colour (0xCC333333));
                g.fillRoundedRectangle (0, 0, 120, 24, 4);
                g.setColour (juce::Colours::white);
                g.setFont (11.0f);
                g.drawText (effectInfo.displayName, 0, 0, 120, 24,
                            juce::Justification::centred);

                container->startDragging (dragID, this,
                    juce::ScaledImage (dragImage), true);
            }
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (onDoubleClick)
            onDoubleClick (effectInfo);
    }

    std::function<void (const InternalEffectInfo&)> onDoubleClick;

    const InternalEffectInfo& getEffectInfo() const { return effectInfo; }

private:
    InternalEffectInfo effectInfo;
    juce::Point<int> dragStartPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InternalPluginItem)
};

// ==============================================================================
//  InternalPluginList — vertical list with section headers
// ==============================================================================
class InternalPluginList : public juce::Component
{
public:
    void setEffects (const std::vector<InternalEffectInfo>& effects)
    {
        items.clear();
        sectionHeaders.clear();
        removeAllChildren();

        // Split into studio, system, and guitar
        std::vector<const InternalEffectInfo*> studioEffects, systemEffects, guitarEffects;
        for (const auto& fx : effects)
        {
            if (fx.category == "Guitar")
                guitarEffects.push_back (&fx);
            else if (fx.category == "System")
                systemEffects.push_back (&fx);
            else
                studioEffects.push_back (&fx);
        }

        int y = 4;

        // --- Studio section ---
        if (! studioEffects.empty())
        {
            sectionHeaders.push_back ({ "STUDIO EFFECTS", juce::Colour (0xFFD4AF37), y });
            y += 20;
            for (const auto* fx : studioEffects)
            {
                auto* item = items.add (new InternalPluginItem (*fx));
                item->setBounds (0, y, juce::jmax (1, getWidth()), 32);
                item->onDoubleClick = onDoubleClick;
                addAndMakeVisible (item);
                y += 34;
            }
        }

        // --- System Tools section ---
        if (! systemEffects.empty())
        {
            y += 6;
            sectionHeaders.push_back ({ "SYSTEM TOOLS", juce::Colour (0xFFDDCC00), y });
            y += 20;
            for (const auto* fx : systemEffects)
            {
                auto* item = items.add (new InternalPluginItem (*fx));
                item->setBounds (0, y, juce::jmax (1, getWidth()), 32);
                item->onDoubleClick = onDoubleClick;
                addAndMakeVisible (item);
                y += 34;
            }
        }

        // --- Guitar section ---
        if (! guitarEffects.empty())
        {
            y += 6;
            sectionHeaders.push_back ({ "GUITAR EFFECTS", juce::Colour (0xFF663399), y });
            y += 20;
            for (const auto* fx : guitarEffects)
            {
                auto* item = items.add (new InternalPluginItem (*fx));
                item->setBounds (0, y, juce::jmax (1, getWidth()), 32);
                item->onDoubleClick = onDoubleClick;
                addAndMakeVisible (item);
                y += 34;
            }
        }

        setSize (getWidth(), y + 4);
    }

    void resized() override
    {
        int w = getWidth();
        for (auto* item : items)
            item->setSize (w, item->getHeight());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF252525));
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        for (const auto& h : sectionHeaders)
        {
            // Dot
            g.setColour (h.accent);
            g.fillEllipse (8.0f, (float) h.y + 6.0f, 6.0f, 6.0f);

            // Text
            g.setColour (h.accent.withAlpha (0.9f));
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText (h.text,
                        juce::Rectangle<int> (20, h.y, getWidth() - 28, 18),
                        juce::Justification::centredLeft);

            // Line
            int lineX = 20 + (int) g.getCurrentFont().getStringWidthFloat (h.text) + 6;
            g.setColour (h.accent.withAlpha (0.3f));
            g.drawLine ((float) lineX, (float) h.y + 9.0f,
                        (float) getWidth() - 8.0f, (float) h.y + 9.0f, 1.0f);
        }
    }

    std::function<void (const InternalEffectInfo&)> onDoubleClick;

private:
    struct SectionHeader { juce::String text; juce::Colour accent; int y; };
    std::vector<SectionHeader> sectionHeaders;
    juce::OwnedArray<InternalPluginItem> items;
};

// ==============================================================================
//  InternalPluginBrowser — search bar + scrollable list panel
// ==============================================================================
class InternalPluginBrowser : public juce::Component,
                              public juce::TextEditor::Listener
{
public:
    InternalPluginBrowser()
    {
        // Title
        titleLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        // Search box
        searchBox.setTextToShowWhenEmpty ("Search...", juce::Colours::grey);
        searchBox.addListener (this);
        searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF2A2A2A));
        searchBox.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF4A4A4A));
        addAndMakeVisible (searchBox);

        // Count label
        countLabel.setFont (10.0f);
        countLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
        countLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (countLabel);

        // List inside viewport
        pluginList = std::make_unique<InternalPluginList>();
        pluginList->onDoubleClick = [this] (const InternalEffectInfo& info)
        {
            if (onEffectDoubleClick)
                onEffectDoubleClick (info);
        };
        viewport.setViewedComponent (pluginList.get(), false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        // Build initial list
        allEffects = getInternalEffects();
        applyFilter();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF252525));
        g.setColour (juce::Colour (0xFF4A4A4A));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6);

        titleLabel.setBounds (area.removeFromTop (22));
        area.removeFromTop (4);

        searchBox.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);

        countLabel.setBounds (area.removeFromBottom (16));
        area.removeFromBottom (2);

        viewport.setBounds (area);

        applyFilter();
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            applyFilter();
            searchBox.grabKeyboardFocus();
        }
    }

    // TextEditor::Listener
    void textEditorTextChanged (juce::TextEditor&) override
    {
        applyFilter();
    }

    // Callback when a user double-clicks an effect
    std::function<void (const InternalEffectInfo&)> onEffectDoubleClick;

private:
    void applyFilter()
    {
        juce::String search = searchBox.getText().toLowerCase();

        std::vector<InternalEffectInfo> filtered;
        for (const auto& fx : allEffects)
        {
            if (search.isEmpty()
                || fx.displayName.toLowerCase().contains (search)
                || fx.typeID.toLowerCase().contains (search)
                || fx.category.toLowerCase().contains (search))
            {
                filtered.push_back (fx);
            }
        }

        int listWidth = viewport.getWidth() - viewport.getScrollBarThickness();
        if (listWidth < 1)
            listWidth = viewport.getWidth();

        pluginList->setSize (juce::jmax (1, listWidth), pluginList->getHeight());
        pluginList->setEffects (filtered);
        pluginList->setSize (juce::jmax (1, listWidth), pluginList->getHeight());

        countLabel.setText (juce::String ((int) filtered.size()) + " effect"
                           + (filtered.size() != 1 ? "s" : ""),
                           juce::dontSendNotification);
    }

    juce::Label titleLabel { "title", "Add Effects" };
    juce::TextEditor searchBox;
    juce::Label countLabel;
    juce::Viewport viewport;
    std::unique_ptr<InternalPluginList> pluginList;

    std::vector<InternalEffectInfo> allEffects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InternalPluginBrowser)
};
