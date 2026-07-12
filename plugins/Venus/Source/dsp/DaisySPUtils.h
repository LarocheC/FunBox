// DaisySPUtils.h
//
// Self-contained, faithful ports of the three DaisySP classes that the Venus
// pedal firmware uses (Tone, SampleRateReducer, Oscillator).  The algorithms
// are copied verbatim from DaisySP (commit d54d8753, the exact version the
// Funbox repo pins) so the plugin's LoFi filtering and "drift" modulation
// behave identically to the hardware.  Vendoring these tiny pieces keeps the
// plugin build dependent only on JUCE.
//
// Per-class upstream licenses are preserved below.
//   * Tone              -> LGPL v2.1  (DaisySP-LGPL/Source/Filters/tone.cpp)
//   * SampleRateReducer -> MIT        (DaisySP/Source/Effects/sampleratereducer.cpp)
//   * Oscillator        -> MIT        (DaisySP/Source/Synthesis/oscillator.cpp)

#pragma once

#include <cmath>
#include <cstdint>

namespace venusdsp
{
// ---------------------------------------------------------------------------
// Constants / helpers copied from DaisySP/Source/Utility/dsp.h (MIT)
// ---------------------------------------------------------------------------
#ifndef VENUS_PI_F
#define VENUS_PI_F 3.1415927410125732421875f
#endif
#ifndef VENUS_TWOPI_F
#define VENUS_TWOPI_F (2.0f * VENUS_PI_F)
#endif

inline float fclampf(float in, float min, float max)
{
    return in < min ? min : (in > max ? max : in);
}

// Ported from pichenettes/eurorack/plaits/dsp/oscillator/oscillator.h
inline float ThisBlepSample(float t)
{
    return 0.5f * t * t;
}
inline float NextBlepSample(float t)
{
    t = 1.0f - t;
    return -0.5f * t * t;
}

// ---------------------------------------------------------------------------
// Tone : first-order recursive low-pass filter
// Copyright (c) 2023 Electrosmith, Corp, Barry Vercoe, John FFitch,
//                    Gabriel Maldonado.  LGPL v2.1.
// (DaisySP-LGPL/Source/Filters/tone.{h,cpp})
// ---------------------------------------------------------------------------
class Tone
{
  public:
    Tone() {}
    ~Tone() {}

    void Init(float sample_rate)
    {
        prevout_     = 0.0f;
        freq_        = 100.0f;
        c1_          = 0.5f;
        c2_          = 0.5f;
        sample_rate_ = sample_rate;
    }

    float Process(float in)
    {
        float out;
        out      = c1_ * in + c2_ * prevout_;
        prevout_ = out;
        return out;
    }

    inline void SetFreq(float freq)
    {
        freq_ = freq;
        CalculateCoefficients();
    }

    inline float GetFreq() { return freq_; }

  private:
    void CalculateCoefficients()
    {
        float b, c1, c2;
        b   = 2.0f - cosf(VENUS_TWOPI_F * freq_ / sample_rate_);
        c2  = b - sqrtf(b * b - 1.0f);
        c1  = 1.0f - c2;
        c1_ = c1;
        c2_ = c2;
    }

    float out_ = 0.0f, prevout_ = 0.0f, in_ = 0.0f, freq_ = 100.0f;
    float c1_ = 0.5f, c2_ = 0.5f, sample_rate_ = 48000.0f;
};

// ---------------------------------------------------------------------------
// SampleRateReducer : band-limited sample-rate decimator
// Copyright (c) 2020 Electrosmith, Corp, Emilie Gillet.  MIT.
// (DaisySP/Source/Effects/sampleratereducer.{h,cpp})
// ---------------------------------------------------------------------------
class SampleRateReducer
{
  public:
    SampleRateReducer() {}
    ~SampleRateReducer() {}

    void Init()
    {
        frequency_       = 0.2f;
        phase_           = 0.0f;
        sample_          = 0.0f;
        next_sample_     = 0.0f;
        previous_sample_ = 0.0f;
    }

    float Process(float in)
    {
        float this_sample = next_sample_;
        next_sample_      = 0.f;
        phase_ += frequency_;
        if(phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
            float t = phase_ / frequency_;
            // t = 0: the transition occurred right at this sample.
            // t = 1: the transition occurred at the previous sample.
            // Use linear interpolation to recover the fractional sample.
            float new_sample
                = previous_sample_ + (in - previous_sample_) * (1.0f - t);
            float discontinuity = new_sample - sample_;
            this_sample += discontinuity * ThisBlepSample(t);
            next_sample_ = discontinuity * NextBlepSample(t);
            sample_      = new_sample;
        }
        next_sample_ += sample_;
        previous_sample_ = in;

        return this_sample;
    }

    void SetFreq(float frequency) { frequency_ = fclampf(frequency, 0.f, 1.f); }

  private:
    float frequency_       = 0.2f;
    float phase_           = 0.0f;
    float sample_          = 0.0f;
    float previous_sample_ = 0.0f;
    float next_sample_     = 0.0f;
};

// ---------------------------------------------------------------------------
// Oscillator : naive waveform oscillator (used here only for the slow "drift"
// LFOs, waveforms WAVE_SIN and WAVE_TRI).  The band-limited POLYBLEP waveforms
// from the DaisySP original are omitted because Venus never selects them.
// Copyright (c) 2020 Electrosmith, Corp.  MIT.
// (DaisySP/Source/Synthesis/oscillator.{h,cpp})
// ---------------------------------------------------------------------------
class Oscillator
{
  public:
    Oscillator() {}
    ~Oscillator() {}

    enum
    {
        WAVE_SIN,
        WAVE_TRI,
        WAVE_SAW,
        WAVE_RAMP,
        WAVE_SQUARE,
        WAVE_LAST,
    };

    void Init(float sample_rate)
    {
        sr_        = sample_rate;
        sr_recip_  = 1.0f / sample_rate;
        freq_      = 100.0f;
        amp_       = 0.5f;
        pw_        = 0.5f;
        phase_     = 0.0f;
        phase_inc_ = CalcPhaseInc(freq_);
        waveform_  = WAVE_SIN;
        eoc_       = true;
        eor_       = true;
    }

    inline void SetFreq(const float f)
    {
        freq_      = f;
        phase_inc_ = CalcPhaseInc(f);
    }

    inline void SetAmp(const float a) { amp_ = a; }

    inline void SetWaveform(const uint8_t wf)
    {
        waveform_ = wf < WAVE_LAST ? wf : WAVE_SIN;
    }

    inline void Reset(float _phase = 0.0f) { phase_ = _phase; }

    float Process()
    {
        float out, t;
        switch(waveform_)
        {
            case WAVE_SIN: out = sinf(phase_ * VENUS_TWOPI_F); break;
            case WAVE_TRI:
                t   = -1.0f + (2.0f * phase_);
                out = 2.0f * (fabsf(t) - 0.5f);
                break;
            case WAVE_SAW: out = -1.0f * (((phase_ * 2.0f)) - 1.0f); break;
            case WAVE_RAMP: out = ((phase_ * 2.0f)) - 1.0f; break;
            case WAVE_SQUARE: out = phase_ < pw_ ? (1.0f) : -1.0f; break;
            default: out = 0.0f; break;
        }
        phase_ += phase_inc_;
        if(phase_ > 1.0f)
        {
            phase_ -= 1.0f;
            eoc_ = true;
        }
        else
        {
            eoc_ = false;
        }
        eor_ = (phase_ - phase_inc_ < 0.5f && phase_ >= 0.5f);

        return out * amp_;
    }

  private:
    float CalcPhaseInc(float f) { return f * sr_recip_; }

    uint8_t waveform_ = WAVE_SIN;
    float   amp_ = 0.5f, freq_ = 100.0f, pw_ = 0.5f;
    float   sr_ = 48000.0f, sr_recip_ = 1.0f / 48000.0f, phase_ = 0.0f, phase_inc_ = 0.0f;
    float   last_out_ = 0.0f, last_freq_ = 0.0f;
    bool    eor_ = true, eoc_ = true;
};

} // namespace venusdsp
