// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VenusAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { kDecay, 1 }, "Decay",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { kMix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { kDamp, 1 }, "Damp",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { kShimmer, 1 }, "Shimmer",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { kShimmerTone, 1 }, "Shimmer Tone",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { kDetune, 1 }, "Detune",
        NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) {
                if (std::abs (v) < 0.001f) return juce::String ("Center");
                return juce::String (v < 0 ? "Down " : "Up ")
                     + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + " %";
            })));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { kShimmerMode, 1 }, "Shimmer Mode",
        StringArray { "Octave Down", "Octave Up", "Octave Up + Down" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { kLoFiMode, 1 }, "LoFi Mode",
        StringArray { "Less LoFi", "Off", "More LoFi" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { kDriftMode, 1 }, "Drift Mode",
        StringArray { "Slow", "Off", "Fast" }, 1));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { kFreeze, 1 }, "Freeze", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { kBypass, 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
VenusAudioProcessor::VenusAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    decayParam_       = apvts.getRawParameterValue (kDecay);
    mixParam_         = apvts.getRawParameterValue (kMix);
    dampParam_        = apvts.getRawParameterValue (kDamp);
    shimmerParam_     = apvts.getRawParameterValue (kShimmer);
    shimmerToneParam_ = apvts.getRawParameterValue (kShimmerTone);
    detuneParam_      = apvts.getRawParameterValue (kDetune);
    shimmerModeParam_ = apvts.getRawParameterValue (kShimmerMode);
    lofiModeParam_    = apvts.getRawParameterValue (kLoFiMode);
    driftModeParam_   = apvts.getRawParameterValue (kDriftMode);
    freezeParam_      = apvts.getRawParameterValue (kFreeze);
    bypassParam_atom_ = apvts.getRawParameterValue (kBypass);

    bypassParam_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (kBypass));
}

//==============================================================================
void VenusAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    engine_.prepare (sampleRate);
    // The dry signal is not delayed, so the plugin reports zero latency; the
    // wet reverb tail follows the dry, which is the musically expected result.
    setLatencySamples (0);
}

bool VenusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void VenusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Silence any output-only channels.
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const bool bypassed = (bypassParam_ != nullptr) && bypassParam_->get();
    if (bypassed)
        return; // dry passes through untouched

    // ---- per-block control update -------------------------------------------
    const float decayKnob   = decayParam_->load();
    const float wetMix      = mixParam_->load();
    const float dampKnob    = dampParam_->load();
    const float shimmerKnob = shimmerParam_->load();
    const float toneKnob    = shimmerToneParam_->load();
    const float detuneBip   = detuneParam_->load();
    const int   shimmerMode = (int) std::lround (shimmerModeParam_->load());
    const int   lofiMode    = (int) std::lround (lofiModeParam_->load());
    const int   driftMode   = (int) std::lround (driftModeParam_->load());
    const bool  freeze      = freezeParam_->load() >= 0.5f;

    engine_.setParameters (decayKnob, dampKnob, shimmerKnob, toneKnob, detuneBip,
                           shimmerMode, lofiMode, driftMode, freeze);

    const float dryMix = 1.0f - wetMix;

    static constexpr int kMaxChans = 32;
    float* chan[kMaxChans];
    const int nOut = juce::jmin (numOut, kMaxChans);
    for (int ch = 0; ch < nOut; ++ch)
        chan[ch] = buffer.getWritePointer (ch);

    const int nIn = juce::jmin (numIn, nOut); // input channels aliased in-place

    for (int i = 0; i < numSamples; ++i)
    {
        // Mono send = average of the input channels.
        float monoIn = 0.0f;
        for (int ch = 0; ch < nIn; ++ch)
            monoIn += chan[ch][i];
        if (nIn > 0)
            monoIn /= (float) nIn;

        const float wet = engine_.processSample (monoIn);

        // Wet is mono (reverb); dry keeps each channel's original signal.
        for (int ch = 0; ch < nOut; ++ch)
        {
            const float dry = (ch < nIn) ? chan[ch][i] : monoIn;
            chan[ch][i] = wet * wetMix + dry * dryMix;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* VenusAudioProcessor::createEditor()
{
    return new VenusAudioProcessorEditor (*this);
}

//==============================================================================
void VenusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VenusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VenusAudioProcessor();
}
