# LJuno-116 TEST87 — OSC source-local saturation

Test development branch based solely on LJuno-116 0.99.54 TEST86. The VST3 product/version identity, existing parameter IDs and previous presets remain unchanged.

## OSC: three independent choice parameters

- `slider424` **Saturation L1**
- `slider425` **Saturation L2**
- `slider426` **Saturation Noise**

All three have the same ordered choices:

0. Off — true bypass (identical sample path to TEST86).
1. Valve Warm 2H — light asymmetric second harmonic.
2. Triode Silk 2H — rounded asymmetric second harmonic.
3. Tube Bloom 2H — richer asymmetry, dominant second harmonic.
4. Diode Edge 2H — curved asymmetric diode tone.
5. Tape Soft 3H — subtle, predominantly third harmonic.
6. Velvet 3H — stronger smooth third harmonic.
7. Soft Clip 3H — rounded nonlinear clipping, odd harmonics.
8. Round Crunch 3H — brighter odd-harmonic curvature.
9. Vintage Mix 2H 3H — blended second and third harmonic tone.

These are *types*, not an amount slider. Selecting a type enables its fixed, calibrated character. The names remain in English for accessibility, as with the existing OSC controls.

## Routing and level

- L1 and L2: saturation is inside each oscillator signal path **after waveform synthesis and before layer Volume/Balance, ADSR, velocity, pan, local filters, main filters, effects and auxiliary routing**. It also processes each active Mono Unison clone before summing its contribution.
- Noise: saturation is inside the Noise generator **after Noise Color and optional Granulation, before Noise Stereo, envelope, Noise Level, pan, filters, effects and aux routing**.
- Each oscillator/Unison clone and Noise channel holds an independent DC remover and gain tracking state; even-harmonic modes reject the DC component at 3 Hz.
- An RMS-based automatic compensator compares source level before and after each shaper over approximately 45 ms, with correction evaluated every 64 samples. Target loudness is 98% of dry and the gain correction is capped for robustness. Source volume controls are never moved or modified. Loudness compensation is approximate during attacks and rapid timbral changes and is not a brickwall peak limiter.
- `Off` avoids waveform processing completely: old presets and initial patches have all three new parameters at `Off` by default. Legacy `.Ljuno` files without these keys load unchanged. No established slider number is reused.
- All three are ordinary host-automatable VST3 parameters and appear in the OSC grid. They also appear in the Sequencer Parameter Lock selector.

## Tests

`Tests/SynthSmokeTest.cpp` contains an added regression covering the OSC catalogue, the three independent source paths, true bypass on inactive sources, relative sustained RMS, and a finite Unison rendering. The previous parameter-count assertion was updated from its obsolete value to the current parameter count.

The `Tools/generate_parameters.py` utility now supports the **VST3-only source archive** that does not contain the older JSFX and Lua references, regenerating only additions from the checked-in stable headers. Do not add the missing Lua/JSFX files simply to regenerate the VST3 catalogue.

**Build status:** Not compiled locally. Intended for building and listening tests via the repository's GitHub Actions workflow.
