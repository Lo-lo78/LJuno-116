# TEST123 — Independent sequencer MIDI input polyphony

Released as LJuno-116 0.99.58 (9 November 2026), based on TEST123.

The Sequencer page contains two permanent, automatable choices:
- `Input Polyphony L1` (existing `slider364`, retained for automation compatibility)
- `Input Polyphony L2` (new `slider452`)

Each uses Mono / Poly and controls only its respective pitched sequencer. Layer 1 and Layer 2 maintain independent held-key chord processing; Noise remains strictly monophonic with no extra input polyphony parameter. Changing the selected track or pattern does not change these per-layer values. Pattern copy/cut/reset does not overwrite them; resetting the full sequencer does reset both to Mono.

The LSQ2 v7 serialisation schema is unchanged. Old projects serialized the shared mode to lane 1 and 2, so loading them gives matching initial values for both independently controllable parameters. The old `slider364` ID is intentionally retained as Layer 1.

Run `python Tests/test123_independent_input_polyphony.py` for source-level validation. VST3 runtime, build and accessibility tests still require the GitHub Windows runner/DAW.
