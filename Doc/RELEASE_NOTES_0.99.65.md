# LJuno-116 0.99.65 — AHDSR Hold and REAPER host automation

**Release date:** 16 November 2026  
**Based on:** TEST130 and TEST131  
**Previous release:** 0.99.64 (14 November 2026)

## New: Hold in both envelopes (AHDSR)

- Added an independent **Hold** stage between **Attack** and **Decay** in both envelope generators: main ADSR and ADSR2.
- New host parameters: **Hold** (`slider463`) and **ADSR2 Hold** (`slider464`). Each supports **0.000–5.000 s** in **0.001 s / 1 ms** steps, default **0.000 s**.
- The controls appear immediately after Attack in the accessible **Env** and **Filter** pages, and are available to REAPER automation and sequencer **Parameter Locks**.
- The envelope stays at full level during Hold. ADSR1 and ADSR2 use independent per-voice counters; Note Off moves directly to Release even during Hold. The pitch envelope remains at full level during the ADSR1 Hold period.
- Hold = 0 skips the extra stage, retaining the previous attack/decay behavior for existing patches. Historical parameter IDs are unchanged.

## New: keyboard automation shortcut for REAPER

- In the main parameter Grid (**Alt+L**), Value control or numeric Value editor, press **Ctrl+Shift+A** to toggle the selected REAPER parameter envelope's **active** state without deleting automation points.
- NVDA announces **", Auto"** only when the envelope is active. Showing an inactive lane does not count as enabled.
- The shortcut is isolated from Alt+Q, the preset browser and modal dialogs.

## Accessibility and documentation

- Updated **About (Alt+A)** to version **0.99.65**, **16 November 2026**.
- Updated VST3 build metadata, GitHub Actions artifact name, the eight-language embedded Help, HTML manuals and README.
- No unrelated DSP changes beyond the previously tested Hold integration. Preset storage and existing automation parameter IDs are preserved.

## Verification

- TEST130/TEST131 source regression tests are included in GitHub Actions alongside the historical project tests.
- The full Windows VST3 build and resulting release asset must be verified on GitHub Actions before publishing the GitHub Release.

See `Doc/TEST130_HOST_AUTOMATION.md` and `Doc/TEST131_AHDSR_HOLD.md` for technical details and test instructions.
