#include "PluginProcessor.h"
#include "PluginEditor.h"

VenusAudioProcessor::VenusAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

VenusAudioProcessor::~VenusAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout VenusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Continuous knobs (matching Venus hardware knobs)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"decay", 1}, "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"damp", 1}, "Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.5f)); // skew for exponential feel

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"shimmer", 1}, "Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"shimmer_tone", 1}, "Shimmer Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"detune", 1}, "Detune",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f)); // 0.5 = center/noon = no detune

    // Switch parameters (matching Venus 3-way switches)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"shimmer_mode", 1}, "Shimmer Mode",
        juce::StringArray{"Octave Down", "Octave Up", "Both"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lofi_mode", 1}, "Lo-Fi Mode",
        juce::StringArray{"Less Lo-Fi", "Normal", "More Lo-Fi"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"drift_mode", 1}, "Drift Mode",
        juce::StringArray{"Slow Drift", "No Drift", "Fast Drift"}, 1));

    // Freeze toggle (matching Venus footswitch 2)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"freeze", 1}, "Freeze", false));

    return {params.begin(), params.end()};
}

void VenusAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    reverb_.init(static_cast<float>(sampleRate));
}

void VenusAudioProcessor::releaseResources() {}

bool VenusAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput = layouts.getMainInputChannelSet();

    // Mono or stereo only
    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    // Input must match output
    return mainInput == mainOutput;
}

void VenusAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Read parameters
    reverb_.setDecay(apvts_.getRawParameterValue("decay")->load());
    reverb_.setMix(apvts_.getRawParameterValue("mix")->load());
    reverb_.setDamp(apvts_.getRawParameterValue("damp")->load());
    reverb_.setShimmer(apvts_.getRawParameterValue("shimmer")->load());
    reverb_.setShimmerTone(apvts_.getRawParameterValue("shimmer_tone")->load());
    reverb_.setDetune(apvts_.getRawParameterValue("detune")->load());
    reverb_.setFreeze(apvts_.getRawParameterValue("freeze")->load() >= 0.5f);

    reverb_.setShimmerMode(static_cast<int>(apvts_.getRawParameterValue("shimmer_mode")->load()));
    reverb_.setLofiMode(static_cast<int>(apvts_.getRawParameterValue("lofi_mode")->load()));
    reverb_.setDriftMode(static_cast<int>(apvts_.getRawParameterValue("drift_mode")->load()));

    // Process mono (left channel), copy to right — same as original Venus
    auto* leftChannel = buffer.getWritePointer(0);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
        leftChannel[i] = reverb_.processSample(leftChannel[i]);

    // Copy to remaining channels (stereo output like original)
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, leftChannel, numSamples);
}

juce::AudioProcessorEditor* VenusAudioProcessor::createEditor()
{
    return new VenusAudioProcessorEditor(*this);
}

bool VenusAudioProcessor::hasEditor() const { return true; }

const juce::String VenusAudioProcessor::getName() const { return JucePlugin_Name; }
bool VenusAudioProcessor::acceptsMidi() const { return false; }
bool VenusAudioProcessor::producesMidi() const { return false; }
bool VenusAudioProcessor::isMidiEffect() const { return false; }
double VenusAudioProcessor::getTailLengthSeconds() const { return 5.0; }

int VenusAudioProcessor::getNumPrograms() { return 1; }
int VenusAudioProcessor::getCurrentProgram() { return 0; }
void VenusAudioProcessor::setCurrentProgram(int) {}
const juce::String VenusAudioProcessor::getProgramName(int) { return {}; }
void VenusAudioProcessor::changeProgramName(int, const juce::String&) {}

void VenusAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VenusAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VenusAudioProcessor();
}
