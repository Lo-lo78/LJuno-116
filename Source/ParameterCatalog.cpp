// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ParameterCatalog.h"
#include "GeneratedParameters.h"

namespace ljuno
{
namespace
{
juce::StringArray splitChoices (const char* encoded)
{
    return juce::StringArray::fromTokens (encoded, "|", "");
}
}

juce::String valueToText (int sliderNumber, float plainValue)
{
    for (const auto& parameter : generated::parameters)
    {
        if (parameter.sliderNumber != sliderNumber)
            continue;

        const auto choices = splitChoices (parameter.choices);
        if (! choices.isEmpty())
            return choices[juce::jlimit (0, choices.size() - 1,
                                        juce::roundToInt (plainValue - parameter.minimum))];

        const auto decimals = parameter.step >= 1.0f ? 0
                            : parameter.step >= 0.1f ? 1
                            : parameter.step >= 0.01f ? 2 : 4;
        return juce::String (plainValue, decimals);
    }

    return juce::String (plainValue);
}

float initialPatchValue (int sliderNumber, float declarationDefault)
{
    // INIT.SYNTH and INIT.ARP in the accessible Lua cover the whole public
    // parameter set. Most values match the JSFX declaration defaults; these
    // are the deliberate exceptions in that Init patch.
    switch (sliderNumber)
    {
        case 8:   return 0.0003f; // Release
        case 11:  return 0.0f;    // Balance L1 L2
        case 13:  return 0.0f;    // Pan L1
        case 14:  return 0.0f;    // Pan L2
        case 60:  return 0.0f;    // Chorus Level
        case 87:  return 0.003f;  // ADSR2 Attack
        case 90:  return 0.0003f; // ADSR2 Release
        case 94:  return 20.0f;   // LFO2 Aftertouch Amount
        case 100: return 0.0f;    // Delay
        default:  return declarationDefault;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& parameter : generated::parameters)
    {
        const auto id = juce::ParameterID { parameter.id, 1 };
        const auto choices = splitChoices (parameter.choices);

        if (! choices.isEmpty())
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                id, parameter.displayName, choices,
                juce::roundToInt (parameter.defaultValue - parameter.minimum)));
            continue;
        }

        auto range = juce::NormalisableRange<float> (
            parameter.minimum, parameter.maximum, parameter.step);
        const auto sliderNumber = parameter.sliderNumber;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id, parameter.displayName, range, parameter.defaultValue,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([sliderNumber] (float value, int)
                {
                    return valueToText (sliderNumber, value);
                })
                .withValueFromStringFunction ([] (const juce::String& text)
                {
                    return text.getFloatValue();
                })));
    }

    return layout;
}
}
