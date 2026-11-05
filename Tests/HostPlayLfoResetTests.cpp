// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/HostPlayLfoReset.h"
#include "../Source/FreeLfoRandom.h"
#include <cassert>
#include <cstdint>
#include <optional>

int main()
{
    using ljuno::HostPlayLfoReset;
    HostPlayLfoReset transport;
    // Stopped to Play: exactly one new start, not once per audio buffer.
    assert (! transport.update (false, 0, 256));
    assert (transport.update (true, 0, 256));
    assert (! transport.update (true, 256, 256));
    assert (! transport.update (true, 512, 128));
    assert (! transport.update (true, 640, 512));
    assert (! transport.update (false, 1152, 256));
    assert (transport.update (true, 0, 256));
    assert (! transport.update (true, 256, 256));

    // Hosts which stop calling processBlock while stopped: detect a new Play
    // from the project sample position instead of requiring a stopped buffer.
    assert (transport.update (true, 0, 256));
    assert (! transport.update (true, 256, 256));
    assert (transport.update (true, 10000, 256));  // restart at a later bar
    assert (! transport.update (true, 10256, 256));

    // A host without a project sample position still provides Play/Stop edges.
    transport.clear();
    assert (transport.update (true, std::nullopt, 256));
    assert (! transport.update (true, std::nullopt, 256));
    assert (! transport.update (false, std::nullopt, 256));
    assert (transport.update (true, std::nullopt, 256));

    // Free S&H must produce identical runs for each host Play even when
    // other synth parts consume unrelated random values between runs.
    std::uint32_t free1 = 0x116f0101u;
    std::uint32_t free2 = 0x116f0202u;
    const auto a1 = ljuno::nextFreeLfoSampleHold (free1);
    const auto a2 = ljuno::nextFreeLfoSampleHold (free1);
    const auto b1 = ljuno::nextFreeLfoSampleHold (free2);
    assert (a1 != b1);
    free1 = 0x116f0101u;
    assert (ljuno::nextFreeLfoSampleHold (free1) == a1);
    assert (ljuno::nextFreeLfoSampleHold (free1) == a2);

    // Poly Free S&H streams vary by note but are stable for each note.
    auto voiceA = ljuno::freeLfoVoiceSeed (0x116f0101u, 60);
    auto voiceB = ljuno::freeLfoVoiceSeed (0x116f0101u, 64);
    assert (voiceA != voiceB);
    assert (voiceA == ljuno::freeLfoVoiceSeed (0x116f0101u, 60));
    const auto voiceValue = ljuno::nextFreeLfoSampleHold (voiceA);
    voiceA = ljuno::freeLfoVoiceSeed (0x116f0101u, 60);
    assert (voiceValue == ljuno::nextFreeLfoSampleHold (voiceA));

    transport.clear();
    assert (transport.update (true, 1024, 128));
    assert (! transport.update (true, 1152, 128));
    return 0;
}
