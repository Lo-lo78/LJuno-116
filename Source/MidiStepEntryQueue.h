// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cstdint>

// Single audio-thread producer, single message-thread consumer. No allocation,
// locking, pattern edits or screen reader calls on the real-time thread.
class MidiStepEntryQueue final
{
public:
    struct Note { int pitch = 60, velocity = 100; };
    void begin() noexcept
    {
        // A previous session is always stopped before this is called.
        reading.store (writing.load (std::memory_order_acquire), std::memory_order_release);
        enabled.store (true, std::memory_order_release);
    }
    void finish() noexcept { enabled.store (false, std::memory_order_release); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_acquire); }
    // Audio-thread-only query: after an automatic stop, swallow outstanding
    // Note Offs from captured keys before returning to ordinary MIDI routing.
    bool needsFiltering() const noexcept { return isEnabled() || heldCount > 0; }

    // True means the input MIDI message was consumed. Notes recorded here
    // never reach the synth, trigger/transpose logic or host MIDI output.
    bool consume (const juce::MidiMessage& message) noexcept
    {
        if (! message.isNoteOnOrOff()) return false;
        const int channel = message.getChannel() - 1;
        const int pitch = message.getNoteNumber();
        if (channel < 0 || channel >= 16 || pitch < 0 || pitch >= 128)
            return false;
        auto& held = heldNotes[static_cast<std::size_t> (channel)]
                              [static_cast<std::size_t> (pitch)];
        if (message.isNoteOff())
        {
            if (held)
            {
                held = false;
                if (heldCount > 0) --heldCount;
                return true; // Also swallow an off arriving after auto-stop.
            }
            return isEnabled();
        }
        if (! isEnabled()) return false;
        if (! held)
        {
            const bool firstInChord = heldCount == 0;
            held = true;
            ++heldCount;
            if (firstInChord)
            {
                const unsigned write = writing.load (std::memory_order_relaxed);
                const unsigned read = reading.load (std::memory_order_acquire);
                if (write - read < capacity)
                {
                    notes[write % capacity] = Note { pitch, static_cast<int> (message.getVelocity()) };
                    writing.store (write + 1, std::memory_order_release);
                }
            }
        }
        return true;
    }
    bool pop (Note& destination) noexcept
    {
        const unsigned read = reading.load (std::memory_order_relaxed);
        if (read == writing.load (std::memory_order_acquire)) return false;
        destination = notes[read % capacity];
        reading.store (read + 1, std::memory_order_release);
        return true;
    }
private:
    static constexpr unsigned capacity = 256;
    std::array<Note, capacity> notes {};
    std::atomic<unsigned> writing { 0 }, reading { 0 };
    std::atomic<bool> enabled { false };
    // These are accessed only on the audio thread.
    std::array<std::array<bool, 128>, 16> heldNotes {};
    int heldCount = 0;
};
