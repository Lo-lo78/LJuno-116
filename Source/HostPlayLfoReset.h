// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

namespace ljuno
{
// Detects an actual DAW transport start, including a rewind/restart in hosts
// which stop sending processBlock while stopped. No musical clock is imposed:
// Free LFOs keep their original frequency and run freely between starts.
class HostPlayLfoReset
{
public:
    void clear() noexcept
    {
        hasTransportState = false;
        wasPlaying = false;
        lastPlaySample.reset();
        previousBlockSamples = 0;
    }

    bool update (bool playing, std::optional<std::int64_t> samplePosition,
                 int blockSamples) noexcept
    {
        bool restart = playing && (! hasTransportState || ! wasPlaying);

        // Some DAWs suspend plug-in callbacks when stopped. If they resume
        // playback with no intermediate stopped block, detect the new start
        // by the non-continuous project sample position instead.
        if (playing && hasTransportState && wasPlaying && samplePosition
            && lastPlaySample && previousBlockSamples > 0)
        {
            const auto expected = *lastPlaySample + previousBlockSamples;
            if (*samplePosition != expected)
                restart = true;
        }

        hasTransportState = true;
        wasPlaying = playing;
        lastPlaySample = playing ? samplePosition : std::nullopt;
        previousBlockSamples = playing ? blockSamples : 0;
        return restart;
    }

private:
    bool hasTransportState = false;
    bool wasPlaying = false;
    std::optional<std::int64_t> lastPlaySample;
    int previousBlockSamples = 0;
};
}
