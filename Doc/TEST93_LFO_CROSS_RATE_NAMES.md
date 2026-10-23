# TEST93 – Cross Rate labels for LJuno-116

LJuno-116 already implements the same bidirectional LFO frequency cross-modulation found in LSampler-24 TEST160. No DSP or parameter additions were needed.

The existing controls are now displayed with the clearer names:

- **LFO 1 Cross Rate** (`slider048`): LFO 2 output modulates the frequency of LFO 1.
- **LFO 2 Cross Rate** (`slider047`): LFO 1 output modulates the frequency of LFO 2.

Both controls remain on both LFO pages in their original positions. They retain their original slider IDs, ranges (`-40` to `+40`), neutral defaults (`0`), value steps (`0.01`), and processing engine behavior. Existing patches and DAW parameter automation do not require conversion.

The generator also preserves the new display aliases when regenerated with the original JSFX and Lua references. This change does not alter the sound of existing patches.

No local compilation is performed. GitHub Actions is responsible for building the VST3.
