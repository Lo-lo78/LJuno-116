# LJuno-116 0.99.54 — Release notes

**Release date:** 14 October 2026  
**Platform:** Windows VST3  
**License:** GNU AGPL-3.0-or-later

## Changes since the 0.99.53 public announcement

- **New Degrade FX:** four independent, host-automatable parameters: Amount, Bits, Hold and Jitter. Amount at zero bypasses the effect; the direct stereo input remains unprocessed.
- **Better Sequencer access:** the Sequencer step editor button stays in the Tab order on every parameter page, between Value and Reset Parameter. Alt+Q continues to open the editor directly.
- **Parameter Lock workflow:** Alt+Enter toggles the selected lock while leaving the parameter picker open; Enter adds and returns to the step.
- **Documentation:** built-in Help updated in eight languages, including FX engines and grid shortcuts, with About reporting version 0.99.54 and the new release date.

## Features already available in 0.99.53 but missing from the earlier release email

- **Delay 2:** stereo dual-tape delay with two tape speeds, individual feedback/filter controls, Tape Glide, Stereo Spread and Tape Drive.
- **Reverb 2:** Open Courtyard eight-line FDN reverb, with contextual Distance, Open Sky, Warmth, RT60 and other controls.

The original Delay 1 and Reverb 1 remain selectable; saved preset compatibility is maintained by preserving the existing Off/On mapping.

## Installation

Download the GitHub Actions Windows VST3 build artifact and extract the VST3 plug-in to your usual VST3 directory. No binary is included in this source archive. Report issues at https://github.com/Lo-lo78/LJuno-116/issues.
