# LJuno-116 — TEST95: Independent BPM Sync for LFO 1 and LFO 2

Two VST3 parameters were **appended** after the TEST94 controls, without renumbering any existing parameters:

- `slider441`: **LFO 1 BPM Sync**, Off / On, default **On**.
- `slider442`: **LFO 2 BPM Sync**, Off / On, default **On**.

Each switch appears immediately before its LFO Frequency control on the corresponding accessible LFO page. The controls are independent and persist/automate through the standard VST3 parameter state.

## Why the defaults are On rather than Off

Historically LJuno-116 multiplied **both** LFO rates by the host BPM divided by 240 *all the time*. Thus it already followed host tempo, although it had no BPM Sync switch. Default **On** is important for preserving the timing and sonic character of existing patches, automation, and REAPER projects. LSampler-24 has a different default (Off); its default is not imposed on LJuno.

- **On (historical)**: LFO effective frequency in Hz = Frequency control × host BPM ÷ 240. The Frequency value represents cycles per four quarter-note beats. For example, Frequency 4 at 120 BPM = 2 Hz, a cycle every two beats. The host's current BPM is used as it changes.
- **Off (new)**: LFO effective frequency in Hz = Frequency control. Rate is free-running with respect to changes in BPM. For example, Frequency 4 = 4 Hz at any BPM.

BPM Sync controls **speed only**: it does not reset phase to the song grid. The separate Free/Trigger and Mono/Poly settings still determine phase behavior. One Shot, Delay, phase offset, envelope-rate modulation and cross-rate modulation use the resulting effective LFO speed as before.

No original DSP signal chain, filter, oscillator, or effects code was changed. The generated parameter catalogue, page tables and fallback parameter generator were updated. Source-level regression checks are available at `Tests/test95_bpm_sync.py`.

## Build and verification

No native compilation was run while preparing this archive. Run the GitHub Actions build and check both LFO pages and the behavior at two project BPMs in REAPER (On follows BPM, Off remains constant). Existing presets should retain their tempo-dependent LFO speed with On selected by default.
