// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ljuno
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
juce::String valueToText (int sliderNumber, float plainValue);
float initialPatchValue (int sliderNumber, float declarationDefault);
}
