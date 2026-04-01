"""
Effect Chain — Connects multiple effects in series, like the Mars signal path

HOW EFFECT CHAINS WORK:
    In a real pedal, effects are wired in series:

    Guitar → [Gain] → [Distortion] → [Tone] → [Delay] → [Level] → Amp

    Each effect processes the output of the previous one. The order matters!
    For example:
    - Distortion BEFORE delay = each echo is clean (normal)
    - Distortion AFTER delay = echoes get re-distorted (chaotic/noisy)

C++ EQUIVALENT:
    In mars.cpp (lines 506-563), the effects are applied in order inside
    the for loop. Each effect processes the output of the previous one:
        1. Gain (line 520): input_arr[0] = in[0][i] * vgain
        2. Neural Net (line 524): ampOut = model.forward(input_arr)
        3. Tone (lines 536-543): filter_out = tone.Process(filter_in)
        4. Delay (line 545): delay_out = delay1.Process(balanced_out)
        5. IR (line 552): impulse_out = mIR.Process(...)
        6. Level (line 558): output = impulse_out * vlevel

    This Python version lets you chain any effects together.
"""

import numpy as np
from .delay import DelayEffect
from .tone_filter import ToneFilter
from .tremolo import TremoloEffect
from .distortion import DistortionEffect


class EffectChain:
    """
    Chain multiple effects together, mimicking the Mars signal path.

    Parameters match the Mars pedal knobs:
        gain:           Input gain (Knob 1) — drive into distortion
        tone:           Tone filter (Knob 4) — 0=dark, 0.5=neutral, 1=bright
        delay_time:     Delay time in seconds (Knob 5)
        delay_feedback: Delay feedback (Knob 6)
        mix:            Dry/wet mix (Knob 2)
        level:          Output level (Knob 3)
    """

    def __init__(self, sample_rate=48000, gain=1.5, tone=0.5,
                 delay_time=0.3, delay_feedback=0.4, mix=0.5, level=0.7,
                 distortion_mode="soft"):
        self.sample_rate = sample_rate
        self.level = level

        # Build the signal chain (same order as Mars)
        self.distortion = DistortionEffect(
            sample_rate=sample_rate,
            gain=gain,
            level=1.0,  # level applied at the end
            mode=distortion_mode
        )
        self.tone_filter = ToneFilter(
            sample_rate=sample_rate,
            cutoff=tone
        )
        self.delay = DelayEffect(
            sample_rate=sample_rate,
            delay_time=delay_time,
            feedback=delay_feedback,
            mix=mix
        )

    def process_sample(self, sample):
        """
        Process one sample through the entire chain.

        This mirrors mars.cpp lines 506-563:
            Gain → Neural Model → Tone → Delay → Level
        """
        # 1. Distortion (replaces Neural Net in Mars)
        x = self.distortion.process_sample(sample)

        # 2. Tone filter
        x = self.tone_filter.process_sample(x)

        # 3. Delay
        x = self.delay.process_sample(x)

        # 4. Output level
        x = x * self.level

        return x

    def process(self, audio):
        """Process an entire audio array through the chain."""
        output = np.zeros_like(audio)
        for i in range(len(audio)):
            output[i] = self.process_sample(audio[i])
        return output
