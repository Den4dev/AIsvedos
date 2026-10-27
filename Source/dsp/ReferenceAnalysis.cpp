#include "ReferenceAnalysis.h"

#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

#include "Analyzer.h"

namespace vp::ReferenceAnalysis
{

BandProfile analyzeFile (const juce::File& file, std::atomic<float>* progress)
{
    BandProfile empty;

    if (! file.existsAsFile())
        return empty;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return empty;

    const double sampleRate = reader->sampleRate;
    const int numChannels = std::clamp ((int) reader->numChannels, 1, 2);

    Analyzer analyzer;
    analyzer.prepare (sampleRate, numChannels);
    analyzer.beginCapture ((float) ((double) reader->lengthInSamples / sampleRate) + 1.0f);

    juce::AudioBuffer<float> buffer (numChannels, 8192);
    juce::int64 position = 0;

    while (position < reader->lengthInSamples)
    {
        const auto remaining = reader->lengthInSamples - position;
        const int chunk = (int) std::min<juce::int64> (8192, remaining);

        buffer.clear();
        reader->read (&buffer, 0, chunk, position, true, numChannels > 1);
        analyzer.process (buffer);

        position += chunk;

        if (progress != nullptr)
        {
            const float value = (float) ((double) position / (double) reader->lengthInSamples);
            progress->store (std::clamp (value, 0.0f, 1.0f), std::memory_order_relaxed);
        }
    }

    BandProfile profile = analyzer.finishCapture();
    profile.crestDb = analyzer.getPeakDb() - analyzer.getRmsDb();

    if (progress != nullptr)
        progress->store (1.0f, std::memory_order_relaxed);

    return profile;
}

} // namespace vp::ReferenceAnalysis
