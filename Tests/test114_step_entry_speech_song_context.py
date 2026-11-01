"""TEST114: MIDI Step Entry announcements and automatic Alt+S preview target."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
ed=(r/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
acc=(r/'Source/ScreenReaderAnnouncer.cpp').read_text(encoding='utf-8')
wf=(r/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')
assert 'MIDI Step Entry On. Step ' in ed
assert 'MIDI Step Entry Off' in ed
assert 'End of Pattern. MIDI Step Entry Off' in ed
assert 'Step announcements On. Step ' in ed
assert 'Step announcements Off' in ed
assert 'midiEntrySpeak = false;' in ed
assert 'NotificationProcessing_ImportantAll' in acc
assert 'preserveOrder' in acc
assert 'processor.songs.setSongMode (songView);' in ed
assert 'processor.songs.setSongMode (false);' in ed
assert 'processor.requestSequencerPreview (processor.getSelectedSequencerIndex(), true);' in ed
assert 'TEST118-MIDI-Step-Entry-Speech-Windows-VST3' in wf
print('PASS: LJuno MIDI speech, guaranteed end, UIA and automatic preview context')
