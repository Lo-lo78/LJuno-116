# LJuno-116 0.99.54 TEST88 — Pre/Post Filter saturation per source

Based on TEST87. No VST3 version number or old parameter ID has been changed.

## OSC controls

Three new binary selection parameters, next to the corresponding saturator type selectors:

| Slider | Label | Options | Default |
| --- | --- | --- | --- |
| 427 | Saturation Position L1 | Pre Filter / Post Filter | Pre Filter |
| 428 | Saturation Position L2 | Pre Filter / Post Filter | Pre Filter |
| 429 | Saturation Position Noise | Pre Filter / Post Filter | Pre Filter |

The existing modes on sliders 424, 425 and 426 remain unchanged. `Off` is always
an exact bypass at either position. New selectors are available for host VST3
automation, the keyboard-accessible OSC grid, and Sequencer Parameter Locks.

## Routing

- **Pre Filter** is the *identical* TEST87 source path. For the two oscillators,
  waveshaping happens before layer Volume/ADSR/pan and before the filters,
  including on individual Mono Unison clones; for Noise, it is just after
  colour/granulation but before its stereo/level/pan controls and filters.
- **Post Filter** operates on the **separate stereo source buses** after their
  *local* low-pass and high-pass filters, optional routed formant, and *main*
  low-pass and high-pass filters, and **before** source-specific Velocity Volume,
  final layer sum, main gain, effects and auxiliary sends. L1 never saturates L2
  or Noise, including in Mono Unison or when filter routing/sends are enabled.
- Each Post Filter source has its own per-voice/per-channel RMS auto-level and
  DC-rejection memory; it does not move Volume L1/L2/Noise Level. As with TEST87,
  auto-level aims at comparable measured RMS (not exact perceived loudness, and
  it does not constrain short-term peaks). In Post mode the input is the
  filtered, already envelope- and volume-scaled signal, so the drive character
  can naturally react to those settings.
- Changing a saturator type **or** its position resets just that source's
  saturator memory. It does not reset any other source's state.
- **Compatibility**: every older preset defaults to Pre Filter because the new
  IDs are appended at the end of the existing parameter catalogue, with
  defaults of zero. Even when Post is selected, Saturation Off is a true bypass.

## Validation

`Tests/SynthSmokeTest.cpp` adds tests of the parameter catalogue, OSC ordering,
Post mode/Off bypass, isolation of unrelated sources, audible pre-vs-post
nonlinearity through local/main filters, output finiteness/level, and Mono Unison.
Legacy smoke checks retained; expected parameter count changed from 426 to 429.

**Build status:** No local compilation or build executed; build and audible
verification are intended for GitHub Actions and REAPER.
