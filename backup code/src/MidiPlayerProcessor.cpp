

// #D:\Workspace\Subterraneum_plugins_daw\src\MidiPlayerProcessor.cpp
// MIDI FILE PLAYER - Implementation
// Handles Type 0 and Type 1 MIDI files with embedded tempo maps
// Controller state snapshots enable clean seeking without stuck notes

#include "MidiPlayerProcessor.h"

// =============================================================================
// Constructor / Destructor
// =============================================================================
MidiPlayerProcessor::MidiPlayerProcessor()
    : AudioProcessor(BusesProperties())  // No audio buses - MIDI only
{
    for (int i = 0; i < 16; i++) {
        channelActive[i].store(false);
        channelMuted[i].store(false);
    }
}

MidiPlayerProcessor::~MidiPlayerProcessor()
{
    stop();
}

// =============================================================================
// Audio Setup
// =============================================================================
void MidiPlayerProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
}

void MidiPlayerProcessor::releaseResources()
{
    stop();
}

bool MidiPlayerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // MIDI-only: accept disabled/empty audio layouts
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet().isDisabled();
}

// =============================================================================
// File Loading
// =============================================================================
bool MidiPlayerProcessor::loadFile(const juce::File& file)
{
    stop();
    
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return false;
    
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return false;
    
    // Store file info
    loadedFile = file;
    loadedFileName = file.getFileNameWithoutExtension();
    midiType = midiFile.getTimeFormat() > 0 ? midiFile.getNumTracks() > 1 ? 1 : 0 : 0;
    numTracks = midiFile.getNumTracks();
    ticksPerBeat = midiFile.getTimeFormat();
    
    if (ticksPerBeat <= 0)
        ticksPerBeat = 480;  // Fallback for SMPTE-based files
    
    // Convert to ticks (from delta times)
    midiFile.convertTimestampTicksToSeconds();
    
    // Actually we need tick-based playback for tempo control
    // Re-read with raw ticks
    stream.setPosition(0);
    juce::MidiFile rawMidiFile;
    if (!rawMidiFile.readFrom(stream))
        return false;
    
    ticksPerBeat = rawMidiFile.getTimeFormat();
    if (ticksPerBeat <= 0)
        ticksPerBeat = 480;
    
    numTracks = rawMidiFile.getNumTracks();
    
    // Merge all tracks into one sequence
    mergedSequence.clear();
    
    for (int t = 0; t < rawMidiFile.getNumTracks(); t++)
    {
        auto* track = rawMidiFile.getTrack(t);
        if (!track) continue;
        
        for (int i = 0; i < track->getNumEvents(); i++)
        {
            auto* event = track->getEventPointer(i);
            mergedSequence.addEvent(event->message);
        }
    }
    
    mergedSequence.sort();
    mergedSequence.updateMatchedPairs();
    
    // Find total ticks
    totalTicks = 0.0;
    if (mergedSequence.getNumEvents() > 0)
    {
        totalTicks = mergedSequence.getEventPointer(
            mergedSequence.getNumEvents() - 1)->message.getTimeStamp();
    }
    
    // Build tempo map from merged sequence
    buildTempoMap();
    
    // Extract markers
    extractMarkers();
    
    // Build channel info table (instruments per channel)
    buildChannelInfoTable();
    
    // Calculate total duration
    totalDurationSeconds = tickToSeconds(totalTicks);
    
    // Store file BPM (from first tempo event)
    fileBpm = tempoMap.empty() ? 120.0 : tempoMap[0].bpm;
    tempoOverrideBpm.store(fileBpm);
    currentBpm.store(fileBpm);
    
    // Reset playback state
    currentTickPosition.store(0.0);
    lastEventIndex = 0;
    fileLoaded.store(true);
    
    return true;
}

// =============================================================================
// Build Tempo Map
// =============================================================================
void MidiPlayerProcessor::buildTempoMap()
{
    tempoMap.clear();
    
    // Default tempo: 120 BPM (500000 microseconds per quarter note)
    TempoEvent defaultTempo;
    defaultTempo.tick = 0.0;
    defaultTempo.microsecondsPerQuarterNote = 500000.0;
    defaultTempo.bpm = 120.0;
    defaultTempo.timeSeconds = 0.0;
    
    bool foundTempo = false;
    
    for (int i = 0; i < mergedSequence.getNumEvents(); i++)
    {
        auto& msg = mergedSequence.getEventPointer(i)->message;
        
        if (msg.isTempoMetaEvent())
        {
            double usPerQN = msg.getTempoSecondsPerQuarterNote() * 1000000.0;
            double tick = msg.getTimeStamp();
            double bpm = 60000000.0 / usPerQN;
            
            TempoEvent te;
            te.tick = tick;
            te.microsecondsPerQuarterNote = usPerQN;
            te.bpm = bpm;
            te.timeSeconds = 0.0;  // Computed below
            
            tempoMap.push_back(te);
            foundTempo = true;
        }
    }
    
    if (!foundTempo)
        tempoMap.push_back(defaultTempo);
    
    // Sort by tick
    std::sort(tempoMap.begin(), tempoMap.end(),
        [](const TempoEvent& a, const TempoEvent& b) { return a.tick < b.tick; });
    
    // Compute absolute time for each tempo event
    tempoMap[0].timeSeconds = 0.0;
    for (size_t i = 1; i < tempoMap.size(); i++)
    {
        double deltaTicks = tempoMap[i].tick - tempoMap[i-1].tick;
        double secondsPerTick = (tempoMap[i-1].microsecondsPerQuarterNote / 1000000.0) / (double)ticksPerBeat;
        tempoMap[i].timeSeconds = tempoMap[i-1].timeSeconds + deltaTicks * secondsPerTick;
    }
}

// =============================================================================
// Extract Markers from MIDI meta-events
// =============================================================================
void MidiPlayerProcessor::extractMarkers()
{
    markers.clear();
    
    for (int i = 0; i < mergedSequence.getNumEvents(); i++)
    {
        auto& msg = mergedSequence.getEventPointer(i)->message;
        
        if (msg.isMetaEvent())
        {
            int metaType = msg.getMetaEventType();
            
            // Marker (0x06) or Cue Point (0x07)
            if (metaType == 0x06 || metaType == 0x07)
            {
                auto data = msg.getMetaEventData();
                int length = msg.getMetaEventLength();
                
                if (data && length > 0)
                {
                    juce::String name = juce::String::fromUTF8((const char*)data, length).trimEnd();
                    
                    // Skip internal format markers
                    if (name == "SFF1" || name == "SFF2" || name == "SInt")
                        continue;
                    
                    Marker m;
                    m.tick = msg.getTimeStamp();
                    m.timeSeconds = tickToSeconds(m.tick);
                    m.name = name;
                    markers.push_back(m);
                }
            }
        }
    }
}

// =============================================================================
// Tick ↔ Seconds Conversion (using tempo map)
// =============================================================================
double MidiPlayerProcessor::tickToSeconds(double tick) const
{
    if (tempoMap.empty())
        return tick / (double)ticksPerBeat * 0.5;  // 120 BPM fallback
    
    // Find the tempo segment this tick falls in
    size_t segIdx = 0;
    for (size_t i = 1; i < tempoMap.size(); i++)
    {
        if (tick >= tempoMap[i].tick)
            segIdx = i;
        else
            break;
    }
    
    double deltaTicks = tick - tempoMap[segIdx].tick;
    double secondsPerTick = (tempoMap[segIdx].microsecondsPerQuarterNote / 1000000.0) / (double)ticksPerBeat;
    
    return tempoMap[segIdx].timeSeconds + deltaTicks * secondsPerTick;
}

double MidiPlayerProcessor::secondsToTick(double seconds) const
{
    if (tempoMap.empty())
        return seconds * (double)ticksPerBeat * 2.0;  // 120 BPM fallback
    
    // Find the tempo segment
    size_t segIdx = 0;
    for (size_t i = 1; i < tempoMap.size(); i++)
    {
        if (seconds >= tempoMap[i].timeSeconds)
            segIdx = i;
        else
            break;
    }
    
    double deltaSeconds = seconds - tempoMap[segIdx].timeSeconds;
    double ticksPerSecond = (double)ticksPerBeat / (tempoMap[segIdx].microsecondsPerQuarterNote / 1000000.0);
    
    return tempoMap[segIdx].tick + deltaSeconds * ticksPerSecond;
}

double MidiPlayerProcessor::getTempoAtTick(double tick) const
{
    if (tempoMap.empty())
        return 120.0;
    
    double bpm = tempoMap[0].bpm;
    for (auto& te : tempoMap)
    {
        if (tick >= te.tick)
            bpm = te.bpm;
        else
            break;
    }
    return bpm;
}

// =============================================================================
// Transport Controls
// =============================================================================
void MidiPlayerProcessor::play()
{
    if (!fileLoaded.load()) return;
    transportState.store(PLAYING);
}

void MidiPlayerProcessor::pause()
{
    transportState.store(PAUSED);
}

void MidiPlayerProcessor::stop()
{
    transportState.store(STOPPED);
    currentTickPosition.store(0.0);
    lastEventIndex = 0;
    needsSeekSnapshot.store(true);
    seekTargetTick.store(0.0);
}

void MidiPlayerProcessor::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

// =============================================================================
// Position / Seek
// =============================================================================
double MidiPlayerProcessor::getPositionNormalized() const
{
    if (totalTicks <= 0.0) return 0.0;
    return juce::jlimit(0.0, 1.0, currentTickPosition.load() / totalTicks);
}

void MidiPlayerProcessor::setPositionNormalized(double pos)
{
    pos = juce::jlimit(0.0, 1.0, pos);
    seekToTick(pos * totalTicks);
}

void MidiPlayerProcessor::seekToTick(double tick)
{
    tick = juce::jlimit(0.0, totalTicks, tick);
    seekTargetTick.store(tick);
    needsSeekSnapshot.store(true);
    currentTickPosition.store(tick);
    
    // Find the event index at or after this tick
    lastEventIndex = 0;
    for (int i = 0; i < mergedSequence.getNumEvents(); i++)
    {
        if (mergedSequence.getEventPointer(i)->message.getTimeStamp() >= tick)
        {
            lastEventIndex = i;
            break;
        }
        lastEventIndex = i + 1;
    }
}

double MidiPlayerProcessor::getCurrentTimeSeconds() const
{
    return tickToSeconds(currentTickPosition.load());
}

void MidiPlayerProcessor::jumpToMarker(int markerIndex)
{
    if (markerIndex >= 0 && markerIndex < (int)markers.size())
        seekToTick(markers[markerIndex].tick);
}

// =============================================================================
// Send All Notes Off (for seeking / stopping)
// =============================================================================
void MidiPlayerProcessor::sendAllNotesOff(juce::MidiBuffer& buffer, int sampleOffset)
{
    for (int ch = 0; ch < 16; ch++)
    {
        // CC 123 = All Notes Off
        buffer.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 123, 0), sampleOffset);
        // CC 121 = Reset All Controllers
        buffer.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 121, 0), sampleOffset);
        // CC 64 = Sustain Off
        buffer.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 64, 0), sampleOffset + 1);
    }
}

// =============================================================================
// Controller Snapshot (restore channel state after seek)
// Scans backwards from tick to find most recent CC/program per channel
// =============================================================================
void MidiPlayerProcessor::sendControllerSnapshot(juce::MidiBuffer& buffer, int sampleOffset, double atTick)
{
    ChannelState states[16];
    bool hasProgram[16] = {};
    bool hasVolume[16] = {};
    bool hasPan[16] = {};
    bool hasExpression[16] = {};
    
    // Scan all events up to atTick
    for (int i = 0; i < mergedSequence.getNumEvents(); i++)
    {
        auto& msg = mergedSequence.getEventPointer(i)->message;
        if (msg.getTimeStamp() > atTick)
            break;
        
        if (msg.isProgramChange())
        {
            int ch = msg.getChannel() - 1;
            if (ch >= 0 && ch < 16)
            {
                states[ch].program = msg.getProgramChangeNumber();
                hasProgram[ch] = true;
            }
        }
        else if (msg.isController())
        {
            int ch = msg.getChannel() - 1;
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();
            
            if (ch >= 0 && ch < 16)
            {
                switch (cc)
                {
                    case 0:  states[ch].bankMSB = val; break;
                    case 1:  states[ch].modWheel = val; break;
                    case 7:  states[ch].volume = val; hasVolume[ch] = true; break;
                    case 10: states[ch].pan = val; hasPan[ch] = true; break;
                    case 11: states[ch].expression = val; hasExpression[ch] = true; break;
                    case 32: states[ch].bankLSB = val; break;
                    case 64: states[ch].sustain = val; break;
                    case 91: states[ch].reverb = val; break;
                    case 93: states[ch].chorus = val; break;
                }
            }
        }
        else if (msg.isPitchWheel())
        {
            int ch = msg.getChannel() - 1;
            if (ch >= 0 && ch < 16)
                states[ch].pitchBend = msg.getPitchWheelValue();
        }
    }
    
    // Send state for channels that had activity
    int offset = sampleOffset + 2;
    for (int ch = 0; ch < 16; ch++)
    {
        int midiCh = ch + 1;
        
        if (hasProgram[ch])
        {
            buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 0, states[ch].bankMSB), offset);
            buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 32, states[ch].bankLSB), offset);
            buffer.addEvent(juce::MidiMessage::programChange(midiCh, states[ch].program), offset);
        }
        if (hasVolume[ch])
            buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 7, states[ch].volume), offset);
        if (hasPan[ch])
            buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 10, states[ch].pan), offset);
        if (hasExpression[ch])
            buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 11, states[ch].expression), offset);
        
        buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 1, states[ch].modWheel), offset);
        buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 64, states[ch].sustain), offset);
        buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 91, states[ch].reverb), offset);
        buffer.addEvent(juce::MidiMessage::controllerEvent(midiCh, 93, states[ch].chorus), offset);
        buffer.addEvent(juce::MidiMessage::pitchWheel(midiCh, states[ch].pitchBend), offset);
    }
}

// =============================================================================
// processBlock - The Playback Engine
// =============================================================================
void MidiPlayerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear audio (we're MIDI-only)
    buffer.clear();
    
    if (!fileLoaded.load())
    {
        midiMessages.clear();
        return;
    }
    
    const int numSamples = buffer.getNumSamples();
    
    // =========================================================================
    // Handle incoming MIDI (external transport control)
    // =========================================================================
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        
        if (msg.isMidiStart())
            play();
        else if (msg.isMidiStop())
            stop();
        else if (msg.isMidiContinue())
            play();
    }
    
    // Clear input MIDI - we'll fill with our own output
    midiMessages.clear();
    
    // =========================================================================
    // Handle Seek Snapshot
    // =========================================================================
    if (needsSeekSnapshot.exchange(false))
    {
        sendAllNotesOff(midiMessages, 0);
        sendControllerSnapshot(midiMessages, 0, seekTargetTick.load());
    }
    
    // =========================================================================
    // Playback Engine
    // =========================================================================
    if (transportState.load() != PLAYING)
    {
        // Decay channel activity indicators
        for (int i = 0; i < 16; i++)
        {
            channelDecay[i] *= 0.95f;
            if (channelDecay[i] < 0.01f)
                channelActive[i].store(false);
        }
        return;
    }
    
    double tickPos = currentTickPosition.load();
    
    // Get effective BPM
    double effectiveBpm = tempoOverrideBpm.load();
    
    if (syncToMaster.load())
    {
        // Sync to master: read BPM from host/app playhead
        if (auto* pHead = getPlayHead())
        {
            auto posInfo = pHead->getPosition();
            if (posInfo.hasValue())
            {
                auto bpmOpt = posInfo->getBpm();
                if (bpmOpt.hasValue() && *bpmOpt > 0.0)
                    effectiveBpm = *bpmOpt;
            }
        }
    }
    else if (!tempoOverrideEnabled.load())
    {
        // Not synced and no manual override: use MIDI file tempo map
        effectiveBpm = getTempoAtTick(tickPos);
    }
    
    currentBpm.store(effectiveBpm);
    
    // Calculate ticks per sample at current tempo
    double ticksPerSecond = (double)ticksPerBeat * effectiveBpm / 60.0;
    double ticksPerSample = ticksPerSecond / currentSampleRate;
    
    // Process sample by sample for accurate timing
    double endTick = tickPos + ticksPerSample * numSamples;
    
    // Scan events in range [tickPos, endTick)
    int totalEvents = mergedSequence.getNumEvents();
    
    while (lastEventIndex < totalEvents)
    {
        auto* eventPtr = mergedSequence.getEventPointer(lastEventIndex);
        double eventTick = eventPtr->message.getTimeStamp();
        
        if (eventTick >= endTick)
            break;
        
        if (eventTick >= tickPos)
        {
            auto& msg = eventPtr->message;
            
            // Skip meta events (tempo, markers, etc.) - only output MIDI channel messages
            if (!msg.isMetaEvent() && !msg.isSysEx())
            {
                // Filter out muted channels
                bool isMuted = false;
                if (msg.getChannel() > 0 && msg.getChannel() <= 16)
                {
                    int ch = msg.getChannel() - 1;
                    isMuted = channelMuted[ch].load();
                }
                
                if (!isMuted)
                {
                    // Calculate sample offset within this block
                    int sampleOffset = (int)((eventTick - tickPos) / ticksPerSample);
                    sampleOffset = juce::jlimit(0, numSamples - 1, sampleOffset);
                    
                    midiMessages.addEvent(msg, sampleOffset);
                }
                
                // Track channel activity (even if muted, so dots still show)
                if (msg.getChannel() > 0 && msg.getChannel() <= 16)
                {
                    int ch = msg.getChannel() - 1;
                    if (msg.isNoteOn())
                    {
                        channelActive[ch].store(true);
                        channelDecay[ch] = 1.0f;
                    }
                }
            }
            
            // Handle tempo changes from file (if not overriding)
            if (msg.isTempoMetaEvent() && !tempoOverrideEnabled.load())
            {
                double usPerQN = msg.getTempoSecondsPerQuarterNote() * 1000000.0;
                double newBpm = 60000000.0 / usPerQN;
                currentBpm.store(newBpm);
            }
        }
        
        lastEventIndex++;
    }
    
    // Advance position
    currentTickPosition.store(endTick);
    
    // Check for end of file
    if (endTick >= totalTicks)
    {
        if (looping.load())
        {
            // Loop back to start
            sendAllNotesOff(midiMessages, numSamples - 1);
            currentTickPosition.store(0.0);
            lastEventIndex = 0;
            
            // Send controller snapshot for position 0
            needsSeekSnapshot.store(true);
            seekTargetTick.store(0.0);
        }
        else
        {
            transportState.store(STOPPED);
            sendAllNotesOff(midiMessages, numSamples - 1);
            currentTickPosition.store(0.0);
            lastEventIndex = 0;
        }
    }
    
    // Decay channel activity
    for (int i = 0; i < 16; i++)
    {
        channelDecay[i] *= 0.98f;
        if (channelDecay[i] < 0.01f)
            channelActive[i].store(false);
    }
}

// =============================================================================
// GM Instrument Name Lookup
// =============================================================================
juce::String MidiPlayerProcessor::getGMInstrumentName(int program)
{
    static const char* names[] = {
        "Acoustic Grand Piano","Bright Acoustic Piano","Electric Grand Piano","Honky-tonk Piano",
        "Electric Piano 1","Electric Piano 2","Harpsichord","Clavinet",
        "Celesta","Glockenspiel","Music Box","Vibraphone",
        "Marimba","Xylophone","Tubular Bells","Dulcimer",
        "Drawbar Organ","Percussive Organ","Rock Organ","Church Organ",
        "Reed Organ","Accordion","Harmonica","Tango Accordion",
        "Nylon Guitar","Steel Guitar","Jazz Guitar","Clean Guitar",
        "Muted Guitar","Overdriven Guitar","Distortion Guitar","Guitar Harmonics",
        "Acoustic Bass","Finger Bass","Pick Bass","Fretless Bass",
        "Slap Bass 1","Slap Bass 2","Synth Bass 1","Synth Bass 2",
        "Violin","Viola","Cello","Contrabass",
        "Tremolo Strings","Pizzicato Strings","Orchestral Harp","Timpani",
        "String Ensemble 1","String Ensemble 2","Synth Strings 1","Synth Strings 2",
        "Choir Aahs","Voice Oohs","Synth Choir","Orchestra Hit",
        "Trumpet","Trombone","Tuba","Muted Trumpet",
        "French Horn","Brass Section","Synth Brass 1","Synth Brass 2",
        "Soprano Sax","Alto Sax","Tenor Sax","Baritone Sax",
        "Oboe","English Horn","Bassoon","Clarinet",
        "Piccolo","Flute","Recorder","Pan Flute",
        "Blown Bottle","Shakuhachi","Whistle","Ocarina",
        "Lead 1 (square)","Lead 2 (sawtooth)","Lead 3 (calliope)","Lead 4 (chiff)",
        "Lead 5 (charang)","Lead 6 (voice)","Lead 7 (fifths)","Lead 8 (bass+lead)",
        "Pad 1 (new age)","Pad 2 (warm)","Pad 3 (polysynth)","Pad 4 (choir)",
        "Pad 5 (bowed)","Pad 6 (metallic)","Pad 7 (halo)","Pad 8 (sweep)",
        "FX 1 (rain)","FX 2 (soundtrack)","FX 3 (crystal)","FX 4 (atmosphere)",
        "FX 5 (brightness)","FX 6 (goblins)","FX 7 (echoes)","FX 8 (sci-fi)",
        "Sitar","Banjo","Shamisen","Koto",
        "Kalimba","Bagpipe","Fiddle","Shanai",
        "Tinkle Bell","Agogo","Steel Drums","Woodblock",
        "Taiko Drum","Melodic Tom","Synth Drum","Reverse Cymbal",
        "Guitar Fret Noise","Breath Noise","Seashore","Bird Tweet",
        "Telephone Ring","Helicopter","Applause","Gunshot"
    };
    
    if (program >= 0 && program < 128)
        return names[program];
    return "Unknown";
}

// =============================================================================
// Build Channel Info Table (on file load)
// =============================================================================
void MidiPlayerProcessor::buildChannelInfoTable()
{
    channelInfoTable.clear();
    
    int programs[16];
    int bankMSBs[16];
    int bankLSBs[16];
    int noteCounts[16];
    bool hasProgram[16];
    
    for (int i = 0; i < 16; i++)
    {
        programs[i] = 0;
        bankMSBs[i] = 0;
        bankLSBs[i] = 0;
        noteCounts[i] = 0;
        hasProgram[i] = false;
    }
    
    for (int i = 0; i < mergedSequence.getNumEvents(); i++)
    {
        auto& msg = mergedSequence.getEventPointer(i)->message;
        
        if (msg.isProgramChange())
        {
            int ch = msg.getChannel() - 1;
            if (ch >= 0 && ch < 16)
            {
                programs[ch] = msg.getProgramChangeNumber();
                hasProgram[ch] = true;
            }
        }
        else if (msg.isController())
        {
            int ch = msg.getChannel() - 1;
            if (ch >= 0 && ch < 16)
            {
                if (msg.getControllerNumber() == 0)
                    bankMSBs[ch] = msg.getControllerValue();
                else if (msg.getControllerNumber() == 32)
                    bankLSBs[ch] = msg.getControllerValue();
            }
        }
        else if (msg.isNoteOn())
        {
            int ch = msg.getChannel() - 1;
            if (ch >= 0 && ch < 16)
                noteCounts[ch]++;
        }
    }
    
    for (int ch = 0; ch < 16; ch++)
    {
        if (noteCounts[ch] > 0 || hasProgram[ch])
        {
            ChannelInfo info;
            info.channel = ch;
            info.program = hasProgram[ch] ? programs[ch] : -1;
            info.bankMSB = bankMSBs[ch];
            info.bankLSB = bankLSBs[ch];
            info.noteCount = noteCounts[ch];
            
            if (ch == 9)
                info.instrumentName = "Drums / Percussion";
            else if (hasProgram[ch])
                info.instrumentName = getGMInstrumentName(programs[ch]);
            else
                info.instrumentName = "(no program change)";
            
            channelInfoTable.push_back(info);
        }
    }
}

// =============================================================================
// State Persistence
// =============================================================================
void MidiPlayerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree vt("MidiPlayerState");
    vt.setProperty("filePath", loadedFile.getFullPathName(), nullptr);
    vt.setProperty("looping", looping.load(), nullptr);
    vt.setProperty("tempoOverride", tempoOverrideEnabled.load(), nullptr);
    vt.setProperty("tempoOverrideBpm", tempoOverrideBpm.load(), nullptr);
    vt.setProperty("syncToMaster", syncToMaster.load(), nullptr);
    
    // Save channel mute states
    int muteMask = 0;
    for (int i = 0; i < 16; i++)
        if (channelMuted[i].load()) muteMask |= (1 << i);
    vt.setProperty("channelMuteMask", muteMask, nullptr);
    
    juce::MemoryOutputStream stream(destData, false);
    vt.writeToStream(stream);
}

void MidiPlayerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto vt = juce::ValueTree::readFromData(data, sizeInBytes);
    if (vt.isValid())
    {
        juce::String filePath = vt.getProperty("filePath", "").toString();
        if (filePath.isNotEmpty())
        {
            juce::File file(filePath);
            if (file.existsAsFile())
                loadFile(file);
        }
        
        looping.store((bool)vt.getProperty("looping", false));
        tempoOverrideEnabled.store((bool)vt.getProperty("tempoOverride", false));
        tempoOverrideBpm.store((double)vt.getProperty("tempoOverrideBpm", 120.0));
        syncToMaster.store((bool)vt.getProperty("syncToMaster", true));
        
        // Restore channel mute states
        int muteMask = (int)vt.getProperty("channelMuteMask", 0);
        for (int i = 0; i < 16; i++)
            channelMuted[i].store((muteMask >> i) & 1);
    }
}





