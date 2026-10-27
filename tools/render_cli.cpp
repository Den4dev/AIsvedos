#include <iostream>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Analyzer.h"
#include "dsp/MixEngine.h"
#include "dsp/ReferenceAnalysis.h"

namespace
{
struct LoadedAudio
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 48000.0;
};

struct RenderStats
{
    float maxGainReductionDb = 0.0f;
    float maxDeEssDb = 0.0f;
};

LoadedAudio loadAudio (const juce::File& file)
{
    LoadedAudio result;

    if (! file.existsAsFile())
    {
        std::cerr << "File not found: " << file.getFullPathName() << "\n";
        return result;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        std::cerr << "Cannot read: " << file.getFullPathName() << "\n";
        return result;
    }

    const int numSamples = (int) reader->lengthInSamples;
    const int sourceChannels = (int) reader->numChannels;
    result.sampleRate = reader->sampleRate;

    juce::AudioBuffer<float> source (sourceChannels, numSamples);
    reader->read (&source, 0, numSamples, 0, true, sourceChannels > 1);

    result.buffer.setSize (2, numSamples);
    result.buffer.clear();
    result.buffer.copyFrom (0, 0, source, 0, 0, numSamples);
    result.buffer.copyFrom (1, 0, source, sourceChannels > 1 ? 1 : 0, 0, numSamples);

    return result;
}

void resampleTo (LoadedAudio& audio, double targetRate)
{
    if (std::abs (audio.sampleRate - targetRate) < 1.0)
        return;

    const double ratio = audio.sampleRate / targetRate;
    const int outputSamples = (int) std::ceil ((double) audio.buffer.getNumSamples() / ratio);
    juce::AudioBuffer<float> output (2, outputSamples);
    output.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process (ratio,
                              audio.buffer.getReadPointer (ch),
                              output.getWritePointer (ch),
                              outputSamples);
    }

    audio.buffer = std::move (output);
    audio.sampleRate = targetRate;
}

void matchLength (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (buffer.getNumSamples() == numSamples)
        return;

    juce::AudioBuffer<float> output (2, numSamples);
    output.clear();
    const int copySamples = std::min (numSamples, buffer.getNumSamples());

    for (int ch = 0; ch < 2; ++ch)
        output.copyFrom (ch, 0, buffer, ch, 0, copySamples);

    buffer = std::move (output);
}

vp::BandProfile analyzeBuffer (const LoadedAudio& audio)
{
    vp::Analyzer analyzer;
    analyzer.prepare (audio.sampleRate, audio.buffer.getNumChannels());
    analyzer.beginCapture ((float) ((double) audio.buffer.getNumSamples() / audio.sampleRate) + 1.0f);
    analyzer.process (audio.buffer);

    auto profile = analyzer.finishCapture();
    profile.crestDb = analyzer.getPeakDb() - analyzer.getRmsDb();
    return profile;
}

vp::BandFrame frameFromProfile (const vp::BandProfile& profile)
{
    vp::BandFrame frame;
    frame.bandsDb = profile.bandsDb;
    frame.lufs = profile.lufs;
    frame.activity = 1.0f;
    return frame;
}

void processBuffer (vp::MixEngine& engine, juce::AudioBuffer<float>& buffer, RenderStats& stats, int blockSize)
{
    juce::AudioBuffer<float> block (2, blockSize);

    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        const int count = std::min (blockSize, buffer.getNumSamples() - offset);
        block.setSize (2, count, false, false, true);

        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, count);

        engine.process (block, 120.0f);

        stats.maxGainReductionDb = std::max (stats.maxGainReductionDb, engine.getGainReductionDb());
        stats.maxDeEssDb = std::max (stats.maxDeEssDb, engine.getDeEssDb());

        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, offset, block, ch, 0, count);
    }
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    auto stream = file.createOutputStream();

    if (stream == nullptr)
        return false;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.release(), sampleRate, 2, 24, {}, 0));

    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

float peakOf (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

    return peak;
}

vp::Genre parseGenre (const juce::String& value)
{
    const auto genre = value.toLowerCase();

    if (genre == "drill") return vp::Genre::drill;
    if (genre == "hiphop" || genre == "hip-hop") return vp::Genre::hiphop;
    if (genre == "pop") return vp::Genre::pop;
    if (genre == "rnb" || genre == "r&b") return vp::Genre::rnb;
    if (genre == "rock") return vp::Genre::rock;
    if (genre == "indie") return vp::Genre::indie;
    return vp::Genre::trap;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::StringArray args;

    for (int i = 0; i < argc; ++i)
        args.add (argv[i]);

    auto valueOf = [&args] (const juce::String& name, const juce::String& fallback = {})
    {
        const int index = args.indexOf (name);

        if (index >= 0 && index + 1 < args.size())
            return args[index + 1];

        return fallback;
    };

    const auto vocalPath = valueOf ("--vocal");
    const auto beatPath = valueOf ("--beat");
    const auto referencePath = valueOf ("--reference");
    const auto outputBase = valueOf ("--out", "test_audio/out");
    const int blockSize = std::max (32, valueOf ("--block", "512").getIntValue());

    if (vocalPath.isEmpty() || beatPath.isEmpty())
    {
        std::cerr << "Usage: render_cli --vocal vocal.wav --beat beat.wav "
                     "[--reference ref.wav] [--out prefix] [--genre trap] "
                     "[--mode fit|reference] [--fit 50] [--ref 50] [--comp 50] "
                     "[--deess 50] [--unmask 50] [--sat 50] [--lowcut 90] [--outgain 0]\n";
        return 1;
    }

    auto vocal = loadAudio (juce::File (vocalPath));
    auto beat = loadAudio (juce::File (beatPath));

    if (vocal.buffer.getNumSamples() == 0 || beat.buffer.getNumSamples() == 0)
        return 2;

    resampleTo (beat, vocal.sampleRate);
    matchLength (beat.buffer, vocal.buffer.getNumSamples());

    const auto beatProfile = analyzeBuffer (beat);
    const auto vocalProfile = analyzeBuffer (vocal);

    vp::BandProfile referenceProfile;

    if (referencePath.isNotEmpty())
    {
        referenceProfile = vp::ReferenceAnalysis::analyzeFile (juce::File (referencePath));

        if (! referenceProfile.valid)
            std::cerr << "Warning: reference analysis failed\n";
    }

    auto bus = vp::getSessionBus ("cli");

    vp::MixParams vocalParams;
    vocalParams.role = vp::Role::vocal;
    vocalParams.mode = valueOf ("--mode", "fit") == "reference" ? vp::Mode::reference : vp::Mode::fit;
    vocalParams.genre = parseGenre (valueOf ("--genre", "trap"));
    vocalParams.fitAmount = valueOf ("--fit", "50").getFloatValue() * 0.01f;
    vocalParams.refMatch = valueOf ("--ref", "50").getFloatValue() * 0.01f;
    vocalParams.compAmount = valueOf ("--comp", "50").getFloatValue() * 0.01f;
    vocalParams.deEssAmount = valueOf ("--deess", "50").getFloatValue() * 0.01f;
    vocalParams.unmaskAmount = valueOf ("--unmask", "50").getFloatValue() * 0.01f;
    vocalParams.saturation = valueOf ("--sat", "50").getFloatValue() * 0.01f;
    vocalParams.lowCutHz = valueOf ("--lowcut", "90").getFloatValue();
    vocalParams.outputGainDb = valueOf ("--outgain", "0").getFloatValue();
    vocalParams.autoGain = ! args.contains ("--no-autogain");

    auto beatParams = vocalParams;
    beatParams.role = vp::Role::beat;

    auto vocalProcessed = vocal.buffer;
    auto beatProcessed = beat.buffer;

    vp::BandFrame beatFrame = frameFromProfile (beatProfile);
    vp::BandFrame vocalFrame = frameFromProfile (vocalProfile);

    RenderStats vocalStats;
    RenderStats beatStats;

    {
        vp::MixEngine vocalEngine;
        vocalEngine.prepare (vocal.sampleRate, 512, 2);
        vocalEngine.setParams (vocalParams);
        vocalEngine.setBus (bus.get());
        vocalEngine.setReferenceProfile (referenceProfile);
        vocalEngine.setVocalProfile (vocalProfile);
        vocalEngine.setOppositeFrameOverride (beatFrame);
        processBuffer (vocalEngine, vocalProcessed, vocalStats, blockSize);
        std::cout << "Vocal auto-gain: " << vocalEngine.getAutoGainDb() << " dB\n";
    }

    {
        vp::MixEngine beatEngine;
        beatEngine.prepare (beat.sampleRate, 512, 2);
        beatEngine.setParams (beatParams);
        beatEngine.setBus (bus.get());
        beatEngine.setOppositeFrameOverride (vocalFrame);
        processBuffer (beatEngine, beatProcessed, beatStats, blockSize);
    }

    juce::AudioBuffer<float> mix (2, vocalProcessed.getNumSamples());
    mix.clear();

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < mix.getNumSamples(); ++i)
            mix.setSample (ch, i, 0.5f * vocalProcessed.getSample (ch, i)
                                 + 0.85f * beatProcessed.getSample (ch, i));

    const float peak = peakOf (mix);

    if (peak > 0.95f)
        mix.applyGain (0.95f / peak);

    const auto outputVocalProfile = analyzeBuffer (LoadedAudio { vocalProcessed, vocal.sampleRate });
    const auto outputBeatProfile = analyzeBuffer (LoadedAudio { beatProcessed, beat.sampleRate });
    const auto mixProfile = analyzeBuffer (LoadedAudio { mix, vocal.sampleRate });

    const auto vocalFile = juce::File (outputBase + "_vocal.wav");
    const auto beatFile = juce::File (outputBase + "_beat.wav");
    const auto mixFile = juce::File (outputBase + "_mix.wav");

    const bool wrote = writeWav (vocalFile, vocalProcessed, vocal.sampleRate)
                    && writeWav (beatFile, beatProcessed, beat.sampleRate)
                    && writeWav (mixFile, mix, vocal.sampleRate);

    std::cout << "Vocal: " << vocalPath << " (" << vocal.sampleRate << " Hz)\n";
    std::cout << "Beat:  " << beatPath << "\n";

    if (referenceProfile.valid)
        std::cout << "Ref:   " << referencePath << " (LUFS " << referenceProfile.lufs << ")\n";

    std::cout << "Vocal profile: " << (vocalProfile.valid ? "ok" : "failed") << "\n";
    std::cout << "Vocal LUFS: in " << vocalProfile.lufs << " -> out " << outputVocalProfile.lufs
              << " | max GR " << vocalStats.maxGainReductionDb
              << " dB | de-ess " << vocalStats.maxDeEssDb << " dB\n";
    std::cout << "Beat  LUFS: in " << beatProfile.lufs << " -> out " << outputBeatProfile.lufs
              << " | max GR " << beatStats.maxGainReductionDb << " dB\n";
    std::cout << "Mix   LUFS: " << mixProfile.lufs << " | peak " << peakOf (mix) << "\n";

    if (! wrote)
    {
        std::cerr << "Failed to write output files\n";
        return 3;
    }

    std::cout << "Wrote: " << vocalFile.getFullPathName() << "\n"
              << "       " << beatFile.getFullPathName() << "\n"
              << "       " << mixFile.getFullPathName() << "\n";

    return 0;
}
