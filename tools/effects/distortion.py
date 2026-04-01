"""
Distortion Effect — Simulates what the neural network in Mars does (simplified)

HOW DISTORTION WORKS:
    Distortion is what happens when you amplify a signal beyond its limits.
    Imagine turning the volume way up until the speaker can't keep up —
    the tops and bottoms of the waveform get "clipped" (flattened), which
    adds harmonics (new frequencies) that create that crunchy/fuzzy sound.

    Clean signal:          Distorted signal:
        /\                    ___
       /  \                  /   \
      /    \                /     \
     /      \              /       \
    ─────────────  vs  ────────────────
         \      /              \       /
          \    /                \     /
           \  /                  \___/
            \/

    In Mars (mars.cpp), the distortion comes from a neural network (GRU model)
    that learned the sound of a real guitar amp. That's complex! Here we use
    simpler math to approximate distortion.

    Three distortion types:
    1. SOFT CLIP (tanh): Smooth, warm overdrive. Like a tube amp.
       formula: output = tanh(input * gain)

    2. HARD CLIP: Harsh, buzzy fuzz. Like a transistor fuzz pedal.
       formula: output = clip(input * gain, -1, +1)

    3. ASYMMETRIC: Different clipping on positive vs negative — adds
       even harmonics, sounds more "amp-like".

C++ EQUIVALENT (mars.cpp lines 519-528):
    The C++ version uses RTNeural to run a trained GRU neural network:
        ampOut = model.forward(input_arr) + input_arr[0];
    Our Python version achieves a similar sonic character with math functions.
"""

import numpy as np


class DistortionEffect:
    """
    A distortion/overdrive effect.

    Parameters:
        sample_rate:  Audio sample rate
        gain:         Input gain / drive amount (0.1 to 10.0)
                      Higher = more distortion
        level:        Output level (0.0 to 1.0) — compensate for volume increase
        mode:         Distortion type: "soft", "hard", or "asymmetric"

    Equivalent C++ knobs in Mars:
        gain   → Knob 1 (Gain, range 0.1 to 2.5)
        level  → Knob 3 (Level)
    """

    def __init__(self, sample_rate=48000, gain=2.0, level=0.5, mode="soft"):
        self.sample_rate = sample_rate
        self.gain = gain
        self.level = np.clip(level, 0.0, 1.0)
        self.mode = mode

    def process_sample(self, sample):
        """
        Process a single audio sample through distortion.

        Compare to Mars (line 520-525):
            input_arr[0] = in[0][i] * vgain;        ← gain stage
            ampOut = model.forward(input_arr) + ...;  ← neural net (we use tanh)
            ampOut *= nnLevelAdjust;                   ← level adjust
        """
        # Step 1: Apply gain (amplify the signal)
        driven = sample * self.gain

        # Step 2: Apply clipping (this is where distortion happens)
        if self.mode == "soft":
            # tanh gives smooth saturation — like a tube amp
            # As input gets louder, output approaches ±1.0 but never exceeds it
            clipped = np.tanh(driven)

        elif self.mode == "hard":
            # Hard clipping — just chop off anything above ±1.0
            # Sounds harsh and fuzzy, like a fuzz pedal
            clipped = np.clip(driven, -1.0, 1.0)

        else:  # asymmetric
            # Different clipping on positive and negative halves
            # This adds even harmonics, sounds more "amp-like"
            if driven >= 0:
                clipped = np.tanh(driven)
            else:
                clipped = np.tanh(driven * 0.5) * 0.8

        # Step 3: Apply output level
        return clipped * self.level

    def process(self, audio):
        """Process an entire audio array."""
        output = np.zeros_like(audio)
        for i in range(len(audio)):
            output[i] = self.process_sample(audio[i])
        return output
