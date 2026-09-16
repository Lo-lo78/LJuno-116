// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterCatalog.h"
#include "PresetManager.h"
#include "SynthEngine.h"

class LJuno116AudioProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit LJuno116AudioProcessor (juce::File presetLibraryRoot = {});
    ~LJuno116AudioProcessor() override;

    void prepareToPlay (double sampleRate, int maximumBlockSize) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 32.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    bool isEngineDeepIdle() const noexcept { return synthEngine.isDeepIdle(); }

    juce::AudioProcessorValueTreeState parameters;
    ljuno::PresetManager presetManager;

private:
    void parameterChanged (const juce::String&, float) override;

    ljuno::SynthEngine synthEngine;
    std::atomic_bool synchronisingMorph { false };
    std::atomic<std::uint64_t> parameterRevision { 0 };
    std::uint64_t lastProcessedParameterRevision = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LJuno116AudioProcessor)
};
