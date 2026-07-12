// VenusEngine.cpp  — see VenusEngine.h for an overview.

#include "VenusEngine.h"

#include <algorithm>
#include <cmath>

void VenusEngine::buildStft()
{
    stft_ = std::make_unique<soundmath::Fourier<float, kN>> (
        [this] (const float* i, float* o) { reverbProcess (i, o); },
        fft_.get(), &hann_, kLaps,
        inBuf_.data(), midBuf_.data(), outBuf_.data());
}

void VenusEngine::prepare (double sampleRate)
{
    hostSampleRate_ = sampleRate;

    inBuf_.assign (kBuffSize, 0.0f);
    midBuf_.assign (kBuffSize, 0.0f);
    outBuf_.assign (kBuffSize, 0.0f);
    reverbEnergy_.assign (kHalfN, 0.0f);

    // Hann window lookup table, identical to venus.cpp's `hann`.
    const double PI = 3.14159265358979323846;
    hann_ = soundmath::Wave<float> ([PI] (float phase) -> float
    {
        return (float) (0.5 * (1.0 - std::cos (2.0 * PI * (double) phase)));
    });

    fft_ = std::make_unique<ShyFFT<float, kN, RotationPhasor>>();
    fft_->Init();
    buildStft();

    samplerateReducer_.Init();
    lowpass_.Init ((float) hostSampleRate_);
    lowpass_.SetFreq (8000.0f);

    for (auto& o : driftOsc_)
    {
        o.Init ((float) hostSampleRate_);
        o.SetAmp (1.0f);
    }
    driftMultiplier_[0] = driftMultiplier_[1] = driftMultiplier_[2] = driftMultiplier_[3] = 1.0f;

    prevDriftMode_  = -1;
    prevReverbMode_ = -1;
    rngState_       = 0x1234567u;
    prepared_       = true;
}

void VenusEngine::reset()
{
    std::fill (inBuf_.begin(),  inBuf_.end(),  0.0f);
    std::fill (midBuf_.begin(), midBuf_.end(), 0.0f);
    std::fill (outBuf_.begin(), outBuf_.end(), 0.0f);
    std::fill (reverbEnergy_.begin(), reverbEnergy_.end(), 0.0f);
}

void VenusEngine::configureLoFi (int lofiMode)
{
    // Mirrors venus.cpp::updateSwitch2 filter setup, but expresses the reduced
    // rate in absolute Hz (9600 / 6400, as documented in the pedal README) so
    // the LoFi grit is identical at any host sample rate.
    if (lofiMode == 0)
    {
        samplerateReducer_.SetFreq (9600.0f / (float) hostSampleRate_);
        lowpass_.SetFreq (8000.0f);
    }
    else if (lofiMode == 2)
    {
        samplerateReducer_.SetFreq (6400.0f / (float) hostSampleRate_);
    }
    // lofiMode == 1 (off): reducer / lowpass are unused.
}

void VenusEngine::configureDrift (int driftMode)
{
    // Mirrors venus.cpp::updateSwitch3.  Each LFO is slightly detuned from the
    // others so the modulation evolves without repeating.
    if (driftMode == 0)          // slow, sine
    {
        driftOsc_[0].SetFreq (0.009f); driftOsc_[0].SetWaveform (0);
        driftOsc_[1].SetFreq (0.01f);  driftOsc_[1].SetWaveform (0);
        driftOsc_[2].SetFreq (0.011f); driftOsc_[2].SetWaveform (0);
        driftOsc_[3].SetFreq (0.012f); driftOsc_[3].SetWaveform (0);
    }
    else if (driftMode == 2)     // fast, triangle
    {
        driftOsc_[0].SetFreq (0.020f); driftOsc_[0].SetWaveform (1);
        driftOsc_[1].SetFreq (0.025f); driftOsc_[1].SetWaveform (1);
        driftOsc_[2].SetFreq (0.03f);  driftOsc_[2].SetWaveform (1);
        driftOsc_[3].SetFreq (0.035f); driftOsc_[3].SetWaveform (1);
    }
    // driftMode == 1 (off): leave oscillators as-is; drift is not applied.
}

void VenusEngine::setParameters (float decayKnob, float dampKnob, float shimmerKnob,
                                 float toneKnob, float detuneBip,
                                 int shimmerMode, int lofiMode, int driftMode, bool freeze)
{
    if (! prepared_)
        return;

    freeze_      = freeze;
    shimmerMode_ = shimmerMode;

    reverbMode_ = lofiMode;
    if (lofiMode != prevReverbMode_)
    {
        configureLoFi (lofiMode);
        prevReverbMode_ = lofiMode;
    }

    driftMode_ = driftMode;
    if (driftMode != prevDriftMode_)
    {
        configureDrift (driftMode);
        prevDriftMode_ = driftMode;
    }

    // ---- knob -> internal, identical to venus.cpp::ProcessControls ----
    float vdecay        = decayKnob * 99.0f + 1.0f;
    float vdamp         = dampKnob * dampKnob;          // Parameter::EXPONENTIAL
    float vshimmer      = shimmerKnob * 0.1f;
    float vshimmer_tone = toneKnob * 0.3f;
    float vdetune_temp  = detuneBip * 0.15f;
    float vdetune       = std::fabs (vdetune_temp);

    // Drift automation.
    if (driftMode_ == 0 || driftMode_ == 2)
    {
        vdamp         = vdamp * std::fabs (driftMultiplier_[0]) * 0.7f + 0.3f;
        vshimmer      *= std::fabs (driftMultiplier_[1]);
        vshimmer_tone *= std::fabs (driftMultiplier_[2]);
        vdetune       *= std::fabs (driftMultiplier_[3]);
    }

    if (vdetune > 0.03f)
    {
        vdetune = vdetune - 0.029f;
        if (vdetune_temp >= 0.0f) { detuneMode_ = 2; detuneMultiplier_ = 1; }
        else                      { detuneMode_ = 0; detuneMultiplier_ = -1; }
    }
    else
    {
        detuneMode_ = 1;
    }

    const float sr = (float) hostSampleRate_;
    float octave_up_rate_persecond    = std::pow (8.0f, vshimmer) - 1.0f;
    float octave_up_rate_perinterval  = std::min (0.75f, octave_up_rate_persecond  / sr * kIntervalSamples);
    float octave_up_rate_persecond2   = std::pow (8.0f, vshimmer_tone) - 1.0f;
    float octave_up_rate_perinterval2 = std::min (0.75f, octave_up_rate_persecond2 / sr * kIntervalSamples);

    shimmerDouble_    = octave_up_rate_perinterval * (1.0f - vshimmer_tone / 1.58f);
    shimmerTriple_    = (octave_up_rate_perinterval2 / 1.58f) * vshimmer_tone;
    shimmerRemainder_ = 1.0f - shimmerDouble_ - shimmerTriple_;

    float detune_rate_persecond   = std::pow (8.0f, vdetune) - 1.0f;
    float detune_rate_perinterval = std::min (0.75f, detune_rate_persecond / sr * kIntervalSamples);
    detuneDouble_    = detune_rate_perinterval;
    detuneRemainder_ = 1.0f - detuneDouble_;

    vdamp_ = vdamp;

    // Sample-rate-corrected reverb decay.  The pedal multiplies each bin's
    // energy by (1 - 1/vdecay) once per STFT frame (one frame every
    // kN/kLaps = 1024 input samples).  Raising that to (32 kHz / host rate)
    // keeps the tail length in seconds identical to the hardware at any rate;
    // at 32 kHz the exponent is 1 and the behaviour is bit-for-bit the pedal.
    float base = 1.0f - 1.0f / vdecay;
    if (base < 0.0f) base = 0.0f;
    if (hostSampleRate_ == (double) kPedalSampleRate)
        decayMulPerFrame_ = base;                    // bit-for-bit the pedal at 32 kHz
    else
        decayMulPerFrame_ = std::pow (base, (float) (kPedalSampleRate / hostSampleRate_));
}

void VenusEngine::reverbProcess (const float* in, float* out)
{
    // shy_fft packs arrays as [real, real, ..., imag, imag, ...].
    static const size_t offset = kHalfN;      // N/2 = 2048
    const float fft_size = (float) kHalfN;    // 2048.0

    for (size_t i = 0; i < kHalfN; ++i)
    {
        const float fft_bin = (float) i + 1.0f;

        float real = in[i];
        float imag = in[i + offset];

        const float energy = real * real + imag * imag;

        float reverb_amp = std::sqrt (reverbEnergy_[i]);
        if (fft_bin / fft_size > vdamp_)
            reverb_amp *= vdamp_ * fft_size / fft_bin;   // reduce amplitude by 1/f

        const float random_phase = nextPhase();
        real = reverb_amp * std::cos (random_phase);
        imag = reverb_amp * std::sin (random_phase);

        if (! freeze_)
        {
            reverbEnergy_[i] += energy / (float) kLaps;  // laps = "overlap factor"
            reverbEnergy_[i] *= decayMulPerFrame_;       // decay

            const size_t half_fft_size = kHalfN / 2;     // 1024
            const float  current = reverbEnergy_[i];

            if (i > 0 && i < half_fft_size - 2)
            {
                // Morph reverb up/down by octaves.
                if (shimmerMode_ == 1 || shimmerMode_ == 2)          // up octave
                {
                    reverbEnergy_[2 * i - 1] += 0.123f * shimmerDouble_ * current;
                    reverbEnergy_[2 * i]     += 0.25f  * shimmerDouble_ * current;
                    reverbEnergy_[2 * i + 1] += 0.123f * shimmerDouble_ * current;
                }
                else if ((shimmerMode_ == 0 || shimmerMode_ == 2) && i > 1 && ! (i % 2)) // down octave
                {
                    reverbEnergy_[i / 2 - 1] += 0.75f * shimmerDouble_ * current;
                    reverbEnergy_[i / 2]     += 1.5f  * shimmerDouble_ * current;
                    reverbEnergy_[i / 2 + 1] += 0.75f * shimmerDouble_ * current;
                }

                // Morph reverb up by octave + 5th.
                if (3 * i + 1 < half_fft_size)
                {
                    reverbEnergy_[3 * i - 2] += 0.055f * shimmerTriple_ * current;
                    reverbEnergy_[3 * i - 1] += 0.11f  * shimmerTriple_ * current;
                    reverbEnergy_[3 * i]     += 0.17f  * shimmerTriple_ * current;
                    reverbEnergy_[3 * i + 1] += 0.11f  * shimmerTriple_ * current;
                    reverbEnergy_[3 * i + 2] += 0.105f * shimmerTriple_ * current;
                }

                // Detune up or down.
                if (i > 2 && i < half_fft_size - 2 && detuneMode_ != 1)
                {
                    reverbEnergy_[i + (3 * detuneMultiplier_)] += 0.123f * detuneDouble_ * current;
                    reverbEnergy_[i + (2 * detuneMultiplier_)] += 0.25f  * detuneDouble_ * current;
                    reverbEnergy_[i + (1 * detuneMultiplier_)] += 0.123f * detuneDouble_ * current;
                }
            }

            if (detuneMode_ == 1)
                detuneRemainder_ = 1.0f;

            reverbEnergy_[i] = detuneRemainder_ * shimmerRemainder_ * current;
        }

        out[i]          = real;
        out[i + offset] = imag;
    }
}
