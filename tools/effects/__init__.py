# FunBox Pedal Effects - Python implementations for debugging and learning
#
# Each effect mirrors a DSP building block used in the C++ pedal code.
# They are heavily commented so you can understand what each line does.

from .delay import DelayEffect
from .tone_filter import ToneFilter
from .tremolo import TremoloEffect
from .distortion import DistortionEffect
from .chain import EffectChain
