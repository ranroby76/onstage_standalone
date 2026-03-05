// #D:\Workspace\Subterraneum_plugins_daw\src\LatcherProcessor.cpp
// THE LATCHER - 4x4 MIDI toggle pad controller
// 
// BEHAVIOR:
//   UI click on pad:  toggle ON/OFF immediately, send proper MIDI
//   External MIDI in: NoteOn matching a pad's triggerNote = toggle that pad
//                     NoteOff is IGNORED (toggle is per-press, not momentary)
//   ON  state: sends NoteOn  (144+ch, outputNote, velocity) per pad settings
//   OFF state: sends NoteOff (128+ch, outputNote, 0) per pad settings
//
// MIDI-only, lock-free UI<->audio communication

#include "LatcherProcessor.h"

LatcherProcessor::LatcherProcessor()
    : AudioProcessor(BusesProperties())  // No audio buses - MIDI only
{
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
}

void LatcherProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void LatcherProcessor::releaseResources()
{
}

bool LatcherProcessor::isBusesLayoutSupported(const BusesLayout& /*layouts*/) const
{
    // Accept any layout — we're MIDI-only, audio buses are irrelevant
    return true;
}

// =============================================================================
// processBlock - THE CORE: handle UI toggles + external MIDI triggers
// =============================================================================
void LatcherProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    juce::MidiBuffer outputMidi;

    // =========================================================================
    // 1) Process pending UI toggles (from mouse clicks / allNotesOff)
    //    padLatched was already flipped in togglePadFromUI() for instant UI.
    //    Here we just emit the correct MIDI message based on current state.
    // =========================================================================
    uint32_t toggles = pendingToggles.exchange(0);
    if (toggles != 0)
    {
        for (int i = 0; i < NumPads; i++)
        {
            if (toggles & (1u << i))
            {
                bool isOn = padLatched[i].load();
                const auto& pad = pads[i];

                if (isOn)
                {
                    // NoteOn: status 0x90 (144) + channel, outputNote, velocity
                    outputMidi.addEvent(
                        juce::MidiMessage::noteOn(pad.midiChannel, pad.outputNote, (juce::uint8)pad.velocity),
                        0);
                }
                else
                {
                    // NoteOff: status 0x80 (128) + channel, outputNote, velocity
                    outputMidi.addEvent(
                        juce::MidiMessage::noteOff(pad.midiChannel, pad.outputNote, (juce::uint8)pad.velocity),
                        0);
                }
            }
        }
    }

    // =========================================================================
    // 2) Process incoming MIDI from the MIDI input pin
    //    TOGGLE LOGIC (per user spec):
    //      - Incoming NoteOn (status 144, vel > 0) matching a pad's triggerNote:
    //        * If pad is OFF -> turn ON, send NoteOn(ch, outputNote, vel) per pad settings
    //        * If pad is ON  -> turn OFF, send NoteOff(ch, outputNote, vel) per pad settings
    //      - Incoming NoteOff (status 128) or NoteOn-vel0 for mapped note:
    //        * IGNORED (eaten) — toggle is per-press, not momentary
    //      - Non-matching MIDI passes through unchanged
    // =========================================================================
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        int sample = metadata.samplePosition;

        bool consumed = false;

        // Only NoteOn with velocity > 0 triggers a toggle
        // (NoteOn vel=0 is treated as NoteOff by MIDI spec — must be ignored)
        if (msg.isNoteOn())  // JUCE: isNoteOn() returns false for vel=0
        {
            int inNote = msg.getNoteNumber();

            for (int i = 0; i < NumPads; i++)
            {
                if (pads[i].triggerNote == inNote)
                {
                    // Toggle this pad on NoteOn only
                    bool wasOn = padLatched[i].load();
                    bool nowOn = !wasOn;
                    padLatched[i].store(nowOn);

                    const auto& pad = pads[i];

                    if (nowOn)
                    {
                        // Send: 144 + (ch-1), outputNote, velocity
                        outputMidi.addEvent(
                            juce::MidiMessage::noteOn(pad.midiChannel, pad.outputNote, (juce::uint8)pad.velocity),
                            sample);
                    }
                    else
                    {
                        // Send: 128 + (ch-1), outputNote, velocity
                        outputMidi.addEvent(
                            juce::MidiMessage::noteOff(pad.midiChannel, pad.outputNote, (juce::uint8)pad.velocity),
                            sample);
                    }

                    consumed = true;
                    break;  // First matching pad wins
                }
            }
        }
        else if (msg.isNoteOff())
        {
            // Eat NoteOff (128) AND NoteOn-vel0 for mapped trigger notes
            // Key release must NOT toggle — only key press does
            int inNote = msg.getNoteNumber();
            for (int i = 0; i < NumPads; i++)
            {
                if (pads[i].triggerNote == inNote)
                {
                    consumed = true;
                    break;
                }
            }
        }

        // Pass through non-consumed MIDI (CC, pitchbend, sysex, etc.)
        if (!consumed)
        {
            outputMidi.addEvent(msg, sample);
        }
    }

    midiMessages.swapWith(outputMidi);
}

// =============================================================================
// UI toggle - flip visual state immediately, queue MIDI for audio thread
// =============================================================================
void LatcherProcessor::togglePadFromUI(int index)
{
    if (index >= 0 && index < NumPads)
    {
        // Flip state immediately for instant UI feedback
        bool newState = !padLatched[index].load();
        padLatched[index].store(newState);

        // Queue MIDI message generation for the audio thread
        uint32_t bit = 1u << index;
        pendingToggles.fetch_or(bit);
    }
}

// =============================================================================
// All notes off - unlatch all pads immediately, queue NoteOff MIDI
// =============================================================================
void LatcherProcessor::allNotesOff()
{
    for (int i = 0; i < NumPads; i++)
    {
        if (padLatched[i].load())
        {
            padLatched[i].store(false);        // Immediate visual feedback
            uint32_t bit = 1u << i;
            pendingToggles.fetch_or(bit);      // Queue NoteOff in processBlock
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

        padLatched[idx].store(false);
    }
}