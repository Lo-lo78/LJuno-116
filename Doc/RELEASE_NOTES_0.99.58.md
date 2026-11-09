# LJuno-116 — Version 0.99.58

**Release date:** 9 November 2026  
**Source baseline:** TEST123 — Independent Sequencer Input Polyphony

## What's new

- The Sequencer page (Alt+L) now provides independent **Input Polyphony L1** and **Input Polyphony L2** selectors (Mono / Poly) for Layer 1 and Layer 2, respectively.
- **Noise** remains monophonic; no polyphony selector is added for its sequencer.
- Switching the selected sequencer track or pattern does not modify either layer's polyphony.
- Existing automation ID `slider364` controls Layer 1; new ID `slider452` controls Layer 2.
- Existing projects and presets retain their previous shared initial setting for both layers, which can now be adjusted separately. The LSQ2 v7 sequencer state format is unchanged.

## Documentation and publishing

Updated About to 0.99.58 (9 November 2026); updated eight-language built-in Help, accessibility manual, What's New, README and CMake version. GitHub Actions now uses the new artifact name and includes the TEST123 static regression check while retaining previous tests and build steps. Historical release notes remain unchanged.

**Validation:** source-level tests can run locally; actual Windows VST3 compilation and runtime behavior must be checked with GitHub Actions and REAPER.
