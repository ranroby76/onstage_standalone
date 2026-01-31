#include "SimpleConnectorProcessor.h"

SimpleConnectorProcessor::SimpleConnectorProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void SimpleConnectorProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {
    inputRms.resize(2, 0.0f);
    outputRms.resize(2, 0.0f);
}

void SimpleConnectorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    // Input metering
    for (int i = 0; i < juce::jmin(2, buffer.getNumChannels()); ++i) {
        inputRms[i] = buffer.getRMSLevel(i, 0, buffer.getNumSamples());
    }
    
    // Process audio
    if (muted.load()) {
        buffer.clear();
    } else {
        float gain = normalizedToGain(volumeNormalized.load());
        buffer.applyGain(gain);
    }
    
    // Output metering
    for (int i = 0; i < juce::jmin(2, buffer.getNumChannels()); ++i) {
        outputRms[i] = buffer.getRMSLevel(i, 0, buffer.getNumSamples());
    }
}

bool SimpleConnectorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void SimpleConnectorProcessor::setVolume(float normalizedValue) {
    volumeNormalized.store(juce::jlimit(0.0f, 1.0f, normalizedValue));
}

float SimpleConnectorProcessor::getVolumeDb() const {
    float normalized = volumeNormalized.load();
    
    if (normalized <= 0.0f)
        return -100.0f;
    
    if (normalized <= 0.5f) {
        float t = normalized / 0.5f;
        return juce::jmap(t, 0.0f, 1.0f, -60.0f, 0.0f);
    } else {
        float t = (normalized - 0.5f) / 0.5f;
        return juce::jmap(t, 0.0f, 1.0f, 0.0f, 25.0f);
    }
}

float SimpleConnectorProcessor::normalizedToGain(float normalized) const {
    if (normalized <= 0.0f)
        return 0.0f;
    
    float db = getVolumeDb();
    return juce::Decibels::decibelsToGain(db);
}

void SimpleConnectorProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream stream(destData, false);
    stream.writeFloat(volumeNormalized.load());
    stream.writeBool(muted.load());
}

void SimpleConnectorProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= (int)sizeof(float)) {
        volumeNormalized.store(stream.readFloat());
    }
    if (sizeInBytes >= (int)(sizeof(float) + sizeof(bool))) {
        muted.store(stream.readBool());
    }
}
