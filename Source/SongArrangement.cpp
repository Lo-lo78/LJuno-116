// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SongArrangement.h"
namespace ljuno
{
SongArrangement::SongArrangement() { reset(); }
SongArrangement::Snapshot SongArrangement::column (int track) const noexcept
{
    return std::atomic_load_explicit (&columns[static_cast<size_t> (juce::jlimit (0, 2, track))],
                                      std::memory_order_acquire);
}
void SongArrangement::restoreSnapshot (int track, Snapshot snapshot) noexcept
{
    if (! snapshot) snapshot = std::make_shared<const Column>();
    std::atomic_store_explicit (&columns[static_cast<size_t> (juce::jlimit (0, 2, track))],
                                std::move (snapshot), std::memory_order_release);
    revision.fetch_add (1, std::memory_order_relaxed);
}
void SongArrangement::replace (int track, Column data)
{
    for (auto& pattern : data) pattern = juce::jlimit (0, 15, pattern);
    restoreSnapshot (track, std::make_shared<const Column> (std::move (data)));
}
void SongArrangement::setSongMode (bool enabled) noexcept
{
    mode.store (enabled, std::memory_order_release);
    revision.fetch_add (1, std::memory_order_relaxed);
}
void SongArrangement::reset()
{
    for (int i = 0; i < trackCount; ++i)
    {
        replace (i, {});
        looping[static_cast<size_t> (i)].store (false, std::memory_order_release);
    }
    setSongMode (false);
    loadEpoch.fetch_add (1, std::memory_order_relaxed);
}
bool SongArrangement::loop (int track) const noexcept
{
    return looping[static_cast<size_t> (juce::jlimit (0, 2, track))].load (std::memory_order_acquire);
}
void SongArrangement::setLoop (int track, bool enabled) noexcept
{
    looping[static_cast<size_t> (juce::jlimit (0, 2, track))].store (enabled, std::memory_order_release);
    revision.fetch_add (1, std::memory_order_relaxed);
}
juce::String SongArrangement::serialise() const
{
    juce::MemoryOutputStream out;
    out.writeInt (0x4c4a5331); // LJS1
    out.writeInt (1);
    out.writeByte (songMode() ? 1 : 0);
    for (int i = 0; i < trackCount; ++i)
    {
        out.writeByte (loop (i) ? 1 : 0);
        const auto data = column (i);
        out.writeInt (static_cast<int> (data->size()));
        for (int p : *data) out.writeByte (static_cast<char> (p));
    }
    return juce::Base64::toBase64 (out.getData(), out.getDataSize());
}
bool SongArrangement::restore (const juce::String& encoded)
{
    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, encoded)) return false;
    juce::MemoryInputStream stream (decoded.getData(), decoded.getDataSize(), false);
    if (stream.getNumBytesRemaining() < 9 || stream.readInt() != 0x4c4a5331
        || stream.readInt() != 1) return false;
    const bool newMode = stream.readByte() != 0;
    std::array<Column, trackCount> staged;
    std::array<bool, trackCount> newLoops {};
    for (int lane = 0; lane < trackCount; ++lane)
    {
        if (stream.getNumBytesRemaining() < 5) return false;
        newLoops[static_cast<size_t> (lane)] = stream.readByte() != 0;
        const int count = stream.readInt();
        if (count < 0 || static_cast<juce::int64> (count) > stream.getNumBytesRemaining()) return false;
        staged[static_cast<size_t> (lane)].reserve (static_cast<size_t> (count));
        for (int n = 0; n < count; ++n)
        {
            const int p = stream.readByte();
            if (p < 0 || p >= 16) return false;
            staged[static_cast<size_t> (lane)].push_back (p);
        }
    }
    if (stream.getNumBytesRemaining() != 0) return false;
    for (int lane = 0; lane < trackCount; ++lane)
    {
        replace (lane, std::move (staged[static_cast<size_t> (lane)]));
        setLoop (lane, newLoops[static_cast<size_t> (lane)]);
    }
    setSongMode (newMode);
    loadEpoch.fetch_add (1, std::memory_order_relaxed);
    return true;
}
}
