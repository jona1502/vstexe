#include <vocalchain/PluginBrowsing.h>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

juce::PluginDescription make(const juce::String& name, const juce::String& manufacturer,
                             const juce::String& category = {})
{
    juce::PluginDescription plugin;
    plugin.name = name;
    plugin.manufacturerName = manufacturer;
    plugin.category = category;
    plugin.pluginFormatName = "VST3";
    return plugin;
}

juce::StringArray namesOf(const juce::Array<juce::PluginDescription>& plugins)
{
    juce::StringArray names;
    for (const auto& plugin : plugins) names.add(plugin.name);
    return names;
}
}

int main()
{
    using Sort = juce::KnownPluginList;

    juce::Array<juce::PluginDescription> all;
    all.add(make("Pro-Q 3", "FabFilter", "EQ"));
    all.add(make("Anthem", "Zynaptiq", "Dynamics"));
    all.add(make("Pro-C 2", "FabFilter", "Dynamics"));
    all.add(make("Nectar", "iZotope", "EQ"));
    all.add(make("Orphan Tool", "", "EQ"));

    // An empty search keeps everything; nothing may quietly disappear.
    expect(vocalchain::filterAndSortPlugins(all, "", Sort::sortAlphabetically).size() == 5,
           "an empty search keeps every plug-in");
    expect(vocalchain::filterAndSortPlugins(all, "   ", Sort::sortAlphabetically).size() == 5,
           "a whitespace-only search counts as empty");

    // Matching the manufacturer is the point: "fabfilter" appears in no name.
    const auto byVendor = vocalchain::filterAndSortPlugins(all, "fabfilter", Sort::sortAlphabetically);
    expect(byVendor.size() == 2, "a manufacturer match finds its plug-ins");
    expect(namesOf(byVendor).joinIntoString(",") == "Pro-C 2,Pro-Q 3",
           "manufacturer matches are sorted by name");

    const auto byName = vocalchain::filterAndSortPlugins(all, "PRO-Q", Sort::sortAlphabetically);
    expect(byName.size() == 1 && byName[0].name == "Pro-Q 3", "matching ignores case");

    expect(vocalchain::filterAndSortPlugins(all, "nothing here", Sort::sortAlphabetically).isEmpty(),
           "a search without matches yields an empty list");

    // Alphabetical order ignores the manufacturer entirely.
    expect(namesOf(vocalchain::filterAndSortPlugins(all, "", Sort::sortAlphabetically))
                   .joinIntoString(",") == "Anthem,Nectar,Orphan Tool,Pro-C 2,Pro-Q 3",
           "sorting by name is alphabetical");

    // Grouped by manufacturer, and by name inside each group. The plug-in
    // without a manufacturer goes last rather than heading the list.
    expect(namesOf(vocalchain::filterAndSortPlugins(all, "", Sort::sortByManufacturer))
                   .joinIntoString(",") == "Pro-C 2,Pro-Q 3,Nectar,Anthem,Orphan Tool",
           "sorting by manufacturer groups and orders within the group");

    expect(namesOf(vocalchain::filterAndSortPlugins(all, "", Sort::sortByCategory))
                   .joinIntoString(",") == "Anthem,Pro-C 2,Nectar,Orphan Tool,Pro-Q 3",
           "sorting by category groups and orders within the group");

    // The grouping field is only prefixed where it explains the order.
    expect(vocalchain::pluginDisplayName(all[0], Sort::sortAlphabetically) == "Pro-Q 3",
           "alphabetical entries show the bare name");
    expect(vocalchain::pluginDisplayName(all[0], Sort::sortByManufacturer) == "FabFilter - Pro-Q 3",
           "manufacturer entries carry their vendor");
    expect(vocalchain::pluginDisplayName(all[0], Sort::sortByCategory) == "EQ - Pro-Q 3",
           "category entries carry their category");
    expect(vocalchain::pluginDisplayName(all[4], Sort::sortByManufacturer) == "Orphan Tool",
           "a missing manufacturer adds no empty prefix");

    if (failures == 0) std::cout << "PluginBrowsingTests passed\n";
    return failures == 0 ? 0 : 1;
}
