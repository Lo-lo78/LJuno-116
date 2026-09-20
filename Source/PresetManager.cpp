// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PresetManager.h"

#include "GeneratedParameters.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace ljuno
{
namespace
{
constexpr auto presetHeader = "LJuno-116 Patch Snapshot";
constexpr auto presetExtension = ".Ljuno";
constexpr auto presetWildcard = "*.Ljuno";
const juce::Identifier currentPresetProperty { "currentPresetRelativePath" };

bool factoryNoiseShouldBeStereo (const juce::String& name)
{
    return name == "Stefano percussione reverb"
        || name == "Stefano cassa con reverb"
        || name == "Stefano sparo"
        || name == "Stefano Claps";
}

int descriptorIndexForNameOrId (juce::String key)
{
    key = key.trim();
    if (key.startsWith ("[SYNTH] "))
        key = key.substring (8).trim();
    else if (key.startsWith ("[ARP] "))
        key = key.substring (6).trim();
    else if (key.startsWith ("[PARAMS] "))
        key = key.substring (9).trim();

    for (int index = 0; index < static_cast<int> (std::size (generated::parameters)); ++index)
    {
        const auto& descriptor = generated::parameters[static_cast<std::size_t> (index)];
        if (key == descriptor.name || key == descriptor.id)
            return index;
    }
    return -1;
}

void normaliseLegacyLfoDepths (std::vector<float>& values,
                               const std::vector<bool>* present = nullptr,
                               bool force = false)
{
    const auto indexFor = [] (const char* id)
    {
        return descriptorIndexForNameOrId (id);
    };
    const auto get = [&] (const char* id)
    {
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < values.size()
             ? values[static_cast<std::size_t> (index)] : 0.0f;
    };
    const auto set = [&] (const char* id, float value)
    {
        const auto index = indexFor (id);
        if (index < 0 || static_cast<std::size_t> (index) >= values.size())
            return;
        const auto& descriptor = generated::parameters[static_cast<std::size_t> (index)];
        values[static_cast<std::size_t> (index)] = juce::jlimit (
            descriptor.minimum, descriptor.maximum, value);
    };
    const auto wasPresent = [&] (const char* id)
    {
        if (present == nullptr)
            return false;
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < present->size()
            && (*present)[static_cast<std::size_t> (index)];
    };

    const auto migrateLfo = [&] (bool first)
    {
        const auto newPitchL1 = first ? "slider336" : "slider338";
        if (! force && wasPresent (newPitchL1))
            return;

        const auto legacyVolume = first ? "slider028" : "slider038";
        const auto legacyLp = first ? "slider029" : "slider039";
        const auto legacyPan = first ? "slider030" : "slider040";
        const auto legacyPitch = first ? "slider031" : "slider041";
        const auto legacyPwm = first ? "slider033" : "slider043";
        const auto legacyHp = first ? "slider034" : "slider044";
        const auto volumeL1 = first ? "slider120" : "slider121";
        const auto volumeL2 = first ? "slider122" : "slider123";
        const auto noisePitch = first ? "slider282" : "slider283";
        const auto pitchL2 = first ? "slider337" : "slider339";
        const auto volumeNoise = first ? "slider340" : "slider341";
        const auto panL1 = first ? "slider342" : "slider345";
        const auto panL2 = first ? "slider343" : "slider346";
        const auto panNoise = first ? "slider344" : "slider347";
        const auto lpL1 = first ? "slider348" : "slider351";
        const auto lpL2 = first ? "slider349" : "slider352";
        const auto lpNoise = first ? "slider350" : "slider353";
        const auto hpL1 = first ? "slider354" : "slider357";
        const auto hpL2 = first ? "slider355" : "slider358";
        const auto hpNoise = first ? "slider356" : "slider359";
        const auto pwmL1 = first ? "slider360" : "slider362";
        const auto pwmL2 = first ? "slider361" : "slider363";

        const auto volume = get (legacyVolume);
        const auto lowPass = get (legacyLp);
        const auto pan = get (legacyPan);
        const auto pitch = get (legacyPitch);
        const auto pwm = get (legacyPwm);
        const auto highPass = get (legacyHp);

        set (newPitchL1, get (newPitchL1) + pitch);
        set (pitchL2, get (pitchL2) + pitch);
        set (noisePitch, get (noisePitch) + pitch);
        set (volumeL1, get (volumeL1) + volume);
        set (volumeL2, get (volumeL2) + volume);
        set (volumeNoise, get (volumeNoise) + volume);
        set (panL1, get (panL1) + pan);
        set (panL2, get (panL2) + pan);
        set (panNoise, get (panNoise) + pan);
        set (lpL1, get (lpL1) + lowPass);
        set (lpL2, get (lpL2) + lowPass);
        set (lpNoise, get (lpNoise) + lowPass);
        set (hpL1, get (hpL1) + highPass);
        set (hpL2, get (hpL2) + highPass);
        set (hpNoise, get (hpNoise) + highPass);
        set (pwmL1, get (pwmL1) + pwm);
        set (pwmL2, get (pwmL2) + pwm);

        set (legacyVolume, 0.0f);
        set (legacyLp, 0.0f);
        set (legacyPan, 0.0f);
        set (legacyPitch, 0.0f);
        set (legacyPwm, 0.0f);
        set (legacyHp, 0.0f);
    };

    migrateLfo (true);
    migrateLfo (false);
}


void normaliseLegacyPerformanceControls (std::vector<float>& values,
                                         const std::vector<bool>* present = nullptr)
{
    const auto indexFor = [] (const char* id) { return descriptorIndexForNameOrId (id); };
    const auto get = [&] (const char* id, float fallback)
    {
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < values.size()
            ? values[static_cast<std::size_t> (index)] : fallback;
    };
    const auto wasPresent = [&] (const char* id)
    {
        if (present == nullptr)
            return false;
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < present->size()
            && (*present)[static_cast<std::size_t> (index)];
    };
    const auto set = [&] (const char* id, float value)
    {
        const auto index = indexFor (id);
        if (index < 0 || static_cast<std::size_t> (index) >= values.size())
            return;
        const auto& descriptor = generated::parameters[static_cast<std::size_t> (index)];
        values[static_cast<std::size_t> (index)] = juce::jlimit (
            descriptor.minimum, descriptor.maximum, value);
    };

    if (! wasPresent ("slider381"))
    {
        const auto portamento = get ("slider009", 0.0f);
        set ("slider381", portamento);
        set ("slider382", portamento);
    }
    if (! wasPresent ("slider386"))
    {
        const auto l1Portamento = get ("slider381", 0.0f);
        const auto l2Portamento = get ("slider382", l1Portamento);
        set ("slider386", std::abs (l1Portamento - l2Portamento) <= 0.000001f
                           ? l1Portamento : 0.0f);
    }
    if (! wasPresent ("slider383"))
    {
        const auto velocity = get ("slider057", 0.0f);
        set ("slider383", velocity);
        set ("slider384", velocity);
        set ("slider385", velocity);
    }
}

void normaliseLegacyNoteSources (std::vector<float>& values,
                                     const std::vector<bool>* present = nullptr)
{
    const auto indexFor = [] (const char* id) { return descriptorIndexForNameOrId (id); };
    const auto get = [&] (const char* id, float fallback)
    {
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < values.size()
            ? values[static_cast<std::size_t> (index)] : fallback;
    };
    const auto wasPresent = [&] (const char* id)
    {
        if (present == nullptr)
            return false;
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < present->size()
            && (*present)[static_cast<std::size_t> (index)];
    };
    const auto set = [&] (const char* id, float value)
    {
        const auto index = indexFor (id);
        if (index < 0 || static_cast<std::size_t> (index) >= values.size())
            return;
        const auto& descriptor = generated::parameters[static_cast<std::size_t> (index)];
        values[static_cast<std::size_t> (index)] = juce::jlimit (
            descriptor.minimum, descriptor.maximum, value);
    };

    if (wasPresent ("slider392"))
        return;

    // Before Note Source existed, Sequencer had priority over LArp. Preserve the
    // audible generator used by older presets when one of them played LJuno.
    const auto sequencerState = juce::roundToInt (get ("slider313", 0.0f));
    const auto larpState = juce::roundToInt (get ("slider202", 0.0f));
    const auto source = (sequencerState == 1 || sequencerState == 3) ? 1.0f
                      : (larpState == 1 || larpState == 3) ? 2.0f
                                                          : 0.0f;
    set ("slider392", source);
    set ("slider393", source);
    set ("slider394", source);
}

void normaliseLegacyRoutingModes (std::vector<float>& values, int revision)
{
    if (revision >= 2)
        return;

    const auto indexFor = [] (const char* id) { return descriptorIndexForNameOrId (id); };
    const auto get = [&] (const char* id, float fallback)
    {
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < values.size()
            ? values[static_cast<std::size_t> (index)] : fallback;
    };
    const auto set = [&] (const char* id, float value)
    {
        const auto index = indexFor (id);
        if (index < 0 || static_cast<std::size_t> (index) >= values.size())
            return;
        values[static_cast<std::size_t> (index)] = value;
    };
    const auto remap = [] (int oldMode)
    {
        // Historical: 0 Off/Direct, 1 Synth+MIDI, 2 MIDI Only, 3 Synth + MIDI Direct.
        // New:        0 Synth+MIDI, 1 Synth + MIDI Direct, 2 MIDI Only.
        if (oldMode == 3) return 1;
        if (oldMode == 2) return 2;
        return 0;
    };

    set ("slider202", static_cast<float> (remap (juce::roundToInt (get ("slider202", 0.0f)))));
    set ("slider313", static_cast<float> (remap (juce::roundToInt (get ("slider313", 0.0f)))));
}

void normaliseLegacyFxSends (std::vector<float>& values,
                             const std::vector<bool>* present = nullptr)
{
    const auto indexFor = [] (const char* id) { return descriptorIndexForNameOrId (id); };
    const auto get = [&] (const char* id, float fallback)
    {
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < values.size()
            ? values[static_cast<std::size_t> (index)] : fallback;
    };
    const auto wasPresent = [&] (const char* id)
    {
        if (present == nullptr)
            return false;
        const auto index = indexFor (id);
        return index >= 0 && static_cast<std::size_t> (index) < present->size()
            && (*present)[static_cast<std::size_t> (index)];
    };
    const auto set = [&] (const char* id, float value)
    {
        const auto index = indexFor (id);
        if (index < 0 || static_cast<std::size_t> (index) >= values.size())
            return;
        const auto& descriptor = generated::parameters[static_cast<std::size_t> (index)];
        values[static_cast<std::size_t> (index)] = juce::jlimit (
            descriptor.minimum, descriptor.maximum, value);
    };

    // Presence of the first new send identifies presets written by the new
    // architecture. Older presets are upgraded from their single wet controls.
    if (wasPresent ("slider365"))
        return;

    const auto chorus = get ("slider060", 0.4f);
    const auto delayMode = juce::roundToInt (get ("slider100", 1.0f));
    const auto delay = delayMode == 2 ? get ("slider309", 0.25f)
                                      : get ("slider103", 0.25f);
    const auto reverb = get ("slider147", 0.2f);
    for (const auto* id : { "slider365", "slider366", "slider367" }) set (id, chorus);
    for (const auto* id : { "slider368", "slider369", "slider370" }) set (id, delay);
    for (const auto* id : { "slider371", "slider372", "slider373" }) set (id, reverb);
}

std::vector<juce::String> tokeniseReaperState (const juce::String& text)
{
    std::vector<juce::String> tokens;
    juce::String current;
    bool quoted = false;
    for (const auto character : text)
    {
        if (character == '"')
        {
            quoted = ! quoted;
            continue;
        }
        if (! quoted && juce::CharacterFunctions::isWhitespace (character))
        {
            if (current.isNotEmpty())
            {
                tokens.push_back (current);
                current.clear();
            }
            continue;
        }
        current += character;
    }
    if (current.isNotEmpty())
        tokens.push_back (current);
    return tokens;
}

juce::String embeddedRplText()
{
    int size = 0;
    if (const auto* data = BinaryData::getNamedResource ("LJuno116_jsfx_rpl", size))
        return juce::String::fromUTF8 (data, size);
    return {};
}
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& stateToUse,
                              juce::File libraryRoot,
                              ExtraStateGetter getter,
                              ExtraStateSetter setter)
    : state (stateToUse),
      root (libraryRoot == juce::File()
              ? juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("LJuno-116")
              : std::move (libraryRoot)),
      extraStateGetter (std::move (getter)),
      extraStateSetter (std::move (setter))
{
}

juce::Result PresetManager::ensureLibraryExists()
{
    if (const auto result = root.createDirectory(); result.failed())
        return juce::Result::fail ("Cannot create the LJuno-116 preset folder");

    const auto factory = root.getChildFile ("Factory");
    if (const auto result = factory.createDirectory(); result.failed())
        return juce::Result::fail ("Cannot create the Factory preset folder");

    // Version 5 synchronises the managed Factory bank and guarantees that
    // parameters not physically present in the 256-slider RPL stay at their
    // declaration defaults (including neutral Micro Motion depths).
    // User presets outside Factory are never touched. Earlier versions only
    // added missing files, which left the old numbering in place when a preset
    // was inserted in the middle of the RPL.
    const auto marker = root.getChildFile (".factory-rpl-5-ljuno-installed");
    if (! marker.existsAsFile())
    {
        const auto factoryPresets = parseReaperLibrary (embeddedRplText());
        if (factoryPresets.empty())
            return juce::Result::fail ("The embedded REAPER preset library is invalid");

        juce::String recalledFactoryName;
        const auto recalledFile = recalledCurrentPreset();
        if (recalledFile.isAChildOf (factory) && recalledFile.existsAsFile())
        {
            ParsedPreset recalledPreset;
            if (parseTextPreset (recalledFile.loadFileAsString(), recalledPreset))
                recalledFactoryName = recalledPreset.name;
        }

        const auto noiseStereoIndex = descriptorIndexForNameOrId ("slider284");
        std::vector<juce::File> installedFiles;
        installedFiles.reserve (factoryPresets.size());
        for (std::size_t index = 0; index < factoryPresets.size(); ++index)
        {
            auto preset = factoryPresets[index];
            if (noiseStereoIndex >= 0 && factoryNoiseShouldBeStereo (preset.name))
                preset.values[static_cast<std::size_t> (noiseStereoIndex)] = 1.0f;
            auto legalName = juce::File::createLegalFileName (preset.name.trim());
            if (legalName.isEmpty())
                legalName = "Factory preset";
            const auto order = juce::String (static_cast<int> (index) + 1).paddedLeft ('0', 3);
            const auto destination = factory.getChildFile (order + " - " + legalName
                                                            + presetExtension);
            if (const auto result = writePreset (preset.name.trim(), preset.values, destination);
                result.failed())
                return result;
            installedFiles.push_back (destination);
        }

        // Remove only numbered files managed by the factory importer. This
        // clears obsolete numbering while leaving any unnumbered custom file.
        const auto previousFactoryFiles = factory.findChildFiles (
            juce::File::findFiles, false, presetWildcard,
            juce::File::FollowSymlinks::no);
        for (const auto& file : previousFactoryFiles)
        {
            const auto name = file.getFileName();
            const auto isManaged = name.length() > 6
                && name.substring (0, 3).containsOnly ("0123456789")
                && name.substring (3, 6) == " - ";
            if (isManaged && std::find (installedFiles.begin(), installedFiles.end(), file)
                                  == installedFiles.end()
                && ! file.deleteFile())
                return juce::Result::fail ("An obsolete Factory preset could not be removed");
        }

        if (recalledFactoryName.isNotEmpty())
        {
            for (std::size_t index = 0; index < factoryPresets.size(); ++index)
                if (factoryPresets[index].name.trim() == recalledFactoryName.trim())
                {
                    state.state.setProperty (currentPresetProperty,
                        installedFiles[index].getRelativePathFrom (root), nullptr);
                    break;
                }
        }

        if (! marker.replaceWithText (
                "LJuno-116 Factory bank synchronised with embedded RPL version 5.\n"))
            return juce::Result::fail ("Factory presets were written, but their installation marker could not be saved");
    }

    // Versioned, one-time correction for factory patches that were originally
    // designed with independent left/right JSFX noise. This also repairs
    // already installed Factory files without touching user presets.
    const auto stereoMarker = root.getChildFile (".factory-noise-stereo-1-installed");
    if (! stereoMarker.existsAsFile())
    {
        const auto noiseStereoIndex = descriptorIndexForNameOrId ("slider284");
        const auto files = factory.findChildFiles (juce::File::findFiles, false,
                                                   presetWildcard,
                                                   juce::File::FollowSymlinks::no);
        for (const auto& file : files)
        {
            ParsedPreset parsed;
            if (noiseStereoIndex >= 0 && parseTextPreset (file.loadFileAsString(), parsed)
                && factoryNoiseShouldBeStereo (parsed.name))
            {
                parsed.values[static_cast<std::size_t> (noiseStereoIndex)] = 1.0f;
                if (const auto result = writePreset (parsed.name, parsed.values, file);
                    result.failed())
                    return result;
            }
        }
        if (! stereoMarker.replaceWithText ("Factory presets 54-57 use stereo noise.\n"))
            return juce::Result::fail ("Factory noise stereo correction could not be saved");
    }
    return juce::Result::ok();
}

bool PresetManager::naturalFileLess (const juce::File& first, const juce::File& second)
{
    return first.getFileName().compareNatural (second.getFileName(), false) < 0;
}

std::vector<PresetManager::BrowserEntry> PresetManager::listDirectory (
    const juce::File& requestedDirectory) const
{
    std::vector<BrowserEntry> result;
    if (! isInsideLibrary (requestedDirectory) || ! requestedDirectory.isDirectory())
        return result;

    auto directories = requestedDirectory.findChildFiles (
        juce::File::findDirectories | juce::File::ignoreHiddenFiles, false, "*",
        juce::File::FollowSymlinks::no);
    std::sort (directories.begin(), directories.end(), naturalFileLess);
    for (const auto& directory : directories)
        result.push_back ({ directory, directory.getFileName(), true });

    auto files = requestedDirectory.findChildFiles (
        juce::File::findFiles | juce::File::ignoreHiddenFiles, false, presetWildcard,
        juce::File::FollowSymlinks::no);
    std::sort (files.begin(), files.end(), naturalFileLess);
    for (const auto& file : files)
        if (isValidPresetFile (file))
            result.push_back ({ file, nameWithoutExtension (file), false });
    return result;
}

std::vector<juce::File> PresetManager::allPresetFiles() const
{
    std::vector<juce::File> result;
    if (! root.isDirectory())
        return result;
    const auto files = root.findChildFiles (
        juce::File::findFiles | juce::File::ignoreHiddenFiles, true, presetWildcard,
        juce::File::FollowSymlinks::no);
    for (const auto& file : files)
        if (isInsideLibrary (file) && isValidPresetFile (file))
            result.push_back (file);
    const auto factory = root.getChildFile ("Factory");
    std::sort (result.begin(), result.end(), [this, factory] (const auto& first, const auto& second)
    {
        const auto firstIsFactory = first.isAChildOf (factory);
        const auto secondIsFactory = second.isAChildOf (factory);
        if (firstIsFactory != secondIsFactory)
            return firstIsFactory;
        return first.getRelativePathFrom (root).compareNatural (
                   second.getRelativePathFrom (root), false) < 0;
    });
    return result;
}

bool PresetManager::isInsideLibrary (const juce::File& file) const
{
    return file == root || file.isAChildOf (root);
}

bool PresetManager::isValidPresetFile (const juce::File& file) const
{
    if (! file.existsAsFile() || ! file.hasFileExtension ("Ljuno") || ! isInsideLibrary (file))
        return false;
    ParsedPreset parsed;
    return parseTextPreset (file.loadFileAsString(), parsed);
}

juce::File PresetManager::getCurrentPresetFile() const
{
    const auto file = recalledCurrentPreset();
    return isValidPresetFile (file) ? file : juce::File {};
}

juce::Result PresetManager::savePreset (const juce::String& requestedName,
                                        const juce::File& requestedDirectory,
                                        juce::File& savedFile,
                                        bool overwriteExisting)
{
    if (const auto result = ensureLibraryExists(); result.failed())
        return result;
    const auto directory = isInsideLibrary (requestedDirectory) && requestedDirectory.isDirectory()
                         ? requestedDirectory : root;
    auto name = requestedName.trim();
    if (name.endsWithIgnoreCase (presetExtension))
        name = name.dropLastCharacters (juce::String (presetExtension).length()).trim();
    else if (name.endsWithIgnoreCase (".txt"))
        name = name.dropLastCharacters (4).trim();
    name = juce::File::createLegalFileName (name);
    if (name.isEmpty() || name == "." || name == "..")
        return juce::Result::fail ("Enter a valid preset name");

    savedFile = directory.getChildFile (name + presetExtension);
    if (! isInsideLibrary (savedFile))
        return juce::Result::fail ("The preset must remain inside the LJuno-116 folder");
    if (savedFile.existsAsFile() && ! overwriteExisting)
        return juce::Result::fail ("A preset with this name already exists");

    const auto sequencerData = extraStateGetter ? extraStateGetter() : juce::String {};
    if (const auto result = writePreset (name, currentValues(), savedFile, sequencerData); result.failed())
        return result;
    rememberCurrentPreset (savedFile);
    return juce::Result::ok();
}

juce::Result PresetManager::deletePreset (const juce::File& file)
{
    if (! isValidPresetFile (file))
        return juce::Result::fail ("Select a valid LJuno-116 preset to delete");

    const auto wasCurrentPreset = recalledCurrentPreset() == file;
    if (! file.deleteFile())
        return juce::Result::fail ("Cannot delete preset "
                                   + nameWithoutExtension (file));

    if (wasCurrentPreset)
        state.state.removeProperty (currentPresetProperty, nullptr);
    return juce::Result::ok();
}

juce::Result PresetManager::loadPreset (const juce::File& file,
                                        juce::String& loadedName, int& loadedNumber)
{
    if (const auto result = ensureLibraryExists(); result.failed())
        return result;
    if (! isValidPresetFile (file))
        return juce::Result::fail ("This is not a valid LJuno-116 preset");
    ParsedPreset parsed;
    if (! parseTextPreset (file.loadFileAsString(), parsed))
        return juce::Result::fail ("This is not a valid LJuno-116 preset");

    applyValues (parsed.values);
    if (extraStateSetter)
        extraStateSetter (parsed.sequencerData);
    rememberCurrentPreset (file);
    loadedName = nameWithoutExtension (file);
    const auto files = allPresetFiles();
    const auto found = std::find (files.begin(), files.end(), file);
    loadedNumber = found == files.end() ? 0
                                       : static_cast<int> (std::distance (files.begin(), found)) + 1;
    return juce::Result::ok();
}

juce::Result PresetManager::loadRelativePreset (int direction,
                                                juce::String& loadedName,
                                                int& loadedNumber)
{
    if (const auto result = ensureLibraryExists(); result.failed())
        return result;
    const auto files = allPresetFiles();
    if (files.empty())
        return juce::Result::fail ("No presets found");

    const auto current = recalledCurrentPreset();
    const auto found = std::find (files.begin(), files.end(), current);
    int index = found == files.end()
              ? (direction >= 0 ? 0 : static_cast<int> (files.size()) - 1)
              : static_cast<int> (std::distance (files.begin(), found));
    if (found != files.end())
    {
        const auto count = static_cast<int> (files.size());
        index = (index + (direction >= 0 ? 1 : -1) + count) % count;
    }
    return loadPreset (files[static_cast<std::size_t> (index)], loadedName, loadedNumber);
}

PresetManager::PatchSnapshot PresetManager::capturePatchSnapshot() const
{
    PatchSnapshot snapshot;
    snapshot.values = currentValues();
    snapshot.hadCurrentPreset = state.state.hasProperty (currentPresetProperty);
    snapshot.currentPresetRelativePath = state.state.getProperty (
        currentPresetProperty).toString();
    snapshot.sequencerData = extraStateGetter ? extraStateGetter() : juce::String {};
    return snapshot;
}

void PresetManager::restorePatchSnapshot (const PatchSnapshot& snapshot)
{
    if (snapshot.values.size() != std::size (generated::parameters))
        return;

    applyValues (snapshot.values);
    if (extraStateSetter)
        extraStateSetter (snapshot.sequencerData);
    if (snapshot.hadCurrentPreset)
        state.state.setProperty (currentPresetProperty,
                                 snapshot.currentPresetRelativePath, nullptr);
    else
        state.state.removeProperty (currentPresetProperty, nullptr);
}

juce::Result PresetManager::previewPreset (const juce::File& file)
{
    if (const auto result = ensureLibraryExists(); result.failed())
        return result;
    if (! isValidPresetFile (file))
        return juce::Result::fail ("This is not a valid LJuno-116 preset");

    ParsedPreset parsed;
    if (! parseTextPreset (file.loadFileAsString(), parsed))
        return juce::Result::fail ("This is not a valid LJuno-116 preset");

    // Preview changes the sound but deliberately leaves the recalled preset
    // path untouched. Enter commits that path later without loading twice.
    applyValues (parsed.values);
    if (extraStateSetter)
        extraStateSetter (parsed.sequencerData);
    return juce::Result::ok();
}

juce::Result PresetManager::commitPresetPreview (const juce::File& file)
{
    // Do not parse or apply the file here: the selected preset is already
    // sounding. This operation only makes it the current preset.
    if (! file.existsAsFile() || ! file.hasFileExtension ("Ljuno")
        || ! isInsideLibrary (file))
        return juce::Result::fail ("The previewed preset is no longer available");

    rememberCurrentPreset (file);
    return juce::Result::ok();
}

bool PresetManager::parseTextPreset (const juce::String& text, ParsedPreset& parsed)
{
    parsed.values.resize (std::size (generated::parameters));
    parsed.present.assign (std::size (generated::parameters), false);
    for (std::size_t index = 0; index < std::size (generated::parameters); ++index)
        parsed.values[index] = generated::parameters[index].defaultValue;

    const auto lines = juce::StringArray::fromLines (text);
    auto headerFound = false;
    auto mapped = 0;
    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();
        if (line.isEmpty())
            continue;
        if (! headerFound)
        {
            if (line != presetHeader)
                return false;
            headerFound = true;
            continue;
        }
        const auto separator = line.indexOfChar (':');
        if (separator <= 0)
            continue;
        const auto key = line.substring (0, separator).trim();
        const auto textValue = line.substring (separator + 1).trim();
        if (key == "Name" || key == "[STATE] Preset Name")
        {
            parsed.name = textValue;
            continue;
        }
        if (key == "[STATE] Noise Color Range")
        {
            parsed.modernNoiseColorRange = (textValue == "0..1");
            continue;
        }
        if (key == "[STATE] Routing Mode Revision")
        {
            parsed.routingModeRevision = textValue.getIntValue();
            continue;
        }
        if (key == "[SEQUENCER] Data")
        {
            parsed.sequencerData = textValue;
            continue;
        }
        const auto descriptorIndex = descriptorIndexForNameOrId (key);
        if (descriptorIndex < 0 || ! textValue.containsOnly ("0123456789+-.eE"))
            continue;
        const auto& descriptor = generated::parameters[static_cast<std::size_t> (descriptorIndex)];
        const auto value = static_cast<float> (textValue.getDoubleValue());
        if (! std::isfinite (value))
            continue;
        const auto legacyRoutingValue = parsed.routingModeRevision < 2
            && (descriptor.sliderNumber == 202 || descriptor.sliderNumber == 313);
        parsed.values[static_cast<std::size_t> (descriptorIndex)] = legacyRoutingValue
            ? juce::jlimit (0.0f, 3.0f, value)
            : juce::jlimit (descriptor.minimum, descriptor.maximum, value);
        parsed.present[static_cast<std::size_t> (descriptorIndex)] = true;
        ++mapped;
    }
    if (! parsed.modernNoiseColorRange)
    {
        for (std::size_t index = 0; index < std::size (generated::parameters); ++index)
        {
            if (generated::parameters[index].sliderNumber == 51 && parsed.present[index])
            {
                parsed.values[index] = juce::jlimit (0.0f, 1.0f,
                                                     (parsed.values[index] + 1.0f) * 0.5f);
                break;
            }
        }
    }
    normaliseLegacyLfoDepths (parsed.values, &parsed.present);
    normaliseLegacyFxSends (parsed.values, &parsed.present);
    normaliseLegacyPerformanceControls (parsed.values, &parsed.present);
    normaliseLegacyNoteSources (parsed.values, &parsed.present);
    normaliseLegacyRoutingModes (parsed.values, parsed.routingModeRevision);
    return headerFound && mapped > 0;
}

std::vector<PresetManager::ParsedPreset> PresetManager::parseReaperLibrary (
    const juce::String& text)
{
    std::vector<ParsedPreset> presets;
    const auto lines = juce::StringArray::fromLines (text);
    juce::String currentName, base64;
    auto insidePreset = false;
    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();
        if (! insidePreset && line.startsWith ("<PRESET `"))
        {
            const auto closing = line.indexOfChar (9, '`');
            if (closing > 9)
            {
                currentName = line.substring (9, closing).trim();
                base64.clear();
                insidePreset = true;
            }
            continue;
        }
        if (! insidePreset)
            continue;
        if (line == ">")
        {
            juce::MemoryOutputStream decoded;
            if (juce::Base64::convertFromBase64 (decoded, base64))
            {
                const auto stateText = juce::String::fromUTF8 (
                    static_cast<const char*> (decoded.getData()),
                    static_cast<int> (decoded.getDataSize()));
                const auto tokens = tokeniseReaperState (stateText);
                if (tokens.size() >= 257)
                {
                    ParsedPreset preset;
                    preset.name = currentName;
                    preset.values.resize (std::size (generated::parameters));
                    preset.present.assign (std::size (generated::parameters), false);
                    for (std::size_t index = 0; index < std::size (generated::parameters); ++index)
                    {
                        const auto& descriptor = generated::parameters[index];
                        preset.values[index] = descriptor.defaultValue;
                        if (descriptor.sliderNumber > 256)
                            continue;
                        const auto tokenIndex = descriptor.sliderNumber <= 64
                                              ? descriptor.sliderNumber - 1
                                              : descriptor.sliderNumber;
                        if (! juce::isPositiveAndBelow (tokenIndex,
                                                        static_cast<int> (tokens.size())))
                            continue;
                        const auto& token = tokens[static_cast<std::size_t> (tokenIndex)];
                        if (token == "-" || ! token.containsOnly ("0123456789+-.eE"))
                            continue;
                        const auto value = static_cast<float> (token.getDoubleValue());
                        if (std::isfinite (value))
                        {
                            const auto migratedValue = descriptor.sliderNumber == 51
                                                     ? (value + 1.0f) * 0.5f : value;
                            preset.values[index] = juce::jlimit (
                                descriptor.minimum, descriptor.maximum, migratedValue);
                            preset.present[index] = true;
                        }
                    }
                    normaliseLegacyLfoDepths (preset.values, &preset.present);
                    normaliseLegacyFxSends (preset.values, &preset.present);
                    normaliseLegacyPerformanceControls (preset.values, &preset.present);
                    normaliseLegacyNoteSources (preset.values, &preset.present);
                    normaliseLegacyRoutingModes (preset.values, 0);
                    if (factoryNoiseShouldBeStereo (preset.name))
                    {
                        const auto stereoIndex = descriptorIndexForNameOrId ("slider284");
                        if (stereoIndex >= 0)
                            preset.values[static_cast<std::size_t> (stereoIndex)] = 1.0f;
                    }
                    presets.push_back (std::move (preset));
                }
            }
            currentName.clear();
            base64.clear();
            insidePreset = false;
            continue;
        }
        base64 += line;
    }
    return presets;
}

juce::String PresetManager::serialisePreset (const juce::String& name,
                                             const std::vector<float>& values,
                                             const juce::String& sequencerData)
{
    juce::String text = juce::String (presetHeader) + "\n\n";
    text += "[STATE] Preset Name: " + name + "\n";
    text += "[STATE] Saved At: "
         + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") + "\n\n";
    text += "[STATE] Noise Color Range: 0..1\n";
    text += "[STATE] Routing Mode Revision: 2\n\n";
    if (sequencerData.isNotEmpty())
        text += "[SEQUENCER] Data: " + sequencerData + "\n\n";
    for (std::size_t index = 0;
         index < std::min (values.size(), std::size (generated::parameters)); ++index)
    {
        text += "[SYNTH] " + juce::String (generated::parameters[index].name)
             + ": " + juce::String (values[index], 9) + "\n";
    }
    return text;
}

juce::String PresetManager::nameWithoutExtension (const juce::File& file)
{
    auto name = file.getFileNameWithoutExtension().trim();
    if (name.length() > 6 && name.substring (0, 3).containsOnly ("0123456789")
        && name.substring (3, 6) == " - ")
        name = name.substring (6).trim();
    return name;
}

std::vector<float> PresetManager::currentValues() const
{
    std::vector<float> values;
    values.reserve (std::size (generated::parameters));
    for (const auto& descriptor : generated::parameters)
    {
        if (const auto* parameter = state.getParameter (descriptor.id))
            values.push_back (parameter->convertFrom0to1 (parameter->getValue()));
        else
            values.push_back (descriptor.defaultValue);
    }
    normaliseLegacyLfoDepths (values, nullptr, true);
    return values;
}

juce::Result PresetManager::writePreset (const juce::String& name,
                                         const std::vector<float>& values,
                                         const juce::File& destination,
                                         const juce::String& sequencerData) const
{
    if (! destination.replaceWithText (serialisePreset (name, values, sequencerData)))
        return juce::Result::fail ("Cannot write the preset file");
    return juce::Result::ok();
}

void PresetManager::applyValues (const std::vector<float>& values)
{
    if (values.size() != std::size (generated::parameters))
        return;

    for (std::size_t index = 0; index < std::size (generated::parameters); ++index)
    {
        const auto& descriptor = generated::parameters[index];
        if (auto* parameter = state.getParameter (descriptor.id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (values[index]));
            parameter->endChangeGesture();
        }
    }
}

void PresetManager::rememberCurrentPreset (const juce::File& file)
{
    state.state.setProperty (currentPresetProperty, file.getRelativePathFrom (root), nullptr);
}

juce::File PresetManager::recalledCurrentPreset() const
{
    const auto relative = state.state.getProperty (currentPresetProperty).toString();
    return relative.isEmpty() ? juce::File() : root.getChildFile (relative);
}

int PresetManager::getEmbeddedFactoryPresetCount()
{
    return static_cast<int> (parseReaperLibrary (embeddedRplText()).size());
}
}
