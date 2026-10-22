# TEST91 — per-source Keyboard Scale (Osc)

Three additional **ordinary numeric sliders**, immediately after `Detune L2` on the OSC page:

| ID | Name | Range | Default |
| --- | --- | --- | --- |
| slider436 | Keyboard Scale L1 | 0–100, step 1 | 100 |
| slider437 | Keyboard Scale L2 | 0–100, step 1 | 100 |
| slider438 | Keyboard Scale Noise | 0–100, step 1 | 100 |

These are **independent**: value 100 is the original 12-semitone-per-octave
keyboard; 50 gives 50 cents between adjacent MIDI keys, 25 gives 25 cents,
and 0 plays a fixed base pitch at C4 (MIDI note 60) on every key. `Octave L1`,
`Semitone L1`, `Octave L2`, `Semitone L2` and `Noise Pitch` can still transpose
this fixed reference independently. No extra reference-note parameter is added.

The mapping is `frequency = C4 * (originalNoteFrequency / C4) ^ (scale / 100)`.
It runs **before** the independent layer tuning, portamento-output modulation,
pitch envelope/LFO, pitch bend, oscillator saturators and FX. The resulting
frequency follows normal portamento through a continuous transform; an active
Pitch Arp 2 note movement is likewise reduced by the corresponding scale.
Keyboard Scale deliberately does not change velocity, voice allocation, MIDI
note routing, or existing filter/volume keyboard tracking.

For Noise it scales the frequency of its independent clock on the
pitch-tracking modes (radio, SID, Amstrad CPC AY, metallic and Digital Dust).
Classic Color, White, Pink, Brown are not artificially pitch-shifted; they
ignore the clock in the existing engine. Noise Pitch and noise LFO/bend remain
functional at every Scale setting.

Default 100 takes the original DSP frequency unchanged, preserving legacy
sound. Automation and preset storage use **new IDs only**; no old parameter IDs
were renumbered, nor were any effects or MIDI handlers moved. No local build was
run. The smoke-test source includes arithmetic and audible path regressions
for all three generators; execution is delegated to GitHub Actions.
