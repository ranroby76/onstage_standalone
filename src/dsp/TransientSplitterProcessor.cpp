// ==============================================================================
//  TransientSplitterProcessor.cpp
//  OnStage — Transient/Sustain splitter DSP implementation
//
//  Adapted from Colosseum. Zero-latency envelope follower transient detection.
//  2-in → 4-out: Transient L/R (ch 0-1), Sustain L/R (ch 2-3).
// ==============================================================================

#include "TransientSplitterProcessor.h"

void TransientSplitterProcessor::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Pre-allocate temp buffer
    tempBuffer.setSize (2, samplesPerBlock, false, false, true);

    // Reset envelope states
    fastEnvL = fastEnvR = 0.0f;
    slowEnvL = slowEnvR = 0.0f;
    gateL = gateR = 0.0f;
    smoothGateL = smoothGateR = 0.0f;
    holdCounterL = holdCounterR = 0;

    // Prepare detection filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 1;

    detHPFilterL.prepare (spec);  detHPFilterR.prepare (spec);
    detLPFilterL.prepare (spec);  detLPFilterR.prepare (spec);
    detHPFilterL.reset();         detHPFilterR.reset();
    detLPFilterL.reset();         detLPFilterR.reset();

    lastHPFreq = -1.0f;   // Force filter update
    lastLPFreq = -1.0f;
    updateDetectionFilters();
}

void TransientSplitterProcessor::reset()
{
    fastEnvL = fastEnvR = 0.0f;
    slowEnvL = slowEnvR = 0.0f;
    gateL = gateR = 0.0f;
    smoothGateL = smoothGateR = 0.0f;
    holdCounterL = holdCounterR = 0;
    detHPFilterL.reset();  detHPFilterR.reset();
    detLPFilterL.reset();  detLPFilterR.reset();
}

void TransientSplitterProcessor::updateDetectionFilters()
{
    float hp = juce::jlimit (20.0f, 20000.0f, focusHPFreq.load());
    float lp = juce::jlimit (20.0f, 20000.0f, focusLPFreq.load());

    float nyquist = (float)(currentSampleRate * 0.49);
    hp = juce::jmin (hp, nyquist);
    lp = juce::jmin (lp, nyquist);

    if (std::abs (hp - lastHPFreq) > 0.1f)
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hp);
        *detHPFilterL.coefficients = *coeffs;
        *detHPFilterR.coefficients = *coeffs;
        lastHPFreq = hp;
    }

    if (std::abs (lp - lastLPFreq) > 0.1f)
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, lp);
        *detLPFilterL.coefficients = *coeffs;
        *detLPFilterR.coefficients = *coeffs;
        lastLPFreq = lp;
    }
}

void TransientSplitterProcessor::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples    = buffer.getNumSamples();
    const int totalChannels = buffer.getNumChannels();

    // Need at least 4 channels (graph should provide this for 2-in/4-out node)
    if (totalChannels < 4) return;

    updateDetectionFilters();

    // Load parameters once per block
    const float sens      = juce::jlimit (0.01f, 1.0f, sensitivity.load());
    const float decayMs   = juce::jlimit (1.0f, 500.0f, decay.load());
    const float holdMs    = juce::jlimit (0.0f, 100.0f, holdTime.load());
    const float smoothMs  = juce::jlimit (0.1f, 50.0f, smoothing.load());
    const float bal       = juce::jlimit (-1.0f, 1.0f, balance.load());
    const float tGainDb   = juce::jlimit (-60.0f, 12.0f, transientGainDb.load());
    const float sGainDb   = juce::jlimit (-60.0f, 12.0f, sustainGainDb.load());
    const bool  linked    = stereoLinked.load();
    const bool  gate      = gateMode.load();
    const bool  invert    = invertMode.load();

    // Linear gains
    const float tGain = juce::Decibels::decibelsToGain (tGainDb, -60.0f);
    const float sGain = juce::Decibels::decibelsToGain (sGainDb, -60.0f);

    // Envelope coefficients
    const float fastAttackCoeff  = msToCoeff (0.2f);
    const float fastReleaseCoeff = msToCoeff (5.0f);
    const float slowAttackCoeff  = msToCoeff (20.0f);
    const float slowReleaseCoeff = msToCoeff (100.0f);
    const float smoothCoeff      = msToCoeff (smoothMs);

    // Sensitivity → threshold: higher sensitivity = lower threshold
    const float threshold = 1.0f + (1.0f - sens) * 4.0f;   // 1.0 to 5.0

    // Decay coefficient
    const float decayCoeff = msToCoeff (decayMs);

    // Hold in samples
    const int holdSamples = (int)(holdMs * 0.001f * (float)currentSampleRate);

    // Balance → transient/sustain multipliers
    float balTransient = 1.0f, balSustain = 1.0f;
    if (bal < 0.0f)  balSustain  = 1.0f + bal;
    if (bal > 0.0f)  balTransient = 1.0f - bal;

    // Save input to temp buffer (because we overwrite channels 0-1 with transient output)
    if (tempBuffer.getNumSamples() < numSamples)
        tempBuffer.setSize (2, numSamples, false, false, true);

    juce::FloatVectorOperations::copy (tempBuffer.getWritePointer (0), buffer.getReadPointer (0), numSamples);
    juce::FloatVectorOperations::copy (tempBuffer.getWritePointer (1), buffer.getReadPointer (1), numSamples);

    const float* tempL = tempBuffer.getReadPointer (0);
    const float* tempR = tempBuffer.getReadPointer (1);

    // Output pointers
    float* outTransL = buffer.getWritePointer (0);
    float* outTransR = buffer.getWritePointer (1);
    float* outSustL  = buffer.getWritePointer (2);
    float* outSustR  = buffer.getWritePointer (3);

    // Metering accumulators
    float tRmsAccL = 0.0f, tRmsAccR = 0.0f;
    float sRmsAccL = 0.0f, sRmsAccR = 0.0f;
    float maxActivity = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = tempL[i];
        float inR = tempR[i];

        // === Detection sidechain (filtered for detection only) ===
        float detL = detHPFilterL.processSample (inL);
        detL       = detLPFilterL.processSample (detL);
        float detR = detHPFilterR.processSample (inR);
        detR       = detLPFilterR.processSample (detR);

        float absDetL = std::abs (detL);
        float absDetR = std::abs (detR);

        if (linked)
        {
            float absDet = juce::jmax (absDetL, absDetR);
            absDetL = absDetR = absDet;
        }

        // === Fast envelope (tracks transients) ===
        if (absDetL > fastEnvL)
            fastEnvL = absDetL + fastAttackCoeff * (fastEnvL - absDetL);
        else
            fastEnvL = absDetL + fastReleaseCoeff * (fastEnvL - absDetL);

        if (absDetR > fastEnvR)
            fastEnvR = absDetR + fastAttackCoeff * (fastEnvR - absDetR);
        else
            fastEnvR = absDetR + fastReleaseCoeff * (fastEnvR - absDetR);

        // === Slow envelope (tracks average level) ===
        if (absDetL > slowEnvL)
            slowEnvL = absDetL + slowAttackCoeff * (slowEnvL - absDetL);
        else
            slowEnvL = absDetL + slowReleaseCoeff * (slowEnvL - absDetL);

        if (absDetR > slowEnvR)
            slowEnvR = absDetR + slowAttackCoeff * (slowEnvR - absDetR);
        else
            slowEnvR = absDetR + slowReleaseCoeff * (slowEnvR - absDetR);

        // === Transient detection: ratio of fast/slow ===
        constexpr float epsilon = 1e-10f;
        float ratioL = fastEnvL / juce::jmax (slowEnvL, epsilon);
        float ratioR = fastEnvR / juce::jmax (slowEnvR, epsilon);

        // === Gate logic with hold ===
        if (ratioL > threshold)       { gateL = 1.0f; holdCounterL = holdSamples; }
        else if (holdCounterL > 0)    { --holdCounterL; gateL = 1.0f; }
        else                          { gateL *= decayCoeff; if (gateL < 0.001f) gateL = 0.0f; }

        if (ratioR > threshold)       { gateR = 1.0f; holdCounterR = holdSamples; }
        else if (holdCounterR > 0)    { --holdCounterR; gateR = 1.0f; }
        else                          { gateR *= decayCoeff; if (gateR < 0.001f) gateR = 0.0f; }

        // === Gate mode: snap to 0 or 1 ===
        float finalGateL = gate ? (gateL > 0.5f ? 1.0f : 0.0f) : gateL;
        float finalGateR = gate ? (gateR > 0.5f ? 1.0f : 0.0f) : gateR;

        // === Smoothing ===
        smoothGateL += (finalGateL - smoothGateL) * (1.0f - smoothCoeff);
        smoothGateR += (finalGateR - smoothGateR) * (1.0f - smoothCoeff);

        float sgL = juce::jlimit (0.0f, 1.0f, smoothGateL);
        float sgR = juce::jlimit (0.0f, 1.0f, smoothGateR);

        // === Invert mode ===
        if (invert) { sgL = 1.0f - sgL; sgR = 1.0f - sgR; }

        // === Split and apply gains ===
        float transL = inL * sgL       * tGain * balTransient;
        float transR = inR * sgR       * tGain * balTransient;
        float sustL  = inL * (1.0f - sgL) * sGain * balSustain;
        float sustR  = inR * (1.0f - sgR) * sGain * balSustain;

        outTransL[i] = transL;
        outTransR[i] = transR;
        outSustL[i]  = sustL;
        outSustR[i]  = sustR;

        // Metering
        tRmsAccL += transL * transL;
        tRmsAccR += transR * transR;
        sRmsAccL += sustL  * sustL;
        sRmsAccR += sustR  * sustR;
        maxActivity = juce::jmax (maxActivity, sgL, sgR);
    }

    // Update meters
    if (numSamples > 0)
    {
        float invN = 1.0f / (float)numSamples;
        transientRmsL.store    (std::sqrt (tRmsAccL * invN));
        transientRmsR.store    (std::sqrt (tRmsAccR * invN));
        sustainRmsL.store      (std::sqrt (sRmsAccL * invN));
        sustainRmsR.store      (std::sqrt (sRmsAccR * invN));
        transientActivity.store (maxActivity);
    }
}

// ==============================================================================
//  State serialisation
// ==============================================================================

void TransientSplitterProcessor::getState (juce::MemoryBlock& dest) const
{
    juce::XmlElement xml ("TransientSplitterState");

    xml.setAttribute ("sensitivity",      (double) sensitivity.load());
    xml.setAttribute ("decay",            (double) decay.load());
    xml.setAttribute ("holdTime",         (double) holdTime.load());
    xml.setAttribute ("smoothing",        (double) smoothing.load());
    xml.setAttribute ("focusHPFreq",      (double) focusHPFreq.load());
    xml.setAttribute ("focusLPFreq",      (double) focusLPFreq.load());
    xml.setAttribute ("transientGainDb",  (double) transientGainDb.load());
    xml.setAttribute ("sustainGainDb",    (double) sustainGainDb.load());
    xml.setAttribute ("balance",          (double) balance.load());
    xml.setAttribute ("stereoLinked",     stereoLinked.load());
    xml.setAttribute ("gateMode",         gateMode.load());
    xml.setAttribute ("invertMode",       invertMode.load());

    juce::MemoryOutputStream stream (dest, false);
    xml.writeTo (stream);
}

void TransientSplitterProcessor::setState (const void* data, int size)
{
    auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 ((const char*) data, size));
    if (! xml || ! xml->hasTagName ("TransientSplitterState")) return;

    sensitivity.store      ((float) xml->getDoubleAttribute ("sensitivity",      0.5));
    decay.store            ((float) xml->getDoubleAttribute ("decay",            50.0));
    holdTime.store         ((float) xml->getDoubleAttribute ("holdTime",         10.0));
    smoothing.store        ((float) xml->getDoubleAttribute ("smoothing",        2.0));
    focusHPFreq.store      ((float) xml->getDoubleAttribute ("focusHPFreq",      20.0));
    focusLPFreq.store      ((float) xml->getDoubleAttribute ("focusLPFreq",      20000.0));
    transientGainDb.store  ((float) xml->getDoubleAttribute ("transientGainDb",  0.0));
    sustainGainDb.store    ((float) xml->getDoubleAttribute ("sustainGainDb",    0.0));
    balance.store          ((float) xml->getDoubleAttribute ("balance",          0.0));
    stereoLinked.store     (xml->getBoolAttribute ("stereoLinked", true));
    gateMode.store         (xml->getBoolAttribute ("gateMode",    false));
    invertMode.store       (xml->getBoolAttribute ("invertMode",  false));
}
