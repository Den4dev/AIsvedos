#pragma once

#include <atomic>

#include <juce_core/juce_core.h>

#include "Params.h"

namespace vp::ReferenceAnalysis
{

BandProfile analyzeFile (const juce::File& file, std::atomic<float>* progress = nullptr);

} // namespace vp::ReferenceAnalysis
