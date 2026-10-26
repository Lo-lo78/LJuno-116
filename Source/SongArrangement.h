// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace ljuno
{
// Each row references P1..P16 of exactly one track; songs never duplicate steps.
// Immutable snapshots ensure audio reads cannot race the editor changing vectors.
class SongArrangement
{
public:
    static constexpr int trackCount = 3;
    using Column = std::vector<int>; // 0..15
    using Snapshot = std::shared_ptr<const Column>;
    SongArrangement();
    Snapshot column (int track) const noexcept;
    void replace (int track, Column data);
    void restoreSnapshot (int track, Snapshot snapshot) noexcept;
    void reset();
    bool loop (int track) const noexcept;
    void setLoop (int track, bool enabled) noexcept;
    bool songMode() const noexcept { return mode.load (std::memory_order_acquire); }
    void setSongMode (bool enabled) noexcept;
    std::uint64_t getRevision() const noexcept { return revision.load (std::memory_order_relaxed); }
    std::uint64_t getLoadEpoch() const noexcept { return loadEpoch.load (std::memory_order_relaxed); }
    juce::String serialise() const;
    bool restore (const juce::String&);
private:
    std::array<Snapshot, trackCount> columns;
    std::array<std::atomic<bool>, trackCount> looping {};
    std::atomic<bool> mode { false };
    std::atomic<std::uint64_t> revision { 1 }, loadEpoch { 1 };
};
}
