# LJuno-116 — Version 0.99.59 TEST124

**Release date:** 14 November 2026
**Baseline:** 0.99.58 TEST123

## New: deterministic LFO Phase Drift and Lace

Both existing LFOs now have five extra controls each on the accessible **LFO 1** and **LFO 2** pages:

- **Drift Amount** (0..1, default 0): phase offset progressing between successive LFO cycles.
- **Drift Cycle** (1..32, default 5): number of cycles before the offset pattern repeats. This does **not** increase LFO frequency.
- **Lace Source** (Off / other LFO, default Off): alternates the current waveform with the other LFO.
- **Lace Rate** (1..32, default 4): number of interlaced divisions per LFO cycle.
- **Lace Mix** (0..1, default 0): depth of replacement on alternating slices.

LFO 1 and LFO 2 each remain common modulators routing independently to **Layer 1**, **Layer 2** and **Noise**; no third LFO was created. These controls affect all six existing LFO waveforms in Mono and Poly modes. Existing cross-rate, smoothing, one-shot, free/triggered operation, host BPM sync and host-Play Free reset remain in place.

All 10 new parameter IDs are appended, `slider453`..`slider462`, so existing parameter IDs are not moved. Old patches and presets start with both extensions Off.

**Example:** LFO1 Wave Square, Drift Amount 0.75, Drift Cycle 5. Assign it to Layer 1 pitch or filter, then try 7 cycles. For Lace select LFO 2 as LFO 1's source and increase Lace Mix.

**Testing:** GitHub Actions Windows build and actual listening with NVDA + REAPER remain to be validated.
