#pragma once

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "Params.h"

namespace vp
{

class Snapshot
{
public:
    void write (const BandFrame& newFrame) noexcept
    {
        const uint32_t s = sequence.load (std::memory_order_relaxed);
        sequence.store (s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        frame = newFrame;
        sequence.store (s + 2, std::memory_order_release);
    }

    bool read (BandFrame& out) const noexcept
    {
        const uint32_t s1 = sequence.load (std::memory_order_acquire);

        if ((s1 & 1u) != 0)
            return false;

        out = frame;
        std::atomic_thread_fence (std::memory_order_acquire);
        return s1 == sequence.load (std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> sequence { 0 };
    BandFrame frame {};
};

class SessionBus
{
public:
    void publishBeat (const BandFrame& frame) noexcept
    {
        beat.write (frame);
        hasBeat.store (true, std::memory_order_release);
    }

    void publishVocal (const BandFrame& frame) noexcept
    {
        vocal.write (frame);
        hasVocal.store (true, std::memory_order_release);
    }

    bool readBeat (BandFrame& frame) const noexcept
    {
        return hasBeat.load (std::memory_order_acquire) && beat.read (frame);
    }

    bool readVocal (BandFrame& frame) const noexcept
    {
        return hasVocal.load (std::memory_order_acquire) && vocal.read (frame);
    }

    bool beatAlive() const noexcept { return hasBeat.load (std::memory_order_acquire); }
    bool vocalAlive() const noexcept { return hasVocal.load (std::memory_order_acquire); }

    void reset() noexcept
    {
        hasBeat.store (false, std::memory_order_release);
        hasVocal.store (false, std::memory_order_release);
    }

private:
    Snapshot beat;
    Snapshot vocal;
    std::atomic<bool> hasBeat { false };
    std::atomic<bool> hasVocal { false };
};

std::shared_ptr<SessionBus> getSessionBus (const std::string& id);
void releaseSessionBus (const std::string& id);

} // namespace vp
