// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GeneratedParameters.h"

#include <algorithm>

LJuno116AudioProcessor::LJuno116AudioProcessor (juce::File presetLibraryRoot)
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "LJuno116State", ljuno::createParameterLayout()),
      presetManager (parameters, std::move (presetLibraryRoot))
{
    for (const auto& parameter : ljuno::generated::parameters)
        parameters.addParameterListener (parameter.id, this);
}

LJuno116AudioProcessor::~LJuno116AudioProcessor()
{
    for (const auto& parameter : ljuno::generated::parameters)
        parameters.removeParameterListener (parameter.id, this);
}

void LJuno116AudioProcessor::parameterChanged (const juce::String& id, float newValue)
{
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
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
        // Arp State and MIDI Channel deliberately remain unchanged.
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

void LJuno116AudioProcessor::prepareToPlay (double sampleRate, int)
{
    synthEngine.prepare (sampleRate);
    lastProcessedParameterRevision = parameterRevision.load (std::memory_order_relaxed);
}
void LJuno116AudioProcessor::releaseResources() {}

bool LJuno116AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto mainInput = layouts.getMainInputChannelSet();
    const auto sidechain = layouts.getChannelSet (true, 1);
    return (mainInput.isDisabled() || mainInput == juce::AudioChannelSet::stereo())
        && (sidechain.isDisabled() || sidechain == juce::AudioChannelSet::stereo());
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
        sidechainLeft = sidechain.getReadPointer (0);
        sidechainRight = sidechain.getReadPointer (1);
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
        getBusBuffer (buffer, false, 0).clear();
        return;
    }

    auto tempoBpm = 120.0;
    if (auto* currentPlayHead = getPlayHead())
        if (const auto position = currentPlayHead->getPosition())
            if (const auto bpm = position->getBpm())
                tempoBpm = *bpm;

    synthEngine.process (buffer, midi, parameters, tempoBpm, hasStereoInput,
                         sidechainLeft, sidechainRight, revisionBefore);
    lastProcessedParameterRevision = revisionBefore;
}

void LJuno116AudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty ("noiseColorRange01", true, nullptr);
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
            parameters.replaceState (state);
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
