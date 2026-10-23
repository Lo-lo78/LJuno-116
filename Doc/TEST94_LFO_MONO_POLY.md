# LJuno-116 — TEST94: independent Mono/Poly LFO modes

## New controls

LFO 1 Voice Mode and LFO 2 Voice Mode are each located near the start of their respective accessible LFO pages. Choices: **Mono** and **Poly**.

* **Mono (default):** preserves the original LJuno-116 common oscillator for all notes. Its historical Free/Trigger behavior and the vintage unified motion remain the defaults. Both global effects and all voices continue to receive the common LFO output.
* **Poly:** each newly triggered voice owns a separate LFO state and phase. Free starts from the shared clock position, and Trigger/One Shot starts a fresh cycle on that voice. Another note cannot reset the already sounding voice's polyphonic LFO.
* Modes can be mixed independently, with one global LFO and one independent per voice. Existing Cross Rate modulation remains operational. Global effects continue to use the common LFO clock, even if an LFO is set to Poly for the synth voices.

## Preset and host compatibility

The original TEST93 parameter catalogue (438 parameter descriptors, in its original order) is unchanged. `slider439` and `slider440` are appended with default `Mono` (0), and are included in the generated accessible LFO pages. The parameter generator retains the additions when run again. Old presets default to the unchanged global behavior.

## Testing

Source checks performed: the 438 existing descriptors were compared with TEST93; Python parameter generation is repeatable; new controls, defaults, pages, and audio voice routing were inspected. No C++ build, plugin loading, or listening test was performed locally. Compile via GitHub Actions and verify in REAPER.
