// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/PluginProcessor.h"
#include "../Source/GeneratedParameters.h"
#include "../Source/GeneratedPages.h"
#include "../Source/ParameterCatalog.h"
#include <BinaryData.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

namespace
{
bool setPlainValue (LJuno116AudioProcessor& processor, const char* id, float plainValue)
{
    if (auto* parameter = processor.parameters.getParameter (id))
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        return true;
    }
    return false;
}

bool loadSnapshotForBenchmark (LJuno116AudioProcessor& processor, const juce::File& file)
{
    auto mapped = 0;
    for (const auto& rawLine : juce::StringArray::fromLines (file.loadFileAsString()))
    {
        auto line = rawLine.trim();
        if (! line.startsWith ("[SYNTH] ") && ! line.startsWith ("[ARP] "))
            continue;
        line = line.substring (line.indexOfChar (']') + 1).trimStart();
        const auto separator = line.lastIndexOfChar (':');
        if (separator <= 0)
            continue;
        const auto name = line.substring (0, separator).trim();
        const auto plainValue = static_cast<float> (
            line.substring (separator + 1).trim().getDoubleValue());
        for (const auto& descriptor : ljuno::generated::parameters)
            if (name == descriptor.name)
            {
                mapped += setPlainValue (processor, descriptor.id, plainValue) ? 1 : 0;
                break;
            }
    }
    return mapped > 200;
}

bool loadInitForBenchmark (LJuno116AudioProcessor& processor)
{
    for (const auto& descriptor : ljuno::generated::parameters)
        if (! setPlainValue (processor, descriptor.id,
                             ljuno::initialPatchValue (descriptor.sliderNumber,
                                                       descriptor.defaultValue)))
            return false;
    return true;
}

bool benchmarkInit (int voiceCount, bool compressorEnabled = true)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 128;
    constexpr auto blocks = 2048;
    LJuno116AudioProcessor processor;
    if (! loadInitForBenchmark (processor)
        || ! setPlainValue (processor, "slider002", static_cast<float> (voiceCount))
        || (! compressorEnabled && ! setPlainValue (processor, "slider085", 0.0f)))
        return false;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer notes;
    for (int note = 0; note < voiceCount; ++note)
        notes.addEvent (juce::MidiMessage::noteOn (1, 36 + note, 0.8f), 0);
    audio.clear();
    processor.processBlock (audio, notes);

    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
    {
        audio.clear();
        juce::MidiBuffer emptyMidi;
        processor.processBlock (audio, emptyMidi);
    }
    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    const auto audioSeconds = blocks * blockSize / sampleRate;
    std::cout << "benchmark mode=init" << (compressorEnabled ? "" : "-no-comp")
              << " voices=" << voiceCount << " elapsed=" << elapsed
              << " realtime-ratio=" << elapsed / audioSeconds << '\n';
    return std::isfinite (elapsed) && elapsed > 0.0;
}

bool benchmarkPatch (const juce::File& file, int voiceCount, const juce::String& mode = {})
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 128;
    constexpr auto blocks = 2048;
    LJuno116AudioProcessor processor;
    if (! loadSnapshotForBenchmark (processor, file)
        || ! setPlainValue (processor, "slider002", static_cast<float> (voiceCount)))
        return false;
    if (mode == "no-effects")
    {
        for (const auto* id : { "slider060", "slider085", "slider100", "slider103", "slider114",
                                "slider115", "slider116", "slider117", "slider119",
                                "slider140", "slider147" })
            if (! setPlainValue (processor, id, 0.0f))
                return false;
    }
    else if (mode == "light-filter")
    {
        for (const auto* id : { "slider016", "slider019", "slider023", "slider029",
                                "slider039", "slider056" })
            if (! setPlainValue (processor, id, 0.0f))
                return false;
        if (! setPlainValue (processor, "slider015", 1.0f)
            || ! setPlainValue (processor, "slider055", 0.5f))
            return false;
    }
    else if (mode == "cheap-osc")
    {
        for (const auto* id : { "slider068", "slider069", "slider070", "slider071",
                                "slider072", "slider073", "slider074", "slider075" })
            if (! setPlainValue (processor, id, 0.0f))
                return false;
        if (! setPlainValue (processor, "slider097", 1.0f)
            || ! setPlainValue (processor, "slider098", 1.0f))
            return false;
    }
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer notes;
    for (int note = 0; note < voiceCount; ++note)
        notes.addEvent (juce::MidiMessage::noteOn (1, 36 + note, 0.8f), 0);
    audio.clear();
    processor.processBlock (audio, notes);

    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
    {
        audio.clear();
        juce::MidiBuffer emptyMidi;
        processor.processBlock (audio, emptyMidi);
    }
    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    const auto audioSeconds = blocks * blockSize / sampleRate;
    std::cout << "benchmark mode=" << (mode.isEmpty() ? "full" : mode)
              << " voices=" << voiceCount
              << " elapsed=" << elapsed
              << " realtime-ratio=" << elapsed / audioSeconds << '\n';
    return std::isfinite (elapsed) && elapsed > 0.0;
}

bool renderPatchReference (const juce::File& file, int voiceCount,
                           const juce::File& destination)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 128;
    constexpr auto blocks = 512;
    LJuno116AudioProcessor processor;
    if (! loadSnapshotForBenchmark (processor, file)
        || ! setPlainValue (processor, "slider002", static_cast<float> (voiceCount)))
        return false;
    processor.prepareToPlay (sampleRate, blockSize);
    auto stream = destination.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::AudioBuffer<float> audio (2, blockSize);
    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            for (int note = 0; note < voiceCount; ++note)
                midi.addEvent (juce::MidiMessage::noteOn (1, 36 + note, 0.8f), 0);
        audio.clear();
        processor.processBlock (audio, midi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            stream->writeFloat (audio.getSample (0, sample));
            stream->writeFloat (audio.getSample (1, sample));
        }
    }
    stream->flush();
    return stream->getStatus().wasOk();
}

bool renderScenario (int scenario)
{
    LJuno116AudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    if (scenario == 1)
    {
        const auto configured =
            setPlainValue (processor, "slider015", -0.25f)
            && setPlainValue (processor, "slider016", 0.85f)
            && setPlainValue (processor, "slider017", 0.35f)
            && setPlainValue (processor, "slider018", 0.6f)
            && setPlainValue (processor, "slider019", 2.0f)
            && setPlainValue (processor, "slider020", 12.0f)
            && setPlainValue (processor, "slider021", 1.0f)
            && setPlainValue (processor, "slider022", 0.5f)
            && setPlainValue (processor, "slider023", 1.0f)
            && setPlainValue (processor, "slider028", 0.4f)
            && setPlainValue (processor, "slider029", 2.0f)
            && setPlainValue (processor, "slider031", 3.0f)
            && setPlainValue (processor, "slider033", 0.2f)
            && setPlainValue (processor, "slider038", -0.25f)
            && setPlainValue (processor, "slider040", 0.5f)
            && setPlainValue (processor, "slider041", -2.0f)
            && setPlainValue (processor, "slider066", 20.0f)
            && setPlainValue (processor, "slider067", 15.0f)
            && setPlainValue (processor, "slider068", 2.0f)
            && setPlainValue (processor, "slider071", 3.0f)
            && setPlainValue (processor, "slider087", 0.01f)
            && setPlainValue (processor, "slider088", 0.2f)
            && setPlainValue (processor, "slider089", 0.5f)
            && setPlainValue (processor, "slider090", 0.3f)
            && setPlainValue (processor, "slider097", 0.42f)
            && setPlainValue (processor, "slider098", 0.68f)
            && setPlainValue (processor, "slider126", 1.1f)
            && setPlainValue (processor, "slider127", -0.7f)
            && setPlainValue (processor, "slider128", 1.4f)
            && setPlainValue (processor, "slider130", 0.2f)
            && setPlainValue (processor, "slider133", -0.25f)
            && setPlainValue (processor, "slider157", 0.3f)
            && setPlainValue (processor, "slider158", 0.7f)
            && setPlainValue (processor, "slider168", 1.0f)
            && setPlainValue (processor, "slider169", 2.5f);
        if (! configured)
            return false;
    }
    else if (scenario == 2)
    {
        const auto configured =
            setPlainValue (processor, "slider003", 5.0f)
            && setPlainValue (processor, "slider052", 5.0f)
            && setPlainValue (processor, "slider066", 8.0f)
            && setPlainValue (processor, "slider067", 5.0f)
            && setPlainValue (processor, "slider175", -12.0f)
            && setPlainValue (processor, "slider176", 7.0f)
            && setPlainValue (processor, "slider177", 12.0f)
            && setPlainValue (processor, "slider180", 3.0f)
            && setPlainValue (processor, "slider182", 9.0f)
            && setPlainValue (processor, "slider185", 2.5f)
            && setPlainValue (processor, "slider191", 12.0f)
            && setPlainValue (processor, "slider194", -7.0f)
            && setPlainValue (processor, "slider197", 5.0f)
            && setPlainValue (processor, "slider200", 11.0f)
            && setPlainValue (processor, "slider201", 0.7f)
            && setPlainValue (processor, "slider226", 5.0f)
            && setPlainValue (processor, "slider227", 9.0f)
            && setPlainValue (processor, "slider228", 1.2f)
            && setPlainValue (processor, "slider231", -0.8f)
            && setPlainValue (processor, "slider232", 1.0f)
            && setPlainValue (processor, "slider233", 1.0f);
        if (! configured)
            return false;
    }
    else if (scenario == 3)
    {
        const auto configured =
            setPlainValue (processor, "slider002", 1.0f)
            && setPlainValue (processor, "slider009", 1.5f)
            && setPlainValue (processor, "slider011", 0.5f)
            && setPlainValue (processor, "slider134", 1.0f)
            && setPlainValue (processor, "slider135", 1.0f)
            && setPlainValue (processor, "slider136", 0.8f)
            && setPlainValue (processor, "slider137", 8.0f)
            && setPlainValue (processor, "slider138", 1.5f)
            && setPlainValue (processor, "slider162", 1.0f)
            && setPlainValue (processor, "slider163", 64.0f)
            && setPlainValue (processor, "slider164", 0.0f);
        if (! configured)
            return false;
    }
    else if (scenario == 4)
    {
        const auto configured =
            setPlainValue (processor, "slider002", 1.0f)
            && setPlainValue (processor, "slider009", 0.35f)
            && setPlainValue (processor, "slider134", 1.0f)
            && setPlainValue (processor, "slider257", 1.0f)
            && setPlainValue (processor, "slider258", 128.0f)
            && setPlainValue (processor, "slider259", 1.0f)
            && setPlainValue (processor, "slider261", 1.0f)
            && setPlainValue (processor, "slider262", 0.5f)
            && setPlainValue (processor, "slider264", 0.4f)
            && setPlainValue (processor, "slider265", -0.2f)
            && setPlainValue (processor, "slider267", 2.0f)
            && setPlainValue (processor, "slider268", 64.0f)
            && setPlainValue (processor, "slider270", -1.0f)
            && setPlainValue (processor, "slider271", 1.0f)
            && setPlainValue (processor, "slider272", 0.25f);
        if (! configured)
            return false;
    }

    double absoluteSum = 0.0;
    double fallbackSum = 0.0;
    float peak = 0.0f;
    for (int block = 0; block < 500; ++block)
    {
        juce::AudioBuffer<float> audio (2, 128);
        audio.clear();
        juce::MidiBuffer midi;
        if (scenario == 3)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
            if (block == 80)
                midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 96), 0);
            if (block == 160)
                midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
            if (block == 300)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        }
        else if (scenario == 4)
        {
            if (block == 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 105), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
            }
            if (block == 300)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
                midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            }
        }
        else
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
            if (block == 300)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        }

        processor.processBlock (audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            const auto* samples = audio.getReadPointer (channel);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                if (! std::isfinite (samples[sample]))
                    return false;
                peak = std::max (peak, std::abs (samples[sample]));
                absoluteSum += std::abs (samples[sample]);
                if (block >= 180 && block < 260)
                    fallbackSum += std::abs (samples[sample]);
            }
        }
    }

    return peak > 0.0001f && absoluteSum > 1.0 && peak < 100.0f
        && (scenario != 3 || fallbackSum > 0.1);
}

std::vector<float> renderPitchArpLayer (bool layer2, float glide,
                                        bool stopArpAtRelease,
                                        bool pitchMovement = true,
                                        float levelMovement = 0.0f)
{
    LJuno116AudioProcessor processor;
    const auto arpMode = layer2 ? "slider267" : "slider257";
    const auto arpRate = layer2 ? "slider268" : "slider258";
    const auto arpOctave = layer2 ? "slider269" : "slider259";
    const auto arpGlide = layer2 ? "slider271" : "slider261";
    const auto arpPitchMovement = layer2 ? "slider279" : "slider278";
    const auto arpLevelMovement = layer2 ? "slider272" : "slider265";
    const auto configured =
        setPlainValue (processor, "slider002", 1.0f)
        && setPlainValue (processor, "slider003", 3.0f)
        && setPlainValue (processor, "slider052", 3.0f)
        && setPlainValue (processor, "slider011", layer2 ? 1.0f : 0.0f)
        && setPlainValue (processor, "slider005", 0.0f)
        && setPlainValue (processor, "slider006", 0.0f)
        && setPlainValue (processor, "slider007", 1.0f)
        && setPlainValue (processor, "slider008", 1.0f)
        && setPlainValue (processor, "slider009", 0.0f)
        && setPlainValue (processor, "slider013", 0.0f)
        && setPlainValue (processor, "slider014", 0.0f)
        && setPlainValue (processor, "slider060", 0.0f)
        && setPlainValue (processor, "slider085", 0.0f)
        && setPlainValue (processor, "slider100", 0.0f)
        && setPlainValue (processor, "slider140", 0.0f)
        && setPlainValue (processor, arpMode, 2.0f)
        && setPlainValue (processor, arpRate, 128.0f)
        && setPlainValue (processor, arpOctave, 1.0f)
        && setPlainValue (processor, arpGlide, glide)
        && setPlainValue (processor, arpPitchMovement, pitchMovement ? 1.0f : 0.0f)
        && setPlainValue (processor, arpLevelMovement, levelMovement);
    if (! configured)
        return {};

    processor.prepareToPlay (48000.0, 128);
    std::vector<float> result;
    result.reserve (140 * 128);
    for (int block = 0; block < 180; ++block)
    {
        juce::AudioBuffer<float> audio (2, 128);
        audio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 105), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
        }
        if (block == 40)
        {
            if (stopArpAtRelease)
                setPlainValue (processor, arpMode, 0.0f);
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        }
        processor.processBlock (audio, midi);
        if (block >= 40)
            result.insert (result.end(), audio.getReadPointer (0),
                           audio.getReadPointer (0) + audio.getNumSamples());
    }
    return result;
}

bool pitchArp2RunsBothLayersWithIndependentGlideAndRelease()
{
    const auto layer1Release = renderPitchArpLayer (false, 0.0f, false);
    const auto layer1Stopped = renderPitchArpLayer (false, 0.0f, true);
    const auto layer1Glide = renderPitchArpLayer (false, 1.0f, false);
    const auto layer2Release = renderPitchArpLayer (true, 0.0f, false);
    const auto layer2Stopped = renderPitchArpLayer (true, 0.0f, true);
    const auto layer2Glide = renderPitchArpLayer (true, 1.0f, false);
    const auto layer1PitchOff = renderPitchArpLayer (false, 0.0f, false, false);
    const auto layer1PitchOffLevelMovement = renderPitchArpLayer (false, 0.0f, false, false, 1.0f);
    const auto layer2PitchOff = renderPitchArpLayer (true, 0.0f, false, false);
    const auto layer2PitchOffLevelMovement = renderPitchArpLayer (true, 0.0f, false, false, 1.0f);
    if (layer1Release.empty() || layer1Release.size() != layer1Stopped.size()
        || layer1Release.size() != layer1Glide.size()
        || layer2Release.size() != layer2Stopped.size()
        || layer2Release.size() != layer2Glide.size()
        || layer1Release.size() != layer1PitchOff.size()
        || layer1PitchOff.size() != layer1PitchOffLevelMovement.size()
        || layer2Release.size() != layer2PitchOff.size()
        || layer2PitchOff.size() != layer2PitchOffLevelMovement.size())
        return false;

    const auto difference = [] (const std::vector<float>& a,
                                const std::vector<float>& b)
    {
        double total = 0.0;
        for (std::size_t index = 0; index < a.size(); ++index)
            total += std::abs (static_cast<double> (a[index] - b[index]));
        return total;
    };
    const auto energy = [] (const std::vector<float>& samples)
    {
        double total = 0.0;
        for (const auto sample : samples)
            total += std::abs (static_cast<double> (sample));
        return total;
    };

    return energy (layer1Release) > 1.0 && energy (layer2Release) > 1.0
        && difference (layer1Release, layer1Stopped) > 1.0
        && difference (layer2Release, layer2Stopped) > 1.0
        && difference (layer1Release, layer1Glide) > 1.0
        && difference (layer2Release, layer2Glide) > 1.0
        && difference (layer1Release, layer1PitchOff) > 1.0
        && difference (layer2Release, layer2PitchOff) > 1.0
        && difference (layer1PitchOff, layer1PitchOffLevelMovement) > 1.0
        && difference (layer2PitchOff, layer2PitchOffLevelMovement) > 1.0;
}

std::vector<float> renderLfoSquash (bool secondLfo, float upper, float lower)
{
    LJuno116AudioProcessor processor;
    if (! setPlainValue (processor, "slider002", 1.0f)
        || ! setPlainValue (processor, "slider011", 0.0f)
        || ! setPlainValue (processor, "slider003", 3.0f)
        || ! setPlainValue (processor, "slider005", 0.0f)
        || ! setPlainValue (processor, "slider006", 0.0f)
        || ! setPlainValue (processor, "slider007", 1.0f)
        || ! setPlainValue (processor, secondLfo ? "slider037" : "slider027", 2.0f)
        || ! setPlainValue (processor, secondLfo ? "slider036" : "slider026", 16.0f)
        || ! setPlainValue (processor, secondLfo ? "slider041" : "slider031", 12.0f)
        || ! setPlainValue (processor, "slider060", 0.0f)
        || ! setPlainValue (processor, "slider085", 0.0f)
        || ! setPlainValue (processor, "slider100", 0.0f)
        || ! setPlainValue (processor, "slider140", 0.0f)
        || ! setPlainValue (processor, secondLfo ? "slider276" : "slider274", upper)
        || ! setPlainValue (processor, secondLfo ? "slider277" : "slider275", lower))
        return {};
    processor.prepareToPlay (48000.0, 128);
    std::vector<float> result;
    for (int block = 0; block < 100; ++block)
    {
        juce::AudioBuffer<float> audio (2, 128);
        audio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
        processor.processBlock (audio, midi);
        result.insert (result.end(), audio.getReadPointer (0),
                       audio.getReadPointer (0) + audio.getNumSamples());
    }
    return result;
}

bool lfoUpperAndLowerSquashAreIndependent()
{
    for (const auto secondLfo : { false, true })
    {
        const auto neutral = renderLfoSquash (secondLfo, 0.0f, 0.0f);
        const auto upper = renderLfoSquash (secondLfo, 1.0f, 0.0f);
        const auto lower = renderLfoSquash (secondLfo, 0.0f, 1.0f);
        if (neutral.empty() || neutral.size() != upper.size()
            || neutral.size() != lower.size())
            return false;
        double upperDifference = 0.0, lowerDifference = 0.0, shapeDifference = 0.0;
        for (std::size_t index = 0; index < neutral.size(); ++index)
        {
            upperDifference += std::abs (static_cast<double> (neutral[index] - upper[index]));
            lowerDifference += std::abs (static_cast<double> (neutral[index] - lower[index]));
            shapeDifference += std::abs (static_cast<double> (upper[index] - lower[index]));
        }
        if (upperDifference <= 1.0 || lowerDifference <= 1.0 || shapeDifference <= 1.0)
            return false;
    }
    return true;
}

std::vector<float> renderNoiseType (int type, int note = 60,
                                    float lfoNoisePitch = 0.0f,
                                    bool useLfo2 = false,
                                    float noiseColour = 0.5f,
                                    float noiseStereo = 0.0f,
                                    int channel = 0)
{
    LJuno116AudioProcessor processor;
    if (! setPlainValue (processor, "slider002", 1.0f)
        || ! setPlainValue (processor, "slider049", 0.0f)
        || ! setPlainValue (processor, "slider058", 0.0f)
        || ! setPlainValue (processor, "slider050", 1.0f)
        || ! setPlainValue (processor, "slider051", noiseColour)
        || ! setPlainValue (processor, "slider055", 0.5f)
        || ! setPlainValue (processor, "slider280", static_cast<float> (type))
        || ! setPlainValue (processor, "slider282", useLfo2 ? 0.0f : lfoNoisePitch)
        || ! setPlainValue (processor, "slider283", useLfo2 ? lfoNoisePitch : 0.0f)
        || ! setPlainValue (processor, "slider284", noiseStereo)
        || ! setPlainValue (processor, "slider025", 0.0f)
        || ! setPlainValue (processor, "slider026", 16.0f)
        || ! setPlainValue (processor, "slider027", 0.0f)
        || ! setPlainValue (processor, "slider035", 0.0f)
        || ! setPlainValue (processor, "slider036", 16.0f)
        || ! setPlainValue (processor, "slider037", 0.0f)
        || ! setPlainValue (processor, "slider005", 0.0f)
        || ! setPlainValue (processor, "slider006", 0.0f)
        || ! setPlainValue (processor, "slider007", 1.0f)
        || ! setPlainValue (processor, "slider008", 0.2f)
        || ! setPlainValue (processor, "slider060", 0.0f)
        || ! setPlainValue (processor, "slider085", 0.0f)
        || ! setPlainValue (processor, "slider100", 0.0f)
        || ! setPlainValue (processor, "slider140", 0.0f))
        return {};

    processor.prepareToPlay (48000.0, 128);
    std::vector<float> result;
    result.reserve (48 * 128);
    for (int block = 0; block < 48; ++block)
    {
        juce::AudioBuffer<float> audio (2, 128);
        audio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 120), 0);
        processor.processBlock (audio, midi);
        result.insert (result.end(), audio.getReadPointer (juce::jlimit (0, 1, channel)),
                       audio.getReadPointer (juce::jlimit (0, 1, channel))
                           + audio.getNumSamples());
    }
    return result;
}

bool allNoiseTypesAreAudibleAndDistinct()
{
    std::vector<float> previous;
    for (int type = 0; type <= 10; ++type)
    {
        const auto samples = renderNoiseType (type);
        if (samples.empty())
            return false;
        double energy = 0.0, difference = 0.0;
        for (std::size_t index = 0; index < samples.size(); ++index)
        {
            if (! std::isfinite (samples[index]))
                return false;
            energy += std::abs (static_cast<double> (samples[index]));
            if (! previous.empty())
                difference += std::abs (static_cast<double> (samples[index] - previous[index]));
        }
        if (energy < 0.1 || (! previous.empty() && difference < 0.1))
            return false;
        previous = samples;
    }
    const auto sidLow = renderNoiseType (7, 48);
    const auto sidHigh = renderNoiseType (7, 72);
    const auto sidLfo1 = renderNoiseType (7, 60, 24.0f);
    const auto sidLfo2 = renderNoiseType (7, 60, 24.0f, true);
    const auto classicBase = renderNoiseType (1, 60);
    const auto classicPitchMod = renderNoiseType (1, 60, 24.0f);
    const auto radioLow = renderNoiseType (5, 48);
    const auto radioHigh = renderNoiseType (5, 72);
    const auto sidDark = renderNoiseType (7, 60, 0.0f, false, 0.0f);
    const auto sidBright = renderNoiseType (7, 60, 0.0f, false, 1.0f);
    const auto monoLeft = renderNoiseType (7, 60, 0.0f, false, 0.5f, 0.0f, 0);
    const auto monoRight = renderNoiseType (7, 60, 0.0f, false, 0.5f, 0.0f, 1);
    const auto stereoRight = renderNoiseType (7, 60, 0.0f, false, 0.5f, 1.0f, 1);
    if (sidLow.size() != sidHigh.size() || sidLow.size() != sidLfo1.size()
        || sidLow.size() != sidLfo2.size() || classicBase.size() != classicPitchMod.size()
        || radioLow.size() != radioHigh.size() || sidDark.size() != sidBright.size()
        || monoLeft.size() != monoRight.size() || monoLeft.size() != stereoRight.size())
        return false;
    double noteDifference = 0.0, lfo1Difference = 0.0, lfo2Difference = 0.0;
    double classicPitchDifference = 0.0, radioNoteDifference = 0.0;
    double colourDifference = 0.0, monoDifference = 0.0, stereoDifference = 0.0;
    const auto sidMiddle = renderNoiseType (7, 60);
    for (std::size_t index = 0; index < sidLow.size(); ++index)
    {
        noteDifference += std::abs (static_cast<double> (sidLow[index] - sidHigh[index]));
        lfo1Difference += std::abs (static_cast<double> (sidMiddle[index] - sidLfo1[index]));
        lfo2Difference += std::abs (static_cast<double> (sidMiddle[index] - sidLfo2[index]));
        classicPitchDifference += std::abs (static_cast<double> (classicBase[index]
                                                                - classicPitchMod[index]));
        radioNoteDifference += std::abs (static_cast<double> (radioLow[index]
                                                             - radioHigh[index]));
        colourDifference += std::abs (static_cast<double> (sidDark[index]
                                                          - sidBright[index]));
        monoDifference += std::abs (static_cast<double> (monoLeft[index]
                                                        - monoRight[index]));
        stereoDifference += std::abs (static_cast<double> (monoLeft[index]
                                                          - stereoRight[index]));
    }
    const auto passed = noteDifference > 1.0 && lfo1Difference > 1.0
                     && lfo2Difference > 1.0 && classicPitchDifference < 0.000001
                     && radioNoteDifference > 1.0 && colourDifference > 1.0
                     && monoDifference < 0.000001 && stereoDifference > 1.0;
    if (! passed)
        std::cerr << "noise detail: sid-note=" << noteDifference
                  << " lfo1=" << lfo1Difference << " lfo2=" << lfo2Difference
                  << " classic-pitch=" << classicPitchDifference
                  << " radio-note=" << radioNoteDifference
                  << " colour=" << colourDifference << " mono=" << monoDifference
                  << " stereo=" << stereoDifference << '\n';
    return passed;
}

bool morphControlsAreLinked()
{
    LJuno116AudioProcessor processor;
    if (! setPlainValue (processor, "slider003", 1.0f))
        return false;
    const auto* morph = processor.parameters.getRawParameterValue ("slider097");
    if (morph == nullptr || std::abs (morph->load() - 0.25f) > 0.0001f)
        return false;

    if (! setPlainValue (processor, "slider097", 0.68f))
        return false;
    const auto* wave = processor.parameters.getRawParameterValue ("slider003");
    return wave != nullptr && std::abs (wave->load() - 3.0f) < 0.0001f;
}

bool luaInitValuesAreMapped()
{
    return std::abs (ljuno::initialPatchValue (8, 0.25f) - 0.0003f) < 0.00001f
        && std::abs (ljuno::initialPatchValue (11, 0.5f)) < 0.00001f
        && std::abs (ljuno::initialPatchValue (60, 0.4f)) < 0.00001f
        && std::abs (ljuno::initialPatchValue (87, 0.0003f) - 0.003f) < 0.00001f
        && std::abs (ljuno::initialPatchValue (90, 0.25f) - 0.0003f) < 0.00001f
        && std::abs (ljuno::initialPatchValue (94, 10.0f) - 20.0f) < 0.00001f
        && std::abs (ljuno::initialPatchValue (100, 1.0f)) < 0.00001f
        && std::abs (ljuno::initialPatchValue (62, 0.0f)) < 0.00001f;
}

bool initializeButtonWorks()
{
    LJuno116AudioProcessor processor;
    if (! setPlainValue (processor, "slider008", 2.0f)
        || ! setPlainValue (processor, "slider011", 0.9f)
        || ! setPlainValue (processor, "slider094", 3.0f)
        || ! setPlainValue (processor, "slider100", 1.0f))
        return false;

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::Button* initialize = nullptr;
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
    {
        if (auto* button = dynamic_cast<juce::Button*> (editor->getChildComponent (child));
            button != nullptr && button->getButtonText() == "Initialize synth")
        {
            initialize = button;
            break;
        }
    }
    if (initialize == nullptr || ! initialize->onClick)
        return false;
    initialize->onClick();

    const auto valueIs = [&processor] (const char* id, float expected)
    {
        const auto* value = processor.parameters.getRawParameterValue (id);
        return value != nullptr && std::abs (value->load() - expected) < 0.00001f;
    };
    return valueIs ("slider008", 0.0003f)
        && valueIs ("slider011", 0.0f)
        && valueIs ("slider094", 20.0f)
        && valueIs ("slider100", 0.0f);
}

bool stereoInputPassesThroughDry()
{
    LJuno116AudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);
    if (! setPlainValue (processor, "slider001", -36.0f)
        || ! setPlainValue (processor, "slider256", -18.0f))
        return false;

    juce::AudioBuffer<float> audio (2, 128);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        audio.setSample (0, sample, 0.2f * std::sin (sample * 0.071f));
        audio.setSample (1, sample, -0.15f * std::cos (sample * 0.053f));
    }
    juce::AudioBuffer<float> expected;
    expected.makeCopyOf (audio);
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);

    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            if (std::abs (audio.getSample (channel, sample)
                          - expected.getSample (channel, sample)) > 0.000001f)
                return false;
    return true;
}

bool effectsChangeTheSynthSignal()
{
    LJuno116AudioProcessor dry;
    LJuno116AudioProcessor wet;
    dry.prepareToPlay (48000.0, 128);
    wet.prepareToPlay (48000.0, 128);

    const auto bypassConfigured = setPlainValue (dry, "slider060", 0.0f)
        && setPlainValue (dry, "slider100", 0.0f)
        && setPlainValue (dry, "slider114", 0.0f)
        && setPlainValue (dry, "slider115", 0.0f)
        && setPlainValue (dry, "slider116", 0.0f)
        && setPlainValue (dry, "slider117", 0.0f)
        && setPlainValue (dry, "slider119", 0.0f);
    const auto effectsConfigured = setPlainValue (wet, "slider060", 0.8f)
        && setPlainValue (wet, "slider061", 0.7f)
        && setPlainValue (wet, "slider063", 1.0f)
        && setPlainValue (wet, "slider100", 1.0f)
        && setPlainValue (wet, "slider101", 0.05f)
        && setPlainValue (wet, "slider102", 0.65f)
        && setPlainValue (wet, "slider103", 0.7f)
        && setPlainValue (wet, "slider104", 0.8f)
        && setPlainValue (wet, "slider106", 0.0f)
        && setPlainValue (wet, "slider114", 5.0f)
        && setPlainValue (wet, "slider116", -4.0f)
        && setPlainValue (wet, "slider119", 6.0f);
    if (! bypassConfigured || ! effectsConfigured)
        return false;

    double difference = 0.0;
    double wetTail = 0.0;
    for (int block = 0; block < 450; ++block)
    {
        juce::AudioBuffer<float> dryAudio (2, 128), wetAudio (2, 128);
        dryAudio.clear();
        wetAudio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 105), 0);
        if (block == 120)
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        dry.processBlock (dryAudio, midi);
        wet.processBlock (wetAudio, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 128; ++sample)
            {
                const auto drySample = dryAudio.getSample (channel, sample);
                const auto wetSample = wetAudio.getSample (channel, sample);
                if (! std::isfinite (drySample) || ! std::isfinite (wetSample))
                    return false;
                difference += std::abs (wetSample - drySample);
                if (block > 300)
                    wetTail += std::abs (wetSample);
            }
    }
    return difference > 10.0 && wetTail > 0.001;
}

bool reverbProducesAStableTail()
{
    LJuno116AudioProcessor dry;
    LJuno116AudioProcessor wet;
    dry.prepareToPlay (48000.0, 128);
    wet.prepareToPlay (48000.0, 128);

    const auto configureCommon = [] (LJuno116AudioProcessor& processor)
    {
        return setPlainValue (processor, "slider008", 0.01f)
            && setPlainValue (processor, "slider060", 0.0f)
            && setPlainValue (processor, "slider085", 0.0f)
            && setPlainValue (processor, "slider100", 0.0f)
            && setPlainValue (processor, "slider156", 0.0f);
    };
    if (! configureCommon (dry) || ! configureCommon (wet)
        || ! setPlainValue (dry, "slider140", 0.0f)
        || ! setPlainValue (wet, "slider140", 1.0f)
        || ! setPlainValue (wet, "slider141", 20.0f)
        || ! setPlainValue (wet, "slider143", 1.3f)
        || ! setPlainValue (wet, "slider144", 1.5f)
        || ! setPlainValue (wet, "slider145", 6500.0f)
        || ! setPlainValue (wet, "slider146", 1.0f)
        || ! setPlainValue (wet, "slider147", 0.65f)
        || ! setPlainValue (wet, "slider148", 0.3f))
        return false;

    double dryTail = 0.0, wetTail = 0.0, difference = 0.0;
    for (int block = 0; block < 900; ++block)
    {
        juce::AudioBuffer<float> dryAudio (2, 128), wetAudio (2, 128);
        dryAudio.clear();
        wetAudio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        if (block == 40)
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        dry.processBlock (dryAudio, midi);
        wet.processBlock (wetAudio, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 128; ++sample)
            {
                const auto drySample = dryAudio.getSample (channel, sample);
                const auto wetSample = wetAudio.getSample (channel, sample);
                if (! std::isfinite (drySample) || ! std::isfinite (wetSample)
                    || std::abs (wetSample) >= 100.0f)
                    return false;
                difference += std::abs (wetSample - drySample);
                if (block >= 250)
                {
                    dryTail += std::abs (drySample);
                    wetTail += std::abs (wetSample);
                }
            }
    }
    return difference > 10.0 && wetTail > dryTail + 0.01;
}

bool sidechainDucksPostCompressor()
{
    LJuno116AudioProcessor unducked;
    LJuno116AudioProcessor ducked;
    const juce::AudioProcessor::BusesLayout layout {
        { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo() },
        { juce::AudioChannelSet::stereo() }
    };
    if (! unducked.setBusesLayout (layout) || ! ducked.setBusesLayout (layout))
        return false;
    unducked.prepareToPlay (48000.0, 128);
    ducked.prepareToPlay (48000.0, 128);

    const auto configure = [] (LJuno116AudioProcessor& processor)
    {
        return setPlainValue (processor, "slider080", -30.0f)
            && setPlainValue (processor, "slider081", 9.0f)
            && setPlainValue (processor, "slider082", 0.0f)
            && setPlainValue (processor, "slider083", 20.0f)
            && setPlainValue (processor, "slider084", 20.0f)
            && setPlainValue (processor, "slider085", 100.0f)
            && setPlainValue (processor, "slider139", 1.0f)
            && setPlainValue (processor, "slider160", 1.0f);
    };
    if (! configure (unducked) || ! configure (ducked))
        return false;

    double unduckedEnergy = 0.0, duckedEnergy = 0.0;
    for (int block = 0; block < 120; ++block)
    {
        juce::AudioBuffer<float> unduckedAudio (4, 128), duckedAudio (4, 128);
        unduckedAudio.clear();
        duckedAudio.clear();
        for (int sample = 0; sample < 128; ++sample)
        {
            const auto mainInput = 0.4f * std::sin ((block * 128 + sample) * 0.031f);
            unduckedAudio.setSample (0, sample, mainInput);
            unduckedAudio.setSample (1, sample, mainInput);
            duckedAudio.setSample (0, sample, mainInput);
            duckedAudio.setSample (1, sample, mainInput);
            duckedAudio.setSample (2, sample, 1.0f);
            duckedAudio.setSample (3, sample, 1.0f);
        }
        juce::MidiBuffer midi;
        unducked.processBlock (unduckedAudio, midi);
        ducked.processBlock (duckedAudio, midi);
        if (block >= 20)
            for (int sample = 0; sample < 128; ++sample)
            {
                const auto a = unduckedAudio.getSample (0, sample);
                const auto b = duckedAudio.getSample (0, sample);
                if (! std::isfinite (a) || ! std::isfinite (b))
                    return false;
                unduckedEnergy += std::abs (a);
                duckedEnergy += std::abs (b);
            }
    }
    return unduckedEnergy > 1.0 && duckedEnergy < unduckedEnergy * 0.75;
}

bool deepIdleSleepsAndWakes()
{
    LJuno116AudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);
    if (! processor.isEngineDeepIdle())
        return false;

    juce::AudioBuffer<float> audio (2, 128);
    audio.clear();
    juce::MidiBuffer midi;
    processor.processBlock (audio, midi);
    if (! processor.isEngineDeepIdle() || audio.getMagnitude (0, 128) != 0.0f)
        return false;

    if (! setPlainValue (processor, "slider008", 0.0f)
        || ! setPlainValue (processor, "slider060", 0.0f)
        || ! setPlainValue (processor, "slider085", 0.0f)
        || ! setPlainValue (processor, "slider100", 0.0f)
        || ! setPlainValue (processor, "slider140", 0.0f))
        return false;
    audio.clear();
    processor.processBlock (audio, midi);
    if (! processor.isEngineDeepIdle())
        return false;

    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    processor.processBlock (audio, midi);
    if (processor.isEngineDeepIdle() || audio.getMagnitude (0, 128) <= 0.00001f)
        return false;

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    for (int block = 0; block < 3000 && ! processor.isEngineDeepIdle(); ++block)
    {
        audio.clear();
        processor.processBlock (audio, midi);
        midi.clear();
    }
    if (! processor.isEngineDeepIdle())
        return false;

    // Parameter automation wakes one block so cached/smoothed state can be
    // refreshed, then the silent engine is allowed to sleep again.
    if (! setPlainValue (processor, "slider024", 0.15f))
        return false;
    audio.clear();
    processor.processBlock (audio, midi);
    return processor.isEngineDeepIdle() && audio.getMagnitude (0, 128) == 0.0f;
}

bool monoUnisonUsesFreePhasesAndStaysCentred()
{
    LJuno116AudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);
    const auto configured = setPlainValue (processor, "slider002", 1.0f)
        && setPlainValue (processor, "slider003", 3.0f)
        && setPlainValue (processor, "slider052", 3.0f)
        && setPlainValue (processor, "slider008", 0.0f)
        && setPlainValue (processor, "slider013", 0.0f)
        && setPlainValue (processor, "slider014", 0.0f)
        && setPlainValue (processor, "slider024", 0.0f)
        && setPlainValue (processor, "slider060", 0.0f)
        && setPlainValue (processor, "slider085", 0.0f)
        && setPlainValue (processor, "slider100", 0.0f)
        && setPlainValue (processor, "slider136", 1.0f)
        && setPlainValue (processor, "slider137", 16.0f)
        && setPlainValue (processor, "slider138", 1.0f)
        && setPlainValue (processor, "slider140", 0.0f);
    if (! configured)
        return false;

    const auto captureAttack = [&processor] (double& leftEnergy, double& rightEnergy)
    {
        juce::AudioBuffer<float> audio (2, 128);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
        processor.processBlock (audio, midi);
        std::array<float, 128> attack {};
        for (int sample = 0; sample < 128; ++sample)
        {
            const auto left = audio.getSample (0, sample);
            const auto right = audio.getSample (1, sample);
            leftEnergy += std::abs (left);
            rightEnergy += std::abs (right);
            attack[static_cast<std::size_t> (sample)] = 0.5f * (left + right);
        }
        return attack;
    };

    double leftEnergy = 0.0, rightEnergy = 0.0;
    const auto firstAttack = captureAttack (leftEnergy, rightEnergy);
    if (leftEnergy <= 0.01 || rightEnergy <= 0.01
        || leftEnergy / rightEnergy < 0.35 || leftEnergy / rightEnergy > 2.85)
        return false;

    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    for (int block = 0; block < 3000 && ! processor.isEngineDeepIdle(); ++block)
    {
        audio.clear();
        processor.processBlock (audio, midi);
        midi.clear();
    }
    if (! processor.isEngineDeepIdle())
        return false;

    double secondLeftEnergy = 0.0, secondRightEnergy = 0.0;
    const auto secondAttack = captureAttack (secondLeftEnergy, secondRightEnergy);
    double difference = 0.0;
    for (std::size_t sample = 0; sample < firstAttack.size(); ++sample)
        difference += std::abs (firstAttack[sample] - secondAttack[sample]);
    return difference > 0.01 && secondLeftEnergy > 0.01 && secondRightEnergy > 0.01;
}

std::vector<float> renderPolyModeTransition (int polyMode)
{
    LJuno116AudioProcessor processor;
    const auto configured = setPlainValue (processor, "slider002", 4.0f)
        && setPlainValue (processor, "slider010", static_cast<float> (polyMode))
        && setPlainValue (processor, "slider003", 3.0f)
        && setPlainValue (processor, "slider097", 0.75f)
        && setPlainValue (processor, "slider011", 0.0f)
        && setPlainValue (processor, "slider009", 1.2f)
        && setPlainValue (processor, "slider008", 0.45f)
        && setPlainValue (processor, "slider015", 1.0f)
        && setPlainValue (processor, "slider016", 0.0f)
        && setPlainValue (processor, "slider060", 0.0f)
        && setPlainValue (processor, "slider085", 0.0f)
        && setPlainValue (processor, "slider100", 0.0f)
        && setPlainValue (processor, "slider140", 0.0f);
    if (! configured)
        return {};

    constexpr auto blockSize = 128;
    processor.prepareToPlay (48000.0, blockSize);
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    static constexpr std::array<int, 4> firstChord { 48, 52, 55, 59 };
    static constexpr std::array<int, 4> secondChord { 50, 53, 57, 60 };
    for (const auto note : firstChord)
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
    audio.clear();
    processor.processBlock (audio, midi);

    for (int block = 0; block < 192; ++block)
    {
        audio.clear();
        juce::MidiBuffer none;
        processor.processBlock (audio, none);
    }

    midi.clear();
    for (const auto note : firstChord)
        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
    for (const auto note : secondChord)
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 1);

    std::vector<float> result;
    result.reserve (blockSize * 24);
    for (int block = 0; block < 24; ++block)
    {
        audio.clear();
        processor.processBlock (audio, midi);
        midi.clear();
        const auto* left = audio.getReadPointer (0);
        result.insert (result.end(), left, left + blockSize);
    }
    return result;
}

bool modernAndRolandPolyphonyAreDistinctAndStable()
{
    const auto modern = renderPolyModeTransition (0);
    const auto roland = renderPolyModeTransition (1);
    if (modern.size() != roland.size() || modern.empty())
        return false;

    auto difference = 0.0;
    auto rolandEnergy = 0.0;
    for (std::size_t sample = 0; sample < modern.size(); ++sample)
    {
        if (! std::isfinite (modern[sample]) || ! std::isfinite (roland[sample]))
            return false;
        difference += std::abs (modern[sample] - roland[sample]);
        rolandEnergy += std::abs (roland[sample]);
    }
    return difference > 1.0 && rolandEnergy > 1.0;
}

std::vector<float> renderHiddenSuperWaveModulator (int unisonVoices,
                                                   float phaseMod21,
                                                   float character)
{
    LJuno116AudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);
    const auto configured = setPlainValue (processor, "slider002", 1.0f)
        && setPlainValue (processor, "slider003", 4.0f)
        && setPlainValue (processor, "slider004", 0.5f)
        && setPlainValue (processor, "slider011", 0.0f)
        && setPlainValue (processor, "slider013", 0.0f)
        && setPlainValue (processor, "slider014", 0.0f)
        && setPlainValue (processor, "slider015", 1.0f)
        && setPlainValue (processor, "slider016", 0.0f)
        && setPlainValue (processor, "slider019", 0.0f)
        && setPlainValue (processor, "slider024", 0.0f)
        && setPlainValue (processor, "slider025", 1.0f)
        && setPlainValue (processor, "slider026", 2.0f)
        && setPlainValue (processor, "slider027", 0.0f)
        && setPlainValue (processor, "slider049", 0.1f)
        && setPlainValue (processor, "slider052", 5.0f)
        && setPlainValue (processor, "slider058", 2.0f)
        && setPlainValue (processor, "slider060", 0.0f)
        && setPlainValue (processor, "slider066", 0.0f)
        && setPlainValue (processor, "slider067", phaseMod21)
        && setPlainValue (processor, "slider074", 1.6f)
        && setPlainValue (processor, "slider076", 0.06f)
        && setPlainValue (processor, "slider085", 0.0f)
        && setPlainValue (processor, "slider097", 1.0f)
        && setPlainValue (processor, "slider100", 0.0f)
        && setPlainValue (processor, "slider137", static_cast<float> (unisonVoices))
        && setPlainValue (processor, "slider138", 0.65f)
        && setPlainValue (processor, "slider140", 0.0f)
        && setPlainValue (processor, "slider186", 0.0f)
        && setPlainValue (processor, "slider187", 0.0f)
        && setPlainValue (processor, "slider188", 2.0f)
        && setPlainValue (processor, "slider189", 4.0f)
        && setPlainValue (processor, "slider190", 0.0f)
        && setPlainValue (processor, "slider201", 0.125f)
        && setPlainValue (processor, "slider227", character)
        && setPlainValue (processor, "slider256", -24.0f);
    if (! configured)
        return {};

    std::vector<float> result;
    result.reserve (1024);
    for (int block = 0; block < 8; ++block)
    {
        juce::AudioBuffer<float> audio (2, 128);
        audio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110), 0);
        processor.processBlock (audio, midi);
        for (int sample = 0; sample < 128; ++sample)
            result.push_back (0.5f * (audio.getSample (0, sample)
                                    + audio.getSample (1, sample)));
    }
    return result;
}

bool bassIndustrialHiddenModulatorMatchesJsfxTopology()
{
    const auto centralPm = renderHiddenSuperWaveModulator (1, 1.0f, 0.0f);
    const auto centralDry = renderHiddenSuperWaveModulator (1, 0.0f, 0.0f);
    const auto unisonPm = renderHiddenSuperWaveModulator (3, 1.0f, 0.0f);
    const auto unisonDry = renderHiddenSuperWaveModulator (3, 0.0f, 0.0f);
    const auto characterPm = renderHiddenSuperWaveModulator (1, 1.0f, 16.0f);
    const auto characterDry = renderHiddenSuperWaveModulator (1, 0.0f, 16.0f);
    if (centralPm.empty() || centralDry.size() != centralPm.size()
        || unisonPm.size() != centralPm.size() || unisonDry.size() != centralPm.size()
        || characterPm.size() != centralPm.size() || characterDry.size() != centralPm.size())
        return false;

    double centralDifference = 0.0;
    double unisonTopologyError = 0.0;
    double characterDifference = 0.0;
    const auto unisonScale = std::sqrt (3.0);
    for (std::size_t sample = 0; sample < centralPm.size(); ++sample)
    {
        const auto centralDelta = static_cast<double> (centralPm[sample] - centralDry[sample]);
        const auto unisonDelta = static_cast<double> (unisonPm[sample] - unisonDry[sample]);
        const auto characterDelta = static_cast<double> (characterPm[sample]
                                                        - characterDry[sample]);
        centralDifference += std::abs (centralDelta);
        unisonTopologyError += std::abs (unisonDelta * unisonScale - centralDelta);
        characterDifference += std::abs (characterDelta - centralDelta);
    }

    // In the JSFX only the central voice receives cross-modulation. Therefore
    // the PM-on minus PM-off delta of three-voice unison is the one-voice delta
    // scaled by 1/sqrt(3). Character must also affect the hidden SuperWave PM
    // source even though Layer 2 itself is outside the audible balance.
    return centralDifference > 0.0001
        && unisonTopologyError / centralDifference < 0.02
        && characterDifference > centralDifference * 0.02;
}

bool pageNavigationBoundariesAreSilent()
{
    LJuno116AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::ComboBox* pages = nullptr;
    juce::ComboBox* parameters = nullptr;
    auto physicalPageButtonsFound = false;
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
    {
        auto* component = editor->getChildComponent (child);
        if (auto* combo = dynamic_cast<juce::ComboBox*> (component);
            combo != nullptr && combo->getTitle() == "Parameter page")
            pages = combo;
        if (auto* combo = dynamic_cast<juce::ComboBox*> (component);
            combo != nullptr && combo->getTitle() == "Parameter")
            parameters = combo;
        if (auto* button = dynamic_cast<juce::Button*> (component))
            physicalPageButtonsFound = physicalPageButtonsFound
                || button->getButtonText() == "Previous page"
                || button->getButtonText() == "Next page";
    }
    if (pages == nullptr || parameters == nullptr || physicalPageButtonsFound
        || pages->getExplicitFocusOrder() != 1
        || parameters->getExplicitFocusOrder() != 2)
        return false;
    const auto pageCount = pages->getNumItems();
    if (pageCount < 4
        || pages->getItemText (pageCount - 4) != "Arp"
        || pages->getItemText (pageCount - 3) != "Arp Modulation"
        || pages->getItemText (pageCount - 2) != "Arp 2"
        || pages->getItemText (pageCount - 1) != "Global")
        return false;

    return true;
}

bool editorRemembersPageAndEachGridPosition()
{
    LJuno116AudioProcessor processor;
    const auto findCombos = [] (juce::AudioProcessorEditor& editor)
    {
        std::array<juce::ComboBox*, 2> result {};
        for (int child = 0; child < editor.getNumChildComponents(); ++child)
            if (auto* combo = dynamic_cast<juce::ComboBox*> (editor.getChildComponent (child)))
            {
                if (combo->getTitle() == "Parameter page") result[0] = combo;
                if (combo->getTitle() == "Parameter") result[1] = combo;
            }
        return result;
    };
    const auto findItem = [] (juce::ComboBox& combo, const juce::String& prefix)
    {
        for (int item = 0; item < combo.getNumItems(); ++item)
            if (combo.getItemText (item).startsWith (prefix))
                return item;
        return -1;
    };

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        const auto combos = findCombos (*editor);
        if (combos[0] == nullptr || combos[1] == nullptr)
            return false;
        const auto drift = findItem (*combos[1], "Drift,");
        if (drift < 0)
            return false;
        combos[1]->setSelectedItemIndex (drift, juce::sendNotificationSync);
        combos[0]->setSelectedId (6, juce::sendNotificationSync); // LFO 1.
        const auto waveform = findItem (*combos[1], "LFO1 Wave,");
        if (waveform < 0)
            return false;
        combos[1]->setSelectedItemIndex (waveform, juce::sendNotificationSync);
        combos[0]->setSelectedId (1, juce::sendNotificationSync);
        if (combos[1]->getSelectedItemIndex() != drift)
            return false;
        combos[0]->setSelectedId (6, juce::sendNotificationSync);
        if (combos[1]->getSelectedItemIndex() != waveform)
            return false;
    }

    std::unique_ptr<juce::AudioProcessorEditor> reopened (processor.createEditor());
    const auto reopenedCombos = findCombos (*reopened);
    return reopenedCombos[0] != nullptr && reopenedCombos[1] != nullptr
        && reopenedCombos[0]->getSelectedId() == 6
        && reopenedCombos[1]->getSelectedItemIndex()
            == findItem (*reopenedCombos[1], "LFO1 Wave,");
}

std::vector<float> renderFilterRoutingCase (int source, int filter,
                                            bool destinationEnabled,
                                            bool routingEnabled)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 128;
    constexpr auto blocks = 48;
    LJuno116AudioProcessor processor;
    if (! loadInitForBenchmark (processor))
        return {};

    // Use one centred, deterministic source and disable the unrelated effects.
    const auto balance = source == 0 ? 0.0f : (source == 1 ? 1.0f : 0.5f);
    if (! setPlainValue (processor, "slider002", 1.0f)
        || ! setPlainValue (processor, "slider003", 3.0f)
        || ! setPlainValue (processor, "slider052", 3.0f)
        || ! setPlainValue (processor, "slider011", balance)
        || ! setPlainValue (processor, "slider013", 0.0f)
        || ! setPlainValue (processor, "slider014", 0.0f)
        || ! setPlainValue (processor, "slider024", 0.0f)
        || ! setPlainValue (processor, "slider049", source == 0 ? 1.0f : 0.0f)
        || ! setPlainValue (processor, "slider058", source == 1 ? 1.0f : 0.0f)
        || ! setPlainValue (processor, "slider050", source == 2 ? 1.0f : 0.0f)
        || ! setPlainValue (processor, "slider280", 1.0f)
        || ! setPlainValue (processor, "slider284", 0.0f)
        || ! setPlainValue (processor, "slider059", routingEnabled ? 1.0f : 0.0f))
        return {};

    for (const auto* id : { "slider060", "slider085", "slider100", "slider103",
                            "slider114", "slider115", "slider116", "slider117",
                            "slider119", "slider140", "slider147" })
        if (! setPlainValue (processor, id, 0.0f))
            return {};

    const std::array<std::array<const char*, 3>, 3> routeIds {{
        {{ "slider167", "slider255", "slider287" }},
        {{ "slider086", "slider099", "slider285" }},
        {{ "slider109", "slider129", "slider286" }}
    }};
    for (const auto& stage : routeIds)
        for (const auto* id : stage)
            if (! setPlainValue (processor, id, 1.0f))
                return {};
    if (! setPlainValue (processor, routeIds[static_cast<size_t> (filter)]
                                           [static_cast<size_t> (source)],
                         destinationEnabled ? 1.0f : 0.0f))
        return {};

    if (! setPlainValue (processor, "slider168", filter == 0 ? 1.0f : 0.0f)
        || ! setPlainValue (processor, "slider169", 2.35f)
        || ! setPlainValue (processor, "slider015", filter == 1 ? -0.8f : 1.0f)
        || ! setPlainValue (processor, "slider016", 0.0f)
        || ! setPlainValue (processor, "slider017", filter == 2 ? 0.7f : 0.0f)
        || ! setPlainValue (processor, "slider018", 0.0f)
        || ! setPlainValue (processor, "slider019", 0.0f)
        || ! setPlainValue (processor, "slider029", 0.0f)
        || ! setPlainValue (processor, "slider034", 0.0f)
        || ! setPlainValue (processor, "slider039", 0.0f)
        || ! setPlainValue (processor, "slider044", 0.0f))
        return {};

    processor.prepareToPlay (sampleRate, blockSize);
    std::vector<float> result;
    result.reserve (blocks * blockSize * 2);
    juce::AudioBuffer<float> audio (2, blockSize);
    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0);
        audio.clear();
        processor.processBlock (audio, midi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            result.push_back (audio.getSample (0, sample));
            result.push_back (audio.getSample (1, sample));
        }
    }
    return result;
}

double absoluteDifference (const std::vector<float>& first,
                           const std::vector<float>& second)
{
    if (first.size() != second.size() || first.empty())
        return std::numeric_limits<double>::infinity();
    auto difference = 0.0;
    for (size_t index = 0; index < first.size(); ++index)
        difference += std::abs (static_cast<double> (first[index] - second[index]));
    return difference;
}

bool filterRoutingIsIndependentAndLegacySafe()
{
    // With the master switch off, changing any new route must be a bit-exact no-op.
    for (int filter = 0; filter < 3; ++filter)
        for (int source = 0; source < 3; ++source)
            if (absoluteDifference (renderFilterRoutingCase (source, filter, false, false),
                                    renderFilterRoutingCase (source, filter, true, false)) != 0.0)
                return false;

    // With routing enabled, every filter has to affect each destination independently.
    for (int filter = 0; filter < 3; ++filter)
        for (int source = 0; source < 3; ++source)
            if (absoluteDifference (renderFilterRoutingCase (source, filter, false, true),
                                    renderFilterRoutingCase (source, filter, true, true)) < 0.1)
                return false;
    return true;
}

bool microMotionParametersAndPageAreCompatible()
{
    if (std::size (ljuno::generated::parameters) != 306
        || std::size (ljuno::generated::pages) != 15
        || juce::String (ljuno::generated::pages[3].name) != "Env"
        || juce::String (ljuno::generated::pages[4].name) != "Micro Motion"
        || juce::String (ljuno::generated::pages[5].name) != "LFO 1")
        return false;

    constexpr std::array<const char*, 10> expectedIds {
        "slider288", "slider289", "slider290", "slider291", "slider292",
        "slider293", "slider294", "slider295", "slider296", "slider297"
    };
    constexpr std::array<const char*, 10> expectedNames {
        "Motion Speed", "Pitch L1", "Pitch L2", "PWM L1", "PWM L2",
        "Pan L1", "Pan L2", "LP", "HP", "Formant"
    };
    const auto& page = ljuno::generated::pages[4];
    if (page.parameterCount != expectedIds.size())
        return false;
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
        if (juce::String (page.parameterIds[index]) != expectedIds[index]
            || juce::String (page.parameterNames[index]) != expectedNames[index])
            return false;

    for (const auto& descriptor : ljuno::generated::parameters)
    {
        if (descriptor.sliderNumber == 288
            && std::abs (descriptor.defaultValue - 0.833333f) > 0.000001f)
            return false;
        if (descriptor.sliderNumber >= 289 && descriptor.sliderNumber <= 297
            && descriptor.defaultValue != 0.0f)
            return false;
    }
    return true;
}

bool microMotionIsNeutralUntilAConfiguredDepthIsUsed()
{
    const auto render = [] (bool enabled)
    {
        LJuno116AudioProcessor processor;
        std::vector<float> result;
        if (! loadInitForBenchmark (processor)
            || ! setPlainValue (processor, "slider002", 1.0f)
            || ! setPlainValue (processor, "slider005", 0.0f)
            || ! setPlainValue (processor, "slider006", 0.0f)
            || ! setPlainValue (processor, "slider007", 1.0f)
            || ! setPlainValue (processor, "slider024", 0.0f)
            || ! setPlainValue (processor, "slider060", 0.0f)
            || ! setPlainValue (processor, "slider085", 0.0f)
            || ! setPlainValue (processor, "slider100", 0.0f)
            || ! setPlainValue (processor, "slider140", 0.0f))
            return result;
        if (enabled)
            for (const auto* id : { "slider289", "slider290", "slider291", "slider292",
                                    "slider293", "slider294", "slider295", "slider296",
                                    "slider297" })
                if (! setPlainValue (processor, id, 0.025f))
                    return std::vector<float> {};
        if (enabled && ! setPlainValue (processor, "slider168", 1.0f))
            return result;

        constexpr auto blockSize = 128;
        processor.prepareToPlay (48000.0, blockSize);
        juce::AudioBuffer<float> audio (2, blockSize);
        result.reserve (blockSize * 16 * 2);
        for (int block = 0; block < 16; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            audio.clear();
            processor.processBlock (audio, midi);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                result.push_back (audio.getSample (0, sample));
                result.push_back (audio.getSample (1, sample));
            }
        }
        return result;
    };

    const auto neutralA = render (false);
    const auto neutralB = render (false);
    const auto active = render (true);
    if (neutralA.empty() || neutralA != neutralB || neutralA.size() != active.size())
        return false;
    auto difference = 0.0;
    for (std::size_t index = 0; index < active.size(); ++index)
    {
        if (! std::isfinite (active[index]))
            return false;
        difference += std::abs (active[index] - neutralA[index]);
    }
    return difference > 0.01;
}

bool presetLibraryWorks()
{
    const auto factoryPresetCount = ljuno::PresetManager::getEmbeddedFactoryPresetCount();
    if (factoryPresetCount != 68)
        return false;

    struct ScopedTemporaryDirectory
    {
        ScopedTemporaryDirectory()
            : file (juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("LJunoPresetTest-" + juce::Uuid().toString()))
        {
            file.createDirectory();
        }
        ~ScopedTemporaryDirectory() { file.deleteRecursively(); }
        juce::File file;
    } temporary;
    const auto root = temporary.file.getChildFile ("LJuno-116");
    LJuno116AudioProcessor processor;
    ljuno::PresetManager manager (processor.parameters, root);
    if (manager.ensureLibraryExists().failed())
        return false;
    const auto factoryFiles = manager.allPresetFiles();
    if (factoryFiles.size() != static_cast<size_t> (factoryPresetCount)
        || factoryFiles.front().getFileName() != "001 - Default.Ljuno"
        || factoryFiles[7].getFileName() != "008 - Lead Wandering in the Galaxy.Ljuno"
        || factoryFiles[8].getFileName() != "009 - Vocal Lead.Ljuno"
        || factoryFiles[9].getFileName() != "010 - Fantasy Piano.Ljuno")
        return false;

    juce::String loadedName;
    auto loadedNumber = 0;
    if (manager.loadRelativePreset (1, loadedName, loadedNumber).failed()
        || loadedName != "Default" || loadedNumber != 1)
        return false;

    const auto defaultPreset = root.getChildFile ("Factory").getChildFile ("001 - Default.Ljuno");
    if (manager.loadPreset (defaultPreset, loadedName, loadedNumber).failed()
        || loadedName != "Default" || loadedNumber <= 0)
        return false;
    const auto valueIs = [&processor] (const char* id, float expected)
    {
        const auto* value = processor.parameters.getRawParameterValue (id);
        return value != nullptr && std::abs (value->load() - expected) < 0.0005f;
    };
    if (! valueIs ("slider001", 0.0f)
        || ! valueIs ("slider003", 3.0f)
        || ! valueIs ("slider051", 0.5f)
        || ! valueIs ("slider284", 0.0f)
        || ! valueIs ("slider064", 0.0f)
        || ! valueIs ("slider080", -11.0f)
        || ! valueIs ("slider256", 0.0f)
        || ! valueIs ("slider288", 0.833333f)
        || ! valueIs ("slider289", 0.0f)
        || ! valueIs ("slider290", 0.0f)
        || ! valueIs ("slider291", 0.0f)
        || ! valueIs ("slider292", 0.0f)
        || ! valueIs ("slider293", 0.0f)
        || ! valueIs ("slider294", 0.0f)
        || ! valueIs ("slider295", 0.0f)
        || ! valueIs ("slider296", 0.0f)
        || ! valueIs ("slider297", 0.0f))
        return false;

    for (const auto* stereoNoiseName : { "Stefano percussione reverb",
                                         "Stefano cassa con reverb",
                                         "Stefano sparo", "Stefano Claps" })
    {
        const auto preset = std::find_if (factoryFiles.begin(), factoryFiles.end(),
            [stereoNoiseName] (const juce::File& file)
            {
                return file.getFileNameWithoutExtension().endsWithIgnoreCase (stereoNoiseName);
            });
        if (preset == factoryFiles.end()
            || manager.loadPreset (*preset, loadedName, loadedNumber).failed()
            || ! valueIs ("slider284", 1.0f))
            return false;
    }

    const auto industrial = std::find_if (factoryFiles.begin(), factoryFiles.end(),
        [] (const juce::File& file)
        {
            return file.getFileNameWithoutExtension().endsWithIgnoreCase ("Bass Industrial");
        });
    if (industrial == factoryFiles.end()
        || manager.loadPreset (*industrial, loadedName, loadedNumber).failed()
        || loadedName != "Bass Industrial"
        || ! valueIs ("slider003", 4.0f)
        || ! valueIs ("slider011", 0.0f)
        || ! valueIs ("slider052", 5.0f)
        || ! valueIs ("slider067", 1.0f)
        || ! valueIs ("slider074", 1.6f)
        || ! valueIs ("slider137", 3.0f)
        || ! valueIs ("slider201", 0.125f))
        return false;

    if (! setPlainValue (processor, "slider015", -0.37f)
        || ! setPlainValue (processor, "slider144", 7.4f))
        return false;
    juce::File saved;
    if (manager.savePreset ("My test preset.txt", root, saved).failed()
        || saved.getFileName() != "My test preset.Ljuno"
        || ! manager.isValidPresetFile (saved))
        return false;

    juce::File deletable;
    if (manager.savePreset ("Delete me", root, deletable).failed()
        || manager.getCurrentPresetFile() != deletable
        || manager.deletePreset (deletable).failed()
        || deletable.existsAsFile()
        || manager.getCurrentPresetFile().existsAsFile()
        || manager.deletePreset (deletable).wasOk())
        return false;
    const auto wrongExtension = root.getChildFile ("Wrong extension.txt");
    if (! wrongExtension.replaceWithText (saved.loadFileAsString())
        || manager.isValidPresetFile (wrongExtension)
        || manager.allPresetFiles().size() != static_cast<size_t> (factoryPresetCount + 1))
        return false;
    if (! setPlainValue (processor, "slider015", 1.0f)
        || ! setPlainValue (processor, "slider144", 0.2f)
        || manager.loadPreset (saved, loadedName, loadedNumber).failed())
        return false;
    if (! valueIs ("slider015", -0.37f) || ! valueIs ("slider144", 7.4f))
        return false;

    // Existing names require an explicit overwrite request. The safe/default
    // path must preserve the old file, while the confirmed path replaces it.
    juce::File overwriteFile;
    if (! setPlainValue (processor, "slider015", -0.21f)
        || manager.savePreset ("Overwrite test", root, overwriteFile).failed()
        || ! setPlainValue (processor, "slider015", 0.64f))
        return false;
    juce::File refusedFile;
    if (manager.savePreset ("Overwrite test", root, refusedFile).wasOk()
        || refusedFile != overwriteFile
        || manager.loadPreset (overwriteFile, loadedName, loadedNumber).failed()
        || ! valueIs ("slider015", -0.21f))
        return false;
    if (! setPlainValue (processor, "slider015", 0.64f)
        || manager.savePreset ("Overwrite test", root, overwriteFile, true).failed()
        || manager.loadPreset (overwriteFile, loadedName, loadedNumber).failed()
        || ! valueIs ("slider015", 0.64f)
        || manager.loadPreset (saved, loadedName, loadedNumber).failed()
        || ! valueIs ("slider015", -0.37f) || ! valueIs ("slider144", 7.4f)
        || ! overwriteFile.deleteFile())
        return false;

    // The browser previews presets transactionally. Cancelling must restore an
    // arbitrary unsaved patch and its previous preset position exactly.
    const auto originalPatch = manager.capturePatchSnapshot();
    if (! originalPatch.isValid()
        || manager.previewPreset (*industrial).failed()
        || ! valueIs ("slider003", 4.0f))
        return false;
    manager.restorePatchSnapshot (originalPatch);
    if (! valueIs ("slider015", -0.37f) || ! valueIs ("slider144", 7.4f)
        || manager.loadRelativePreset (1, loadedName, loadedNumber).failed()
        || loadedName != "Default" || loadedNumber != 1)
        return false;

    // Enter commits only the already-sounding preview. A value changed after
    // previewing must survive the commit, proving the file was not loaded twice.
    if (manager.previewPreset (*industrial).failed()
        || ! setPlainValue (processor, "slider015", -0.12f)
        || manager.commitPresetPreview (*industrial).failed()
        || ! valueIs ("slider015", -0.12f))
        return false;
    const auto filesAfterSave = manager.allPresetFiles();
    const auto committed = std::find (filesAfterSave.begin(), filesAfterSave.end(), *industrial);
    if (committed == filesAfterSave.end())
        return false;
    const auto expectedNextNumber = (static_cast<int> (
        std::distance (filesAfterSave.begin(), committed)) + 1)
        % static_cast<int> (filesAfterSave.size()) + 1;
    if (manager.loadRelativePreset (1, loadedName, loadedNumber).failed()
        || loadedNumber != expectedNextNumber)
        return false;

    const auto category = root.getChildFile ("User category");
    if (category.createDirectory().failed())
        return false;
    const auto entries = manager.listDirectory (root);
    if (entries.empty() || ! entries.front().isDirectory
        || entries.front().name != "Factory")
        return false;

    // Simulate an installation carrying a previous import marker and the old
    // numbering. Version 5 must remove the obsolete managed filename and
    // install Vocal Lead at number 9 without touching folders outside Factory.
    const auto legacyRoot = temporary.file.getChildFile ("Legacy LJuno-116");
    const auto legacyFactory = legacyRoot.getChildFile ("Factory");
    if (legacyFactory.createDirectory().failed()
        || ! legacyRoot.getChildFile (".factory-rpl-3-ljuno-installed")
                .replaceWithText ("old marker\n"))
        return false;
    const auto obsoleteFantasy = legacyFactory.getChildFile ("009 - Fantasy Piano.Ljuno");
    if (! obsoleteFantasy.replaceWithText ("old factory file\n"))
        return false;
    LJuno116AudioProcessor legacyProcessor (legacyRoot);
    ljuno::PresetManager legacyManager (legacyProcessor.parameters, legacyRoot);
    if (legacyManager.ensureLibraryExists().failed() || obsoleteFantasy.existsAsFile())
        return false;
    const auto upgradedFiles = legacyManager.allPresetFiles();
    return upgradedFiles.size() == static_cast<size_t> (factoryPresetCount)
        && upgradedFiles[8].getFileName() == "009 - Vocal Lead.Ljuno"
        && upgradedFiles[9].getFileName() == "010 - Fantasy Piano.Ljuno";
}

bool presetButtonsExist()
{
    LJuno116AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::StringArray required {
        "Previous preset", "Next preset", "Browser", "Save preset", "Help"
    };
    juce::StringArray overwriteRequired { "Yes", "No", "Close" };
    auto helpShortcutIsExposed = false;
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
        if (const auto* button = dynamic_cast<juce::Button*> (editor->getChildComponent (child)))
        {
            required.removeString (button->getButtonText());
            overwriteRequired.removeString (button->getButtonText());
            if (button->getButtonText() == "Help")
                helpShortcutIsExposed = button->getDescription().containsIgnoreCase ("Alt+H");
        }

    int helpSize = 0;
    const auto* helpData = BinaryData::getNamedResource ("LJuno116Help_html", helpSize);
    const auto helpHtml = helpData != nullptr && helpSize > 0
        ? juce::String::fromUTF8 (helpData, helpSize) : juce::String();
    const auto languagesAreEmbedded = helpHtml.contains ("id=\"en\"")
        && helpHtml.contains ("id=\"it\"") && helpHtml.contains ("id=\"es\"")
        && helpHtml.contains ("id=\"pt\"") && helpHtml.contains ("id=\"fr\"")
        && helpHtml.contains ("id=\"ru\"") && helpHtml.contains ("id=\"zh\"")
        && helpHtml.contains ("id=\"ja\"")
        && helpHtml.indexOf ("id=\"es\"") < helpHtml.indexOf ("id=\"pt\"")
        && helpHtml.indexOf ("id=\"pt\"") < helpHtml.indexOf ("id=\"fr\"")
        && helpHtml.indexOf ("id=\"zh\"") < helpHtml.indexOf ("id=\"ja\"")
        && helpHtml.contains ("Filter Routing");
    return required.isEmpty() && overwriteRequired.isEmpty()
        && helpShortcutIsExposed && languagesAreEmbedded;
}

bool editorFocusAndPresetBrowserAreAccessible()
{
    LJuno116AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    // A headless editor has no native window, so getAccessibilityHandler()
    // intentionally returns null. Construct the declared handler directly to
    // verify the root role without depending on a desktop peer.
    auto editorHandler = editor->createAccessibilityHandler();
    if (! editor->getWantsKeyboardFocus()
        || editor->getTitle().isNotEmpty()
        || editor->getDescription().isNotEmpty()
        || editorHandler == nullptr
        || ! editorHandler->isIgnored())
        return false;

    auto browserIsAccessible = false;
    auto browserPreviewIsExposed = false;
    auto browserCancelIsExposed = false;
    auto browserDeletionIsExposed = false;
    auto valueShortcutIsExposed = false;
    auto parameterPageIsExposed = false;
    auto luaDisplayNameIsExposed = false;
    auto oscillatorGroupsAreOrdered = false;
    auto enterFocusHintsAreExposed = false;
    juce::ComboBox* parameterCombo = nullptr;
    juce::Slider* valueSlider = nullptr;
    juce::Label* editorStatus = nullptr;
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
    {
        if (const auto* combo = dynamic_cast<juce::ComboBox*> (editor->getChildComponent (child));
            combo != nullptr && combo->getTitle() == "Parameter page")
            parameterPageIsExposed = combo->isAccessible()
                && combo->getDescription().containsIgnoreCase ("Alt+D");

        if (const auto* combo = dynamic_cast<juce::ComboBox*> (editor->getChildComponent (child));
            combo != nullptr && combo->getTitle() == "Parameter")
        {
            parameterCombo = const_cast<juce::ComboBox*> (combo);
            for (int item = 0; item < combo->getNumItems(); ++item)
                if (combo->getItemText (item).startsWith ("Balance,"))
                    luaDisplayNameIsExposed = ! combo->getItemText (item).contains ("Balance L1 L2");
            const juce::StringArray expected {
                "Wave L1,", "Morph L1,", "Wave L2,", "Morph L2,",
                "Noise Level,", "Noise Type,", "Noise Pitch,", "Noise Color,", "Noise Stereo,"
            };
            const int positions[] { 2, 3, 4, 5, 9, 10, 11, 12, 13 };
            oscillatorGroupsAreOrdered = true;
            for (int index = 0; index < expected.size(); ++index)
                oscillatorGroupsAreOrdered = oscillatorGroupsAreOrdered
                    && combo->getItemText (positions[index]).startsWith (expected[index]);
            enterFocusHintsAreExposed = combo->getDescription().containsIgnoreCase (
                "Enter for Value")
                && combo->getDescription().containsIgnoreCase ("Backspace to reset");
        }

        if (const auto* list = dynamic_cast<juce::ListBox*> (editor->getChildComponent (child));
            list != nullptr && list->getTitle() == "Preset browser")
        {
            browserIsAccessible = list->isAccessible() && list->getWantsKeyboardFocus()
                && list->getDescription().containsIgnoreCase ("Alt C");
            browserPreviewIsExposed = list->getDescription().containsIgnoreCase ("preview");
            browserDeletionIsExposed = list->getDescription().containsIgnoreCase ("Delete");
        }

        if (const auto* button = dynamic_cast<juce::Button*> (editor->getChildComponent (child));
            button != nullptr && button->getButtonText() == "Close")
            browserCancelIsExposed = browserCancelIsExposed
                || button->getDescription().containsIgnoreCase ("restore");

        if (const auto* slider = dynamic_cast<juce::Slider*> (editor->getChildComponent (child));
            slider != nullptr && slider->getTitle().isNotEmpty())
        {
            valueSlider = const_cast<juce::Slider*> (slider);
            valueShortcutIsExposed = slider->getDescription().containsIgnoreCase ("Alt+V");
            enterFocusHintsAreExposed = enterFocusHintsAreExposed
                && slider->getDescription().containsIgnoreCase ("Enter returns")
                && slider->getDescription().containsIgnoreCase ("Backspace resets");
        }
        if (auto* label = dynamic_cast<juce::Label*> (editor->getChildComponent (child));
            label != nullptr && label->getTitle() == "Migration status")
            editorStatus = label;
    }
    if (parameterCombo == nullptr || valueSlider == nullptr || editorStatus == nullptr)
        return false;

    // Exercise the real TextEditor created by Alt+E. It accepts only decimal
    // digits and a point, announces Backspace, and still exposes Alt+letters.
    valueSlider->showTextBox();
    std::function<juce::TextEditor* (juce::Component&)> findTextEditor =
        [&findTextEditor] (juce::Component& component) -> juce::TextEditor*
        {
            if (auto* text = dynamic_cast<juce::TextEditor*> (&component))
                return text;
            for (int child = 0; child < component.getNumChildComponents(); ++child)
                if (auto* found = findTextEditor (*component.getChildComponent (child)))
                    return found;
            return nullptr;
        };
    const auto* inputGain = processor.parameters.getRawParameterValue ("slider001");
    auto* valueTextEditor = findTextEditor (*valueSlider);
    if (valueTextEditor == nullptr || inputGain == nullptr)
    {
        std::cerr << "editor shortcut detail: text-editor=" << (valueTextEditor != nullptr)
                  << " input-gain=" << (inputGain != nullptr) << '\n';
        return false;
    }
    const auto alt = juce::ModifierKeys (juce::ModifierKeys::altModifier);
    const auto textBefore = valueTextEditor->getText();
    const auto valueBefore = inputGain->load();
    const auto altUpHandled = valueTextEditor->keyPressed (
        juce::KeyPress (juce::KeyPress::upKey, alt, 0));
    const auto altUpChanged = inputGain->load() > valueBefore
                           && valueTextEditor->getText() != textBefore;
    const auto valueOnlyAnnouncement = editorStatus->getText()
        == processor.parameters.getParameter ("slider001")->getCurrentValueAsText();

    // Updating the attached slider may legitimately rebuild JUCE's temporary
    // label editor, so reacquire it before exercising manual text entry.
    valueSlider->hideTextBox (false);
    valueSlider->showTextBox();
    valueTextEditor = findTextEditor (*valueSlider);
    if (valueTextEditor == nullptr)
        return false;
    valueTextEditor->setText ({}, false);
    valueTextEditor->keyPressed (juce::KeyPress ('1', {}, '1'));
    valueTextEditor->keyPressed (juce::KeyPress ('2', {}, '2'));
    valueTextEditor->keyPressed (juce::KeyPress ('.', {}, '.'));
    valueTextEditor->keyPressed (juce::KeyPress ('5', {}, '5'));
    valueTextEditor->keyPressed (juce::KeyPress ('q', {}, 'q'));
    const auto numericFilterWorks = valueTextEditor->getText() == "12.5";
    const auto typedCharacterAnnounced = editorStatus->getText() == "5";
    valueTextEditor->keyPressed (juce::KeyPress (juce::KeyPress::backspaceKey));
    const auto backspaceAnnounced = editorStatus->getText() == "5"
                                 && valueTextEditor->getText() == "12.";
    valueTextEditor->keyPressed (juce::KeyPress (juce::KeyPress::backspaceKey));
    const auto punctuationIsLanguageNeutral = editorStatus->getText() == "."
                                           && valueTextEditor->getText() == "12";

    valueSlider->hideTextBox (false);
    valueSlider->showTextBox();
    valueTextEditor = findTextEditor (*valueSlider);
    if (valueTextEditor == nullptr)
        return false;
    const auto lHandled = valueTextEditor->keyPressed (juce::KeyPress ('l', alt, 'l'));

    valueSlider->hideTextBox (false);
    valueSlider->showTextBox();
    valueTextEditor = findTextEditor (*valueSlider);
    if (valueTextEditor == nullptr)
        return false;
    const auto enterHandled = valueTextEditor->keyPressed (
        juce::KeyPress (juce::KeyPress::returnKey));
    valueSlider->hideTextBox (true);
    if (! altUpHandled || ! altUpChanged || ! valueOnlyAnnouncement
        || ! numericFilterWorks || ! typedCharacterAnnounced || ! backspaceAnnounced
        || ! punctuationIsLanguageNeutral || ! lHandled || ! enterHandled)
    {
        std::cerr << "editor shortcut detail: alt-up=" << altUpHandled
                  << " changed=" << altUpChanged
                  << " value-only=" << valueOnlyAnnouncement
                  << " numeric=" << numericFilterWorks
                  << " typed=" << typedCharacterAnnounced
                  << " backspace=" << backspaceAnnounced
                  << " punctuation=" << punctuationIsLanguageNeutral
                  << " l=" << lHandled << " enter=" << enterHandled
                  << " before=" << valueBefore
                  << " after=" << inputGain->load() << '\n';
        return false;
    }

    const auto* balance = processor.parameters.getParameter ("slider011");
    return parameterPageIsExposed && browserIsAccessible && browserPreviewIsExposed
        && browserCancelIsExposed && browserDeletionIsExposed
        && valueShortcutIsExposed && luaDisplayNameIsExposed
        && oscillatorGroupsAreOrdered && enterFocusHintsAreExposed
        && balance != nullptr && balance->getName (100) == "Balance";
}

bool presetBrowserRemembersPosition()
{
    struct ScopedTemporaryDirectory
    {
        ScopedTemporaryDirectory()
            : file (juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("LJunoBrowserTest-" + juce::Uuid().toString()))
        {
            file.createDirectory();
        }
        ~ScopedTemporaryDirectory() { file.deleteRecursively(); }
        juce::File file;
    } temporary;
    LJuno116AudioProcessor processor (temporary.file.getChildFile ("LJuno-116"));
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::Button* load = nullptr;
    juce::Button* save = nullptr;
    juce::Button* close = nullptr;
    juce::ListBox* browser = nullptr;
    juce::Label* path = nullptr;
    juce::TextEditor* saveName = nullptr;
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
    {
        auto* component = editor->getChildComponent (child);
        if (auto* button = dynamic_cast<juce::Button*> (component))
        {
            if (button->getButtonText() == "Browser")
                load = button;
            if (button->getButtonText() == "Save preset")
                save = button;
            if (button->getButtonText() == "Close"
                && button->getDescription().containsIgnoreCase ("restore"))
                close = button;
        }
        if (auto* list = dynamic_cast<juce::ListBox*> (component);
            list != nullptr && list->getTitle() == "Preset browser")
            browser = list;
        if (auto* label = dynamic_cast<juce::Label*> (component);
            label != nullptr && label->getTitle() == "Preset browser folder")
            path = label;
        if (auto* text = dynamic_cast<juce::TextEditor*> (component);
            text != nullptr && text->getTitle() == "Preset name")
            saveName = text;
    }
    if (load == nullptr || save == nullptr || close == nullptr || browser == nullptr
        || path == nullptr || saveName == nullptr
        || ! load->onClick || ! save->onClick || ! close->onClick)
        return false;

    // Alt+B and Browser use one toggle path. Repeating it must cancel and restore
    // browser state rather than trying to focus an already-open overlay.
    load->onClick();
    if (! browser->isVisible() || load->isEnabled())
        return false;
    load->onClick();
    if (browser->isVisible() || ! load->isEnabled())
        return false;

    load->onClick();
    auto* model = browser->getListBoxModel();
    if (model == nullptr || model->getNumRows() <= 0)
        return false;
    model->returnKeyPressed (0); // Factory category.
    if (! path->getText().containsIgnoreCase ("Factory") || model->getNumRows() < 2)
        return false;
    const auto rememberedRow = juce::jmin (12, model->getNumRows() - 1);
    browser->selectRow (rememberedRow);
    close->onClick();

    load->onClick();
    const auto restored = path->getText().containsIgnoreCase ("Factory")
                       && browser->getSelectedRow() == rememberedRow;
    close->onClick();
    if (! restored)
        return false;

    juce::String loadedName;
    auto loadedNumber = 0;
    if (processor.presetManager.loadRelativePreset (1, loadedName, loadedNumber).failed())
        return false;
    const auto currentFile = processor.presetManager.getCurrentPresetFile();
    save->onClick();
    const auto highlighted = saveName->getHighlightedRegion();
    return currentFile.existsAsFile()
        && saveName->getText() == currentFile.getFileNameWithoutExtension()
        && highlighted.getStart() == 0
        && highlighted.getEnd() == saveName->getText().length();
}

bool firstLArpRoutesSynthAndMidi()
{
    struct Result
    {
        double audioEnergy = 0.0;
        int noteOns = 0, noteOffs = 0;
        std::set<int> notes;
    };

    const auto run = [] (int mode)
    {
        LJuno116AudioProcessor processor;
        for (const auto& setting : std::array<std::pair<const char*, float>, 17> {{
                 { "slider002", 8.0f }, { "slider003", 3.0f },
                 { "slider005", 0.0f }, { "slider006", 0.0f },
                 { "slider007", 1.0f }, { "slider008", 0.01f },
                 { "slider060", 0.0f }, { "slider085", 0.0f },
                 { "slider100", 0.0f }, { "slider140", 0.0f },
                 { "slider202", static_cast<float> (mode) },
                 { "slider203", 32.0f }, { "slider204", 0.0f },
                 { "slider207", 0.5f }, { "slider215", 1.0f },
                 { "slider246", 1.0f },
                 { "slider392", mode == 2 ? 0.0f : 2.0f }
             }})
            if (! setPlainValue (processor, setting.first, setting.second))
                return Result {};

        processor.prepareToPlay (48000.0, 128);
        Result result;
        for (int block = 0; block < 150; ++block)
        {
            juce::AudioBuffer<float> audio (2, 128);
            audio.clear();
            juce::MidiBuffer midi;
            if (block == 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 105), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
            }
            if (block == 100)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
                midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            }
            processor.processBlock (audio, midi);
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    result.audioEnergy += std::abs (audio.getSample (channel, sample));
            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();
                if (message.isNoteOn())
                {
                    ++result.noteOns;
                    result.notes.insert (message.getNoteNumber());
                }
                else if (message.isNoteOff())
                    ++result.noteOffs;
            }
        }
        return result;
    };

    const auto synthAndMidi = run (0);
    const auto synthOnly = run (1);
    const auto midiOnly = run (2);
    return synthAndMidi.audioEnergy > 1.0
        && synthAndMidi.noteOns >= 5 && synthAndMidi.noteOffs >= 5
        && synthAndMidi.notes.count (60) != 0
        && synthAndMidi.notes.count (64) != 0
        && synthAndMidi.notes.count (67) != 0
        && synthOnly.audioEnergy > 1.0
        && synthOnly.noteOns == 3 && synthOnly.noteOffs == 3
        && synthOnly.notes == std::set<int> ({ 60, 64, 67 })
        && std::abs (synthOnly.audioEnergy - synthAndMidi.audioEnergy) < 0.001
        && midiOnly.audioEnergy > 1.0
        && midiOnly.noteOns >= 5 && midiOnly.noteOffs >= 5
        && midiOnly.notes == synthAndMidi.notes;
}

bool c64VisualStyleIsActive()
{
    LJuno116AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const auto darkBlue = juce::Colour::fromRGB (0x50, 0x45, 0x9b);
    const auto lightBlue = juce::Colour::fromRGB (0x88, 0x7e, 0xcb);
    auto bootTitleFound = false;
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
        if (const auto* label = dynamic_cast<juce::Label*> (editor->getChildComponent (child));
            label != nullptr && label->getText().contains ("LJUNO-116 SYNTHESIZER"))
            bootTitleFound = ! label->isAccessible();

    return bootTitleFound
        && editor->findColour (juce::ComboBox::backgroundColourId) == darkBlue
        && editor->findColour (juce::ComboBox::textColourId) == lightBlue
        && editor->findColour (juce::ListBox::backgroundColourId) == darkBlue
        && editor->findColour (juce::ListBox::textColourId) == lightBlue;
}

bool renderEditorSnapshot (const juce::File& destination)
{
    LJuno116AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    if (! image.isValid())
        return false;
    if (auto stream = destination.createOutputStream())
        return juce::PNGImageFormat().writeImageToStream (image, *stream);
    return false;
}

}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (argc == 3 && juce::String (argv[1]) == "--render-ui")
        return renderEditorSnapshot (juce::File (argv[2])) ? 0 : 1;
    if (argc == 4 && juce::String (argv[1]) == "--benchmark-patch")
        return benchmarkPatch (juce::File (argv[2]), juce::String (argv[3]).getIntValue()) ? 0 : 1;
    if (argc == 5 && juce::String (argv[1]) == "--benchmark-patch")
        return benchmarkPatch (juce::File (argv[2]), juce::String (argv[3]).getIntValue(),
                               juce::String (argv[4])) ? 0 : 1;
    if ((argc == 3 || argc == 4) && juce::String (argv[1]) == "--benchmark-init")
        return benchmarkInit (juce::String (argv[2]).getIntValue(),
                              argc == 3 || juce::String (argv[3]) != "no-comp") ? 0 : 1;
    if (argc == 5 && juce::String (argv[1]) == "--render-patch")
        return renderPatchReference (juce::File (argv[2]),
                                     juce::String (argv[3]).getIntValue(),
                                     juce::File (argv[4])) ? 0 : 1;

    const auto defaultPatchPassed = renderScenario (0);
    const auto complexPatchPassed = renderScenario (1);
    const auto superWavePatchPassed = renderScenario (2);
    const auto monoPatchPassed = renderScenario (3);
    const auto pitchArp2Passed = renderScenario (4);
    const auto pitchArp2LayerRoutingPassed =
        pitchArp2RunsBothLayersWithIndependentGlideAndRelease();
    const auto lfoSquashPassed = lfoUpperAndLowerSquashAreIndependent();
    const auto noiseTypesPassed = allNoiseTypesAreAudibleAndDistinct();
    const auto morphControlsPassed = morphControlsAreLinked();
    const auto luaInitPassed = luaInitValuesAreMapped();
    const auto initializeButtonPassed = initializeButtonWorks();
    const auto stereoInputPassed = stereoInputPassesThroughDry();
    const auto effectsPassed = effectsChangeTheSynthSignal();
    const auto reverbPassed = reverbProducesAStableTail();
    const auto sidechainPassed = sidechainDucksPostCompressor();
    const auto deepIdlePassed = deepIdleSleepsAndWakes();
    const auto monoUnisonPassed = monoUnisonUsesFreePhasesAndStaysCentred();
    const auto polyModesPassed = modernAndRolandPolyphonyAreDistinctAndStable();
    const auto bassIndustrialPassed = bassIndustrialHiddenModulatorMatchesJsfxTopology();
    const auto pageNavigationPassed = pageNavigationBoundariesAreSilent();
    const auto editorPositionMemoryPassed = editorRemembersPageAndEachGridPosition();
    const auto filterRoutingPassed = filterRoutingIsIndependentAndLegacySafe();
    const auto microMotionCatalogPassed = microMotionParametersAndPageAreCompatible();
    const auto microMotionDspPassed = microMotionIsNeutralUntilAConfiguredDepthIsUsed();
    const auto presetLibraryPassed = presetLibraryWorks();
    const auto presetButtonsPassed = presetButtonsExist();
    const auto editorFocusPassed = editorFocusAndPresetBrowserAreAccessible();
    const auto browserPositionPassed = presetBrowserRemembersPosition();
    const auto firstLArpPassed = firstLArpRoutesSynthAndMidi();
    const auto c64VisualPassed = c64VisualStyleIsActive();
    std::cout << "default=" << (defaultPatchPassed ? "pass" : "fail")
              << " complex=" << (complexPatchPassed ? "pass" : "fail")
              << " superwave=" << (superWavePatchPassed ? "pass" : "fail")
              << " mono-unison-split=" << (monoPatchPassed ? "pass" : "fail")
              << " pitch-arp2=" << (pitchArp2Passed ? "pass" : "fail")
              << " pitch-arp2-layers-glide-release="
              << (pitchArp2LayerRoutingPassed ? "pass" : "fail")
              << " lfo-upper-lower-squash=" << (lfoSquashPassed ? "pass" : "fail")
              << " noise-types=" << (noiseTypesPassed ? "pass" : "fail")
              << " morph-controls=" << (morphControlsPassed ? "pass" : "fail")
              << " lua-init=" << (luaInitPassed ? "pass" : "fail")
              << " init-button=" << (initializeButtonPassed ? "pass" : "fail")
              << " stereo-input=" << (stereoInputPassed ? "pass" : "fail")
              << " chorus-delay-eq=" << (effectsPassed ? "pass" : "fail")
              << " reverb=" << (reverbPassed ? "pass" : "fail")
              << " sidechain-comp=" << (sidechainPassed ? "pass" : "fail")
              << " deep-idle=" << (deepIdlePassed ? "pass" : "fail")
              << " mono-unison-phase-pan=" << (monoUnisonPassed ? "pass" : "fail")
              << " modern-roland-poly=" << (polyModesPassed ? "pass" : "fail")
              << " bass-industrial-hidden-pm=" << (bassIndustrialPassed ? "pass" : "fail")
              << " page-nav=" << (pageNavigationPassed ? "pass" : "fail")
              << " editor-position-memory=" << (editorPositionMemoryPassed ? "pass" : "fail")
              << " filter-routing=" << (filterRoutingPassed ? "pass" : "fail")
              << " micro-motion-catalog=" << (microMotionCatalogPassed ? "pass" : "fail")
              << " micro-motion-dsp=" << (microMotionDspPassed ? "pass" : "fail")
              << " preset-library=" << (presetLibraryPassed ? "pass" : "fail")
              << " preset-buttons=" << (presetButtonsPassed ? "pass" : "fail")
              << " editor-focus-browser-a11y=" << (editorFocusPassed ? "pass" : "fail")
              << " browser-position=" << (browserPositionPassed ? "pass" : "fail")
              << " first-larp-routing=" << (firstLArpPassed ? "pass" : "fail")
              << " c64-ui=" << (c64VisualPassed ? "pass" : "fail") << '\n';
    return defaultPatchPassed && complexPatchPassed && superWavePatchPassed
        && monoPatchPassed && pitchArp2Passed && pitchArp2LayerRoutingPassed
        && lfoSquashPassed && noiseTypesPassed && morphControlsPassed && luaInitPassed
        && initializeButtonPassed && stereoInputPassed && effectsPassed
        && reverbPassed && sidechainPassed && deepIdlePassed && monoUnisonPassed
        && polyModesPassed
        && bassIndustrialPassed
        && pageNavigationPassed
        && editorPositionMemoryPassed
        && filterRoutingPassed
        && microMotionCatalogPassed
        && microMotionDspPassed
        && presetLibraryPassed && presetButtonsPassed && editorFocusPassed
        && browserPositionPassed
        && firstLArpPassed
        && c64VisualPassed ? 0 : 1;
}
