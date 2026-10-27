# LJuno-116 0.99.55 — Release notes

**Release date:** 27 October 2026  
**Platform:** Windows x64 VST3  
**License:** GNU AGPL-3.0-or-later

## Documentation and version update

- Updated the built-in Help in all eight original languages: English, Italian, Spanish, Portuguese, French, Russian, Chinese and Japanese.
- Updated the accessible **About** (Alt+A) and CMake/VST3 version to **0.99.55** and **27 October 2026**. About's read-only text supports Ctrl+A and Ctrl+C, and includes the project and contact links.
- Documented 48 independent patterns across Layer 1, Layer 2 and Noise, the three-column Song Editor, keyboard selection of song rows and multiple step edits.
- Documented Ctrl+C/X/V for steps and Ctrl+Shift+C/X/V for patterns, and Delete's deselection-only action on the Steps view.
- Documented the preset browser rule: only **confirmed preset loads** update the last-used location, also across instances/sessions. Previewing, navigating, cancelling and saving do not.
- Preserved the historic 0.99.54 notes (14 October 2026) as release history.

## Compatibility

Based on the user's **TEST106C** source, without adding TEST107's Note/Length recall changes. No changes to DSP, MIDI routing, parameters, patterns, preset file formats, or sequencing behavior. Help/About presentation and build metadata only.

## Installation

Use GitHub Actions to compile the Windows VST3, then place the built `LJuno-116.vst3` directory in `C:\Program Files\Common Files\VST3\`.

Project: https://github.com/Lo-lo78/LJuno-116
