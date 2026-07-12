// PluginEditor.cpp
#include "PluginEditor.h"

namespace
{
    const juce::Colour kBgTop      { 0xff241033 };
    const juce::Colour kBgBottom   { 0xff0d0618 };
    const juce::Colour kAccent     { 0xffb46bff }; // Venus purple
    const juce::Colour kAccentSoft { 0xff6a3fa0 };
    const juce::Colour kText       { 0xffe9def7 };

    struct KnobDef { const char* id; const char* name; };
    const KnobDef kKnobDefs[] = {
        { VenusAudioProcessor::kDecay,       "Decay" },
        { VenusAudioProcessor::kMix,         "Mix" },
        { VenusAudioProcessor::kDamp,        "Damp" },
        { VenusAudioProcessor::kShimmer,     "Shimmer" },
        { VenusAudioProcessor::kShimmerTone, "Shimmer Tone" },
        { VenusAudioProcessor::kDetune,      "Detune" },
    };

    struct ComboDef { const char* id; const char* name; juce::StringArray items; };
}

//==============================================================================
VenusLookAndFeel::VenusLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ComboBox::backgroundColourId, kBgBottom.brighter (0.15f));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::ComboBox::outlineColourId, kAccentSoft);
    setColour (juce::PopupMenu::backgroundColourId, kBgTop);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccentSoft);
    setColour (juce::ToggleButton::textColourId, kText);
    setColour (juce::ToggleButton::tickColourId, kAccent);
    setColour (juce::ToggleButton::tickDisabledColourId, kAccentSoft.withAlpha (0.5f));
}

void VenusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float lineW = juce::jmax (2.5f, radius * 0.14f);
    const float arcR  = radius - lineW * 0.5f;

    // Background arc
    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                      rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (kBgBottom.brighter (0.25f));
    g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // Value arc
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                       rotaryStartAngle, angle, true);
    g.setColour (kAccent);
    g.strokePath (arc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Knob body
    const float bodyR = radius * 0.62f;
    juce::ColourGradient grad (kAccentSoft.brighter (0.2f), centre.x, centre.y - bodyR,
                               kBgBottom, centre.x, centre.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (kAccentSoft);
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Pointer
    juce::Point<float> tip (centre.x + arcR * std::cos (angle - juce::MathConstants<float>::halfPi),
                            centre.y + arcR * std::sin (angle - juce::MathConstants<float>::halfPi));
    juce::Point<float> base (centre.x + bodyR * 0.35f * std::cos (angle - juce::MathConstants<float>::halfPi),
                             centre.y + bodyR * 0.35f * std::sin (angle - juce::MathConstants<float>::halfPi));
    g.setColour (kText);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, lineW * 0.7f));
}

//==============================================================================
VenusAudioProcessorEditor::VenusAudioProcessorEditor (VenusAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& k = knobs[i];
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
        k.setColour (juce::Slider::textBoxTextColourId, kText);
        addAndMakeVisible (k);
        knobAtt[i] = std::make_unique<SliderAttachment> (processorRef.apvts, kKnobDefs[i].id, k);

        auto& lbl = knobLabels[i];
        lbl.setText (kKnobDefs[i].name, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centred);
        lbl.setFont (juce::Font (14.0f, juce::Font::bold));
        addAndMakeVisible (lbl);
    }

    const ComboDef comboDefs[kNumCombos] = {
        { VenusAudioProcessor::kShimmerMode, "Shimmer Mode",
          { "Octave Down", "Octave Up", "Octave Up + Down" } },
        { VenusAudioProcessor::kLoFiMode,    "LoFi Mode",
          { "Less LoFi", "Off", "More LoFi" } },
        { VenusAudioProcessor::kDriftMode,   "Drift Mode",
          { "Slow", "Off", "Fast" } },
    };

    for (int i = 0; i < kNumCombos; ++i)
    {
        auto& cb = combos[i];
        for (int j = 0; j < comboDefs[i].items.size(); ++j)
            cb.addItem (comboDefs[i].items[j], j + 1);
        addAndMakeVisible (cb);
        comboAtt[i] = std::make_unique<ComboBoxAttachment> (processorRef.apvts, comboDefs[i].id, cb);

        auto& lbl = comboLabels[i];
        lbl.setText (comboDefs[i].name, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centredLeft);
        lbl.setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (lbl);
    }

    freezeButton.setColour (juce::ToggleButton::textColourId, kText);
    addAndMakeVisible (freezeButton);
    freezeAtt = std::make_unique<ButtonAttachment> (processorRef.apvts,
                                                    VenusAudioProcessor::kFreeze, freezeButton);

    setSize (660, 460);
}

VenusAudioProcessorEditor::~VenusAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VenusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBgTop, 0, 0, kBgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().removeFromTop (66);
    g.setColour (kText);
    g.setFont (juce::Font (30.0f, juce::Font::bold));
    g.drawText ("VENUS", header.reduced (18, 8).removeFromLeft (220),
                juce::Justification::centredLeft);

    g.setColour (kAccent);
    g.setFont (juce::Font (14.0f));
    g.drawText ("Spectral Reverb", header.reduced (20, 10),
                juce::Justification::centredRight);

    g.setColour (kAccentSoft.withAlpha (0.6f));
    g.drawLine (16.0f, 66.0f, (float) getWidth() - 16.0f, 66.0f, 1.0f);
}

void VenusAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (72);

    // Two rows of three knobs.
    auto knobArea = area.removeFromTop (250);
    const int colW = knobArea.getWidth() / 3;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const int row = i / 3;
        const int col = i % 3;
        juce::Rectangle<int> cell (knobArea.getX() + col * colW,
                                   knobArea.getY() + row * (knobArea.getHeight() / 2),
                                   colW, knobArea.getHeight() / 2);
        cell.reduce (10, 6);
        knobLabels[i].setBounds (cell.removeFromTop (18));
        knobs[i].setBounds (cell);
    }

    // Bottom row: three mode selectors and the freeze toggle.
    auto bottom = area.reduced (16, 8);
    const int cellW = bottom.getWidth() / 4;
    for (int i = 0; i < kNumCombos; ++i)
    {
        auto cell = bottom.removeFromLeft (cellW).reduced (6, 0);
        comboLabels[i].setBounds (cell.removeFromTop (18));
        combos[i].setBounds (cell.removeFromTop (28));
    }
    auto fzCell = bottom.reduced (6, 0);
    fzCell.removeFromTop (18);
    freezeButton.setBounds (fzCell.removeFromTop (28));
}
