// VenusEngine.h
//
// A faithful port of the GuitarML Funbox "Venus" spectral reverb
// (software/Venus/venus.cpp) to a host-samplerate, real-time engine class.
//
// The DSP is a 4x-overlap STFT (N = 4096, Hann window) feeding a
// frequency-domain "energy reverb": per-bin energy is accumulated, damped,
// resynthesised with a random phase, and morphed by octave / octave+5th
// "shimmer" and detune spreading.  A spectral "freeze", two LoFi modes
// (sample-rate reduction + tone lowpass) and four slow "drift" LFOs round it
// out.  See README.md for the mapping from pedal controls to plugin params
// and for the two documented, sample-rate-related deviations.
//
// Everything that was a global in venus.cpp is a member here so that multiple
// plugin instances can run independently.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>
#include <memory>
#include <functional>

#include "dsp/shy_fft.h"
#include "dsp/wave.h"
#include "dsp/fourier.h"
#include "dsp/DaisySPUtils.h"

class VenusEngine
{
public:
    VenusEngine() = default;
    ~VenusEngine() = default;

    // Allocate buffers and (re)initialise all DSP.  Call from prepareToPlay.
    void prepare (double sampleRate);

    // Clear the reverb tail and STFT buffers (silence).  Real-time safe.
    void reset();

    // Per-block control update.  Arguments use the pedal's effective knob
    // ranges so the internal maths is identical to venus.cpp::ProcessControls.
    //   decayKnob   0..1   -> vdecay        = decayKnob * 99 + 1
    //   dampKnob    0..1   -> vdamp         = dampKnob^2   (Parameter::EXPONENTIAL)
    //   shimmerKnob 0..1   -> vshimmer      = shimmerKnob * 0.1
    //   toneKnob    0..1   -> vshimmer_tone = toneKnob * 0.3
    //   detuneBip  -1..1   -> vdetune_temp  = detuneBip * 0.15  (centre = none)
    //   shimmerMode 0=oct down, 1=oct up, 2=both
    //   lofiMode    0=less lofi (SR down + 8k lowpass), 1=off, 2=more lofi
    //   driftMode   0=slow (sine), 1=off, 2=fast (triangle)
    void setParameters (float decayKnob, float dampKnob, float shimmerKnob,
                        float toneKnob, float detuneBip,
                        int shimmerMode, int lofiMode, int driftMode, bool freeze);

    // Process one mono input sample, returning the (LoFi-processed) wet sample.
    // The dry/wet mix is done by the caller so the dry stereo image is kept.
    inline float processSample (float in)
    {
        // Advance the four "drift" LFOs once per sample, exactly as the pedal
        // does inside its audio loop.  Their values are consumed at the top of
        // the *next* block by setParameters() (i.e. one block of latency, as on
        // the hardware).
        driftMultiplier_[0] = driftOsc_[0].Process();
        driftMultiplier_[1] = driftOsc_[1].Process();
        driftMultiplier_[2] = driftOsc_[2].Process();
        driftMultiplier_[3] = driftOsc_[3].Process();

        stft_->write (in);
        const float raw = stft_->read();

        float wet;
        if (reverbMode_ == 0)        // less lofi
            wet = lowpass_.Process (samplerateReducer_.Process (raw));
        else if (reverbMode_ == 1)   // normal
            wet = raw;
        else                         // more lofi
            wet = samplerateReducer_.Process (raw);

        return wet;
    }

    // Algorithmic latency of the wet path, in samples (informational; the dry
    // path has zero latency so the plugin reports 0 to the host).
    static constexpr int wetLatencySamples() { return (int) kN; }

private:
    // Frequency-domain reverb kernel — mirrors venus.cpp::reverb().
    void reverbProcess (const float* in, float* out);

    void configureDrift (int driftMode);
    void configureLoFi  (int lofiMode);

    // Uniform random phase in [0, 2*pi).  Replaces the pedal's rand()*2*PI;
    // perceptually identical diffuse random-phase resynthesis.
    inline float nextPhase()
    {
#ifdef VENUS_DETERMINISTIC_PHASE
        // Test-only hook: makes the resynthesis deterministic so the port can
        // be diffed bit-for-bit against the original pedal reverb().  Never
        // defined in a normal plugin build.
        return 0.0f;
#else
        rngState_ ^= rngState_ << 13;
        rngState_ ^= rngState_ >> 17;
        rngState_ ^= rngState_ << 5;
        return (float) (rngState_ >> 8) * (1.0f / 16777216.0f) * VENUS_TWOPI_F;
#endif
    }

    void buildStft();

    // ---- fixed STFT geometry (identical to the pedal) --------------------
    static constexpr size_t kOrder          = 12;
    static constexpr size_t kN              = (size_t) 1 << kOrder;   // 4096
    static constexpr size_t kLaps           = 4;
    static constexpr size_t kBuffSize       = 2 * kLaps * kN;         // 32768
    static constexpr size_t kHalfN          = kN / 2;                 // 2048
    static constexpr float  kIntervalSamples = (float) (kBuffSize / kLaps); // 8192
    static constexpr float  kPedalSampleRate = 32000.0f;

    // ---- STFT / FFT objects and their backing buffers --------------------
    std::vector<float> inBuf_, midBuf_, outBuf_;   // size kBuffSize
    std::vector<float> reverbEnergy_;              // size kHalfN
    std::unique_ptr<ShyFFT<float, kN, RotationPhasor>> fft_;
    std::unique_ptr<soundmath::Fourier<float, kN>>     stft_;
    soundmath::Wave<float> hann_;

    // ---- LoFi + drift DSP -------------------------------------------------
    venusdsp::SampleRateReducer samplerateReducer_;
    venusdsp::Tone              lowpass_;
    venusdsp::Oscillator        driftOsc_[4];
    float driftMultiplier_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // ---- control state (set per block) -----------------------------------
    double hostSampleRate_ = 48000.0;

    float vdamp_            = 0.1f;
    float decayMulPerFrame_ = 0.0f;   // sample-rate-corrected reverb decay
    float shimmerDouble_    = 0.0f;
    float shimmerTriple_    = 0.0f;
    float shimmerRemainder_ = 1.0f;
    float detuneDouble_     = 0.0f;
    float detuneRemainder_  = 1.0f;

    int  shimmerMode_       = 1;      // 0=down, 1=up, 2=both
    int  reverbMode_        = 1;      // 0=less lofi, 1=off, 2=more lofi
    int  driftMode_         = 1;      // 0=slow, 1=off, 2=fast
    int  detuneMode_        = 1;      // 0=down, 1=none, 2=up
    int  detuneMultiplier_  = 1;
    bool freeze_            = false;

    int  prevDriftMode_     = -1;
    int  prevReverbMode_    = -1;

    uint32_t rngState_      = 0x1234567u;
    bool prepared_          = false;
};
