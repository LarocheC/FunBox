#!/usr/bin/env python3
"""
FunBox Pedal Simulator — Process WAV files through guitar effects on your PC.

This tool lets you test and understand audio effects without the Daisy Seed
hardware. It reads a WAV file, processes it through an effect, and writes
the output to a new WAV file.

The effects here are Python equivalents of the C++ DSP code running on the
FunBox pedal. They're simplified for clarity but produce similar results.

USAGE:
    # List available effects and their parameters
    python pedal_sim.py --list

    # Basic: process through an effect
    python pedal_sim.py input.wav output.wav --effect delay

    # With parameters (like turning knobs on the pedal)
    python pedal_sim.py input.wav output.wav --effect delay --time 0.4 --feedback 0.6

    # Full signal chain (like Mars pedal)
    python pedal_sim.py input.wav output.wav --effect chain --gain 2.0 --tone 0.3

    # Generate a test tone if you don't have a WAV file
    python pedal_sim.py --generate-test test_tone.wav

REQUIREMENTS:
    pip install numpy soundfile

    (soundfile is a simple library for reading/writing WAV files)

HOW THIS RELATES TO THE C++ CODE:
    On the Daisy Seed hardware, audio flows like this:
        ADC → AudioCallback() → DAC

    In this simulator, it flows like this:
        WAV file → process() → WAV file

    The process() function does the same math as AudioCallback().
    The only difference is where the audio comes from and goes to.
"""

import argparse
import sys
import os
import numpy as np

# Add the tools directory to the path so we can import effects
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from effects import DelayEffect, ToneFilter, TremoloEffect, DistortionEffect, EffectChain


# ──────────────────────────────────────────────────────────────────────
# Audio I/O helpers
# ──────────────────────────────────────────────────────────────────────

def read_wav(filepath):
    """
    Read a WAV file and return (audio_data, sample_rate).

    audio_data is a numpy array of float32 values, typically -1.0 to +1.0.
    This is the same format used inside the Daisy Seed's AudioCallback.
    """
    import soundfile as sf
    audio, sr = sf.read(filepath, dtype='float32')

    # If stereo, take only the left channel (the pedal processes mono input)
    # In mars.cpp, only in[0][i] (left channel) is used for processing
    if audio.ndim > 1:
        print(f"  Input is stereo — using left channel only (like the pedal)")
        audio = audio[:, 0]

    print(f"  Loaded: {filepath}")
    print(f"  Sample rate: {sr} Hz")
    print(f"  Duration: {len(audio) / sr:.2f} seconds")
    print(f"  Samples: {len(audio)}")
    return audio, sr


def write_wav(filepath, audio, sample_rate):
    """
    Write audio data to a WAV file.

    The output is stereo (both channels identical), matching how
    the pedal outputs: out[0][i] = output; out[1][i] = output;
    """
    import soundfile as sf

    # Make stereo (same signal on both channels, like mars.cpp lines 559-560)
    stereo = np.column_stack([audio, audio])
    sf.write(filepath, stereo, sample_rate)
    print(f"  Written: {filepath}")
    print(f"  Duration: {len(audio) / sample_rate:.2f} seconds")


def generate_test_tone(filepath, sample_rate=48000, duration=3.0):
    """
    Generate a test WAV file with a guitar-like tone.

    Creates a 330 Hz tone (E4, open high E string on guitar) with
    some harmonics to simulate a simple plucked string sound.
    Useful when you don't have a guitar recording handy.
    """
    import soundfile as sf
    t = np.linspace(0, duration, int(sample_rate * duration), dtype=np.float32)

    # Fundamental + harmonics (simulates a plucked string)
    freq = 330.0  # E4
    signal = (
        0.5 * np.sin(2 * np.pi * freq * t) +        # fundamental
        0.25 * np.sin(2 * np.pi * 2 * freq * t) +    # 2nd harmonic
        0.125 * np.sin(2 * np.pi * 3 * freq * t) +   # 3rd harmonic
        0.0625 * np.sin(2 * np.pi * 4 * freq * t)    # 4th harmonic
    )

    # Apply an envelope (volume fades out, like a plucked string)
    envelope = np.exp(-t * 2.0)
    signal = (signal * envelope).astype(np.float32)

    # Normalize to -0.8 to +0.8 (leave some headroom)
    signal = signal / np.max(np.abs(signal)) * 0.8

    stereo = np.column_stack([signal, signal])
    sf.write(filepath, stereo, sample_rate)
    print(f"Generated test tone: {filepath}")
    print(f"  Frequency: {freq} Hz (E4)")
    print(f"  Duration: {duration} seconds")
    print(f"  Sample rate: {sample_rate} Hz")


# ──────────────────────────────────────────────────────────────────────
# Processing (this is the Python equivalent of AudioCallback)
# ──────────────────────────────────────────────────────────────────────

def process_audio(audio, sample_rate, effect, block_size=48):
    """
    Process audio through an effect, block by block.

    This mimics exactly how the Daisy Seed processes audio:
    - Audio arrives in blocks of 48 samples
    - Each block is processed by AudioCallback()
    - The block size is set in main(): hw.SetAudioBlockSize(48)

    We process block-by-block here to match the hardware behavior,
    even though we could process the entire array at once.
    """
    output = np.zeros_like(audio)
    num_blocks = len(audio) // block_size

    print(f"  Processing {num_blocks} blocks of {block_size} samples...")
    print(f"  (This is how the Daisy Seed processes: {num_blocks} calls to AudioCallback)")

    for block_idx in range(num_blocks):
        start = block_idx * block_size
        end = start + block_size

        # Process each sample in the block
        # This is the inner for loop: for(size_t i = 0; i < size; i++)
        for i in range(block_size):
            output[start + i] = effect.process_sample(audio[start + i])

    # Handle remaining samples (if audio length isn't divisible by block_size)
    remaining_start = num_blocks * block_size
    for i in range(remaining_start, len(audio)):
        output[i] = effect.process_sample(audio[i])

    # Clip to prevent distortion from exceeding [-1, 1]
    output = np.clip(output, -1.0, 1.0)

    return output


# ──────────────────────────────────────────────────────────────────────
# CLI
# ──────────────────────────────────────────────────────────────────────

EFFECTS_INFO = """
Available effects:
──────────────────────────────────────────────────────────────────────

  delay       Echo/delay effect (like Mars Knobs 5+6)
              --time SECONDS    Delay time, 0.01-2.0 (default: 0.3)
              --feedback FLOAT  Echo feedback, 0.0-0.99 (default: 0.5)
              --mix FLOAT       Dry/wet mix, 0.0-1.0 (default: 0.5)

  tone        Tone filter (like Mars Knob 4)
              --cutoff FLOAT    0.0=dark, 0.5=neutral, 1.0=bright (default: 0.5)

  tremolo     Volume modulation (great first effect to understand)
              --rate FLOAT      Speed in Hz, 0.5-20.0 (default: 4.0)
              --depth FLOAT     Intensity, 0.0-1.0 (default: 0.8)
              --shape STRING    "sine" or "square" (default: sine)

  distortion  Overdrive/distortion (simplified version of Mars neural net)
              --gain FLOAT      Drive amount, 0.1-10.0 (default: 2.0)
              --level FLOAT     Output volume, 0.0-1.0 (default: 0.5)
              --mode STRING     "soft", "hard", or "asymmetric" (default: soft)

  chain       Full signal chain (mimics Mars pedal)
              --gain FLOAT      Drive (default: 1.5)
              --tone FLOAT      Tone 0-1 (default: 0.5)
              --delay-time FLOAT  Delay seconds (default: 0.3)
              --delay-feedback FLOAT  Delay feedback (default: 0.4)
              --mix FLOAT       Dry/wet (default: 0.5)
              --level FLOAT     Output level (default: 0.7)
              --mode STRING     Distortion mode (default: soft)

──────────────────────────────────────────────────────────────────────
Generate a test tone (no input file needed):
  python pedal_sim.py --generate-test test_tone.wav

Examples:
  python pedal_sim.py input.wav out.wav --effect delay --time 0.5 --feedback 0.7
  python pedal_sim.py input.wav out.wav --effect chain --gain 3.0 --tone 0.2
  python pedal_sim.py input.wav out.wav --effect tremolo --rate 6 --depth 1.0
"""


def build_effect(args, sample_rate):
    """Create the requested effect with the given parameters."""
    if args.effect == "delay":
        return DelayEffect(
            sample_rate=sample_rate,
            delay_time=args.time,
            feedback=args.feedback,
            mix=args.mix
        )
    elif args.effect == "tone":
        return ToneFilter(
            sample_rate=sample_rate,
            cutoff=args.cutoff
        )
    elif args.effect == "tremolo":
        return TremoloEffect(
            sample_rate=sample_rate,
            rate=args.rate,
            depth=args.depth,
            shape=args.shape
        )
    elif args.effect == "distortion":
        return DistortionEffect(
            sample_rate=sample_rate,
            gain=args.gain,
            level=args.level,
            mode=args.mode
        )
    elif args.effect == "chain":
        return EffectChain(
            sample_rate=sample_rate,
            gain=args.gain,
            tone=args.tone,
            delay_time=args.delay_time,
            delay_feedback=args.delay_feedback,
            mix=args.mix,
            level=args.level,
            distortion_mode=args.mode
        )
    else:
        print(f"Unknown effect: {args.effect}")
        print("Use --list to see available effects")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="FunBox Pedal Simulator — Process WAV files through guitar effects",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Use --list to see all effects and their parameters"
    )

    # Positional arguments
    parser.add_argument("input", nargs="?", help="Input WAV file")
    parser.add_argument("output", nargs="?", help="Output WAV file")

    # Effect selection
    parser.add_argument("--effect", "-e", default="delay",
                        choices=["delay", "tone", "tremolo", "distortion", "chain"],
                        help="Effect to apply (default: delay)")
    parser.add_argument("--list", "-l", action="store_true",
                        help="List all available effects and their parameters")

    # Test tone generation
    parser.add_argument("--generate-test", metavar="FILE",
                        help="Generate a test tone WAV file")

    # Delay parameters
    parser.add_argument("--time", type=float, default=0.3,
                        help="Delay time in seconds (default: 0.3)")
    parser.add_argument("--feedback", type=float, default=0.5,
                        help="Delay feedback 0.0-0.99 (default: 0.5)")

    # Tone parameters
    parser.add_argument("--cutoff", type=float, default=0.5,
                        help="Tone filter: 0=dark, 0.5=neutral, 1=bright (default: 0.5)")

    # Tremolo parameters
    parser.add_argument("--rate", type=float, default=4.0,
                        help="Tremolo rate in Hz (default: 4.0)")
    parser.add_argument("--depth", type=float, default=0.8,
                        help="Tremolo depth 0.0-1.0 (default: 0.8)")
    parser.add_argument("--shape", default="sine", choices=["sine", "square"],
                        help="Tremolo LFO shape (default: sine)")

    # Distortion / chain parameters
    parser.add_argument("--gain", type=float, default=1.5,
                        help="Distortion gain (default: 1.5)")
    parser.add_argument("--level", type=float, default=0.7,
                        help="Output level 0.0-1.0 (default: 0.7)")
    parser.add_argument("--mode", default="soft",
                        choices=["soft", "hard", "asymmetric"],
                        help="Distortion mode (default: soft)")

    # Chain parameters
    parser.add_argument("--tone", type=float, default=0.5,
                        help="Chain tone filter 0-1 (default: 0.5)")
    parser.add_argument("--delay-time", type=float, default=0.3,
                        help="Chain delay time in seconds (default: 0.3)")
    parser.add_argument("--delay-feedback", type=float, default=0.4,
                        help="Chain delay feedback (default: 0.4)")
    parser.add_argument("--mix", type=float, default=0.5,
                        help="Dry/wet mix 0.0-1.0 (default: 0.5)")

    # Audio settings
    parser.add_argument("--block-size", type=int, default=48,
                        help="Audio block size, matches Daisy (default: 48)")

    args = parser.parse_args()

    # Handle special commands
    if args.list:
        print(EFFECTS_INFO)
        return

    if args.generate_test:
        generate_test_tone(args.generate_test)
        return

    # Validate input/output
    if not args.input or not args.output:
        parser.print_help()
        print("\nError: Both input and output WAV files are required.")
        print("       Or use --generate-test to create a test file first.")
        sys.exit(1)

    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)

    # Process!
    print(f"\n{'='*60}")
    print(f"FunBox Pedal Simulator")
    print(f"{'='*60}")
    print(f"\nEffect: {args.effect}")
    print(f"\nReading input...")
    audio, sample_rate = read_wav(args.input)

    print(f"\nBuilding effect...")
    effect = build_effect(args, sample_rate)

    print(f"\nProcessing audio...")
    output = process_audio(audio, sample_rate, effect, args.block_size)

    # Report peak levels (useful for debugging)
    input_peak = np.max(np.abs(audio))
    output_peak = np.max(np.abs(output))
    print(f"\n  Input peak level:  {input_peak:.4f} ({20*np.log10(max(input_peak, 1e-10)):.1f} dB)")
    print(f"  Output peak level: {output_peak:.4f} ({20*np.log10(max(output_peak, 1e-10)):.1f} dB)")

    print(f"\nWriting output...")
    write_wav(args.output, output, sample_rate)

    print(f"\nDone! Play the output with any audio player.")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
