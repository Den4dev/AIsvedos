#include <cmath>
#include <iostream>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
juce::AudioProcessorParameter* findParameter (juce::AudioProcessor& processor, const juce::String& name)
{
    for (auto* parameter : processor.getParameters())
        if (parameter->getName (64) == name)
            return parameter;

    return nullptr;
}

bool bufferIsFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}

float processBlocks (juce::AudioPluginInstance& instance, int blocks, bool checkBypass = false)
{
    instance.setPlayConfigDetails (2, 2, 48000.0, 512);
    instance.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    float maxOut = 0.0f;
    double phase = 0.0;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto sample = 0.18f * (float) std::sin (phase)
                              + 0.04f * (float) std::sin (phase * 2.7);
            phase += juce::MathConstants<double>::twoPi * 220.0 / 48000.0;
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, 0.85f * sample);
        }

        const auto before = buffer.getMagnitude (0, buffer.getNumSamples());
        instance.processBlock (buffer, midi);

        if (! bufferIsFinite (buffer))
        {
            std::cerr << "Non-finite samples in output\n";
            return -1.0f;
        }

        const auto after = buffer.getMagnitude (0, buffer.getNumSamples());

        if (checkBypass && std::abs (after - before) > 1.0e-6f)
        {
            std::cerr << "Bypass changed the signal: " << before << " -> " << after << "\n";
            return -2.0f;
        }

        maxOut = std::max (maxOut, after);
    }

    instance.releaseResources();
    return maxOut;
}

bool checkOutputSmoothing (juce::AudioPluginInstance& instance)
{
    instance.setPlayConfigDetails (2, 2, 48000.0, 512);
    instance.prepareToPlay (48000.0, 512);

    if (auto* autoGain = findParameter (instance, "Auto Gain"))
        autoGain->setValueNotifyingHost (0.0f);

    auto* output = findParameter (instance, "Output");

    if (output == nullptr)
        return true;

    output->setValueNotifyingHost (0.6667f); // 0 dB on the -24..+12 range

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;

    auto renderBlock = [&]() -> float
    {
        buffer.clear();

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float sample = 0.2f * (float) std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * 220.0 / 48000.0;
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, sample);
        }

        instance.processBlock (buffer, midi);
        return buffer.getRMSLevel (0, 0, buffer.getNumSamples());
    };

    float previousRms = 0.0f;

    for (int i = 0; i < 16; ++i)
        previousRms = renderBlock();

    output->setValueNotifyingHost (0.0f);
    const float afterRms = renderBlock();
    instance.releaseResources();

    const float ratio = previousRms > 0.0f ? afterRms / previousRms : 0.0f;
    std::cout << "Output step ratio (smoothing): " << ratio << "\n";
    return ratio > 0.3f;
}
} // namespace

int main (int argc, char* argv[])
{
    std::cout << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI initialiser;
    juce::ignoreUnused (initialiser);

    if (argc < 2)
    {
        std::cerr << "usage: host_test path\\to\\VocalPocket.vst3\n";
        return 1;
    }

    const juce::File pluginFile { juce::String (argv[1]) };

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, pluginFile.getFullPathName());

    if (descriptions.isEmpty())
    {
        std::cerr << "No plugin types found in " << pluginFile.getFullPathName() << "\n";
        return 2;
    }

    std::cout << "Found " << descriptions.size() << " type(s)\n";

    for (auto* description : descriptions)
    {
        std::cout << "  " << description->name << " | " << description->manufacturerName
                  << " | " << description->version << " | " << description->pluginFormatName << "\n";
    }

    juce::String error;
    auto instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 512, error);

    if (instance == nullptr)
    {
        std::cerr << "Cannot instantiate: " << error << "\n";
        return 3;
    }

    std::cout << "Parameters: " << instance->getParameters().size() << "\n";

    for (auto* parameter : instance->getParameters())
        std::cout << "  " << parameter->getName (64) << "\n";

    const float vocalMax = processBlocks (*instance, 64);
    std::cout << "Vocal role max output: " << vocalMax << "\n";

    if (vocalMax <= 0.0f)
        return 4;

    {
        instance->prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> big (2, 4096);
        juce::MidiBuffer midi;
        big.clear();

        for (int i = 0; i < big.getNumSamples(); ++i)
        {
            const float sample = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / 48000.0);
            big.setSample (0, i, sample);
            big.setSample (1, i, sample);
        }

        instance->processBlock (big, midi);

        const bool oversizedOk = bufferIsFinite (big) && big.getMagnitude (0, big.getNumSamples()) > 0.0f;
        std::cout << "Oversized block (4096 @ max 512): " << (oversizedOk ? "ok" : "FAIL") << "\n";

        if (! oversizedOk)
            return 8;

        instance->releaseResources();
    }

    if (auto* bypass = findParameter (*instance, "FX Bypass"))
    {
        bypass->setValueNotifyingHost (1.0f);
        const float bypassMax = processBlocks (*instance, 8, true);
        std::cout << "Bypass max output: " << bypassMax << "\n";

        if (bypassMax < 0.0f)
            return 5;

        bypass->setValueNotifyingHost (0.0f);
    }

    if (auto* fit = findParameter (*instance, "Fit"))
        fit->setValueNotifyingHost (0.73f);

    juce::MemoryBlock state;
    instance->getStateInformation (state);
    std::cout << "State size: " << state.getSize() << " bytes\n";

    juce::String secondError;
    auto second = format.createInstanceFromDescription (*descriptions[0], 48000.0, 512, secondError);

    if (second == nullptr)
    {
        std::cerr << "Cannot instantiate second: " << secondError << "\n";
        return 6;
    }

    second->setStateInformation (state.getData(), (int) state.getSize());

    if (auto* fit = findParameter (*second, "Fit"))
        std::cout << "Restored Fit: " << fit->getValue() << "\n";

    if (auto* role = findParameter (*second, "Role"))
        role->setValueNotifyingHost (1.0f);

    const float beatMax = processBlocks (*second, 64);
    std::cout << "Beat role max output: " << beatMax << "\n";

    if (beatMax <= 0.0f)
        return 7;

    if (! checkOutputSmoothing (*second))
    {
        std::cerr << "Output parameter is not smoothed\n";
        return 9;
    }

    std::cout << "OK\n";
    return 0;
}
