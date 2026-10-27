#include "PluginEditor.h"

#include <cmath>

#include "dsp/Params.h"

namespace
{
const juce::Colour kBackground { 0xff14161a };
const juce::Colour kPanel { 0xff1b1e24 };
const juce::Colour kPanelOutline { 0xff2a2f37 };
const juce::Colour kText { 0xffd7dbe2 };
const juce::Colour kMuted { 0xff7d8590 };
const juce::Colour kAccent { 0xffff9f43 };

struct VocalPocketLookAndFeel : juce::LookAndFeel_V4
{
    VocalPocketLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, kBackground);
        setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2f37));
        setColour (juce::Slider::thumbColourId, kAccent);
        setColour (juce::Slider::textBoxTextColourId, kText);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, kText);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23272e));
        setColour (juce::TextButton::buttonOnColourId, kAccent);
        setColour (juce::TextButton::textColourOffId, kText);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff23272e));
        setColour (juce::ComboBox::textColourId, kText);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2f343d));
        setColour (juce::ComboBox::arrowColourId, kMuted);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1e24));
        setColour (juce::PopupMenu::textColourId, kText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    }
};
} // namespace

EqCurveComponent::EqCurveComponent (VocalPocketProcessor& processorToUse)
    : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
}

void EqCurveComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0xff101318));
    g.fillRoundedRectangle (bounds, 6.0f);

    auto plot = bounds.reduced (10.0f, 12.0f);
    const float minFreq = 40.0f;
    const float maxFreq = 16000.0f;
    const float maxDb = 12.0f;

    auto frequencyToX = [&] (float frequency)
    {
        return plot.getX() + std::log (frequency / minFreq) / std::log (maxFreq / minFreq) * plot.getWidth();
    };

    auto dbToY = [&] (float db)
    {
        return plot.getCentreY() - db / maxDb * (plot.getHeight() * 0.5f);
    };

    g.setColour (juce::Colour (0xff232831));

    for (const float frequency : { 100.0f, 300.0f, 1000.0f, 3000.0f, 10000.0f })
        g.drawVerticalLine ((int) frequencyToX (frequency), plot.getY(), plot.getBottom());

    for (const float db : { -9.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, 9.0f })
    {
        const float y = dbToY (db);
        g.setColour (db == 0.0f ? juce::Colour (0xff3a414c) : juce::Colour (0xff20242b));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    float gains[vp::kNumEqBands] {};
    processor.getEqCurve (gains);

    auto gainAt = [&] (float frequency)
    {
        if (frequency <= vp::kEqFreqs[0])
            return gains[0];

        if (frequency >= vp::kEqFreqs[vp::kNumEqBands - 1])
            return gains[vp::kNumEqBands - 1];

        int index = 0;

        while (index < vp::kNumEqBands - 1 && vp::kEqFreqs[(size_t) index + 1] < frequency)
            ++index;

        const float low = vp::kEqFreqs[(size_t) index];
        const float high = vp::kEqFreqs[(size_t) index + 1];
        const float t = std::log (frequency / low) / std::log (high / low);
        return gains[index] * (1.0f - t) + gains[index + 1] * t;
    };

    juce::Path curve;
    constexpr int points = 96;

    for (int i = 0; i < points; ++i)
    {
        const float t = (float) i / (float) (points - 1);
        const float frequency = minFreq * std::pow (maxFreq / minFreq, t);
        const float x = frequencyToX (frequency);
        const float y = dbToY (juce::jlimit (-maxDb, maxDb, gainAt (frequency)));

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    auto fill = curve;
    fill.lineTo (plot.getRight(), dbToY (0.0f));
    fill.lineTo (plot.getX(), dbToY (0.0f));
    fill.closeSubPath();

    g.setColour (kAccent.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (kAccent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

VocalPocketEditor::VocalPocketEditor (VocalPocketProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      eqCurve (processorToUse)
{
    lookAndFeel = std::make_unique<VocalPocketLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    titleLabel.setText ("VOCALPOCKET", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("vocal fitting into the beat  /  v0.1", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (subtitleLabel);

    sessionLabel.setText ("session: " + processor.getSessionId(), juce::dontSendNotification);
    sessionLabel.setFont (juce::Font (11.0f));
    sessionLabel.setJustificationType (juce::Justification::centredRight);
    sessionLabel.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (sessionLabel);

    roleVocal.setClickingTogglesState (false);
    roleBeat.setClickingTogglesState (false);
    roleVocal.setRadioGroupId (1001);
    roleBeat.setRadioGroupId (1001);
    roleVocal.onClick = [this] { setRole (0); };
    roleBeat.onClick = [this] { setRole (1); };
    addAndMakeVisible (roleVocal);
    addAndMakeVisible (roleBeat);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    autoGainButton.setClickingTogglesState (true);
    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "autogain", autoGainButton);

    modeBox.addItemList ({ "Fit", "Style", "Reference" }, 1);
    genreBox.addItemList ({ "Trap", "Drill", "Hip-Hop", "Pop", "R&B", "Rock", "Indie" }, 1);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (genreBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "mode", modeBox);
    genreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "genre", genreBox);

    referenceTitle.setText ("REFERENCE", juce::dontSendNotification);
    referenceTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    referenceTitle.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (referenceTitle);

    referenceStatus.setFont (juce::Font (12.0f));
    referenceStatus.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (referenceStatus);

    learnStatus.setFont (juce::Font (12.0f));
    learnStatus.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (learnStatus);

    loadReferenceButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load reference track", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();

                if (file.existsAsFile())
                    processor.loadReferenceFile (file);
            });
    };
    addAndMakeVisible (loadReferenceButton);

    learnButton.onClick = [this] { processor.startLearn(); };
    addAndMakeVisible (learnButton);

    addAndMakeVisible (eqCurve);

    metersLabel.setFont (juce::Font (12.0f));
    metersLabel.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (metersLabel);

    linkStatus.setFont (juce::Font (12.0f, juce::Font::bold));
    linkStatus.setJustificationType (juce::Justification::centredRight);
    linkStatus.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (linkStatus);

    addKnob ("FIT", "fit", 24, 300, 78, 92);
    addKnob ("REF", "ref", 105, 300, 78, 92);
    addKnob ("COMP", "comp", 186, 300, 78, 92);
    addKnob ("DE-ESS", "deess", 267, 300, 78, 92);
    addKnob ("UNMASK", "unmask", 348, 300, 78, 92);
    addKnob ("SAT", "sat", 429, 300, 78, 92);
    addKnob ("LOW CUT", "lowcut", 510, 300, 78, 92);
    addKnob ("OUT", "output", 591, 300, 78, 92);
    addKnob ("MIX", "mix", 672, 300, 78, 92);

    setSize (776, 452);
    setResizable (false, false);

    setRole ((int) processor.apvts.getRawParameterValue ("role")->load());
    startTimerHz (30);
}

VocalPocketEditor::~VocalPocketEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalPocketEditor::setRole (int index)
{
    setChoiceParameter ("role", index);
}

void VocalPocketEditor::setChoiceParameter (const char* parameterId, int index)
{
    if (auto* parameter = processor.apvts.getParameter (parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        parameter->endChangeGesture();
    }
}

void VocalPocketEditor::addKnob (const juce::String& labelText, const juce::String& parameterId,
                                 int x, int y, int width, int height)
{
    Knob knob;
    knob.label = std::make_unique<juce::Label>();
    knob.label->setText (labelText, juce::dontSendNotification);
    knob.label->setFont (juce::Font (10.0f, juce::Font::bold));
    knob.label->setJustificationType (juce::Justification::centred);
    knob.label->setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (*knob.label);

    knob.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                  juce::Slider::TextBoxBelow);
    knob.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    knob.slider->setColour (juce::Slider::textBoxTextColourId, kText);
    addAndMakeVisible (*knob.slider);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, parameterId, *knob.slider);

    knob.label->setBounds (x, y, width, 16);
    knob.slider->setBounds (x, y + 16, width, height - 16);

    knobs.push_back (std::move (knob));
}

void VocalPocketEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (kAccent);
    g.fillRect (24, 64, 42, 3);

    auto panel = [&g] (juce::Rectangle<int> bounds)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour (kPanelOutline);
        g.drawRoundedRectangle (bounds.toFloat(), 8.0f, 1.0f);
    };

    panel ({ 24, 118, 360, 132 });
    panel ({ 396, 118, 356, 132 });
}

void VocalPocketEditor::resized()
{
    titleLabel.setBounds (24, 14, 320, 30);
    subtitleLabel.setBounds (24, 42, 360, 18);
    sessionLabel.setBounds (520, 22, 156, 18);
    bypassButton.setBounds (686, 18, 66, 28);

    roleVocal.setBounds (24, 74, 88, 30);
    roleBeat.setBounds (116, 74, 88, 30);
    modeBox.setBounds (220, 74, 120, 30);
    genreBox.setBounds (348, 74, 140, 30);
    autoGainButton.setBounds (496, 74, 70, 30);

    referenceTitle.setBounds (36, 128, 160, 16);
    loadReferenceButton.setBounds (36, 150, 110, 30);
    learnButton.setBounds (154, 150, 110, 30);
    referenceStatus.setBounds (36, 188, 336, 18);
    learnStatus.setBounds (36, 208, 336, 18);

    eqCurve.setBounds (404, 124, 340, 120);

    metersLabel.setBounds (24, 406, 600, 20);
    linkStatus.setBounds (600, 406, 152, 20);
}

void VocalPocketEditor::timerCallback()
{
    const int role = (int) processor.apvts.getRawParameterValue ("role")->load();
    roleVocal.setToggleState (role == 0, juce::dontSendNotification);
    roleBeat.setToggleState (role == 1, juce::dontSendNotification);
    roleVocal.setColour (juce::TextButton::buttonColourId, role == 0 ? kAccent : juce::Colour (0xff23272e));
    roleBeat.setColour (juce::TextButton::buttonColourId, role == 1 ? kAccent : juce::Colour (0xff23272e));

    if (processor.isLearning())
    {
        learnStatus.setText ("Learning: " + juce::String (processor.getLearnProgress() * 100.0f, 0) + "%  (play the vocal)",
                             juce::dontSendNotification);
        learnStatus.setColour (juce::Label::textColourId, kAccent);
    }
    else if (processor.hasVocalProfile())
    {
        learnStatus.setText ("Vocal profile: ready", juce::dontSendNotification);
        learnStatus.setColour (juce::Label::textColourId, juce::Colour (0xff61d095));
    }
    else
    {
        learnStatus.setText ("Vocal profile: not learned", juce::dontSendNotification);
        learnStatus.setColour (juce::Label::textColourId, kMuted);
    }

    if (processor.isLearnComplete())
        processor.completeLearnIfReady();

    if (processor.isReferenceLoading())
    {
        referenceStatus.setText ("Analyzing: " + juce::String (processor.getReferenceProgress() * 100.0f, 0) + "%",
                                 juce::dontSendNotification);
        referenceStatus.setColour (juce::Label::textColourId, kAccent);
    }
    else if (processor.hasReference())
    {
        referenceStatus.setText ("Ref: " + processor.getReferenceName(), juce::dontSendNotification);
        referenceStatus.setColour (juce::Label::textColourId, kText);
    }
    else
    {
        referenceStatus.setText ("No reference loaded", juce::dontSendNotification);
        referenceStatus.setColour (juce::Label::textColourId, kMuted);
    }

    metersLabel.setText (
        "IN " + juce::String (processor.getInputLufs(), 1) + " LUFS   "
        "OUT " + juce::String (processor.getOutputLufs(), 1) + " LUFS   "
        "GR " + juce::String (processor.getGainReductionDb(), 1) + " dB   "
        "DE-ESS " + juce::String (processor.getDeEssDb(), 1) + " dB   "
        "AUTO " + juce::String (processor.getAutoGainDb(), 1) + " dB",
        juce::dontSendNotification);

    const bool linked = processor.isOppositeLinked();
    const int currentRole = (int) processor.apvts.getRawParameterValue ("role")->load();
    linkStatus.setText (linked ? (currentRole == 0 ? "LINK: BEAT OK" : "LINK: VOCAL OK") : "LINK: WAITING",
                        juce::dontSendNotification);
    linkStatus.setColour (juce::Label::textColourId, linked ? juce::Colour (0xff61d095) : kMuted);

    eqCurve.repaint();
}
