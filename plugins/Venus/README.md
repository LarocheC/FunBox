# Venus — DAW plugin

A VST3 / AU / Standalone port of the Funbox **Venus** spectral reverb, so you
can use the same effect from your pedal inside a DAW.

Venus is an STFT (short-time Fourier transform) reverb: it analyses your signal
into overlapping 4096-point windows, accumulates spectral *energy* per frequency
bin, and resynthesises it with a random phase. On top of that it adds octave and
octave-plus-fifth **shimmer**, spectral **detune**, a spectral **freeze**, two
**LoFi** modes, and slow **drift** modulation. The DSP here is ported directly
from [`software/Venus/venus.cpp`](../../software/Venus/venus.cpp) and verified to
match the pedal (see *Fidelity* below).

## Controls

The six pedal knobs and three toggle switches become plugin parameters (all
automatable):

| Plugin parameter | Pedal control | Notes |
| --- | --- | --- |
| **Decay** | Ctrl 1 | Reverb decay time |
| **Mix** | Ctrl 2 | Dry / wet |
| **Damp** | Ctrl 3 | High-frequency damping (exponential taper, as on the pedal) |
| **Shimmer** | Ctrl 4 | Amount of octave shimmer |
| **Shimmer Tone** | Ctrl 5 | Blends octave + 5th into the shimmer |
| **Detune** | Ctrl 6 | Centre = none; left detunes down, right detunes up |
| **Shimmer Mode** | 3-way Switch 1 | Octave Down / Octave Up / Octave Up + Down |
| **LoFi Mode** | 3-way Switch 2 | Less LoFi (≈9.6 kHz + 8 kHz lowpass) / Off / More LoFi (≈6.4 kHz) |
| **Drift Mode** | 3-way Switch 3 | Slow (sine) / Off / Fast (triangle) — slowly modulates Damp, Shimmer, Shimmer Tone and Detune |
| **Freeze** | Footswitch 2 | Holds the current spectral texture indefinitely |
| **Bypass** | Footswitch 1 | Standard host bypass |

Knob parameters are `0..1` (Detune is `-1..1`, centre-detented) and map to the
pedal's internal ranges exactly, so a given position sounds the same as the
hardware.

The pedal's **expression pedal** and **MIDI CC learn** features are intentionally
omitted — in a DAW you get the same result (and more) from host automation of the
parameters above.

## Build

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE is fetched automatically.

```bash
cd plugins/Venus
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs (under `build/Venus_artefacts/Release/`):

- **VST3** — `VST3/Venus.vst3`
- **Standalone** — `Standalone/Venus`
- **AU** (macOS only) — `AU/Venus.component`

Options:

- Use a JUCE checkout you already have (skips the download):
  `cmake -B build -DJUCE_PATH=/path/to/JUCE`
- Pin a different JUCE version: `cmake -B build -DJUCE_TAG=8.0.6`

On Linux the JUCE GUI/audio backends need these dev packages:
`libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev
libasound2-dev libfreetype6-dev libgl1-mesa-dev`.

Copy the built VST3/AU into your plugin folder (e.g. `~/.vst3`,
`~/Library/Audio/Plug-Ins/VST3`, or `%COMMONPROGRAMFILES%\VST3`).

## Fidelity

The port reproduces the pedal's algorithm exactly, with two deliberate,
documented adjustments so it behaves consistently at DAW sample rates (the pedal
runs internally at 32 kHz):

1. **Decay time is sample-rate compensated.** The pedal decays the spectral
   energy once per STFT frame; a frame is a fixed number of samples, so at a
   higher host rate that would shorten the tail. The decay is rescaled so the
   tail length in *seconds* matches the pedal at any sample rate. At 32 kHz the
   compensation is identity, i.e. bit-for-bit the pedal.
2. **LoFi rates are expressed in Hz.** The two LoFi modes reduce to ≈9.6 kHz and
   ≈6.4 kHz (the pedal's values at 32 kHz) regardless of host sample rate, so the
   LoFi grit is unchanged.

Everything else — the STFT, spectral damping, octave / octave-plus-fifth shimmer,
detune spreading and freeze — uses the pedal's exact maths. The reverb is
processed in mono (like the pedal) and mixed back against the original,
per-channel dry signal so a stereo source keeps its image. The random
resynthesis phase uses a fast internal RNG (the diffuse result is identical in
character to the pedal's `rand()`).

The port was checked against the original `venus.cpp` `reverb()` and control code
with deterministic phase at 32 kHz: the two outputs match to ~3e-7 (floating-point
rounding), i.e. the spectral engine, control mapping, shimmer and detune are
faithful.

## Credits / licenses

- Venus DSP and Funbox platform: **GuitarML** (Keith Bloemer).
- STFT (`fourier.h`, `wave.h`) adapted from **amcerbu/DaisySTFT**.
- `shy_fft.h`: real FFT by **Émilie Gillet** (Mutable Instruments), MIT.
- The reverb maths is inspired by **Geraint Luff**'s Atlantis Reverb.
- `DaisySPUtils.h` contains faithful ports of three DaisySP classes:
  `SampleRateReducer` and `Oscillator` (Electrosmith, **MIT**) and `Tone`
  (Electrosmith/Vercoe/FFitch/Maldonado, **LGPL v2.1**). Per-class notices are
  preserved in that file.
- Built with **JUCE** (fetched at configure time; see its own licensing terms).
