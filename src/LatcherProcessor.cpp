// #D:\Workspace\Subterraneum_plugins_daw\src\LatcherProcessor.cpp
// THE LATCHER - 4x4 MIDI toggle pad controller
// MIDI-only, lock-free UI<->audio communication

#include "LatcherProcessor.h"

LatcherProcessor::LatcherProcessor()
    : AudioProcessor(BusesProperties())  // No audio buses - MIDI only
{
    // Initialize pads with sensible defaults (C4-D#5 mapping)
    for (int i = 0; i < NumPads; i++)
    {
        pads[i] = Pad();
        pads[i].triggerNote  = 60 + i;   // C4, C#4, D4, ... D#5
        pads[i].outputNote   = 60 + i;   // Same by default
        pads[i].velocity     = 100;
        pads[i].midiChannel  = 1;
        padLatched[i].store(false);
    }
}

LatcherProcessor::~LatcherProcessor()
{
    // Send note-off for any latched pads before destruction
    // (can't send MIDI here, but state will be cleaned up)
}

void LatcherProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void LatcherProcessor::releaseResources()
{
}

bool LatcherProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // MIDI-only: accept disabled/empty audio layouts
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet().isDisabled();
}

// =============================================================================
// processBlock - Handle incoming MIDI triggers and UI toggle requests
// =============================================================================
void LatcherProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // =========================================================================
    // 1) Process pending UI toggles (from mouse clicks)
    //    Consume all pending toggles atomically
    // =========================================================================
    uint32_t toggles = pendingToggles.exchange(0);
    if (toggles != 0)
    {
        for (int i = 0; i < NumPads; i++)
        {
            if (toggles & (1u << i))
            {
                bool wasLatched = padLatched[i].load();
                bool nowLatched = !wasLatched;
                padLatched[i].store(nowLatched);

                const auto& pad = pads[i];
                if (nowLatched)
                {
                    midiMessages.addEvent(
                        juce::MidiMessage::noteOn(pad.midiChannel, pad.outputNote, (juce::uint8)pad.velocity),
                        0);
                }
                else
                {
                    midiMessages.addEvent(
                        juce::MidiMessage::noteOff(pad.midiChannel, pad.outputNote, (juce::uint8)0),
                        0);
                }
            }
        }
    }

    // =========================================================================
    // 2) Process incoming MIDI - check for trigger notes
    //    Pass through non-matching MIDI events
    // =========================================================================
    juce::MidiBuffer outputMidi;

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        int sample = metadata.samplePosition;

        bool consumed = false;

        if (msg.isNoteOn())
        {
            int inNote = msg.getNoteNumber();

            // Check if this note matches any pad's trigger note
            for (int i = 0; i < NumPads; i++)
            {
                if (pads[i].triggerNote == inNote)
                {
                    // Toggle this pad
                    bool wasLatched = padLatched[i].load();
                    bool nowLatched = !wasLatched;
                    padLatched[i].store(nowLatched);

                    const auto& pad = pads[i];
                    if (nowLatched)
                    {
                        outputMidi.addEvent(
                            juce::MidiMessage::noteOn(pad.midiChannel, pad.outputNote, (juce::uint8)pad.velocity),
                            sample);
                    }
                    else
                    {
                        outputMidi.addEvent(
                            juce::MidiMessage::noteOff(pad.midiChannel, pad.outputNote, (juce::uint8)0),
                            sample);
                    }

                    consumed = true;
                    break;  // One trigger note per pad - first match wins
                }
            }
        }

        // Pass through events that weren't consumed as triggers
        if (!consumed)
        {
            outputMidi.addEvent(msg, sample);
        }
    }

    midiMessages.swapWith(outputMidi);
}

// =============================================================================
// UI toggle - queue a toggle for the audio thread
// =============================================================================
void LatcherProcessor::togglePadFromUI(int index)
{
    if (index >= 0 && index < NumPads)
    {
        // Set the bit for this pad - atomic OR
        uint32_t bit = 1u << index;
        pendingToggles.fetch_or(bit);
    }
}

// =============================================================================
// All notes off - unlatch all pads
// =============================================================================
void LatcherProcessor::allNotesOff()
{
    // Queue all currently-latched pads for toggle off
    for (int i = 0; i < NumPads; i++)
    {
        if (padLatched[i].load())
        {
            uint32_t bit = 1u << i;
            pendingToggles.fetch_or(bit);
        }
    }
}

// =============================================================================
// State save/restore
// =============================================================================
void LatcherProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree vt("LatcherState");

    for (int i = 0; i < NumPads; i++)
    {
        const auto& pad = pads[i];
        juce::ValueTree padVT("Pad");
        padVT.setProperty("index",       i,                nullptr);
        padVT.setProperty("triggerNote", pad.triggerNote,  nullptr);
        padVT.setProperty("outputNote",  pad.outputNote,   nullptr);
        padVT.setProperty("velocity",    pad.velocity,     nullptr);
        padVT.setProperty("midiChannel", pad.midiChannel,  nullptr);
        vt.addChild(padVT, -1, nullptr);
    }

    juce::MemoryOutputStream mos(destData, false);
    vt.writeToStream(mos);
}

void LatcherProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto vt = juce::ValueTree::readFromData(data, (size_t)sizeInBytes);
    if (!vt.isValid() || vt.getType().toString() != "LatcherState")
        return;

    for (int c = 0; c < vt.getNumChildren(); c++)
    {
        auto padVT = vt.getChild(c);
        if (padVT.getType().toString() != "Pad") continue;

        int idx = (int)padVT.getProperty("index", -1);
        if (idx < 0 || idx >= NumPads) continue;

        auto& pad = pads[idx];
        pad.triggerNote  = juce::jlimit(0, 127, (int)padVT.getProperty("triggerNote", 60 + idx));
        pad.outputNote   = juce::jlimit(0, 127, (int)padVT.getProperty("outputNote",  60 + idx));
        pad.velocity     = juce::jlimit(1, 127, (int)padVT.getProperty("velocity",    100));
        pad.midiChannel  = juce::jlimit(1, 16,  (int)padVT.getProperty("midiChannel", 1));

        // Always start unlatched on restore
        padLatched[idx].store(false);
    }
}
