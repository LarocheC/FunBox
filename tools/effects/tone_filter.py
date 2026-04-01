"""
Tone Filter — Python equivalent of the tone controls in Mars (mars.cpp lines 462-469)

HOW FILTERS WORK:
    A low-pass filter (LPF) lets low frequencies through and removes highs.
    → Makes the sound warmer/darker (like rolling off the tone knob on a guitar)

    A high-pass filter (HPF) lets high frequencies through and removes lows.
    → Makes the sound thinner/brighter

    In Mars, the filter knob (Knob 4) controls both:
    - Knob at 0% to 50%: Low-pass filter, sweeping from dark to neutral
    - Knob at 50% to 100%: High-pass filter, sweeping from neutral to thin

    The simplest digital filter is the "one-pole" filter, which uses just
    one line of math per sample:

    LOW-PASS:   output = output + coefficient * (input - output)
    HIGH-PASS:  output = input - lowpass(input)

    The "coefficient" controls the cutoff frequency:
    - Small coefficient (0.01) = very dark, only bass gets through
    - Large coefficient (0.99) = almost no filtering

C++ EQUIVALENT:
    Mars uses DaisySP's Tone (low-pass) and ATone (high-pass) classes,
    plus Balance for volume correction. Those are one-pole IIR filters
    under the hood.
"""

import numpy as np


class ToneFilter:
    """
    A simple one-pole tone filter (low-pass and high-pass).

    Parameters:
        sample_rate:  Audio sample rate in Hz
        cutoff:       Filter knob position (0.0 to 1.0)
                      0.0 = dark (strong low-pass)
                      0.5 = neutral (no filtering)
                      1.0 = thin (strong high-pass)

    Equivalent C++ knob in Mars: Knob 4 (filter)
    """

    def __init__(self, sample_rate=48000, cutoff=0.5):
        self.sample_rate = sample_rate
        self.cutoff = np.clip(cutoff, 0.0, 1.0)
        # Internal state for the filter (remembers the previous output)
        self.lp_state = 0.0

        # Calculate filter coefficient from cutoff
        self._update_coefficient()

    def _update_coefficient(self):
        """
        Convert the knob position (0-1) to a filter coefficient.

        This mimics how Mars maps the knob to filter frequency:
        - mars.cpp line 464: filter_value = (vfilter * 39800) + 100  (LP)
        - mars.cpp line 468: filter_value = (vfilter-0.5) * 800 + 40 (HP)

        Then converts frequency to a one-pole coefficient using:
        coefficient = 1 - exp(-2π * freq / sample_rate)
        """
        if self.cutoff <= 0.5:
            # Low-pass mode: map 0.0-0.5 to frequency 100-20000 Hz
            freq = self.cutoff * 2.0 * 19900.0 + 100.0
            self.coefficient = 1.0 - np.exp(-2.0 * np.pi * freq / self.sample_rate)
            self.mode = "lowpass"
        else:
            # High-pass mode: map 0.5-1.0 to frequency 40-440 Hz
            freq = (self.cutoff - 0.5) * 2.0 * 400.0 + 40.0
            self.coefficient = 1.0 - np.exp(-2.0 * np.pi * freq / self.sample_rate)
            self.mode = "highpass"

    def process_sample(self, sample):
        """
        Process a single audio sample.

        LOW-PASS (one-pole IIR):
            state += coeff * (input - state)
            output = state

        HIGH-PASS:
            output = input - lowpass(input)
            (Subtracting the low frequencies leaves only the highs)
        """
        # One-pole low-pass: always compute this
        self.lp_state += self.coefficient * (sample - self.lp_state)

        if self.mode == "lowpass":
            return self.lp_state
        else:
            # High-pass = original minus low-pass
            return sample - self.lp_state

    def process(self, audio):
        """Process an entire audio array."""
        output = np.zeros_like(audio)
        for i in range(len(audio)):
            output[i] = self.process_sample(audio[i])
        return output
