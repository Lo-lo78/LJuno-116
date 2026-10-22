# LJuno-116 0.99.54 TEST89 — Independent OSC saturation amount

Based strictly on TEST88. No existing VST3 parameter ID has been changed.

## New continuous OSC sliders

| ID | Accessible parameter name | Range / step | Default |
| --- | --- | --- | --- |
| 430 | Saturation Amount L1 | 0–100 / 1 | 50 |
| 431 | Saturation Amount L2 | 0–100 / 1 | 50 |
| 432 | Saturation Amount Noise | 0–100 / 1 | 50 |

Each OSC source shows its three adjacent controls in this order:
`Saturation [L1/L2/Noise]`, `Saturation Amount [L1/L2/Noise]`,
`Saturation Position [L1/L2/Noise]`.

The intensity sliders are **ordinary continuous VST3 floating-point sliders**, not
choice/drop-down selectors. They support host automation and Sequencer Parameter
Locks, just like the preceding controls. Normal values use whole steps.

## Amount behavior

- **0:** exact dry bypass even when a saturator type is selected, in Pre and Post
  configurations. Saturator type `Off` remains an exact bypass at any amount.
- **1–49:** quieter harmonic drive. The added nonlinear component scales from
  2% at Amount 1 to 98% at Amount 49 of the existing TEST88 curve.
- **50:** identical waveshaping curve, DC rejection and automatic RMS gain to
  TEST88. This is deliberately the default to preserve presets already using
  TEST87/TEST88 saturation.
- **51–100:** more intensity than TEST88. The *added nonlinear component* rises
  smoothly up to 250% of its old strength at Amount 100. This is **not** a 250%
  output volume boost; the established independent source-local RMS auto-gain
  continues to compensate average level. Peaks and subjective loudness can vary.

The Amount control modifies nonlinear harmonics, not layer volume, pan,
filter settings or global effects. The existing Pre Filter / Post Filter routes
remain in their original places, including individual Unison-clone processing.

## Legacy compatibility

- Existing parameters keep their numerical IDs, order and values; new IDs
  430–432 are appended, so DAW automation references are not renumbered.
- Old projects with no Amount fields are migrated to 50 individually. The
  existing library preset parser also supplies the new parameter defaults.
- Increasing or decreasing amount does not reset a playing voice's DC/RMS
  memories on every step. The bypass-to-active transition resets only that
  source's previous saturator memory.

## Source-level checks

`Tests/SynthSmokeTest.cpp` includes a TEST89 regression for catalogue and
OSC ordering, source independence, Amount=0 dry bypass, default 50 equals
explicit 50, audible differences at 25/50/100, output finiteness and level,
Pre and Post modes and Unison.

**Build status:** No CMake, MSBuild, local compilation, or audio test binaries
have been executed. Compile/test on GitHub Actions, then compare by ear in REAPER.
