#!/usr/bin/env python3
"""TEST96 source and navigation invariant checks (no JUCE build required)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "Source/PluginEditor.h").read_text(encoding="utf-8")


def active_block_navigation(start_step: int, end_step: int, current_step: int, direction: int):
    """Independent model of the intended 1-based active range behavior."""
    first = (start_step - 1) // 16
    last = (end_step - 1) // 16
    current_step = max(start_step - 1, min(end_step - 1, current_step))
    current_block = current_step // 16
    target = max(first, min(last, current_block + direction))
    if target == current_block:
        return current_step, False
    local = current_step % 16
    step = max(start_step - 1, min(end_step - 1, target * 16 + local))
    return step, True


class SequencerEditorTest96(unittest.TestCase):
    def test_all_active_ranges_tab_stays_inside_and_silent_at_edges(self):
        for start in range(1, 129):
            for end in range(start, 129):
                for step in (start - 1, end - 1):
                    for direction in (-1, 1):
                        next_step, moved = active_block_navigation(start, end, step, direction)
                        self.assertGreaterEqual(next_step, start - 1)
                        self.assertLess(next_step, end)
                        self.assertEqual(moved, (next_step // 16 != step // 16))

    def test_examples(self):
        self.assertEqual(active_block_navigation(1, 32, 0, 1), (16, True))
        self.assertEqual(active_block_navigation(1, 32, 16, 1), (16, False))
        self.assertEqual(active_block_navigation(17, 32, 16, -1), (16, False))
        self.assertEqual(active_block_navigation(1, 16, 0, 1), (0, False))
        self.assertEqual(active_block_navigation(17, 25, 16, 1), (16, False))
        self.assertEqual(active_block_navigation(12, 18, 11, 1), (17, True))

    def test_source_contains_active_bounds_in_tab_handler(self):
        self.assertIn("void clampSequencerEditorToActiveRange();", HEADER)
        self.assertIn("config.startStep - 1", CPP)
        self.assertIn("config.endStep - 1", CPP)
        self.assertIn("const auto firstBlock = firstStep / 16;", CPP)
        self.assertIn("const auto lastBlock = lastStep / 16;", CPP)
        self.assertIn("if (target == currentBlock && currentBlock == sequencerEditorBlock)", CPP)
        self.assertIn("changeSequencerEditorBlock (key.getModifiers().isShiftDown() ? -1 : 1);", CPP)
        self.assertIn("if (step < config.startStep - 1 || step >= config.endStep)", CPP)

    def test_step_clamping_on_editor_open_track_change_and_zxcv(self):
        self.assertGreaterEqual(CPP.count("clampSequencerEditorToActiveRange();"), 6)
        self.assertIn("sequencerEditorCurrentStep = juce::jlimit (first, last, sequencerEditorCurrentStep);", CPP)
        # TEST105 moved active-range scanning into selectedSequencerStepIndices(),
        # so both selection and single-focus edits observe Start/End.
        self.assertIn("const int first = juce::jlimit (0, ljuno::SequencerState::stepsPerSequence - 1,", CPP)
        self.assertIn("const int last = juce::jlimit (first, ljuno::SequencerState::stepsPerSequence - 1,", CPP)
        self.assertIn("const auto indices = selectedSequencerStepIndices();", CPP)

    def test_note_enter_toggle_is_independent_from_parameter_enter(self):
        self.assertIn("sequencerEditorLayer == ljuno::SequencerLayer::parameters)\n    {\n        openSequencerParameterPicker();", CPP)
        self.assertIn("sequencerEditorLayer == ljuno::SequencerLayer::note)\n    {", CPP)
        self.assertIn("juce::roundToInt (previous) == 0 ? 60.0f : 0.0f", CPP)
        self.assertIn("announceSequencerStep();", CPP)


if __name__ == "__main__":
    unittest.main()
