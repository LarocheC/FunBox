#include "PluginEditor.h"

static const int kWindowWidth = 500;
static const int kWindowHeight = 420;

VenusAudioProcessorEditor::VenusAudioProcessorEditor(VenusAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    // Knobs
    setupSlider(decaySlider_, decayLabel_, "Decay");
    setupSlider(mixSlider_, mixLabel_, "Mix");
    setupSlider(dampSlider_, dampLabel_, "Damp");
    setupSlider(shimmerSlider_, shimmerLabel_, "Shimmer");
    setupSlider(shimmerToneSlider_, shimmerToneLabel_, "Shimmer Tone");
    setupSlider(detuneSlider_, detuneLabel_, "Detune");

    // Switches
    setupComboBox(shimmerModeBox_, shimmerModeLabel_, "Shimmer Mode",
                  {"Octave Down", "Octave Up", "Both"});
    setupComboBox(lofiModeBox_, lofiModeLabel_, "Lo-Fi",
                  {"Less Lo-Fi", "Normal", "More Lo-Fi"});
    setupComboBox(driftModeBox_, driftModeLabel_, "Drift",
                  {"Slow Drift", "No Drift", "Fast Drift"});

    // Freeze button
    addAndMakeVisible(freezeButton_);

    // Attachments
    auto& apvts = processor_.getAPVTS();
    decayAttach_        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", decaySlider_);
    mixAttach_          = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider_);
    dampAttach_         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "damp", dampSlider_);
    shimmerAttach_      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "shimmer", shimmerSlider_);
    shimmerToneAttach_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "shimmer_tone", shimmerToneSlider_);
    detuneAttach_       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "detune", detuneSlider_);

    shimmerModeAttach_  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "shimmer_mode", shimmerModeBox_);
    lofiModeAttach_     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "lofi_mode", lofiModeBox_);
    driftModeAttach_    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "drift_mode", driftModeBox_);

    freezeAttach_       = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "freeze", freezeButton_);

    setSize(kWindowWidth, kWindowHeight);
}

VenusAudioProcessorEditor::~VenusAudioProcessorEditor() = default;

void VenusAudioProcessorEditor::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void VenusAudioProcessorEditor::setupComboBox(juce::ComboBox& box, juce::Label& label,
                                               const juce::String& text, const juce::StringArray& items)
{
    for (int i = 0; i < items.size(); ++i)
        box.addItem(items[i], i + 1);
    addAndMakeVisible(box);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void VenusAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Title
    g.setColour(juce::Colour(0xffe0c097));
    g.setFont(juce::FontOptions(24.0f));
    g.drawText("VENUS", getLocalBounds().removeFromTop(40), juce::Justification::centred);

    g.setFont(juce::FontOptions(12.0f));
    g.drawText("Spectral Reverb", getLocalBounds().removeFromTop(58), juce::Justification::centred);
}

void VenusAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(55); // title space

    // Row 1: 3 knobs (Decay, Mix, Damp)
    auto knobRow1 = area.removeFromTop(130);
    const int knobWidth = knobRow1.getWidth() / 3;
    const int knobSize = 100;

    auto layoutKnob = [&](juce::Slider& slider, juce::Label& label, juce::Rectangle<int> col)
    {
        label.setBounds(col.removeFromTop(18));
        auto knobArea = col.withSizeKeepingCentre(knobSize, knobSize);
        slider.setBounds(knobArea);
    };

    layoutKnob(decaySlider_, decayLabel_, knobRow1.removeFromLeft(knobWidth));
    layoutKnob(mixSlider_, mixLabel_, knobRow1.removeFromLeft(knobWidth));
    layoutKnob(dampSlider_, dampLabel_, knobRow1);

    // Row 2: 3 knobs (Shimmer, Shimmer Tone, Detune)
    auto knobRow2 = area.removeFromTop(130);
    layoutKnob(shimmerSlider_, shimmerLabel_, knobRow2.removeFromLeft(knobWidth));
    layoutKnob(shimmerToneSlider_, shimmerToneLabel_, knobRow2.removeFromLeft(knobWidth));
    layoutKnob(detuneSlider_, detuneLabel_, knobRow2);

    area.removeFromTop(5);

    // Row 3: 3 combo boxes (switches)
    auto switchRow = area.removeFromTop(50);
    const int switchWidth = switchRow.getWidth() / 3;

    auto layoutSwitch = [&](juce::ComboBox& box, juce::Label& label, juce::Rectangle<int> col)
    {
        col = col.reduced(5, 0);
        label.setBounds(col.removeFromTop(18));
        box.setBounds(col.removeFromTop(28));
    };

    layoutSwitch(shimmerModeBox_, shimmerModeLabel_, switchRow.removeFromLeft(switchWidth));
    layoutSwitch(lofiModeBox_, lofiModeLabel_, switchRow.removeFromLeft(switchWidth));
    layoutSwitch(driftModeBox_, driftModeLabel_, switchRow);

    // Freeze button
    area.removeFromTop(5);
    freezeButton_.setBounds(area.withSizeKeepingCentre(120, 30));
}
