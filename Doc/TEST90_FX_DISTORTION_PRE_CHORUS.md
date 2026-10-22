# LJuno-116 0.99.54 — TEST90: Global FX distortion before Chorus

Base: TEST89. This is a source-only test for the existing GitHub Actions workflow.
No VST3 was compiled locally.

## Accessible FX page controls

Three consecutive controls have been placed **at the beginning of the FX page**, before the Chorus sends:

1. `FX Distortion Type` (`slider433`, default `Off`): `Off`, `Soft Clip`, `Warm Tube`, `Tape Drive`, `Classic Overdrive`, `Hard Clip`, `Diode Crunch`, `Thick Fuzz`, `Wave Fold`, `Rectifier`, `Bit Crush`, `Sine Warp`, `Broken Radio`.
2. `FX Distortion Amount` (`slider434`, default `0`, from `0` to `100`, steps of `1`). Higher values raise nonlinear drive and the wet distortion amount.
3. `FX Harmonics Even Odd` (`slider435`, default `0`, from `-100` to `100`, steps of `1`). `-100 Even` prioritises even harmonics, `0 Both` combines both, `100 Odd` prioritises odd harmonics. The numeric value remains first for normal keyboard editing and NVDA can read the direction.

All new VST3 parameter IDs are appended after `slider432`, without reassigning existing IDs. They are saved in presets, exposed to REAPER automation and available for sequencer parameter locks.

## Audio routing

The existing Layer 1, Layer 2 and Noise synthesis/filters/source saturation are unchanged. The new distortion applies once to the combined *synth dry* stereo bus **before the Chorus, Delay and Reverb sends**, not to the audio-input passthrough.

The nonlinear stereo difference is distributed across the three dry stems by instantaneous magnitude. This preserves the combined distorted bus while keeping per-source send amounts, legacy pre-reverb global EQ/compressor, post-reverb compressor, finalizer and existing aux/output topology in their former positions.

**The distortion is audible even when all Chorus, Delay and Reverb sends are zero.** The later effects receive the distorted sources when enabled.

When the type is `Off` or amount `0`, no DSP nor stem-splitting change takes place: the previous dry path is preserved. The state is reset when switching algorithms.

## DSP notes

The algorithm creates odd/even components from the antisymmetric/symmetric parts of a nonlinear transfer function. The even component is DC-filtered independently for L and R. The output has a 4 ms smoothing envelope for the amount and a linked stereo RMS-based makeup, limited to a moderate range. The global safety clip prevents excessive intermediate amplitudes before the existing master processing.

For extreme `Bit Crush`, `Wave Fold`, `Sine Warp` and `Broken Radio` settings, higher harmonics can alias. The implementation uses a two-point interpolated nonlinear evaluation to soften abrupt transitions, **not full band-limited oversampling**. Evaluate high notes by ear at normal and high sample rates.

The compressor, limiter, input routing and all pre-existing FX internals are unchanged. With hard clipping or aggressive waveshaping, timbre and transients do of course change, even when average gain is compensated.

## Automated regressions added

`fxDistortionIsPreChorusAndCompatible()` in `Tests/SynthSmokeTest.cpp` checks:
- the three stable IDs, step/ranges/defaults and FX grid order;
- exact sample equality when type is Off or Amount is zero;
- audible nonlinear difference even with Chorus sends disabled;
- balance changes the waveform for even vs odd selection;
- Chorus wet signal receives the changed audio;
- numerical stability across all 12 distortion models;
- reasonable signal-energy bounds for Warm Tube.

These tests require the GitHub Actions build/test run; no local C++ compilation was performed.
