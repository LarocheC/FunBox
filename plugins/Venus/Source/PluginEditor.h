// PluginEditor.h
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class VenusLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VenusLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

//==============================================================================
class VenusAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VenusAudioProcessorEditor (VenusAudioProcessor&);
    ~VenusAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    static constexpr int kNumKnobs  = 6;
    static constexpr int kNumCombos = 3;

    VenusAudioProcessor& processorRef;
    VenusLookAndFeel     lnf;

    juce::Slider knobs[kNumKnobs];
    juce::Label  knobLabels[kNumKnobs];
    std::unique_ptr<SliderAttachment> knobAtt[kNumKnobs];

    juce::ComboBox combos[kNumCombos];
    juce::Label    comboLabels[kNumCombos];
    std::unique_ptr<ComboBoxAttachment> comboAtt[kNumCombos];

    juce::ToggleButton freezeButton { "Freeze" };
    std::unique_ptr<ButtonAttachment> freezeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VenusAudioProcessorEditor)
};
