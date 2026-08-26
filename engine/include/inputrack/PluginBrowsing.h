#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace inputrack {
/**
 * Narrows a scanned plug-in list to what the user is looking for.
 *
 * The search is case-insensitive and matches the name as well as the
 * manufacturer, so typing a vendor finds their effects even when the vendor
 * name is absent from the plug-in name itself. An empty search keeps
 * everything. Sorting is stable and always falls back to the name, so entries
 * within one manufacturer or category keep a predictable order.
 */
juce::Array<juce::PluginDescription> filterAndSortPlugins(
    const juce::Array<juce::PluginDescription>& types,
    const juce::String& search,
    juce::KnownPluginList::SortMethod);

/**
 * How one entry reads in the picker. Grouping is invisible in a flat list, so
 * sorting by manufacturer or category puts that field in front of the name.
 */
juce::String pluginDisplayName(const juce::PluginDescription&,
                               juce::KnownPluginList::SortMethod);
}
