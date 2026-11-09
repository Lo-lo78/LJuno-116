"""Source-level regression checks for TEST123's per-layer sequencer input modes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
processor = (ROOT / 'Source/PluginProcessor.cpp').read_text()
state = (ROOT / 'Source/SequencerState.cpp').read_text()
parameters = (ROOT / 'Source/GeneratedParameters.h').read_text()
pages = (ROOT / 'Source/GeneratedPages.h').read_text()
assert '"slider364", "slider452"' in pages
assert '"Input Polyphony L1", "Input Polyphony L2"' in pages
assert '"slider452", "Sequencer Input Polyphony L2"' in parameters
assert 'const auto lane = id == "slider364" ? 0 : 1;' in processor
assert 'const auto inputLane = clampedSequence % trackCount;' in state
assert 'sequences[static_cast<std::size_t> (inputLane)].midiInputPolyphony.store' in state
assert 'sequences[0].midiInputPolyphony.store (mode' not in state
assert 'sequences[1].midiInputPolyphony.store (mode' not in state
assert 'const auto sharedPolyphony' not in state
assert 'sequences[2].midiInputPolyphony.store (0, std::memory_order_relaxed)' in state
assert 'std::array<std::pair<const char*, SeqParam>, 21> sequencerBankParameters' in processor
print('TEST123: per-layer controls, independent storage, old shared-mode removal OK')
