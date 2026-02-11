// ==============================================================================
//  GuitarWahProcessor.h
//  OnStage - Wah-Wah effect
//
//  Modeled after the Dunlop CryBaby GCB-95 circuit analysis
//  (ElectroSmash / public domain measurements):
//  - Resonant bandpass filter sweeping 450 Hz – 1.6 kHz
//  - Q factor ~7.9 (resonant peak, ~18 dB boost at center)
//  - Log-taper pedal sweep (matching 100k audio pot curve)
//
//  Filter: Cytomic (Andy Simper) SVF in bandpass mode — stable under
//  fast modulation, correct at all frequencies up to Nyquist.
//
//  Three control modes:
//    Manual  — Pedal position from slider / MIDI CC
//    Auto    — Envelope follower drives sweep (touch-sensitive)
//    LFO     — Triangle oscillator drives sweep (rhythmic wah)
//
//  Three wah models:
//    CryBaby   — 450 Hz – 1.6 kHz, Q 7.9 (classic GCB-95)
//    Boutique  — 350 Hz – 2.5 kHz, Q 10  (vocal, peaky)
//    FullRange — 200 Hz – 5.0 kHz, Q 5   (wide, modern)
//
//  Parameters (8):
//    Pedal    — Manual sweep position (0..1)
//    Mode     — 0=Manual, 1=Auto, 2=LFO
//    Model    — 0=CryBaby, 1=Boutique, 2=FullRange
//    Q        — Resonance override (1..15, default per model)
//    Sens     — Auto-wah envelope sensitivity (0..1)
//    Attack   — Envelope attack time (0..1: fast..slow)
//    LFO Rate — LFO speed in Hz (0.1..10)
//    Mix      — Dry/wet blend (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class GuitarWahProcessor
{
public:
    struct Params
    {
        float pedal    = 0.5f;   // 0..1
        int   mode     = 0;      // 0=Manual, 1=Auto, 2=LFO
        int   model    = 0;      // 0=CryBaby, 1=Boutique, 2=FullRange
        float q        = 7.9f;   // 1..15
        float sens     = 0.7f;   // 0..1
        float attack   = 0.3f;   // 0..1
        float lfoRate  = 1.0f;   // 0.1..10 Hz
        float mix      = 1.0f;   // 0..1

        bool operator==(const Params& o) const
        {
            return pedal == o.pedal && mode == o.mode && model == o.model
                && q == o.q && sens == o.sens && attack == o.attack
                && lfoRate == o.lfoRate && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarWahProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            ic1eq[ch] = 0.0f; // SVF integrator state 1 (bandpass)
            ic2eq[ch] = 0.0f; // SVF integrator state 2 (lowpass)
        }
        envFollower = 0.0f;
        lfoPhase = 0.0f;
        smoothedPedal = 0.5f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // Get model frequency range
        float freqLo, freqHi;
        getModelRange(params.model, freqLo, freqHi);

        const float qVal   = juce::jlimit(1.0f, 15.0f, params.q);
        const float mixWet = juce::jlimit(0.0f, 1.0f, params.mix);
        const float mixDry = 1.0f - mixWet;

        // Envelope follower coefficients
        // attack: 0 → 1 ms (fast), 1 → 50 ms (slow)
        // release: 5× attack (wah pedal spring-back feel)
        float attackMs  = 1.0f + params.attack * 49.0f;
        float releaseMs = attackMs * 5.0f;
        float envAtt = std::exp(-1.0f / ((float)sampleRate * attackMs * 0.001f));
        float envRel = std::exp(-1.0f / ((float)sampleRate * releaseMs * 0.001f));

        // LFO
        float lfoInc = juce::jlimit(0.1f, 10.0f, params.lfoRate) / (float)sampleRate;

        // Pedal smoothing (~5 ms)
        float pedSmooth = std::exp(-1.0f / ((float)sampleRate * 0.005f));

        for (int i = 0; i < numSamples; ++i)
        {
            // --- Determine sweep position (0..1) ---
            float sweep = 0.5f;

            switch (params.mode)
            {
                case 0: // Manual
                {
                    smoothedPedal += (1.0f - pedSmooth) * (params.pedal - smoothedPedal);
                    sweep = smoothedPedal;
                    break;
                }
                case 1: // Auto-wah
                {
                    // Mono envelope from input
                    float absIn = 0.0f;
                    for (int ch = 0; ch < numChannels; ++ch)
                        absIn += std::abs(buffer.getSample(ch, i));
                    absIn /= (float)numChannels;

                    // Asymmetric envelope follower
                    if (absIn > envFollower)
                        envFollower += (1.0f - envAtt) * (absIn - envFollower);
                    else
                        envFollower += (1.0f - envRel) * (absIn - envFollower);

                    // Map to sweep: sensitivity scales the envelope
                    // Guitar signal is typically -1..+1 peak, RMS ~0.05..0.3
                    // Scale by 10 to get useful range, sens controls amount
                    sweep = juce::jlimit(0.0f, 1.0f,
                                         envFollower * juce::jlimit(0.0f, 1.0f, params.sens) * 10.0f);
                    break;
                }
                case 2: // LFO
                {
                    // Triangle wave (smooth rocking motion)
                    sweep = (lfoPhase < 0.5f) ? (lfoPhase * 2.0f) : (2.0f - lfoPhase * 2.0f);

                    lfoPhase += lfoInc;
                    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                    break;
                }
            }

            // --- Map sweep to frequency (log taper like real wah pot) ---
            // CryBaby uses a 100k audio (log) pot:
            // freq = freqLo * (freqHi/freqLo)^sweep
            float freq = freqLo * std::pow(freqHi / freqLo, sweep);
            freq = juce::jlimit(20.0f, (float)(sampleRate * 0.45), freq);

            // --- Cytomic SVF (Andy Simper, 2012) ---
            // Topology-preserving, stable under fast modulation.
            //   g  = tan(pi * fc / fs)
            //   k  = 1 / Q
            //   a1 = 1 / (1 + g*(g + k))
            //   a2 = g * a1
            //   a3 = g * a2
            //
            // Per sample:
            //   v3 = v0 - ic2eq
            //   v1 = a1*ic1eq + a2*v3     (bandpass)
            //   v2 = ic2eq   + a2*ic1eq + a3*v3  (lowpass)
            //   ic1eq = 2*v1 - ic1eq
            //   ic2eq = 2*v2 - ic2eq
            //
            float g  = std::tan(pi * freq / (float)sampleRate);
            float k  = 1.0f / qVal;
            float a1 = 1.0f / (1.0f + g * (g + k));
            float a2 = g * a1;
            float a3 = g * a2;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float v0 = data[i];

                float v3 = v0 - ic2eq[ch];
                float v1 = a1 * ic1eq[ch] + a2 * v3;           // bandpass
                float v2 = ic2eq[ch] + a2 * ic1eq[ch] + a3 * v3; // lowpass

                ic1eq[ch] = 2.0f * v1 - ic1eq[ch];
                ic2eq[ch] = 2.0f * v2 - ic2eq[ch];

                // Output: bandpass (v1)
                // SVF bandpass gain at resonance ≈ Q, which matches
                // the CryBaby's ~18 dB peak boost (Q=7.9 → 18 dB)
                float wahOut = v1;

                data[i] = v0 * mixDry + wahOut * mixWet;
            }
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

    static const char* getModeName(int mode)
    {
        static const char* names[] = { "Manual", "Auto", "LFO" };
        if (mode >= 0 && mode < 3) return names[mode];
        return "Manual";
    }

    static const char* getModelName(int model)
    {
        static const char* names[] = { "CryBaby", "Boutique", "FullRange" };
        if (model >= 0 && model < 3) return names[model];
        return "CryBaby";
    }

private:
    static constexpr float pi = 3.141592653589793f;

    static void getModelRange(int model, float& lo, float& hi)
    {
        switch (model)
        {
            default:
            case 0: lo = 450.0f;  hi = 1600.0f; break;  // CryBaby GCB-95
            case 1: lo = 350.0f;  hi = 2500.0f; break;  // Boutique (vocal, wider)
            case 2: lo = 200.0f;  hi = 5000.0f; break;  // Full Range (modern)
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    // Cytomic SVF state per channel
    float ic1eq[2] = {}; // bandpass integrator state
    float ic2eq[2] = {}; // lowpass integrator state

    // Envelope follower for auto-wah
    float envFollower = 0.0f;

    // LFO
    float lfoPhase = 0.0f;

    // Smoothed pedal
    float smoothedPedal = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarWahProcessor)
};
