#include <inputrack/PluginBrowsing.h>
#include <algorithm>
#include <vector>

namespace inputrack {
namespace {
bool matchesSearch(const juce::PluginDescription& plugin, const juce::String& search)
{
    return plugin.name.containsIgnoreCase(search)
        || plugin.manufacturerName.containsIgnoreCase(search);
}

/** The field a sort method groups by, empty when it groups by nothing. */
juce::String groupingField(const juce::PluginDescription& plugin,
                           juce::KnownPluginList::SortMethod sort)
{
    if (sort == juce::KnownPluginList::sortByManufacturer) return plugin.manufacturerName;
    if (sort == juce::KnownPluginList::sortByCategory) return plugin.category;
    return {};
}

/*
 * Plug-ins that report no manufacturer or category would otherwise collect at
 * the top under a blank heading, which reads as if the list were broken. They
 * are pushed behind everything named instead, and never dropped.
 */
bool sortsBefore(const juce::PluginDescription& a, const juce::PluginDescription& b,
                 juce::KnownPluginList::SortMethod sort)
{
    const auto left = groupingField(a, sort);
    const auto right = groupingField(b, sort);
    if (left != right) {
        if (left.isEmpty()) return false;
        if (right.isEmpty()) return true;
        const auto byGroup = left.compareIgnoreCase(right);
        if (byGroup != 0) return byGroup < 0;
    }
    return a.name.compareIgnoreCase(b.name) < 0;
}
}

juce::Array<juce::PluginDescription> filterAndSortPlugins(
    const juce::Array<juce::PluginDescription>& types,
    const juce::String& search,
    juce::KnownPluginList::SortMethod sort)
{
    std::vector<juce::PluginDescription> matches;
    matches.reserve(static_cast<size_t>(types.size()));
    const auto trimmed = search.trim();
    for (const auto& plugin : types)
        if (trimmed.isEmpty() || matchesSearch(plugin, trimmed)) matches.push_back(plugin);

    std::stable_sort(matches.begin(), matches.end(),
                     [sort](const auto& a, const auto& b) { return sortsBefore(a, b, sort); });

    juce::Array<juce::PluginDescription> result;
    result.ensureStorageAllocated(static_cast<int>(matches.size()));
    for (auto& plugin : matches) result.add(std::move(plugin));
    return result;
}

juce::String pluginDisplayName(const juce::PluginDescription& plugin,
                               juce::KnownPluginList::SortMethod sort)
{
    const auto group = groupingField(plugin, sort);
    return group.isEmpty() ? plugin.name : group + " - " + plugin.name;
}
}
