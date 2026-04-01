# Understanding the FunBox Guitar Pedal Code

This guide is written for developers familiar with Python who want to understand
how this C++ embedded audio project works before writing their own effects.

---

## 1. The Big Picture

```
┌─────────────────────────────────────────────────────────┐
│                    FunBox Hardware                       │
│                                                         │
│  Guitar ──► ADC ──► [AudioCallback()] ──► DAC ──► Amp   │
│                          ▲                              │
│                 Knobs, Switches, Expression pedal        │
└─────────────────────────────────────────────────────────┘
```

The **Daisy Seed** is a small microcontroller board (STM32H7, ARM Cortex-M7)
that converts your guitar signal to digital, processes it in software, and
converts it back to analog. Think of it as a tiny computer dedicated to
processing audio in real time.

**Key numbers:**
- Sample rate: **48,000 Hz** (48,000 audio snapshots per second)
- Block size: **48 samples** (the callback processes 48 samples at a time)
- Audio format: **32-bit float** (values roughly between -1.0 and +1.0)
- Channels: **Stereo** (left = `in[0]`, right = `in[1]`)

---

## 2. Repository Structure

```
FunBox/
├── mod/                        # Modified Daisy hardware drivers
│   └── daisy_petal.cpp         # Hardware init: knobs, switches, ADC, audio
│
├── include/
│   ├── funbox.h                # Maps knob/switch names to hardware pins
│   └── expressionHandler.h     # Expression pedal logic
│
├── software/
│   ├── Template/               # ★ START HERE - blank effect template
│   │   └── template.cpp
│   │
│   ├── Mars/                   # Neural amp sim + delay
│   │   └── mars.cpp
│   ├── Jupiter/                # Reverb with EQ shaper
│   ├── Saturn/                 # Spectral FFT delay
│   ├── Uranus/                 # Granular delay + FM synth
│   ├── Pluto/                  # Dual stereo looper
│   ├── Venus/                  # Spectral effect
│   ├── Earth/                  # (another effect)
│   ├── Neptune/                # CloudSeed reverb
│   ├── Mercury/                # (another effect)
│   └── Experiments/            # Prototypes and experiments
│
├── tools/                      # ★ Python debug/simulation tools (NEW)
│   └── pedal_sim.py            # Process WAV files through effects on your PC
│
├── libDaisy/                   # Daisy hardware library (don't modify)
└── DaisySP/                    # DSP building blocks (filters, delays, etc.)
```

---

## 3. How an Effect Works (Read This First)

Open `software/Template/template.cpp` — it's the simplest example.

### 3.1 The Audio Callback — The Heart of Everything

```cpp
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    // This function is called ~1000 times per second (48000 / 48 = 1000)
    // Each call, it receives 48 fresh audio samples to process

    for(size_t i = 0; i < size; i++)    // Loop through each sample
    {
        float inL = in[0][i];           // Left input sample (-1.0 to 1.0)
        float inR = in[1][i];           // Right input sample

        // ★ YOUR EFFECT GOES HERE ★
        // Do math on inL/inR to create your effect

        out[0][i] = inL;               // Write left output
        out[1][i] = inR;               // Write right output
    }
}
```

**Python equivalent:**
```python
def audio_callback(input_block):
    """Process a block of 48 audio samples."""
    output_block = []
    for sample in input_block:
        # Your effect here
        output_sample = sample * 0.5  # Example: reduce volume by half
        output_block.append(output_sample)
    return output_block
```

### 3.2 The Main Function — Setup

```cpp
int main(void)
{
    hw.Init();                              // Initialize hardware
    samplerate = hw.AudioSampleRate();      // Get sample rate (48000)
    hw.SetAudioBlockSize(48);               // 48 samples per callback

    // Map physical knobs to parameter ranges
    param1.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    //          ^^^ physical knob         ^^^ min ^^^ max  ^^^ curve

    hw.StartAdc();                          // Start reading knobs
    hw.StartAudio(AudioCallback);           // Start the audio loop!

    while(1) { }                            // Run forever
}
```

### 3.3 Controls Available

| Control          | C++ Access                  | Count | Notes                        |
|------------------|-----------------------------|-------|------------------------------|
| Knobs            | `hw.knob[Funbox::KNOB_1]`  | 6     | Analog 0.0–1.0              |
| 3-way switches   | `hw.switches[...]`         | 3     | Left/Center/Right            |
| DIP switches     | `hw.switches[...]`         | 4     | On/Off                       |
| Footswitches     | `hw.switches[...]`         | 2     | Momentary (bypass, preset)   |
| Expression pedal | `hw.expression`            | 1     | Analog 0.0–1.0              |
| LEDs             | `led1`, `led2`             | 2     | Brightness 0.0–1.0          |

---

## 4. Walking Through a Real Effect: Mars (Amp Simulator)

`software/Mars/mars.cpp` is a neural-network amp simulator with delay. Here's
the signal flow:

```
Guitar Input
    │
    ▼
[Gain Knob] ──► Multiply input signal (like turning up the volume)
    │
    ▼
[Neural Network] ──► GRU model that mimics a real guitar amp's distortion
    │                 (trained on recordings of real amps)
    │
    ▼
[Tone Filter] ──► Low-pass or high-pass filter (like a tone knob on a guitar)
    │
    ▼
[Delay] ──► Echo effect (with feedback for repeating echoes)
    │
    ▼
[Impulse Response] ──► Simulates a speaker cabinet's sound (convolution)
    │
    ▼
[Level Knob] ──► Final volume control
    │
    ▼
Output to Speaker
```

### Key code sections in mars.cpp:

- **Lines 100-102**: Neural network model definition (GRU with 9 hidden units)
- **Lines 59-91**: Delay line implementation (echo effect)
- **Lines 506-563**: The actual audio processing loop — this is where the magic happens
- **Lines 462-469**: Tone filter (low-pass below 50%, high-pass above 50%)
- **Lines 474-482**: Dry/wet mix using energy-constant crossfade

---

## 5. Common DSP Building Blocks (What They Do)

These are the Lego bricks used to build effects:

### Delay Line
Stores past audio samples and plays them back later = echo.
```
Input ──► [Buffer of N samples] ──► Output (delayed)
              ▲                          │
              └──── feedback ◄───────────┘  (creates repeating echoes)
```

### Low-Pass Filter (Tone/LPF)
Removes high frequencies = makes sound darker/warmer.

### High-Pass Filter (ATone/HPF)
Removes low frequencies = makes sound thinner/brighter.

### Mix (Dry/Wet)
Blends original signal with processed signal.
- dry=100%, wet=0% → no effect (bypass)
- dry=0%, wet=100% → full effect
- dry=50%, wet=50% → half and half

### Impulse Response (IR/Convolution)
Applies the sonic fingerprint of a real speaker cabinet or room.

### FFT (Fast Fourier Transform)
Splits audio into individual frequencies for spectral processing.

### Granular Synthesis
Chops audio into tiny grains (1-300ms) and rearranges/pitch-shifts them.

---

## 6. How to Read the C++ (for Python Developers)

| C++ Pattern                          | Python Equivalent                     |
|--------------------------------------|---------------------------------------|
| `float x = 0.5f;`                   | `x = 0.5`                            |
| `for(size_t i=0; i<size; i++)`      | `for i in range(size):`              |
| `out[0][i] = in[0][i] * gain;`      | `output[i] = input[i] * gain`        |
| `tone.Process(sample)`              | `my_filter.process(sample)`           |
| `DelayLine<float, MAX> delay;`      | `delay_buffer = [0.0] * MAX`         |
| `fonepole(current, target, coeff)`  | `current += coeff * (target-current)` |
| `DSY_SDRAM_BSS`                     | (ignore — tells chip to use external RAM) |
| `#define MAX_DELAY ...`              | `MAX_DELAY = ...`                     |
| `Parameter::LINEAR` / `::CUBE`      | Linear vs cubic knob response curve   |

### The `fonepole` function (you'll see it everywhere)
This is a one-pole smoothing filter. It prevents clicks when a knob value changes:
```python
# C++: fonepole(current, target, 0.0002f)
# Python equivalent:
current = current + 0.0002 * (target - current)
# Slowly moves "current" toward "target" — like a spring
```

---

## 7. Using the Python Simulator

Instead of flashing firmware to the Daisy Seed, you can test effects on your PC:

```bash
cd tools/

# List available effects
python pedal_sim.py --list

# Process a WAV file through the delay effect
python pedal_sim.py input.wav output.wav --effect delay --time 0.3 --feedback 0.5

# Process through the tone filter
python pedal_sim.py input.wav output.wav --effect tone --cutoff 0.3

# Chain multiple effects
python pedal_sim.py input.wav output.wav --effect chain --gain 1.5 --tone 0.3 --delay-time 0.25 --delay-feedback 0.4 --mix 0.7

# Process through tremolo
python pedal_sim.py input.wav output.wav --effect tremolo --rate 4.0 --depth 0.8
```

See `tools/pedal_sim.py` for all options and `tools/effects/` for the effect
implementations (heavily commented to help you learn).

---

## 8. Creating Your Own Effect

### On the hardware (C++):
1. Copy `software/Template/` to `software/MyEffect/`
2. Edit the `AudioCallback` function
3. Run `make` then `make program-dfu` to flash

### In the Python simulator (to prototype first):
1. Copy `tools/effects/example_tremolo.py`
2. Write your `process_sample()` function
3. Test with `python pedal_sim.py input.wav output.wav --effect my_effect`
4. Once it sounds good, translate to C++ for the hardware

---

## 9. Key Gotchas

1. **Audio values are floats between -1.0 and +1.0.** Going beyond clips/distorts.
2. **48kHz means 48000 samples = 1 second.** A 0.5s delay needs a 24000-sample buffer.
3. **Block size is 48 samples.** The callback processes 48 samples at once, not one at a time.
4. **The neural network in Mars** is a trained GRU model — it learned amp distortion from real recordings. You don't need to understand ML to use the other effects.
5. **`DSY_SDRAM_BSS`** means "put this in external RAM" — the Daisy has 64MB of SDRAM for large buffers (delay lines, loopers). Ignore this annotation for understanding the DSP.
6. **DaisySP library** (`DaisySP/`) provides ready-made filters, oscillators, delays, etc. Browse it for building blocks when creating your own effects.
