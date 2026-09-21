// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_core/juce_core.h>

namespace ljuno
{
enum class SequencerLayer : int
{
    note = 0,
    length,
    velocity,
    repeat,
    shift,
    ccNumber,
    ccValue,
    count
};

struct SequencerStep
{
    int note = 60;
    int length = 50;
    int velocity = 100;
    int repeat = 1;
    float shift = 0.5f;
    int ccNumber = 16;
    int ccValue = 60;
};

struct SequencerConfig
{
    int startStep = 1;
    int endStep = 16;
    int playbackMode = 0;
    float bpmDivision = 4.0f;
    float shuffle = 0.0f;
    float noteSkipProbability = 0.0f;
    float noteLengthRandomDepth = 0.0f;
    bool legato = false;
    float noteRandomDepth = 0.0f;
    float velocityRandomDepth = 0.0f;
    float repeatRandomDepth = 0.0f;
    int octaveShift = 0;
    int semitoneShift = 0;
    float globalStepLength = 0.5f;
    float globalStepVelocity = 0.5f;
    int globalStepRepeat = 0;
    float globalStepShift = 0.5f;
    int midiInputMode = 0;
    int midiInputPolyphony = 0; // 0 = Mono, 1 = Poly
    int midiChannel = 0; // 0 = Omni, 1..16 = fixed
    int launchStep = 1;
    float launchOffsetMs = 0.0f;
};

class SequencerState
{
public:
    static constexpr int maximumSequences = 16;
    static constexpr int stepsPerSequence = 128;

    enum class ConfigParameter
    {
        startStep,
        endStep,
        playbackMode,
        bpmDivision,
        shuffle,
        noteSkipProbability,
        noteLengthRandomDepth,
        legato,
        noteRandomDepth,
        velocityRandomDepth,
        repeatRandomDepth,
        octaveShift,
        semitoneShift,
        globalStepLength,
        globalStepVelocity,
        globalStepRepeat,
        globalStepShift,
        midiInputMode,
        midiInputPolyphony,
        midiChannel,
        launchStep,
        launchOffsetMs
    };

    SequencerState();

    void reset();

    int getRoutingMode() const noexcept;
    void setRoutingMode (int) noexcept;
    int getSelectedSequence() const noexcept;
    void setSelectedSequence (int) noexcept;

    SequencerConfig getConfig (int sequence) const noexcept;
    float getConfigValue (int sequence, ConfigParameter) const noexcept;
    void setConfigValue (int sequence, ConfigParameter, float value) noexcept;

    SequencerStep getStep (int sequence, int step) const noexcept;
    float getStepValue (int sequence, int step, SequencerLayer) const noexcept;
    void setStepValue (int sequence, int step, SequencerLayer, float value) noexcept;
    void addStepDelta (int sequence, int step, SequencerLayer, float delta) noexcept;

    juce::String serialiseToBase64() const;
    bool restoreFromBase64 (const juce::String&);

    std::uint64_t getRevision() const noexcept
    {
        return revision.load (std::memory_order_relaxed);
    }

private:
    struct AtomicStep
    {
        std::atomic<int> note { 60 };
        std::atomic<int> length { 50 };
        std::atomic<int> velocity { 100 };
        std::atomic<int> repeat { 1 };
        std::atomic<float> shift { 0.5f };
        std::atomic<int> ccNumber { 16 };
        std::atomic<int> ccValue { 60 };
    };

    struct AtomicSequence
    {
        std::atomic<int> startStep { 1 };
        std::atomic<int> endStep { 16 };
        std::atomic<int> playbackMode { 0 };
        std::atomic<float> bpmDivision { 4.0f };
        std::atomic<float> shuffle { 0.0f };
        std::atomic<float> noteSkipProbability { 0.0f };
        std::atomic<float> noteLengthRandomDepth { 0.0f };
        std::atomic<int> legato { 0 };
        std::atomic<float> noteRandomDepth { 0.0f };
        std::atomic<float> velocityRandomDepth { 0.0f };
        std::atomic<float> repeatRandomDepth { 0.0f };
        std::atomic<int> octaveShift { 0 };
        std::atomic<int> semitoneShift { 0 };
        std::atomic<float> globalStepLength { 0.5f };
        std::atomic<float> globalStepVelocity { 0.5f };
        std::atomic<int> globalStepRepeat { 0 };
        std::atomic<float> globalStepShift { 0.5f };
        std::atomic<int> midiInputMode { 0 };
        std::atomic<int> midiInputPolyphony { 0 };
        std::atomic<int> midiChannel { 0 };
        std::atomic<int> launchStep { 1 };
        std::atomic<float> launchOffsetMs { 0.0f };
        std::array<AtomicStep, stepsPerSequence> steps;
    };

    static int clampSequence (int) noexcept;
    static int clampStep (int) noexcept;
    static float clampLayerValue (SequencerLayer, float) noexcept;
    void touch() noexcept;
    void applyGlobalDelta (AtomicSequence&, ConfigParameter, float oldValue,
                           float newValue) noexcept;

    std::array<AtomicSequence, maximumSequences> sequences;
    std::atomic<int> routingMode { 0 };
    std::atomic<int> selectedSequence { 0 };
    std::atomic<std::uint64_t> revision { 1 };
};
}
