"""
Tremolo Effect — A simple volume-modulation effect

HOW TREMOLO WORKS:
    Tremolo is one of the simplest guitar effects: it modulates the volume
    up and down using a low-frequency oscillator (LFO).

    Think of it as someone quickly turning the volume knob up and down
    in a rhythmic pattern.

    signal ──► [ × LFO wave ] ──► output
                    │
                    └── LFO = sin(2π × rate × time)

    The LFO (Low Frequency Oscillator) generates a wave at a low frequency
    (typically 1-10 Hz, way below audible range). This wave is used to
    multiply the audio signal, making it louder and quieter rhythmically.

    Parameters:
    - Rate: How fast the volume oscillates (Hz). 4 Hz = 4 pulses per second.
    - Depth: How much the volume changes. 0 = no effect, 1 = full silence to full volume.
    - Shape: Sine wave (smooth) or square wave (choppy).

WHY THIS IS A GOOD FIRST EFFECT TO BUILD:
    It's dead simple — just multiply each sample by a changing number.
    No buffers, no feedback, no complex math. If you understand this,
    you understand the core concept of real-time audio processing.

    This effect is NOT in the original FunBox effects but is perfect
    for learning before moving to more complex effects.
"""

import numpy as np


class TremoloEffect:
    """
    A tremolo (volume modulation) effect.

    Parameters:
        sample_rate:  Audio sample rate in Hz
        rate:         LFO speed in Hz (0.5 to 20.0). 4.0 = 4 pulses/sec
        depth:        Modulation depth (0.0 to 1.0). How much volume changes.
        shape:        LFO shape: "sine" (smooth) or "square" (choppy)
    """

    def __init__(self, sample_rate=48000, rate=4.0, depth=0.8, shape="sine"):
        self.sample_rate = sample_rate
        self.rate = rate
        self.depth = np.clip(depth, 0.0, 1.0)
        self.shape = shape

        # Phase tracks where we are in the LFO cycle (0.0 to 1.0)
        self.phase = 0.0
        # How much phase advances per sample
        # At 48000 Hz sample rate and 4 Hz rate: increment = 4/48000 = 0.0000833
        self.phase_increment = rate / sample_rate

    def process_sample(self, sample):
        """
        Process a single audio sample.

        The math:
            1. Generate LFO value (0.0 to 1.0)
            2. Scale it by depth to get modulation amount
            3. Multiply the audio sample by the modulation
        """
        # Step 1: Generate LFO value based on shape
        if self.shape == "sine":
            # Sine wave oscillates smoothly between -1 and +1
            # We shift it to 0..1 range: (sin + 1) / 2
            lfo = (np.sin(2.0 * np.pi * self.phase) + 1.0) / 2.0
        else:  # square
            # Square wave: on/off, like a strobe light
            lfo = 1.0 if self.phase < 0.5 else 0.0

        # Step 2: Scale by depth
        # At depth=0: modulation = 1.0 (no effect)
        # At depth=1: modulation swings from 0.0 to 1.0 (full tremolo)
        modulation = 1.0 - self.depth * (1.0 - lfo)

        # Step 3: Advance the LFO phase
        self.phase += self.phase_increment
        if self.phase >= 1.0:
            self.phase -= 1.0

        # Step 4: Apply modulation to the audio sample
        return sample * modulation

    def process(self, audio):
        """Process an entire audio array."""
        output = np.zeros_like(audio)
        for i in range(len(audio)):
            output[i] = self.process_sample(audio[i])
        return output
