#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VenusAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VenusAudioProcessorEditor(VenusAudioProcessor&);
    ~VenusAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VenusAudioProcessor& processor_;

    // Knobs
    juce::Slider decaySlider_, mixSlider_, dampSlider_;
    juce::Slider shimmerSlider_, shimmerToneSlider_, detuneSlider_;

    // Labels
    juce::Label decayLabel_, mixLabel_, dampLabel_;
    juce::Label shimmerLabel_, shimmerToneLabel_, detuneLabel_;

    // Switches
    juce::ComboBox shimmerModeBox_, lofiModeBox_, driftModeBox_;
    juce::Label shimmerModeLabel_, lofiModeLabel_, driftModeLabel_;

    // Freeze button
    juce::ToggleButton freezeButton_{"Freeze"};

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shimmerAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shimmerToneAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAttach_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shimmerModeAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lofiModeAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> driftModeAttach_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach_;

    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setupComboBox(juce::ComboBox& box, juce::Label& label, const juce::String& text,
                       const juce::StringArray& items);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VenusAudioProcessorEditor)
};
