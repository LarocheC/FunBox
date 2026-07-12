// PluginProcessor.h
#pragma once

#include <JuceHeader.h>
#include "VenusEngine.h"

//==============================================================================
// Venus — a DAW plugin port of the GuitarML Funbox "Venus" spectral reverb.
class VenusAudioProcessor : public juce::AudioProcessor
{
public:
    VenusAudioProcessor();
    ~VenusAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam_; }

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Parameter identifiers (shared with the editor).
    static constexpr const char* kDecay       = "decay";
    static constexpr const char* kMix         = "mix";
    static constexpr const char* kDamp        = "damp";
    static constexpr const char* kShimmer     = "shimmer";
    static constexpr const char* kShimmerTone = "shimmerTone";
    static constexpr const char* kDetune      = "detune";
    static constexpr const char* kShimmerMode = "shimmerMode";
    static constexpr const char* kLoFiMode    = "lofiMode";
    static constexpr const char* kDriftMode   = "driftMode";
    static constexpr const char* kFreeze      = "freeze";
    static constexpr const char* kBypass      = "bypass";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    VenusEngine engine_;

    // Cached parameter pointers for the audio thread.
    std::atomic<float>* decayParam_       = nullptr;
    std::atomic<float>* mixParam_         = nullptr;
    std::atomic<float>* dampParam_        = nullptr;
    std::atomic<float>* shimmerParam_     = nullptr;
    std::atomic<float>* shimmerToneParam_ = nullptr;
    std::atomic<float>* detuneParam_      = nullptr;
    std::atomic<float>* shimmerModeParam_ = nullptr;
    std::atomic<float>* lofiModeParam_    = nullptr;
    std::atomic<float>* driftModeParam_   = nullptr;
    std::atomic<float>* freezeParam_      = nullptr;
    std::atomic<float>* bypassParam_atom_ = nullptr;

    juce::AudioParameterBool* bypassParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VenusAudioProcessor)
};
