"""
Delay Effect — Python equivalent of the delay in Mars (mars.cpp lines 59-91)

HOW A DELAY WORKS:
    A delay is just a long buffer (array) that stores past audio samples.
    You write new samples in, and read old samples out from a position
    further back in the buffer. The "delay time" controls how far back you read.

    Feedback takes the delayed output and feeds it back into the buffer,
    creating repeating echoes that gradually fade out.

    ┌──────────────────────────────────┐
    │  Buffer: [0.0, 0.0, ..., 0.0]   │  ← circular buffer of N samples
    │           ▲ write      ▲ read    │
    │           │            │         │
    │  in ──────┘    read ───┘         │
    │                  │               │
    │         feedback │               │
    │            ┌─────┘               │
    │            ▼                     │
    │  write = in + feedback * read    │
    └──────────────────────────────────┘

C++ EQUIVALENT (mars.cpp):
    The C++ version uses DelayLine2Tap<float, MAX_DELAY> which is a
    template-based circular buffer. Our Python version does the same
    thing with a plain list.
"""

import numpy as np


class DelayEffect:
    """
    A simple delay (echo) effect.

    Parameters:
        sample_rate:  Audio sample rate in Hz (default 48000, same as Daisy)
        delay_time:   Delay time in seconds (0.0 to 2.0)
        feedback:     How much of the delayed signal feeds back (0.0 to 0.99)
                      0.0 = single echo, 0.9 = long trail of echoes
        mix:          Wet/dry mix (0.0 = dry only, 1.0 = delay only)

    Equivalent C++ knobs in Mars:
        delay_time  → Knob 5 (delayTime)
        feedback    → Knob 6 (delayFdbk)
        mix         → Knob 2 (Mix)
    """

    def __init__(self, sample_rate=48000, delay_time=0.3, feedback=0.5, mix=0.5):
        self.sample_rate = sample_rate
        self.feedback = np.clip(feedback, 0.0, 0.99)
        self.mix = np.clip(mix, 0.0, 1.0)

        # Maximum delay: 2 seconds (same as Mars: MAX_DELAY = 48000 * 2)
        max_delay_samples = sample_rate * 2
        self.buffer = np.zeros(max_delay_samples)

        # Convert delay time in seconds to samples
        # e.g., 0.3 seconds * 48000 Hz = 14400 samples
        self.delay_samples = int(delay_time * sample_rate)
        self.delay_samples = min(self.delay_samples, max_delay_samples - 1)

        # Write position in the circular buffer
        self.write_pos = 0

    def process_sample(self, sample):
        """
        Process a single audio sample through the delay.

        This is the Python equivalent of the delay::Process() method
        in mars.cpp (lines 69-89).
        """
        # Calculate read position (where to read the delayed sample from)
        # This is like del->Read() in C++
        read_pos = (self.write_pos - self.delay_samples) % len(self.buffer)
        delayed_sample = self.buffer[read_pos]

        # Write new sample + feedback into buffer
        # This is like del->Write((feedback * read) + in) in C++
        self.buffer[self.write_pos] = sample + self.feedback * delayed_sample

        # Advance write position (circular — wraps around)
        self.write_pos = (self.write_pos + 1) % len(self.buffer)

        # Mix dry and wet signals
        # dry/wet crossfade (simplified version of the energy-constant
        # crossfade in mars.cpp lines 474-482)
        output = sample * (1.0 - self.mix) + delayed_sample * self.mix
        return output

    def process(self, audio):
        """Process an entire audio array (numpy array of floats)."""
        output = np.zeros_like(audio)
        for i in range(len(audio)):
            output[i] = self.process_sample(audio[i])
        return output
