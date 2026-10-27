// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// This is user-level browser navigation history, NOT plug-in/project state.
// Instantiating PropertiesFile for each read/write prevents an older instance
// from overwriting the last selection made by a different plug-in instance.
// Never read or write this file from the audio callback.
namespace preset_browser_memory
{
inline juce::PropertiesFile::Options options (const juce::String& application)
{
    juce::PropertiesFile::Options o;
    o.applicationName = application;
    o.folderName = "Lo-lo";
    o.filenameSuffix = "settings";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    o.osxLibrarySubFolder = "Application Support";
    o.millisecondsBeforeSaving = 0;
    return o;
}

inline juce::File recalled (const juce::File& library, const juce::String& application)
{
    juce::PropertiesFile properties (options (application));
    const auto relative = properties.getValue ("lastLoadedPreset");
    if (relative.isEmpty() || relative == ".") return {};
    const auto file = library.getChildFile (relative);
    return file.isAChildOf (library) && file.existsAsFile() ? file : juce::File {};
}

inline void remember (const juce::File& library, const juce::File& file,
                      const juce::String& application)
{
    if (! file.isAChildOf (library) || ! file.existsAsFile()) return;
    juce::PropertiesFile properties (options (application));
    properties.setValue ("lastLoadedPreset", file.getRelativePathFrom (library));
    properties.saveIfNeeded();
}

inline void remove (const juce::File& library, const juce::File& item,
                    const juce::String& application)
{
    juce::PropertiesFile properties (options (application));
    const auto relative = properties.getValue ("lastLoadedPreset");
    if (relative.isEmpty()) return;
    const auto file = library.getChildFile (relative);
    if (file == item || file.isAChildOf (item))
    {
        properties.removeValue ("lastLoadedPreset");
        properties.saveIfNeeded();
    }
}

// Update the remembered path when a preset OR its parent folder is renamed.
inline void renamed (const juce::File& library, const juce::File& oldItem,
                     const juce::File& newItem, const juce::String& application)
{
    juce::PropertiesFile properties (options (application));
    const auto relative = properties.getValue ("lastLoadedPreset");
    if (relative.isEmpty()) return;
    const auto previous = library.getChildFile (relative);
    juce::File updated;
    if (previous == oldItem) updated = newItem;
    else if (previous.isAChildOf (oldItem))
        updated = newItem.getChildFile (previous.getRelativePathFrom (oldItem));
    if (updated.existsAsFile() && updated.isAChildOf (library))
    {
        properties.setValue ("lastLoadedPreset", updated.getRelativePathFrom (library));
        properties.saveIfNeeded();
    }
}
}
