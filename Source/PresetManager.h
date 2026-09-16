// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace ljuno
{
class PresetManager
{
public:
    struct PatchSnapshot
    {
        std::vector<float> values;
        juce::String currentPresetRelativePath;
        bool hadCurrentPreset = false;

        bool isValid() const noexcept { return ! values.empty(); }
    };

    struct BrowserEntry
    {
        juce::File file;
        juce::String name;
        bool isDirectory = false;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState&,
                            juce::File libraryRoot = {});

    juce::Result ensureLibraryExists();
    juce::File getLibraryRoot() const { return root; }
    std::vector<BrowserEntry> listDirectory (const juce::File&) const;
    std::vector<juce::File> allPresetFiles() const;
    bool isInsideLibrary (const juce::File&) const;
    bool isValidPresetFile (const juce::File&) const;
    juce::File getCurrentPresetFile() const;

    juce::Result savePreset (const juce::String& name, const juce::File& directory,
                             juce::File& savedFile, bool overwriteExisting = false);
    juce::Result deletePreset (const juce::File&);
    juce::Result loadPreset (const juce::File&, juce::String& loadedName,
                             int& loadedNumber);
    juce::Result loadRelativePreset (int direction, juce::String& loadedName,
                                     int& loadedNumber);
    PatchSnapshot capturePatchSnapshot() const;
    void restorePatchSnapshot (const PatchSnapshot&);
    juce::Result previewPreset (const juce::File&);
    juce::Result commitPresetPreview (const juce::File&);

    static int getEmbeddedFactoryPresetCount();

private:
    struct ParsedPreset
    {
        juce::String name;
        std::vector<float> values;
        std::vector<bool> present;
        bool modernNoiseColorRange = false;
    };

    juce::AudioProcessorValueTreeState& state;
    juce::File root;

    static bool parseTextPreset (const juce::String&, ParsedPreset&);
    static std::vector<ParsedPreset> parseReaperLibrary (const juce::String&);
    static juce::String serialisePreset (const juce::String&,
                                         const std::vector<float>&);
    static juce::String nameWithoutExtension (const juce::File&);
    static bool naturalFileLess (const juce::File&, const juce::File&);
    std::vector<float> currentValues() const;
    juce::Result writePreset (const juce::String&, const std::vector<float>&,
                              const juce::File&) const;
    void applyValues (const std::vector<float>&);
    void rememberCurrentPreset (const juce::File&);
    juce::File recalledCurrentPreset() const;
};
}
