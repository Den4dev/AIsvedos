#include "PluginProcessor.h"

#include <memory>

#include "PluginEditor.h"
#include "dsp/ReferenceAnalysis.h"

namespace
{
juce::String profileToText (const vp::BandProfile& profile)
{
    juce::String text;
    text << (profile.valid ? 1 : 0) << ";" << juce::String (profile.lufs, 2) << ";";

    for (int i = 0; i < vp::kNumBands; ++i)
    {
        if (i > 0)
            text << ",";

        text << juce::String (profile.tiltDb[(size_t) i], 3);
    }

    return text;
}

vp::BandProfile profileFromText (const juce::String& text)
{
    vp::BandProfile profile;
    auto parts = juce::StringArray::fromTokens (text, ";", "");

    if (parts.size() < 3)
        return profile;

    profile.valid = parts[0].getIntValue() != 0;
    profile.lufs = parts[1].getFloatValue();

    auto values = juce::StringArray::fromTokens (parts[2], ",", "");

    for (int i = 0; i < vp::kNumBands && i < values.size(); ++i)
    {
        profile.tiltDb[(size_t) i] = values[i].getFloatValue();
        profile.bandsDb[(size_t) i] = profile.tiltDb[(size_t) i];
    }

    return profile;
}
} // namespace

VocalPocketProcessor::VocalPocketProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalPocketState", createLayout())
{
    cacheParameterPointers();
    setSession ("default");
}

VocalPocketProcessor::~VocalPocketProcessor()
{
    if (referenceThread.joinable())
        referenceThread.join();
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalPocketProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "role", 1 }, "Role", juce::StringArray { "Vocal", "Beat" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Fit", "Style", "Reference" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "genre", 1 }, "Genre",
        juce::StringArray { "Trap", "Drill", "Hip-Hop", "Pop", "R&B", "Rock", "Indie" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fit", 1 }, "Fit", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ref", 1 }, "Reference Match", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lowcut", 1 }, "Low Cut", juce::NormalisableRange<float> (40.0f, 200.0f, 0.5f), 90.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp", 1 }, "Compression", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "deess", 1 }, "De-Ess", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "unmask", 1 }, "Unmask", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sat", 1 }, "Saturation", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "FX Bypass", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "autogain", 1 }, "Auto Gain", true));

    return { params.begin(), params.end() };
}

void VocalPocketProcessor::cacheParameterPointers()
{
    roleParam = apvts.getRawParameterValue ("role");
    modeParam = apvts.getRawParameterValue ("mode");
    genreParam = apvts.getRawParameterValue ("genre");
    fitParam = apvts.getRawParameterValue ("fit");
    refParam = apvts.getRawParameterValue ("ref");
    lowCutParam = apvts.getRawParameterValue ("lowcut");
    compParam = apvts.getRawParameterValue ("comp");
    deEssParam = apvts.getRawParameterValue ("deess");
    unmaskParam = apvts.getRawParameterValue ("unmask");
    satParam = apvts.getRawParameterValue ("sat");
    outputParam = apvts.getRawParameterValue ("output");
    mixParam = apvts.getRawParameterValue ("mix");
    bypassParam = apvts.getRawParameterValue ("bypass");
    autoGainParam = apvts.getRawParameterValue ("autogain");
}

void VocalPocketProcessor::setSession (const juce::String& id)
{
    sessionId = id.isEmpty() ? "default" : id;
    auto newBus = vp::getSessionBus (sessionId.toStdString());

    if (busHolder != nullptr)
        retiredBuses.push_back (busHolder);

    busHolder = newBus;
    busPtr.store (newBus.get(), std::memory_order_release);
}

void VocalPocketProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    engine.setBus (busPtr.load (std::memory_order_acquire));
}

void VocalPocketProcessor::releaseResources()
{
}

bool VocalPocketProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void VocalPocketProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (totalIn == 0 || buffer.getNumSamples() == 0)
        return;

    vp::MixParams params;
    params.role = (vp::Role) (int) roleParam->load();
    params.mode = (vp::Mode) (int) modeParam->load();
    params.genre = (vp::Genre) (int) genreParam->load();
    params.fitAmount = fitParam->load() * 0.01f;
    params.refMatch = refParam->load() * 0.01f;
    params.lowCutHz = lowCutParam->load();
    params.compAmount = compParam->load() * 0.01f;
    params.deEssAmount = deEssParam->load() * 0.01f;
    params.unmaskAmount = unmaskParam->load() * 0.01f;
    params.saturation = satParam->load() * 0.01f;
    params.outputGainDb = outputParam->load();
    params.mix = mixParam->load() * 0.01f;
    params.bypass = bypassParam->load() > 0.5f;
    params.autoGain = autoGainParam->load() > 0.5f;

    engine.setParams (params);
    engine.setBus (busPtr.load (std::memory_order_acquire));

    float tempo = 0.0f;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                tempo = (float) *bpm;
        }
    }

    engine.process (buffer, tempo);
}

void VocalPocketProcessor::loadReferenceFile (const juce::File& file)
{
    if (referenceThread.joinable())
        referenceThread.join();

    {
        const juce::ScopedLock lock (referenceNameLock);
        referenceName = file.getFileNameWithoutExtension();
    }

    referenceLoading.store (true, std::memory_order_release);
    referenceProgress.store (0.0f, std::memory_order_relaxed);

    referenceThread = std::thread ([this, file]
    {
        auto profile = vp::ReferenceAnalysis::analyzeFile (file, &referenceProgress);
        engine.setReferenceProfile (profile);
        referenceLoading.store (false, std::memory_order_release);
    });
}

void VocalPocketProcessor::startLearn()
{
    engine.beginVocalCapture (8.0f);
}

void VocalPocketProcessor::completeLearnIfReady()
{
    if (engine.isCaptureComplete())
        engine.takeVocalProfile();
}

juce::String VocalPocketProcessor::getReferenceName() const
{
    const juce::ScopedLock lock (referenceNameLock);
    return referenceName;
}

void VocalPocketProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("sessionId", sessionId, nullptr);
    state.setProperty ("referenceName", getReferenceName(), nullptr);
    state.setProperty ("referenceProfile", profileToText (engine.getReferenceProfile()), nullptr);
    state.setProperty ("vocalProfile", profileToText (engine.getVocalProfile()), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalPocketProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    apvts.replaceState (tree);
    setSession (tree.getProperty ("sessionId", "default").toString());

    {
        const juce::ScopedLock lock (referenceNameLock);
        referenceName = tree.getProperty ("referenceName", "").toString();
    }

    engine.setReferenceProfile (profileFromText (tree.getProperty ("referenceProfile").toString()));
    engine.setVocalProfile (profileFromText (tree.getProperty ("vocalProfile").toString()));
}

juce::AudioProcessorEditor* VocalPocketProcessor::createEditor()
{
    return new VocalPocketEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalPocketProcessor();
}
