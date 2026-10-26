# LJuno TEST104 — Song Shift+Space and single Selected announcement

- Inside Alt+Q / Alt+S Song, **Shift+Space** toggles the focused element in the explicit selection. Ctrl+Space does not select an element.
- When a single element has been marked, arrows announce `Selected P2, 12 of 64` on that element. Tab reports `1 selected` and the visual Song column includes the `*` mark.
- Rows that are only implicitly focused (no explicit selection) do not say `Selected`. Plain Space still controls sequencer transport per Alt+P.
- Multiple / discontinuous selections, clipboard semantics, audio and presets are unchanged.
