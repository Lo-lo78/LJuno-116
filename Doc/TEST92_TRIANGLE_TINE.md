# TEST92 — Triangle Classic Tine

This update changes the sonic behavior of the existing **Metal L1 / Metal L2** knobs **only while Triangle Classic is selected**. There are no new parameter IDs, page positions, or presets.

- **Sine:** existing Metallizer is unchanged.
- **Triangle Classic:** Metal 0 retains the original triangle exactly; as Metal increases (0–10), progressive wavefolding and tuned 2nd/5th harmonics add electric-tine, bell and glassy character. Independent per layer.
- **Triangle Sharktooth:** the existing **Shark Harm** control remains unchanged. Saw, Pulse and all their Sync/PWM features remain unchanged.
- **Morph:** the new triangle treatment applies during morphing when Triangle Classic contributes to the blend.
- **SuperWave:** Triangle Classic steps also receive the new Metal treatment; other steps remain as before.
- **Wave Mod LFO:** continues to modulate Metal according to the existing configuration.

The waveshaper runs in the oscillator itself, before the independent layer saturators, filters, FX and chorus. Extra harmonics fade progressively at high pitches to reduce aliasing, without introducing oversampling, additional latency or state. The original oscillator is bypassed at Metal 0. The Metal parameter is intentionally unchanged to preserve automation and the entire VST3 parameter map.

**Preset compatibility note:** older Triangle Classic patches with Metal > 0 retain their setting, but will intentionally sound different because this control now has a triangle-specific character. Presets with Metal 0 (including init patches) sound unchanged; Sine Metallizer patches are unchanged.

A JUCE smoke test checks bypass, output limits, the even harmonic component and RMS bounds. No local JUCE/VST3 build was run; use the existing GitHub Actions workflow.
