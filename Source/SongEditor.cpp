// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include <algorithm>

namespace
{
constexpr const char* clipboardHeader = "LJUNO-SONG-ROWS-V1:";
juce::String layerName (int i)
{
    return i == 0 ? "Layer 1" : i == 1 ? "Layer 2" : "Noise";
}
}

void LJuno116AudioProcessorEditor::refreshSongEditor()
{
    const auto epoch = processor.songs.getLoadEpoch();
    if (epoch != songLoadEpoch)
    {
        songLoadEpoch = epoch;
        songUndo.clear(); songRedo.clear();
        songConfirmClear = false;
        for (auto& selected : songSelected) selected.clear();
        songShiftAnchor.fill (-1);
    }
    juce::String contents = "LJuno-116 Song Editor. Three independent columns.\n";
    for (int i = 0; i < 3; ++i)
    {
        const auto snapshot = processor.songs.column (i);
        contents << layerName (i) << ": " << static_cast<int> (snapshot->size()) << " elements, "
                 << (processor.songs.loop (i) ? "Loop On" : "Loop Off") << ". ";
    }
    contents << "\nPlayback: " << (processor.songs.songMode() ? "Song" : "Pattern")
             << ". Active: " << layerName (songTrack) << ".\n\n";
    for (int row = 0; row < 17; ++row)
    {
        for (int lane = 0; lane < 3; ++lane)
        {
            const auto snapshot = processor.songs.column (lane);
            const int start = juce::jmax (0, songCursor[static_cast<size_t> (lane)] - 8);
            const int index = start + row;
            juce::String cell;
            if (index < static_cast<int> (snapshot->size()))
            {
                const bool selected = songSelected[static_cast<size_t> (lane)].count (index) != 0;
                cell << (songCursor[static_cast<size_t> (lane)] == index ? ">" : " ")
                     << (selected ? "*" : " ") << (index + 1) << ": P"
                     << (snapshot->at (static_cast<size_t> (index)) + 1);
            }
            contents << cell.paddedRight (' ', 19);
            if (lane < 2) contents << " | ";
        }
        contents << "\n";
    }
    contents << "\nLeft Right change track, arrows move, PgUp PgDn 5, Ctrl Up Down changes pattern, "
             << "Shift arrows select, Shift Space toggles selection, Ctrl A all, Ctrl C X V edit Song, "
             << "Alt I insert, Delete remove, Ctrl Delete clear, Alt L loop, Tab details, Alt S steps, Alt M playback mode.";
    sequencerEditorPanel.setText (contents, juce::dontSendNotification);
}

void LJuno116AudioProcessorEditor::announceSongRow (bool details)
{
    const auto track = static_cast<size_t> (songTrack);
    const auto snapshot = processor.songs.column (songTrack);
    const int position = songCursor[track];
    if (snapshot->empty())
    {
        announceMessageFrom (sequencerEditorPanel,
            layerName (songTrack) + ", empty Song. Alt I inserts P1");
        return;
    }
    const auto selected = songSelected[track].size();
    const bool marked = songSelected[track].count (position) != 0;
    juce::String message;
    if (details)
    {
        message << "Song, " << layerName (songTrack) << ", element " << (position + 1)
                << " of " << static_cast<int> (snapshot->size()) << ", Pattern "
                << (snapshot->at (static_cast<size_t> (position)) + 1) << ", "
                << (processor.songs.loop (songTrack) ? "Loop On" : "Loop Off") << ", "
                << (processor.songs.songMode() ? "Song playback" : "Pattern playback");
        if (selected > 0) message << ", " << static_cast<int> (selected) << " selected";
    }
    else
        message << (marked ? "Selected " : "") << "P"
                << (snapshot->at (static_cast<size_t> (position)) + 1) << ", "
                << (position + 1) << " of " << static_cast<int> (snapshot->size());
    announceMessageFrom (sequencerEditorPanel, message);
}

void LJuno116AudioProcessorEditor::moveSongCursor (int delta, bool extend)
{
    const auto track = static_cast<size_t> (songTrack);
    const auto snapshot = processor.songs.column (songTrack);
    if (snapshot->empty()) return;
    const int old = songCursor[track];
    const int next = juce::jlimit (0, static_cast<int> (snapshot->size()) - 1, old + delta);
    if (old == next) return; // Silent at the edge.
    if (extend)
    {
        if (songShiftAnchor[track] < 0)
        {
            songShiftAnchor[track] = old;
            songShiftBaseline[track] = songSelected[track];
            if (songShiftBaseline[track].empty()) songShiftBaseline[track].insert (old);
        }
        songSelected[track] = songShiftBaseline[track];
        for (int i = juce::jmin (songShiftAnchor[track], next);
             i <= juce::jmax (songShiftAnchor[track], next); ++i)
            songSelected[track].insert (i);
    }
    else
        songShiftAnchor[track] = -1; // Keep selection when Shift is released.
    songCursor[track] = next;
    refreshSongEditor(); announceSongRow();
}

void LJuno116AudioProcessorEditor::writeSongColumn (ljuno::SongArrangement::Column data)
{
    const auto before = processor.songs.column (songTrack);
    if (*before == data) return;
    const int track = songTrack;
    const bool looping = processor.songs.loop (track);
    processor.songs.replace (track, std::move (data));
    songUndo.push_back ({ track, before, processor.songs.column (track), looping, looping });
    if (songUndo.size() > 100) songUndo.erase (songUndo.begin());
    songRedo.clear();
    const auto updated = processor.songs.column (track);
    auto& position = songCursor[static_cast<size_t> (track)];
    position = updated->empty() ? 0 : juce::jlimit (0, static_cast<int> (updated->size()) - 1, position);
    songSelected[static_cast<size_t> (track)].clear();
    songShiftAnchor[static_cast<size_t> (track)] = -1;
    refreshSongEditor();
}

void LJuno116AudioProcessorEditor::songHistoryStep (bool redo)
{
    auto& source = redo ? songRedo : songUndo;
    auto& target = redo ? songUndo : songRedo;
    if (source.empty())
    {
        announceMessageFrom (sequencerEditorPanel, redo ? "Nothing to redo" : "Nothing to undo");
        return;
    }
    const auto action = source.back(); source.pop_back();
    processor.songs.restoreSnapshot (action.track, redo ? action.after : action.before);
    processor.songs.setLoop (action.track, redo ? action.loopAfter : action.loopBefore);
    target.push_back (action);
    songTrack = action.track;
    const auto snapshot = processor.songs.column (songTrack);
    auto& cursor = songCursor[static_cast<size_t> (songTrack)];
    cursor = snapshot->empty() ? 0 : juce::jlimit (0, static_cast<int> (snapshot->size()) - 1, cursor);
    songSelected[static_cast<size_t> (songTrack)].clear();
    songShiftAnchor[static_cast<size_t> (songTrack)] = -1;
    refreshSongEditor();
    announceMessageFrom (sequencerEditorPanel, redo ? "Song edit redone" : "Song edit undone");
}

bool LJuno116AudioProcessorEditor::handleSongEditorKey (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const auto mods = key.getModifiers();
    const auto letter = [code, &key] (juce::juce_wchar c)
    {
        return code == c || code == juce::CharacterFunctions::toUpperCase (c)
            || juce::CharacterFunctions::toLowerCase (key.getTextCharacter()) == c;
    };
    const auto lane = static_cast<size_t> (songTrack);
    auto data = *processor.songs.column (songTrack);
    auto& cursor = songCursor[lane];
    if (songConfirmClear)
    {
        if (code == juce::KeyPress::escapeKey)
        { songConfirmClear = false; refreshSongEditor(); announceMessageFrom (sequencerEditorPanel, "Clear cancelled"); return true; }
        if (code == juce::KeyPress::returnKey || (mods.isCtrlDown() && code == juce::KeyPress::deleteKey))
        { songConfirmClear = false; writeSongColumn ({}); announceMessageFrom (sequencerEditorPanel, layerName (songTrack) + ", Song column cleared"); return true; }
        return true;
    }
    if (code == juce::KeyPress::escapeKey) { closeSequencerEditor(); return true; }
    if (code == juce::KeyPress::tabKey) { announceSongRow (true); return true; }
    // Shift+Space alone toggles Song selection, including explicit single rows.
    // Ctrl+Space is not a selection shortcut; plain Space remains transport.
    const auto toggleSongRow = [&]()
    {
        if (data.empty()) return;
        const bool select = songSelected[lane].count (cursor) == 0;
        if (select) songSelected[lane].insert (cursor);
        else songSelected[lane].erase (cursor);
        songShiftAnchor[lane] = -1;
        refreshSongEditor();
        announceMessageFrom (sequencerEditorPanel,
            juce::String (select ? "Selected P" : "Deselected P")
            + juce::String (data[static_cast<size_t> (cursor)] + 1)
            + ", " + juce::String (cursor + 1) + " of " + juce::String (data.size()));
    };
    if (code == juce::KeyPress::spaceKey && mods.isShiftDown()
        && ! mods.isCtrlDown() && ! mods.isAltDown())
    { toggleSongRow(); return true; }
    if (mods.isAltDown())
    {
        if (letter ('r') && ! mods.isShiftDown())
        {
            changePreviewMode (sequencerEditorPanel);
            return true;
        }
        if (letter ('l'))
        {
            const bool old = processor.songs.loop (songTrack);
            processor.songs.setLoop (songTrack, ! old);
            const auto snapshot = processor.songs.column (songTrack);
            songUndo.push_back ({ songTrack, snapshot, snapshot, old, ! old });
            songRedo.clear(); refreshSongEditor();
            announceMessageFrom (sequencerEditorPanel, layerName (songTrack) + (old ? ", Loop Off" : ", Loop On"));
            return true;
        }
        if (letter ('i'))
        {
            const int pattern = data.empty() ? 0 : data[static_cast<size_t> (cursor)];
            const int at = data.empty() ? 0 : cursor + 1;
            data.insert (data.begin() + at, pattern);
            writeSongColumn (std::move (data)); cursor = at;
            refreshSongEditor();
            announceMessageFrom (sequencerEditorPanel, "Inserted P" + juce::String (pattern + 1)
                                + ", " + juce::String (at + 1));
            return true;
        }
        return true;
    }
    if (mods.isCtrlDown() || mods.isCommandDown())
    {
        if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ! mods.isShiftDown())
        {
            if (! data.empty())
            {
                const int old = data[static_cast<size_t> (cursor)];
                const int next = juce::jlimit (0, 15, old + (code == juce::KeyPress::upKey ? 1 : -1));
                if (next != old)
                {
                    data[static_cast<size_t> (cursor)] = next;
                    writeSongColumn (std::move (data)); announceSongRow();
                }
            }
            return true;
        }
        if (code == juce::KeyPress::deleteKey)
        {
            if (! data.empty())
            {
                songConfirmClear = true;
                announceMessageFrom (sequencerEditorPanel,
                    "Clear entire " + layerName (songTrack)
                    + " Song? Enter or Ctrl Delete confirms, Escape cancels");
            }
            return true;
        }
        if (letter ('z') || letter ('y')) { songHistoryStep (letter ('y') || mods.isShiftDown()); return true; }
        if (letter ('a'))
        {
            songSelected[lane].clear();
            for (int i = 0; i < static_cast<int> (data.size()); ++i) songSelected[lane].insert (i);
            songShiftAnchor[lane] = -1; refreshSongEditor();
            announceMessageFrom (sequencerEditorPanel, juce::String (data.size()) + " song elements selected");
            return true;
        }
        if (mods.isShiftDown() && (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey))
        { moveSongCursor (code == juce::KeyPress::upKey ? -1 : 1, true); return true; }
        if (letter ('c') || letter ('x'))
        {
            songClipboard.clear();
            if (data.empty()) return true;
            const auto selected = songSelected[lane];
            if (! selected.empty())
                for (int i : selected)
                    if (juce::isPositiveAndBelow (i, static_cast<int> (data.size())))
                        songClipboard.push_back (data[static_cast<size_t> (i)]);
            else songClipboard.push_back (data[static_cast<size_t> (cursor)]);
            juce::String encoded = clipboardHeader;
            for (int p : songClipboard) encoded << "," << (p + 1);
            juce::SystemClipboard::copyTextToClipboard (encoded);
            const int total = static_cast<int> (songClipboard.size());
            if (letter ('x'))
            {
                if (! selected.empty())
                    for (auto it = selected.rbegin(); it != selected.rend(); ++it)
                        if (juce::isPositiveAndBelow (*it, static_cast<int> (data.size())))
                            data.erase (data.begin() + *it);
                else data.erase (data.begin() + cursor);
                writeSongColumn (std::move (data));
            }
            songSelected[lane].clear(); refreshSongEditor();
            announceMessageFrom (sequencerEditorPanel,
                juce::String (total) + (letter ('x') ? " song elements cut" : " song elements copied"));
            return true;
        }
        if (letter ('v'))
        {
            const auto encoded = juce::SystemClipboard::getTextFromClipboard();
            std::vector<int> paste;
            if (encoded.startsWith (clipboardHeader))
            {
                const auto fields = juce::StringArray::fromTokens (
                    encoded.substring (juce::String (clipboardHeader).length()), ",", "");
                for (const auto& field : fields)
                {
                    if (field.isEmpty()) continue;
                    const int value = field.getIntValue();
                    if (value < 1 || value > 16 || field.trim() != juce::String (value))
                    { paste.clear(); break; }
                    paste.push_back (value - 1);
                }
            }
            if (paste.empty())
            {
                announceMessageFrom (sequencerEditorPanel, "Song clipboard empty or invalid");
                return true;
            }
            const int at = data.empty() ? 0 : cursor + 1;
            data.insert (data.begin() + at, paste.begin(), paste.end());
            writeSongColumn (std::move (data)); cursor = at;
            refreshSongEditor();
            announceMessageFrom (sequencerEditorPanel,
                juce::String (paste.size()) + " song elements inserted");
            return true;
        }
        return true;
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int next = juce::jlimit (0, 2, songTrack + (code == juce::KeyPress::rightKey ? 1 : -1));
        if (next != songTrack)
        {
            songTrack = next; refreshSongEditor(); announceSongRow (true);
        }
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    { moveSongCursor (code == juce::KeyPress::upKey ? -1 : 1, mods.isShiftDown()); return true; }
    if (code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey)
    { moveSongCursor (code == juce::KeyPress::pageUpKey ? -5 : 5, mods.isShiftDown()); return true; }
    if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
    {
        if (! data.empty())
            moveSongCursor ((code == juce::KeyPress::homeKey ? 0 : static_cast<int> (data.size()) - 1)
                            - cursor, mods.isShiftDown());
        return true;
    }
    if (code == juce::KeyPress::returnKey)
    { announceSongRow (true); return true; }
    if (code == juce::KeyPress::deleteKey)
    {
        if (data.empty()) return true;
        const auto selected = songSelected[lane];
        if (selected.size() == data.size() && selected.size() > 1)
        {
            songConfirmClear = true;
            announceMessageFrom (sequencerEditorPanel,
                "Clear entire " + layerName (songTrack)
                + " Song? Enter confirms, Escape cancels");
            return true;
        }
        if (! selected.empty())
        {
            for (auto it = selected.rbegin(); it != selected.rend(); ++it)
                if (juce::isPositiveAndBelow (*it, static_cast<int> (data.size())))
                    data.erase (data.begin() + *it);
        }
        else data.erase (data.begin() + cursor);
        writeSongColumn (std::move (data)); announceSongRow(); return true;
    }
    return true; // Song editor is keyboard modal.
}
