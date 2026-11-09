# LJuno-116 — Version 0.99.57

**Release date:** 9 November 2026  
**Source baseline:** TEST122 — LSampler FDN Reverbs

This release publishes the current TEST122 source as LJuno-116 VST3 version 0.99.57.

## Included changes since the previous public version

- **TEST122:** Four additional reverbs on the FX page: Studio Room, Concert Hall, Plate and Cathedral (Reverb Type 3–6), adapted from LSampler-24. Six additional automatable parameters: Room Size, FDN Damping, Tail Tone, Gain, Gate Threshold and Gate Decay. The previous Off, Reverb 1 and Reverb 2 choices remain in their original positions.
- **TEST121:** Both Free LFOs restart deterministically from their configured Phase when the DAW starts playback from Stop. Triggered mode remains unchanged.
- **TEST120:** Incremental multi-character search in the main Alt+L parameter grid and Alt+Q Parameter Locks picker, including reverse search with Shift.

The release version, About date, embedded eight-language Help metadata, CMake version, GitHub Actions artifact name and automated release checks have been updated. No additional DSP changes are introduced by this versioning pass. Existing parameter identifiers are preserved, with the six TEST122 IDs appended as 446–451.

**Verification:** project static tests are included in GitHub Actions. The compiled VST3 and accessible behavior should be verified on Windows with REAPER / NVDA / OSARA before publishing the release asset.
