// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GeneratedParameters.h"

#include <algorithm>
#include <cmath>

LJuno116AudioProcessor::LJuno116AudioProcessor (juce::File presetLibraryRoot)
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput ("Master 1-2", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 3-4", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 5-6", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 7-8", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 9-10", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "LJuno116State", ljuno::createParameterLayout()),
      presetManager (parameters, std::move (presetLibraryRoot),
                     [this] { return sequencerState.serialiseToBase64(); },
                     [this] (const juce::String& data) { restoreSequencerData (data); })
{
    for (const auto& parameter : ljuno::generated::parameters)
        parameters.addParameterListener (parameter.id, this);
    syncSequencerBankToParameters();
}

LJuno116AudioProcessor::~LJuno116AudioProcessor()
{
    for (const auto& parameter : ljuno::generated::parameters)
        parameters.removeParameterListener (parameter.id, this);
}

void LJuno116AudioProcessor::parameterChanged (const juce::String& id, float newValue)
{
    parameterRevision.fetch_add (1, std::memory_order_relaxed);

    if (synchronisingSequencerBank.load (std::memory_order_relaxed))
        return;

    const auto sliderNumber = id.startsWith ("slider") ? id.substring (6).getIntValue() : -1;
    if ((sliderNumber >= 313 && sliderNumber <= 335) || sliderNumber == 364)
    {
        handleSequencerParameterChanged (id, newValue);
        return;
    }

    if (synchronisingMorph.exchange (true))
        return;

    const auto setPlainValue = [this] (const char* targetId, float plainValue)
    {
        if (auto* target = parameters.getParameter (targetId))
            target->setValueNotifyingHost (target->convertTo0to1 (plainValue));
    };

    if (id == "slider243" && newValue >= 0.5f)
    {
        // LArp's own one-shot Init mirrors the defaults in the fused JSFX.
        // LArp Routing and MIDI Channel deliberately remain unchanged.
        const std::array<std::pair<const char*, float>, 35> defaults {{
            { "slider203", 4.0f }, { "slider204", 0.0f },
            { "slider205", 0.0f }, { "slider206", 0.0f },
            { "slider207", 0.5f }, { "slider208", 0.0f },
            { "slider209", 0.0f }, { "slider210", 0.0f },
            { "slider211", 0.0f }, { "slider212", 0.0f },
            { "slider213", 0.0f }, { "slider214", 1.0f },
            { "slider215", 1.0f }, { "slider216", 0.0f },
            { "slider217", 0.0f }, { "slider218", 0.0f },
            { "slider219", 0.0f }, { "slider234", 0.0f },
            { "slider235", 0.5f }, { "slider236", 0.0f },
            { "slider237", 0.0f }, { "slider238", 0.0f },
            { "slider239", 0.0f }, { "slider240", 0.0f },
            { "slider241", 0.5f }, { "slider242", 1.0f },
            { "slider244", 0.0f }, { "slider247", 0.0f },
            { "slider248", 0.0f }, { "slider249", 0.0f },
            { "slider250", 0.0f }, { "slider251", 0.0f },
            { "slider252", 0.0f }, { "slider253", 0.0f },
            { "slider254", 0.0f }
        }};
        for (const auto& setting : defaults)
            setPlainValue (setting.first, setting.second);
        setPlainValue ("slider243", 0.0f);
    }
    else if (id == "slider003" && newValue < 4.5f)
        setPlainValue ("slider097", juce::roundToInt (newValue) * 0.25f);
    else if (id == "slider052" && newValue < 4.5f)
        setPlainValue ("slider098", juce::roundToInt (newValue) * 0.25f);
    else if (id == "slider097")
    {
        if (const auto* wave = parameters.getRawParameterValue ("slider003");
            wave != nullptr && wave->load() < 4.5f)
            setPlainValue ("slider003", static_cast<float> (juce::roundToInt (
                juce::jlimit (0.0f, 1.0f, newValue) * 4.0f)));
    }
    else if (id == "slider098")
    {
        if (const auto* wave = parameters.getRawParameterValue ("slider052");
            wave != nullptr && wave->load() < 4.5f)
            setPlainValue ("slider052", static_cast<float> (juce::roundToInt (
                juce::jlimit (0.0f, 1.0f, newValue) * 4.0f)));
    }

    synchronisingMorph.store (false);
}


namespace
{
using SeqParam = ljuno::SequencerState::ConfigParameter;

bool sequencerConfigParameterForId (const juce::String& id, SeqParam& parameter)
{
    if (id == "slider315") parameter = SeqParam::startStep;
    else if (id == "slider316") parameter = SeqParam::endStep;
    else if (id == "slider317") parameter = SeqParam::bpmDivision;
    else if (id == "slider318") parameter = SeqParam::playbackMode;
    else if (id == "slider319") parameter = SeqParam::shuffle;
    else if (id == "slider320") parameter = SeqParam::noteSkipProbability;
    else if (id == "slider321") parameter = SeqParam::noteLengthRandomDepth;
    else if (id == "slider322") parameter = SeqParam::legato;
    else if (id == "slider323") parameter = SeqParam::noteRandomDepth;
    else if (id == "slider324") parameter = SeqParam::velocityRandomDepth;
    else if (id == "slider325") parameter = SeqParam::repeatRandomDepth;
    else if (id == "slider326") parameter = SeqParam::octaveShift;
    else if (id == "slider327") parameter = SeqParam::semitoneShift;
    else if (id == "slider328") parameter = SeqParam::globalStepLength;
    else if (id == "slider329") parameter = SeqParam::globalStepVelocity;
    else if (id == "slider330") parameter = SeqParam::globalStepRepeat;
    else if (id == "slider331") parameter = SeqParam::globalStepShift;
    else if (id == "slider332") parameter = SeqParam::midiInputMode;
    else if (id == "slider364") parameter = SeqParam::midiInputPolyphony;
    else if (id == "slider333") parameter = SeqParam::midiChannel;
    else if (id == "slider334") parameter = SeqParam::launchStep;
    else if (id == "slider335") parameter = SeqParam::launchOffsetMs;
    else return false;
    return true;
}

const std::array<std::pair<const char*, SeqParam>, 22> sequencerBankParameters {{
    { "slider315", SeqParam::startStep },
    { "slider316", SeqParam::endStep },
    { "slider317", SeqParam::bpmDivision },
    { "slider318", SeqParam::playbackMode },
    { "slider319", SeqParam::shuffle },
    { "slider320", SeqParam::noteSkipProbability },
    { "slider321", SeqParam::noteLengthRandomDepth },
    { "slider322", SeqParam::legato },
    { "slider323", SeqParam::noteRandomDepth },
    { "slider324", SeqParam::velocityRandomDepth },
    { "slider325", SeqParam::repeatRandomDepth },
    { "slider326", SeqParam::octaveShift },
    { "slider327", SeqParam::semitoneShift },
    { "slider328", SeqParam::globalStepLength },
    { "slider329", SeqParam::globalStepVelocity },
    { "slider330", SeqParam::globalStepRepeat },
    { "slider331", SeqParam::globalStepShift },
    { "slider332", SeqParam::midiInputMode },
    { "slider364", SeqParam::midiInputPolyphony },
    { "slider333", SeqParam::midiChannel },
    { "slider334", SeqParam::launchStep },
    { "slider335", SeqParam::launchOffsetMs }
}};
}

void LJuno116AudioProcessor::setPlainParameterValue (const char* parameterId, float value)
{
    if (auto* parameter = parameters.getParameter (parameterId))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

int LJuno116AudioProcessor::getAvailableSequencerCount() const noexcept
{
    // LJuno exposes three fixed sequencer lanes: Sequence 1 drives Layer 1,
    // Sequence 2 drives Layer 2, and Sequence 3 drives the Noise engine.
    // The Voices parameter remains only the synth polyphony control.
    return 3;
}

int LJuno116AudioProcessor::getSelectedSequencerIndex() const noexcept
{
    return juce::jlimit (0, getAvailableSequencerCount() - 1,
                         sequencerState.getSelectedSequence());
}

ljuno::SequencerConfig LJuno116AudioProcessor::getSequencerConfig (int sequence) const noexcept
{
    return sequencerState.getConfig (sequence);
}

float LJuno116AudioProcessor::getSequencerStepValue (int sequence, int step,
                                                      ljuno::SequencerLayer layer) const noexcept
{
    return sequencerState.getStepValue (sequence, step, layer);
}

void LJuno116AudioProcessor::setSequencerStepValue (int sequence, int step,
                                                     ljuno::SequencerLayer layer,
                                                     float value) noexcept
{
    sequencerState.setStepValue (sequence, step, layer, value);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::addSequencerStepDelta (int sequence, int step,
                                                     ljuno::SequencerLayer layer,
                                                     float delta) noexcept
{
    sequencerState.addStepDelta (sequence, step, layer, delta);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

int LJuno116AudioProcessor::getSequencerStepParameterCount (int sequence, int step) const noexcept
{
    return sequencerState.getStepParameterCount (sequence, step);
}

ljuno::SequencerParameterLock LJuno116AudioProcessor::getSequencerStepParameterLock (
    int sequence, int step, int lockIndex) const noexcept
{
    return sequencerState.getStepParameterLock (sequence, step, lockIndex);
}

int LJuno116AudioProcessor::findSequencerStepParameterLock (int sequence, int step,
                                                             int sliderNumber) const noexcept
{
    return sequencerState.findStepParameterLock (sequence, step, sliderNumber);
}

int LJuno116AudioProcessor::addSequencerStepParameterLock (int sequence, int step,
                                                            int sliderNumber, float value) noexcept
{
    const auto index = sequencerState.addStepParameterLock (sequence, step, sliderNumber, value);
    if (index >= 0)
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return index;
}

bool LJuno116AudioProcessor::setSequencerStepParameterLockValue (int sequence, int step,
                                                                 int lockIndex, float value) noexcept
{
    const auto changed = sequencerState.setStepParameterLockValue (sequence, step, lockIndex, value);
    if (changed)
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return changed;
}

bool LJuno116AudioProcessor::removeSequencerStepParameterLock (int sequence, int step,
                                                               int lockIndex) noexcept
{
    const auto changed = sequencerState.removeStepParameterLock (sequence, step, lockIndex);
    if (changed)
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return changed;
}

float LJuno116AudioProcessor::getPlainParameterValue (const char* parameterId) const noexcept
{
    if (const auto* raw = parameters.getRawParameterValue (parameterId))
        return raw->load();
    return 0.0f;
}

void LJuno116AudioProcessor::selectSequencerFromEditor (int sequence)
{
    const auto clamped = juce::jlimit (0, getAvailableSequencerCount() - 1, sequence);
    setPlainParameterValue ("slider314", static_cast<float> (clamped + 1));
}

bool LJuno116AudioProcessor::nudgeSequencerPageParameter (const juce::String& parameterId,
                                                          float delta)
{
    if (auto* parameter = parameters.getParameter (parameterId))
    {
        const auto currentNormalised = parameter->getValue();
        const auto current = parameter->convertFrom0to1 (currentNormalised);
        const auto targetNormalised = juce::jlimit (0.0f, 1.0f,
            parameter->convertTo0to1 (current + delta));

        if (std::abs (targetNormalised - currentNormalised) <= 1.0e-7f)
            return false;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (targetNormalised);
        parameter->endChangeGesture();
        return true;
    }

    return false;
}

void LJuno116AudioProcessor::resetSequencerState()
{
    sequencerState.reset();
    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::restoreSequencerData (const juce::String& data)
{
    // Routing is an automatable VST parameter and is therefore authoritative.
    // SequencerData also contains a historical copy; preserve the parameter so
    // legacy data cannot overwrite the migrated 3-choice routing value.
    const auto routingMode = parameters.getRawParameterValue ("slider313") != nullptr
        ? juce::roundToInt (parameters.getRawParameterValue ("slider313")->load())
        : sequencerState.getRoutingMode();
    if (! sequencerState.restoreFromBase64 (data))
        sequencerState.reset();
    sequencerState.setRoutingMode (routingMode);
    const auto maximum = getAvailableSequencerCount();
    if (sequencerState.getSelectedSequence() >= maximum)
        sequencerState.setSelectedSequence (maximum - 1);
    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::syncSequencerBankToParameters()
{
    synchronisingSequencerBank.store (true, std::memory_order_relaxed);

    const auto maximum = getAvailableSequencerCount();
    currentSequenceBankIndex = juce::jlimit (0, maximum - 1,
                                             sequencerState.getSelectedSequence());
    sequencerState.setSelectedSequence (currentSequenceBankIndex);

    setPlainParameterValue ("slider313", static_cast<float> (sequencerState.getRoutingMode()));
    setPlainParameterValue ("slider314", static_cast<float> (currentSequenceBankIndex + 1));
    for (const auto& bank : sequencerBankParameters)
        setPlainParameterValue (bank.first,
                                sequencerState.getConfigValue (currentSequenceBankIndex,
                                                               bank.second));
    synchronisingSequencerBank.store (false, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::handleSequencerParameterChanged (const juce::String& id,
                                                               float newValue)
{
    if (id == "slider313")
    {
        sequencerState.setRoutingMode (juce::roundToInt (newValue));
        return;
    }

    if (id == "slider314")
    {
        const auto requested = juce::roundToInt (newValue) - 1;
        const auto clamped = juce::jlimit (0, getAvailableSequencerCount() - 1, requested);
        sequencerState.setSelectedSequence (clamped);
        currentSequenceBankIndex = clamped;
        syncSequencerBankToParameters();
        return;
    }

    SeqParam parameter;
    if (! sequencerConfigParameterForId (id, parameter))
        return;

    sequencerState.setConfigValue (currentSequenceBankIndex, parameter, newValue);

    // These controls can clamp a paired value: Start/End constrain each other,
    // and Launch Step 1 deliberately forbids a negative millisecond offset.
    if (id == "slider315" || id == "slider316" || id == "slider334" || id == "slider335"
        || id == "slider364")
        syncSequencerBankToParameters();
}

void LJuno116AudioProcessor::prepareToPlay (double sampleRate, int maximumBlockSize)
{
    synthEngine.prepare (sampleRate);
    sidechainScratch.setSize (2, std::max (1, maximumBlockSize), false, false, true);
    sidechainScratch.clear();
    lastProcessedParameterRevision = parameterRevision.load (std::memory_order_relaxed);
}
void LJuno116AudioProcessor::releaseResources() {}

bool LJuno116AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto sidechain = layouts.getChannelSet (true, 1);
    if (! (mainInput.isDisabled() || mainInput == juce::AudioChannelSet::stereo())
        || ! (sidechain.isDisabled() || sidechain == juce::AudioChannelSet::stereo()))
        return false;

    // Master must remain stereo. Auxiliary buses may be disabled by the host,
    // but when enabled each is always stereo, matching LR-608's host-friendly
    // multichannel layout handling.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int bus = 1; bus < getBusCount (false); ++bus)
    {
        const auto channels = layouts.getChannelSet (false, bus);
        if (! (channels.isDisabled() || channels == juce::AudioChannelSet::stereo()))
            return false;
    }
    return true;
}

void LJuno116AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto hasStereoInput = getBusCount (true) > 0
                             && getChannelCountOfBus (true, 0) >= 2;
    const auto hasSidechain = getBusCount (true) > 1
                           && getChannelCountOfBus (true, 1) >= 2;
    const float* sidechainLeft = nullptr;
    const float* sidechainRight = nullptr;
    if (hasSidechain)
    {
        const auto sidechain = getBusBuffer (buffer, true, 1);
        if (sidechainScratch.getNumSamples() < buffer.getNumSamples())
            sidechainScratch.setSize (2, buffer.getNumSamples(), false, false, true);
        sidechainScratch.copyFrom (0, 0, sidechain, 0, 0, buffer.getNumSamples());
        sidechainScratch.copyFrom (1, 0, sidechain, 1, 0, buffer.getNumSamples());
        sidechainLeft = sidechainScratch.getReadPointer (0);
        sidechainRight = sidechainScratch.getReadPointer (1);
    }

    auto inputIsSilent = true;
    if (hasStereoInput)
    {
        const auto mainInput = getBusBuffer (buffer, true, 0);
        for (int channel = 0; channel < std::min (2, mainInput.getNumChannels()); ++channel)
            inputIsSilent = inputIsSilent
                         && mainInput.getMagnitude (channel, 0, mainInput.getNumSamples()) == 0.0f;
    }

    const auto revisionBefore = parameterRevision.load (std::memory_order_relaxed);
    if (synthEngine.isDeepIdle() && midi.isEmpty() && inputIsSilent
        && revisionBefore == lastProcessedParameterRevision)
    {
        buffer.clear();
        return;
    }

    auto tempoBpm = 120.0;
    if (auto* currentPlayHead = getPlayHead())
        if (const auto position = currentPlayHead->getPosition())
            if (const auto bpm = position->getBpm())
                tempoBpm = *bpm;

    std::array<float*, 8> auxOutputs {};
    // Real hosts provide a buffer large enough for the active output layout.
    // Some offline unit tests call processBlock with only the main stereo pair;
    // in that case simply omit aux writes rather than touching nonexistent data.
    if (buffer.getNumChannels() >= getTotalNumOutputChannels())
    {
        for (int bus = 1; bus < std::min (5, getBusCount (false)); ++bus)
        {
            if (! getBus (false, bus)->isEnabled())
                continue;
            auto destination = getBusBuffer (buffer, false, bus);
            const auto offset = static_cast<std::size_t> ((bus - 1) * 2);
            auxOutputs[offset] = destination.getWritePointer (0);
            auxOutputs[offset + 1] = destination.getWritePointer (1);
        }
    }

    auto mainOutput = getBusBuffer (buffer, false, 0);
    synthEngine.process (mainOutput, midi, parameters, sequencerState, tempoBpm, hasStereoInput,
                         sidechainLeft, sidechainRight, auxOutputs, revisionBefore);
    lastProcessedParameterRevision = revisionBefore;
}

void LJuno116AudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty ("noiseColorRange01", true, nullptr);
    state.setProperty ("routingModeRevision", 2, nullptr);
    state.setProperty ("sequencerData", sequencerState.serialiseToBase64(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void LJuno116AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (! state.hasProperty ("noiseColorRange01") && state.hasProperty ("slider051"))
            {
                const auto legacy = static_cast<float> (state.getProperty ("slider051"));
                state.setProperty ("slider051", juce::jlimit (0.0f, 1.0f,
                                                              (legacy + 1.0f) * 0.5f),
                                   nullptr);
            }
            state.setProperty ("noiseColorRange01", true, nullptr);

            // Multi-output migration: old projects did not contain aux routing.
            // Keep all four auxiliary stems Off so 1-2 remains the only audible
            // output exactly as before until the user explicitly routes a stem.
            if (! state.hasProperty ("slider374"))
                for (const auto* id : { "slider374", "slider375", "slider376", "slider377" })
                    state.setProperty (id, 0.0f, nullptr);

            // Source MIDI routing migration: old projects received direct notes
            // on every MIDI channel. Omni keeps that behaviour for all three sources.
            if (! state.hasProperty ("slider378"))
                for (const auto* id : { "slider378", "slider379", "slider380" })
                    state.setProperty (id, 0.0f, nullptr);

            // Per-source performance migration. Older projects had one Portamento
            // and one Velocity Volume control. Copy them to the new source controls
            // so old projects retain their sound until edited.
            if (! state.hasProperty ("slider381"))
            {
                const auto legacyPortamento = state.hasProperty ("slider009")
                    ? static_cast<float> (state.getProperty ("slider009")) : 0.0f;
                state.setProperty ("slider381", legacyPortamento, nullptr);
                state.setProperty ("slider382", legacyPortamento, nullptr);
            }
            if (! state.hasProperty ("slider386"))
            {
                const auto l1Portamento = state.hasProperty ("slider381")
                    ? static_cast<float> (state.getProperty ("slider381")) : 0.0f;
                const auto l2Portamento = state.hasProperty ("slider382")
                    ? static_cast<float> (state.getProperty ("slider382")) : l1Portamento;
                // Before Noise had its own control it shared the combined voice
                // glide whenever L1/L2 matched; with split glides it was instant.
                const auto noisePortamento = std::abs (l1Portamento - l2Portamento) <= 0.000001f
                    ? l1Portamento : 0.0f;
                state.setProperty ("slider386", noisePortamento, nullptr);
            }
            if (! state.hasProperty ("slider383"))
            {
                const auto legacyVelocity = state.hasProperty ("slider057")
                    ? static_cast<float> (state.getProperty ("slider057")) : 0.0f;
                for (const auto* id : { "slider383", "slider384", "slider385" })
                    state.setProperty (id, legacyVelocity, nullptr);
            }

            // Per-source pitch-bend range and voice-mode migration. The original
            // single range is copied to all three sources. Voice Mode defaults to
            // Follow Global so older projects preserve the historical Voices=1
            // monophonic behaviour and Voices>1 polyphonic behaviour exactly.
            if (! state.hasProperty ("slider387"))
            {
                const auto legacyBendRange = state.hasProperty ("slider078")
                    ? static_cast<float> (state.getProperty ("slider078")) : 2.0f;
                for (const auto* id : { "slider387", "slider388", "slider389" })
                    state.setProperty (id, legacyBendRange, nullptr);
            }
            if (! state.hasProperty ("slider390"))
            {
                state.setProperty ("slider390", 0.0f, nullptr);
                state.setProperty ("slider391", 0.0f, nullptr);
            }

            // Noise Pan was added after the original L1/L2 pan controls.
            // Old project states are always centred.
            if (! state.hasProperty ("slider395"))
                state.setProperty ("slider395", 0.0f, nullptr);

            // Independent local pre-main filters were added after 0.99.51.
            // Neutral values are true bypass, so old projects retain the exact
            // historical main-filter path until a local filter is edited.
            if (! state.hasProperty ("slider396"))
            {
                for (const auto* id : { "slider396", "slider400", "slider404" })
                    state.setProperty (id, 1.0f, nullptr);
                for (const auto* id : { "slider397", "slider398", "slider399",
                                        "slider401", "slider402", "slider403",
                                        "slider405", "slider406", "slider407" })
                    state.setProperty (id, 0.0f, nullptr);
            }
            if (! state.hasProperty ("slider408"))
            {
                const auto legacyLocalSlope = state.hasProperty ("slider023")
                    ? static_cast<float> (state.getProperty ("slider023")) : 0.0f;
                for (const auto* id : { "slider408", "slider409", "slider410" })
                    state.setProperty (id, legacyLocalSlope, nullptr);
            }

            // Per-source note-generator migration. Before these controls existed,
            // the enabled Sequencer owned the synth before LArp. Recreate that
            // audible routing for older projects; new projects default to Direct.
            if (! state.hasProperty ("slider392"))
            {
                const auto sequencerMode = state.hasProperty ("slider313")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider313"))) : 0;
                const auto larpMode = state.hasProperty ("slider202")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider202"))) : 0;
                const auto source = (sequencerMode == 1 || sequencerMode == 3) ? 1.0f
                                  : (larpMode == 1 || larpMode == 3) ? 2.0f
                                                                    : 0.0f;
                for (const auto* id : { "slider392", "slider393", "slider394" })
                    state.setProperty (id, source, nullptr);
            }

            // Routing mode revision 2 removes the historical Off/Direct choice.
            // Global Note Source now provides the neutral Direct path. Preserve
            // the three musical routing functions and reorder them as:
            // 0 Synth+MIDI, 1 Synth + MIDI Direct, 2 MIDI Only.
            if (! state.hasProperty ("routingModeRevision"))
            {
                const auto remapRouting = [] (int oldMode)
                {
                    if (oldMode == 3) return 1;
                    if (oldMode == 2) return 2;
                    return 0;
                };
                const auto oldLArp = state.hasProperty ("slider202")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider202"))) : 0;
                const auto oldSequencer = state.hasProperty ("slider313")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider313"))) : 0;
                state.setProperty ("slider202", remapRouting (oldLArp), nullptr);
                state.setProperty ("slider313", remapRouting (oldSequencer), nullptr);
            }
            state.setProperty ("routingModeRevision", 2, nullptr);

            // 0.99.3 source-send migration. Older projects had one global wet
            // level per effect. Copy that value to all three source sends so
            // recalling an old project keeps the same overall FX amount until
            // the user chooses different L1/L2/Noise sends.
            if (! state.hasProperty ("slider365"))
            {
                const auto chorus = state.hasProperty ("slider060")
                    ? static_cast<float> (state.getProperty ("slider060")) : 0.4f;
                const auto delayMode = state.hasProperty ("slider100")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider100"))) : 1;
                const auto delay = delayMode == 2 && state.hasProperty ("slider309")
                    ? static_cast<float> (state.getProperty ("slider309"))
                    : (state.hasProperty ("slider103")
                        ? static_cast<float> (state.getProperty ("slider103")) : 0.25f);
                const auto reverb = state.hasProperty ("slider147")
                    ? static_cast<float> (state.getProperty ("slider147")) : 0.2f;
                for (const auto* id : { "slider365", "slider366", "slider367" })
                    state.setProperty (id, juce::jlimit (0.0f, 1.0f, chorus), nullptr);
                for (const auto* id : { "slider368", "slider369", "slider370" })
                    state.setProperty (id, juce::jlimit (0.0f, 1.0f, delay), nullptr);
                for (const auto* id : { "slider371", "slider372", "slider373" })
                    state.setProperty (id, juce::jlimit (0.0f, 1.0f, reverb), nullptr);
            }

            const auto sequencerData = state.getProperty ("sequencerData").toString();
            parameters.replaceState (state);
            restoreSequencerData (sequencerData);
        }
}

juce::AudioProcessorEditor* LJuno116AudioProcessor::createEditor()
{
    return new LJuno116AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LJuno116AudioProcessor();
}
