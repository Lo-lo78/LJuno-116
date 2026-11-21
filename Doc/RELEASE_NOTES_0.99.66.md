# LJuno-116 0.99.66 — SID/AY noise continuity

**Release date:** 21 November 2026  
**Based on:** TEST133  
**Previous release:** 0.99.65 (16 November 2026)

## Fixed: authentic SID/AY noise continuity between notes

- **Biscot SID 6581 Noise Type 7** retains its independent 23-bit shift-register
  and fractional pitch clock across successive Note On/Off events.
- **Amstrad CPC AY Noise Type 8** likewise retains its independent 17-bit
  register and clock phase; it does not restart from its initial seed on each
  new note.
- The saved last sample and fractional clock phase survive repeated notes,
  release, zero-voice cleanup, mono retriggers, polyphonic reuse, voice
  stealing, sequencer playback and MIDI routing.
- SID and AY hardware-register states are separate: switching Noise Type does
  not reset the other generator.
- Each generator still uses its previous chip-specific taps, DAC mapping and
  clock calculations. A full audio `prepare()` starts a fresh hardware state.
- Silent voices are not processed just to advance the noise clock.

This prevents the **artificially identical digital attack** that could occur
with short high-pitched noise notes. The AY's natural register periodicity and
high-clock aliasing are not removed.

## Compatibility and accessibility

- No new VST3 parameters, preset fields, parameter-ID changes, MIDI routing
  rules or keyboard shortcuts. The existing NVDA/OSARA-accessible interface,
  sequencer and Parameter Locks remain unchanged.
- Other noise types (Radio, Metallic, Digital Dust and White/Pink/Brown) and
  their established voice/DSP behavior are preserved.
- Previous **0.99.65** additions — independent AHDSR Hold controls and
  **Ctrl+Shift+A** for REAPER automation activation — remain included.
- Updated About (Alt+A), eight-language Help, HTML manuals, CMake metadata
  and GitHub Actions artifact name.

## Suggested listening check in REAPER

1. Choose **Noise Type: Biscot SID 6581**, with high **Noise Pitch** and audible
   **Noise Level**. Repeatedly play short notes separated by a small pause.
2. Repeat with **Amstrad CPC AY**. Both should continue their bitstreams
   between notes rather than play the same first noise fragment each time.
3. Check mono, poly, the dedicated Noise sequencer lane, layer routing and
   pre-existing presets with other Noise Types.

## Verification

The project includes `Tests/test133_chip_noise_continuity.py` and the pure C++
`LJunoChipNoiseContinuityTest`, which checks the SID/AY algorithms and repeated
note attacks. This release also includes a metadata/documentation regression
check. The full Windows VST3 build and real-time listening in REAPER still
need verification using GitHub Actions and the target host.

See `Doc/TEST133_SID_AY_NOISE_CONTINUITY.md` for implementation details.
