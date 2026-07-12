# Plugins

DAW plugin (VST3 / AU / Standalone) ports of Funbox pedal effects, built with
[JUCE](https://juce.com). These let you run the same DSP from your Funbox pedal
inside a DAW.

Each effect's DSP is ported directly from its pedal firmware under
[`software/`](../software); only the hardware layer (knobs, switches,
footswitches, LEDs, expression, MIDI) is replaced by plugin parameters. Where a
pedal runs at a fixed internal sample rate, the port is adjusted so it behaves
consistently at DAW sample rates while matching the pedal exactly at its native
rate — see each plugin's README for details.

## Available

| Plugin | Effect | Source |
| --- | --- | --- |
| [Venus](Venus) | Spectral (STFT) reverb with shimmer, detune, freeze, LoFi and drift | [`software/Venus`](../software/Venus) |

## Building

Each plugin is a self-contained CMake project. JUCE is downloaded automatically
at configure time (or point at a local checkout with `-DJUCE_PATH=...`).

```bash
cd plugins/Venus
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Linux, install JUCE's build dependencies first:

```bash
sudo apt-get install libx11-dev libxext-dev libxinerama-dev libxrandr-dev \
    libxcursor-dev libasound2-dev libfreetype6-dev libgl1-mesa-dev
```

See the individual plugin READMEs for controls, output locations, and fidelity
notes.
