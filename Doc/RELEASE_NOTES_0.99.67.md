# LJuno-116 0.99.67 — Linked filters and modulation

**Release date:** 25 November 2026  
**Based on:** TEST135  
**Previous release:** 0.99.66

## New filter controls

On the Filter page, BP Cutoff (0–1, step 0.01), immediately after HP Cutoff, physically moves LP Cutoff and HP Cutoff together on a logarithmic frequency scale. Set the two individual cutoffs first to define a narrow or wide spectral window; both stop when either reaches its limit. Editing either cutoff individually re-anchors the macro. This is not a third filter; the two resonances remain independent.

HP Key Follow is available on Filter and Env, after Key Follow Filter Mode: −4.20 to +4.20 octaves per octave, step 0.01; default +0.08 preserves the previous HP tracking. Zero disables tracking, +1 follows pitch one octave per octave. LP tracking remains independent.

On the LFO 1 and LFO 2 pages, LFO1 BP L1, LFO1 BP L2, LFO1 BP Noise and LFO2 BP L1, LFO2 BP L2, LFO2 BP Noise follow the existing HP destinations. Range −8 to +8 octaves, step 0.01, default 0. The common depth adds to each source’s individual LP and HP depths within the existing modulation limits; it does not change the stored cutoffs. Example: LP Cutoff 0.50, HP Cutoff 0.25; raise BP Cutoff from about 0.59 to 0.69 and both move to about 0.65 and 0.41. Automate BP Cutoff alone rather than simultaneously automating its dependent cutoffs. The local filters in Filter Routing are unchanged.

## Compatibility and accessibility

- Eight new parameters, slider465–472, appended without renumbering the 464 previous IDs.
- Legacy HP tracking is preserved by default (+0.08); all six BP depths default to zero. Restored projects and presets use the saved physical cutoffs.
- No third DSP filter or shared Key Follow; individual filter controls remain independent.
- Updated accessible About (Alt+A), Help in eight languages, six HTML manuals, README, CMake and GitHub Actions metadata.

## Verification

The release retains the validated TEST135 audio engine. See `TEST135_LINKED_FILTERS.md` for manual tests and the linked-filter mathematics and host-state regression checks.
