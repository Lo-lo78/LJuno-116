// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SequencerState.h"

#include <algorithm>
#include <cmath>

namespace ljuno
{
namespace
{
template <typename T>
T loadRelaxed (const std::atomic<T>& value) noexcept
{
    return value.load (std::memory_order_relaxed);
}

template <typename T>
void storeRelaxed (std::atomic<T>& target, T value) noexcept
{
    target.store (value, std::memory_order_relaxed);
}
}

SequencerState::SequencerState()
{
    reset();
}

int SequencerState::clampSequence (int value) noexcept
{
    return juce::jlimit (0, maximumSequences - 1, value);
}

int SequencerState::clampStep (int value) noexcept
{
    return juce::jlimit (0, stepsPerSequence - 1, value);
}

float SequencerState::clampLayerValue (SequencerLayer layer, float value) noexcept
{
    switch (layer)
    {
        case SequencerLayer::note:     return juce::jlimit (0.0f, 128.0f, value);
        case SequencerLayer::length:   return juce::jlimit (0.0f, 100.0f, value);
        case SequencerLayer::velocity: return juce::jlimit (0.0f, 127.0f, value);
        case SequencerLayer::repeat:   return juce::jlimit (1.0f, 16.0f, value);
        case SequencerLayer::shift:    return juce::jlimit (0.0f, 1.0f, value);
        case SequencerLayer::ccNumber:
        case SequencerLayer::ccValue:  return juce::jlimit (0.0f, 127.0f, value);
        default:                       return value;
    }
}

void SequencerState::touch() noexcept
{
    revision.fetch_add (1, std::memory_order_relaxed);
}

void SequencerState::reset()
{
    routingMode.store (0, std::memory_order_relaxed);
    selectedSequence.store (0, std::memory_order_relaxed);

    for (auto& sequence : sequences)
    {
        storeRelaxed (sequence.startStep, 1);
        storeRelaxed (sequence.endStep, 16);
        storeRelaxed (sequence.playbackMode, 0);
        storeRelaxed (sequence.bpmDivision, 4.0f);
        storeRelaxed (sequence.shuffle, 0.0f);
        storeRelaxed (sequence.noteSkipProbability, 0.0f);
        storeRelaxed (sequence.noteLengthRandomDepth, 0.0f);
        storeRelaxed (sequence.legato, 0);
        storeRelaxed (sequence.noteRandomDepth, 0.0f);
        storeRelaxed (sequence.velocityRandomDepth, 0.0f);
        storeRelaxed (sequence.repeatRandomDepth, 0.0f);
        storeRelaxed (sequence.octaveShift, 0);
        storeRelaxed (sequence.semitoneShift, 0);
        storeRelaxed (sequence.globalStepLength, 0.5f);
        storeRelaxed (sequence.globalStepVelocity, 0.5f);
        storeRelaxed (sequence.globalStepRepeat, 0);
        storeRelaxed (sequence.globalStepShift, 0.5f);
        storeRelaxed (sequence.midiInputMode, 0);
        storeRelaxed (sequence.midiInputPolyphony, 0);
        storeRelaxed (sequence.midiChannel, 1);
        storeRelaxed (sequence.launchStep, 1);
        storeRelaxed (sequence.launchOffsetMs, 0.0f);

        for (auto& step : sequence.steps)
        {
            storeRelaxed (step.note, 60);
            storeRelaxed (step.length, 50);
            storeRelaxed (step.velocity, 100);
            storeRelaxed (step.repeat, 1);
            storeRelaxed (step.shift, 0.5f);
            storeRelaxed (step.ccNumber, 16);
            storeRelaxed (step.ccValue, 60);
        }
    }
    touch();
}

int SequencerState::getRoutingMode() const noexcept
{
    return juce::jlimit (0, 3, routingMode.load (std::memory_order_relaxed));
}

void SequencerState::setRoutingMode (int value) noexcept
{
    routingMode.store (juce::jlimit (0, 3, value), std::memory_order_relaxed);
    touch();
}

int SequencerState::getSelectedSequence() const noexcept
{
    return clampSequence (selectedSequence.load (std::memory_order_relaxed));
}

void SequencerState::setSelectedSequence (int value) noexcept
{
    selectedSequence.store (clampSequence (value), std::memory_order_relaxed);
    touch();
}

SequencerConfig SequencerState::getConfig (int sequenceIndex) const noexcept
{
    const auto clampedSequence = clampSequence (sequenceIndex);
    const auto& sequence = sequences[static_cast<std::size_t> (clampedSequence)];
    SequencerConfig config;
    config.startStep = loadRelaxed (sequence.startStep);
    config.endStep = loadRelaxed (sequence.endStep);
    config.playbackMode = loadRelaxed (sequence.playbackMode);
    config.bpmDivision = loadRelaxed (sequence.bpmDivision);
    config.shuffle = loadRelaxed (sequence.shuffle);
    config.noteSkipProbability = loadRelaxed (sequence.noteSkipProbability);
    config.noteLengthRandomDepth = loadRelaxed (sequence.noteLengthRandomDepth);
    config.legato = loadRelaxed (sequence.legato) != 0;
    config.noteRandomDepth = loadRelaxed (sequence.noteRandomDepth);
    config.velocityRandomDepth = loadRelaxed (sequence.velocityRandomDepth);
    config.repeatRandomDepth = loadRelaxed (sequence.repeatRandomDepth);
    config.octaveShift = loadRelaxed (sequence.octaveShift);
    config.semitoneShift = loadRelaxed (sequence.semitoneShift);
    config.globalStepLength = loadRelaxed (sequence.globalStepLength);
    config.globalStepVelocity = loadRelaxed (sequence.globalStepVelocity);
    config.globalStepRepeat = loadRelaxed (sequence.globalStepRepeat);
    config.globalStepShift = loadRelaxed (sequence.globalStepShift);
    config.midiInputMode = loadRelaxed (sequence.midiInputMode);
    // Sequence 3 drives the single Noise generator and is intentionally mono.
    config.midiInputPolyphony = clampedSequence < 2
        ? loadRelaxed (sequence.midiInputPolyphony) : 0;
    config.midiChannel = loadRelaxed (sequence.midiChannel);
    config.launchStep = loadRelaxed (sequence.launchStep);
    config.launchOffsetMs = loadRelaxed (sequence.launchOffsetMs);
    return config;
}

float SequencerState::getConfigValue (int sequenceIndex, ConfigParameter parameter) const noexcept
{
    const auto config = getConfig (sequenceIndex);
    switch (parameter)
    {
        case ConfigParameter::startStep:             return static_cast<float> (config.startStep);
        case ConfigParameter::endStep:               return static_cast<float> (config.endStep);
        case ConfigParameter::playbackMode:          return static_cast<float> (config.playbackMode);
        case ConfigParameter::bpmDivision:           return config.bpmDivision;
        case ConfigParameter::shuffle:               return config.shuffle;
        case ConfigParameter::noteSkipProbability:   return config.noteSkipProbability;
        case ConfigParameter::noteLengthRandomDepth: return config.noteLengthRandomDepth;
        case ConfigParameter::legato:                return config.legato ? 1.0f : 0.0f;
        case ConfigParameter::noteRandomDepth:       return config.noteRandomDepth;
        case ConfigParameter::velocityRandomDepth:   return config.velocityRandomDepth;
        case ConfigParameter::repeatRandomDepth:     return config.repeatRandomDepth;
        case ConfigParameter::octaveShift:           return static_cast<float> (config.octaveShift);
        case ConfigParameter::semitoneShift:         return static_cast<float> (config.semitoneShift);
        case ConfigParameter::globalStepLength:      return config.globalStepLength;
        case ConfigParameter::globalStepVelocity:    return config.globalStepVelocity;
        case ConfigParameter::globalStepRepeat:      return static_cast<float> (config.globalStepRepeat);
        case ConfigParameter::globalStepShift:       return config.globalStepShift;
        case ConfigParameter::midiInputMode:         return static_cast<float> (config.midiInputMode);
        case ConfigParameter::midiInputPolyphony:    return static_cast<float> (config.midiInputPolyphony);
        case ConfigParameter::midiChannel:           return static_cast<float> (config.midiChannel);
        case ConfigParameter::launchStep:            return static_cast<float> (config.launchStep);
        case ConfigParameter::launchOffsetMs:        return config.launchOffsetMs;
    }
    return 0.0f;
}

void SequencerState::applyGlobalDelta (AtomicSequence& sequence, ConfigParameter parameter,
                                       float oldValue, float newValue) noexcept
{
    if (std::abs (newValue - oldValue) < 0.0000001f)
        return;

    if (parameter == ConfigParameter::globalStepLength)
    {
        const auto delta = (newValue - oldValue) * 100.0f;
        for (auto& step : sequence.steps)
            step.length.store (juce::roundToInt (clampLayerValue (
                SequencerLayer::length, static_cast<float> (loadRelaxed (step.length)) + delta)),
                std::memory_order_relaxed);
    }
    else if (parameter == ConfigParameter::globalStepVelocity)
    {
        const auto delta = (newValue - oldValue) * 127.0f;
        for (auto& step : sequence.steps)
            step.velocity.store (juce::roundToInt (clampLayerValue (
                SequencerLayer::velocity, static_cast<float> (loadRelaxed (step.velocity)) + delta)),
                std::memory_order_relaxed);
    }
    else if (parameter == ConfigParameter::globalStepRepeat)
    {
        const auto delta = juce::roundToInt (newValue - oldValue);
        for (auto& step : sequence.steps)
            step.repeat.store (juce::roundToInt (clampLayerValue (
                SequencerLayer::repeat, static_cast<float> (loadRelaxed (step.repeat) + delta))),
                std::memory_order_relaxed);
    }
    else if (parameter == ConfigParameter::globalStepShift)
    {
        const auto delta = newValue - oldValue;
        for (auto& step : sequence.steps)
            step.shift.store (clampLayerValue (
                SequencerLayer::shift, loadRelaxed (step.shift) + delta),
                std::memory_order_relaxed);
    }
}

void SequencerState::setConfigValue (int sequenceIndex, ConfigParameter parameter,
                                     float requestedValue) noexcept
{
    const auto clampedSequence = clampSequence (sequenceIndex);
    auto& sequence = sequences[static_cast<std::size_t> (clampedSequence)];
    const auto oldValue = getConfigValue (sequenceIndex, parameter);
    auto value = requestedValue;

    switch (parameter)
    {
        case ConfigParameter::startStep:
        {
            const auto start = juce::jlimit (1, stepsPerSequence, juce::roundToInt (value));
            sequence.startStep.store (start, std::memory_order_relaxed);
            if (loadRelaxed (sequence.endStep) < start)
                sequence.endStep.store (start, std::memory_order_relaxed);
            break;
        }
        case ConfigParameter::endStep:
        {
            const auto end = juce::jlimit (1, stepsPerSequence, juce::roundToInt (value));
            sequence.endStep.store (end, std::memory_order_relaxed);
            if (loadRelaxed (sequence.startStep) > end)
                sequence.startStep.store (end, std::memory_order_relaxed);
            break;
        }
        case ConfigParameter::playbackMode:
            sequence.playbackMode.store (juce::jlimit (0, 4, juce::roundToInt (value)), std::memory_order_relaxed);
            break;
        case ConfigParameter::bpmDivision:
            sequence.bpmDivision.store (juce::jlimit (0.03125f, 64.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::shuffle:
            sequence.shuffle.store (juce::jlimit (0.0f, 1.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::noteSkipProbability:
            sequence.noteSkipProbability.store (juce::jlimit (0.0f, 1.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::noteLengthRandomDepth:
            sequence.noteLengthRandomDepth.store (juce::jlimit (0.0f, 1.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::legato:
            sequence.legato.store (value >= 0.5f ? 1 : 0, std::memory_order_relaxed);
            break;
        case ConfigParameter::noteRandomDepth:
            sequence.noteRandomDepth.store (juce::jlimit (0.0f, 4.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::velocityRandomDepth:
            sequence.velocityRandomDepth.store (juce::jlimit (0.0f, 4.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::repeatRandomDepth:
            sequence.repeatRandomDepth.store (juce::jlimit (0.0f, 1.0f, value), std::memory_order_relaxed);
            break;
        case ConfigParameter::octaveShift:
            sequence.octaveShift.store (juce::jlimit (-4, 4, juce::roundToInt (value)), std::memory_order_relaxed);
            break;
        case ConfigParameter::semitoneShift:
            sequence.semitoneShift.store (juce::jlimit (-12, 12, juce::roundToInt (value)), std::memory_order_relaxed);
            break;
        case ConfigParameter::globalStepLength:
            value = juce::jlimit (0.0f, 1.0f, value);
            applyGlobalDelta (sequence, parameter, oldValue, value);
            sequence.globalStepLength.store (value, std::memory_order_relaxed);
            break;
        case ConfigParameter::globalStepVelocity:
            value = juce::jlimit (0.0f, 1.0f, value);
            applyGlobalDelta (sequence, parameter, oldValue, value);
            sequence.globalStepVelocity.store (value, std::memory_order_relaxed);
            break;
        case ConfigParameter::globalStepRepeat:
            value = static_cast<float> (juce::jlimit (0, 16, juce::roundToInt (value)));
            applyGlobalDelta (sequence, parameter, oldValue, value);
            sequence.globalStepRepeat.store (juce::roundToInt (value), std::memory_order_relaxed);
            break;
        case ConfigParameter::globalStepShift:
            value = juce::jlimit (0.0f, 1.0f, value);
            applyGlobalDelta (sequence, parameter, oldValue, value);
            sequence.globalStepShift.store (value, std::memory_order_relaxed);
            break;
        case ConfigParameter::midiInputMode:
            sequence.midiInputMode.store (juce::jlimit (0, 3, juce::roundToInt (value)), std::memory_order_relaxed);
            break;
        case ConfigParameter::midiInputPolyphony:
            sequence.midiInputPolyphony.store (clampedSequence < 2
                ? juce::jlimit (0, 1, juce::roundToInt (value)) : 0,
                std::memory_order_relaxed);
            break;
        case ConfigParameter::midiChannel:
            sequence.midiChannel.store (juce::jlimit (1, 16, juce::roundToInt (value)), std::memory_order_relaxed);
            break;
        case ConfigParameter::launchStep:
        {
            const auto launch = juce::jlimit (1, stepsPerSequence, juce::roundToInt (value));
            sequence.launchStep.store (launch, std::memory_order_relaxed);
            if (launch == 1 && loadRelaxed (sequence.launchOffsetMs) < 0.0f)
                sequence.launchOffsetMs.store (0.0f, std::memory_order_relaxed);
            break;
        }
        case ConfigParameter::launchOffsetMs:
            if (loadRelaxed (sequence.launchStep) <= 1)
                value = std::max (0.0f, value);
            sequence.launchOffsetMs.store (juce::jlimit (-99.0f, 99.0f, value), std::memory_order_relaxed);
            break;
    }
    touch();
}

SequencerStep SequencerState::getStep (int sequenceIndex, int stepIndex) const noexcept
{
    const auto& step = sequences[static_cast<std::size_t> (clampSequence (sequenceIndex))]
                           .steps[static_cast<std::size_t> (clampStep (stepIndex))];
    SequencerStep result;
    result.note = loadRelaxed (step.note);
    result.length = loadRelaxed (step.length);
    result.velocity = loadRelaxed (step.velocity);
    result.repeat = loadRelaxed (step.repeat);
    result.shift = loadRelaxed (step.shift);
    result.ccNumber = loadRelaxed (step.ccNumber);
    result.ccValue = loadRelaxed (step.ccValue);
    return result;
}

float SequencerState::getStepValue (int sequenceIndex, int stepIndex,
                                    SequencerLayer layer) const noexcept
{
    const auto step = getStep (sequenceIndex, stepIndex);
    switch (layer)
    {
        case SequencerLayer::note:     return static_cast<float> (step.note);
        case SequencerLayer::length:   return static_cast<float> (step.length);
        case SequencerLayer::velocity: return static_cast<float> (step.velocity);
        case SequencerLayer::repeat:   return static_cast<float> (step.repeat);
        case SequencerLayer::shift:    return step.shift;
        case SequencerLayer::ccNumber: return static_cast<float> (step.ccNumber);
        case SequencerLayer::ccValue:  return static_cast<float> (step.ccValue);
        default:                       return 0.0f;
    }
}

void SequencerState::setStepValue (int sequenceIndex, int stepIndex, SequencerLayer layer,
                                   float requestedValue) noexcept
{
    auto& step = sequences[static_cast<std::size_t> (clampSequence (sequenceIndex))]
                     .steps[static_cast<std::size_t> (clampStep (stepIndex))];
    const auto value = clampLayerValue (layer, requestedValue);
    switch (layer)
    {
        case SequencerLayer::note:     step.note.store (juce::roundToInt (value), std::memory_order_relaxed); break;
        case SequencerLayer::length:   step.length.store (juce::roundToInt (value), std::memory_order_relaxed); break;
        case SequencerLayer::velocity: step.velocity.store (juce::roundToInt (value), std::memory_order_relaxed); break;
        case SequencerLayer::repeat:   step.repeat.store (juce::roundToInt (value), std::memory_order_relaxed); break;
        case SequencerLayer::shift:    step.shift.store (value, std::memory_order_relaxed); break;
        case SequencerLayer::ccNumber: step.ccNumber.store (juce::roundToInt (value), std::memory_order_relaxed); break;
        case SequencerLayer::ccValue:  step.ccValue.store (juce::roundToInt (value), std::memory_order_relaxed); break;
        default: break;
    }
    touch();
}

void SequencerState::addStepDelta (int sequenceIndex, int stepIndex, SequencerLayer layer,
                                   float delta) noexcept
{
    setStepValue (sequenceIndex, stepIndex, layer,
                  getStepValue (sequenceIndex, stepIndex, layer) + delta);
}

juce::String SequencerState::serialiseToBase64() const
{
    juce::MemoryOutputStream stream;
    stream.writeInt (0x4c535132); // LSQ2
    stream.writeInt (2);
    stream.writeInt (getRoutingMode());
    stream.writeInt (getSelectedSequence());

    for (int sequenceIndex = 0; sequenceIndex < maximumSequences; ++sequenceIndex)
    {
        const auto config = getConfig (sequenceIndex);
        stream.writeInt (config.startStep);
        stream.writeInt (config.endStep);
        stream.writeInt (config.playbackMode);
        stream.writeFloat (config.bpmDivision);
        stream.writeFloat (config.shuffle);
        stream.writeFloat (config.noteSkipProbability);
        stream.writeFloat (config.noteLengthRandomDepth);
        stream.writeBool (config.legato);
        stream.writeFloat (config.noteRandomDepth);
        stream.writeFloat (config.velocityRandomDepth);
        stream.writeFloat (config.repeatRandomDepth);
        stream.writeInt (config.octaveShift);
        stream.writeInt (config.semitoneShift);
        stream.writeFloat (config.globalStepLength);
        stream.writeFloat (config.globalStepVelocity);
        stream.writeInt (config.globalStepRepeat);
        stream.writeFloat (config.globalStepShift);
        stream.writeInt (config.midiInputMode);
        stream.writeInt (config.midiInputPolyphony);
        stream.writeInt (config.midiChannel);
        stream.writeInt (config.launchStep);
        stream.writeFloat (config.launchOffsetMs);

        for (int stepIndex = 0; stepIndex < stepsPerSequence; ++stepIndex)
        {
            const auto step = getStep (sequenceIndex, stepIndex);
            stream.writeByte (static_cast<char> (juce::jlimit (0, 255, step.note)));
            stream.writeByte (static_cast<char> (juce::jlimit (0, 255, step.length)));
            stream.writeByte (static_cast<char> (juce::jlimit (0, 255, step.velocity)));
            stream.writeByte (static_cast<char> (juce::jlimit (0, 255, step.repeat)));
            stream.writeFloat (step.shift);
            stream.writeByte (static_cast<char> (juce::jlimit (0, 255, step.ccNumber)));
            stream.writeByte (static_cast<char> (juce::jlimit (0, 255, step.ccValue)));
        }
    }

    return juce::Base64::toBase64 (stream.getData(), stream.getDataSize());
}

bool SequencerState::restoreFromBase64 (const juce::String& encoded)
{
    if (encoded.trim().isEmpty())
    {
        reset();
        return true;
    }

    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, encoded.trim()))
        return false;

    juce::MemoryInputStream stream (decoded.getData(), decoded.getDataSize(), false);
    if (stream.readInt() != 0x4c535132)
        return false;
    const auto formatVersion = stream.readInt();
    if (formatVersion != 1 && formatVersion != 2)
        return false;

    // Silence the generator while its atomic fields are replaced.
    routingMode.store (0, std::memory_order_relaxed);
    const auto restoredMode = juce::jlimit (0, 3, stream.readInt());
    const auto restoredSelected = clampSequence (stream.readInt());

    for (int sequenceIndex = 0; sequenceIndex < maximumSequences; ++sequenceIndex)
    {
        auto& sequence = sequences[static_cast<std::size_t> (sequenceIndex)];
        sequence.startStep.store (juce::jlimit (1, stepsPerSequence, stream.readInt()), std::memory_order_relaxed);
        sequence.endStep.store (juce::jlimit (1, stepsPerSequence, stream.readInt()), std::memory_order_relaxed);
        sequence.playbackMode.store (juce::jlimit (0, 4, stream.readInt()), std::memory_order_relaxed);
        sequence.bpmDivision.store (juce::jlimit (0.03125f, 64.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.shuffle.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.noteSkipProbability.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.noteLengthRandomDepth.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.legato.store (stream.readBool() ? 1 : 0, std::memory_order_relaxed);
        sequence.noteRandomDepth.store (juce::jlimit (0.0f, 4.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.velocityRandomDepth.store (juce::jlimit (0.0f, 4.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.repeatRandomDepth.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.octaveShift.store (juce::jlimit (-4, 4, stream.readInt()), std::memory_order_relaxed);
        sequence.semitoneShift.store (juce::jlimit (-12, 12, stream.readInt()), std::memory_order_relaxed);
        sequence.globalStepLength.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.globalStepVelocity.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.globalStepRepeat.store (juce::jlimit (0, 16, stream.readInt()), std::memory_order_relaxed);
        sequence.globalStepShift.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
        sequence.midiInputMode.store (juce::jlimit (0, 3, stream.readInt()), std::memory_order_relaxed);
        sequence.midiInputPolyphony.store (formatVersion >= 2
            ? juce::jlimit (0, 1, stream.readInt()) : 0, std::memory_order_relaxed);
        sequence.midiChannel.store (juce::jlimit (1, 16, stream.readInt()), std::memory_order_relaxed);
        sequence.launchStep.store (juce::jlimit (1, stepsPerSequence, stream.readInt()), std::memory_order_relaxed);
        auto launchOffset = juce::jlimit (-99.0f, 99.0f, stream.readFloat());
        if (loadRelaxed (sequence.launchStep) <= 1)
            launchOffset = std::max (0.0f, launchOffset);
        sequence.launchOffsetMs.store (launchOffset, std::memory_order_relaxed);

        if (loadRelaxed (sequence.startStep) > loadRelaxed (sequence.endStep))
            sequence.endStep.store (loadRelaxed (sequence.startStep), std::memory_order_relaxed);

        for (int stepIndex = 0; stepIndex < stepsPerSequence; ++stepIndex)
        {
            auto& step = sequence.steps[static_cast<std::size_t> (stepIndex)];
            step.note.store (juce::jlimit (0, 128, static_cast<int> (static_cast<unsigned char> (stream.readByte()))), std::memory_order_relaxed);
            step.length.store (juce::jlimit (0, 100, static_cast<int> (static_cast<unsigned char> (stream.readByte()))), std::memory_order_relaxed);
            step.velocity.store (juce::jlimit (0, 127, static_cast<int> (static_cast<unsigned char> (stream.readByte()))), std::memory_order_relaxed);
            step.repeat.store (juce::jlimit (1, 16, static_cast<int> (static_cast<unsigned char> (stream.readByte()))), std::memory_order_relaxed);
            step.shift.store (juce::jlimit (0.0f, 1.0f, stream.readFloat()), std::memory_order_relaxed);
            step.ccNumber.store (juce::jlimit (0, 127, static_cast<int> (static_cast<unsigned char> (stream.readByte()))), std::memory_order_relaxed);
            step.ccValue.store (juce::jlimit (0, 127, static_cast<int> (static_cast<unsigned char> (stream.readByte()))), std::memory_order_relaxed);
        }
    }

    selectedSequence.store (restoredSelected, std::memory_order_relaxed);
    routingMode.store (restoredMode, std::memory_order_relaxed);
    touch();
    return true;
}
}
