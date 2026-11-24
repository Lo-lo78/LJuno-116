# LJuno-116 0.99.66 TEST135 — linked main filters

Port of the validated LSampler-24 filter workflow, adapted to LJuno's existing main filter and three source-routing destinations. No third filter is introduced. The static source-local filters in Filter Routing are unchanged.

- Filter: BP Cutoff immediately after HP Cutoff (0–1, step 0.01). It writes the two physical host cutoff parameters. Their existing ranges differ (LP -1..1, HP 0..1), so the common movement is computed in log-frequency units rather than adding identical raw increments. Both stop when either reaches its raw range limit. Direct edits re-anchor the pair. The original 0.01 cutoff steps and DSP frequency clamps still apply; the underlying continuous movement retains sub-step precision between macro edits.
- Filter and Env: HP Key Follow after Key Follow Filter Mode. Range -4.20 to +4.20 octaves per octave, step 0.01; default +0.08 reproduces the historical fixed HP tracking. 0 = no tracking; +1 = one octave per octave. The legacy LP Key Follow and its envelope mode are unchanged. HP bypass at cutoff 0 remains unchanged unless an existing HP modulation activates it.
- LFO 1 and LFO 2: BP L1, BP L2, BP Noise, after the HP destinations. Each depth is bipolar -8..+8 octaves, step 0.01, default zero. Each adds to the source's original LP and HP depths before the existing +/-8 octave clamp for each LFO/filter; different LFO waveforms and per-source settings remain independent. These are DSP modulation only, not writes to stored cutoffs. No shared Key Follow parameter.
- IDs slider465–472 appended; all previous IDs and ranges retained. Project restoration and preset loading use the two saved physical cutoffs, not the derived macro; missing new fields get legacy-safe defaults. A BP step lock is a transient DSP override, consistent with the sequencer's existing lock model.

## Manual checks at 44.1/48 kHz

1. Filter page: LP Cutoff 0.50, HP Cutoff 0.25. BP Cutoff reads approximately 0.59. Increase to 0.69. LP should become approximately 0.65 and HP 0.41. Reduce to 0.59; values should return close to the originals (within the original slider resolution). At either boundary both stop. Adjust one cutoff manually and verify that BP re-anchors.
2. HP Key Follow: set LP open, HP Cutoff 0.40, disable filter modulation. Compare notes an octave apart at 0, +1, -1. +0.08 reproduces previous HP tracking. Test changes during held notes.
3. LFO1 BP L1 at +1 octave with LP/HP L1 depths zero and L2/Noise silent; audition a slow LFO. Repeat with LFO2 and each source. Verify cutoff values themselves are unchanged. Check individual depths add to common depth.
4. Save and reload a preset and a REAPER project; check the actual LP/HP and new values. Load an older preset and project. Try REAPER automation of BP Cutoff alone; avoid simultaneous automation of the macro and its two dependent controls.

TEST135 retains the public 0.99.66 version and release help/about metadata until the test is approved. The workflow includes pure C++ math tests and a JUCE host-parameter/state round-trip test; the latter requires a full build.
