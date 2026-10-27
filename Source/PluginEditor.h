#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class EqCurveComponent : public juce::Component
{
public:
    explicit EqCurveComponent (VocalPocketProcessor& processorToUse);

    void paint (juce::Graphics&) override;

private:
    VocalPocketProcessor& processor;
};

class VocalPocketEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit VocalPocketEditor (VocalPocketProcessor&);
    ~VocalPocketEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setRole (int index);
    void setChoiceParameter (const char* parameterId, int index);
    void addKnob (const juce::String& labelText, const juce::String& parameterId,
                  int x, int y, int width, int height);

    VocalPocketProcessor& processor;
    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label sessionLabel;
    juce::Label referenceTitle;
    juce::Label referenceStatus;
    juce::Label learnStatus;
    juce::Label metersLabel;
    juce::Label linkStatus;

    juce::TextButton roleVocal { "VOCAL" };
    juce::TextButton roleBeat { "BEAT" };
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton autoGainButton { "AUTO" };
    juce::TextButton loadReferenceButton { "LOAD REF..." };
    juce::TextButton learnButton { "LEARN 8s" };

    juce::ComboBox modeBox;
    juce::ComboBox genreBox;

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::vector<Knob> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> genreAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;

    EqCurveComponent eqCurve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalPocketEditor)
};
