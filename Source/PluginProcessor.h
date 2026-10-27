#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/MixEngine.h"

class VocalPocketProcessor : public juce::AudioProcessor
{
public:
    VocalPocketProcessor();
    ~VocalPocketProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VocalPocket"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Editor API
    void loadReferenceFile (const juce::File& file);
    void startLearn();
    void completeLearnIfReady();

    bool isReferenceLoading() const noexcept { return referenceLoading.load (std::memory_order_acquire); }
    float getReferenceProgress() const noexcept { return referenceProgress.load (std::memory_order_relaxed); }
    juce::String getReferenceName() const;
    bool hasReference() const noexcept { return engine.hasReference(); }
    bool hasVocalProfile() const noexcept { return engine.hasVocalProfile(); }
    bool isLearning() const noexcept { return engine.isCapturing(); }
    bool isLearnComplete() const noexcept { return engine.isCaptureComplete(); }
    float getLearnProgress() const noexcept { return engine.getCaptureProgress(); }
    bool isOppositeLinked() const noexcept { return engine.isOppositeLinked(); }

    float getInputLufs() const noexcept { return engine.getInputFrame().lufs; }
    float getOutputLufs() const noexcept { return engine.getOutputFrame().lufs; }
    float getGainReductionDb() const noexcept { return engine.getGainReductionDb(); }
    float getDeEssDb() const noexcept { return engine.getDeEssDb(); }
    float getAutoGainDb() const noexcept { return engine.getAutoGainDb(); }
    void getEqCurve (float* out) const noexcept { engine.getEqCurve (out); }
    juce::String getSessionId() const { return sessionId; }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void cacheParameterPointers();
    void setSession (const juce::String& id);

    vp::MixEngine engine;

    std::atomic<vp::SessionBus*> busPtr { nullptr };
    std::shared_ptr<vp::SessionBus> busHolder;
    std::vector<std::shared_ptr<vp::SessionBus>> retiredBuses;
    juce::String sessionId { "default" };

    std::thread referenceThread;
    std::atomic<bool> referenceLoading { false };
    std::atomic<float> referenceProgress { 0.0f };
    mutable juce::CriticalSection referenceNameLock;
    juce::String referenceName;

    std::atomic<float>* roleParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* genreParam = nullptr;
    std::atomic<float>* fitParam = nullptr;
    std::atomic<float>* refParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* compParam = nullptr;
    std::atomic<float>* deEssParam = nullptr;
    std::atomic<float>* unmaskParam = nullptr;
    std::atomic<float>* satParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* autoGainParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalPocketProcessor)
};
