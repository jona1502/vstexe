#include <inputrack/ProfileStore.h>
#include <iostream>

int main()
{
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("inputrack-profile-test-" + juce::Uuid().toString());
    if (!directory.createDirectory()) return 1;
    const auto file = directory.getChildFile("profiles.json");
    inputrack::ProfileStore store(file);
    inputrack::WorkflowProfile profile{"Streaming", "Mic", "Cable Input",
                                       {"obs64.exe", "Discord.exe"}, "{\"rack\":true}"};
    store.upsert(profile);
    juce::String error;
    if (!store.save(error)) return 2;

    inputrack::ProfileStore restored(file);
    if (!restored.load(error)) return 3;
    const auto byName = restored.find("streaming");
    if (!byName.has_value() || byName->outputDevice != "Cable Input") return 4;
    const auto byApplication = restored.matchApplication("DISCORD.EXE");
    if (!byApplication.has_value() || byApplication->name != "Streaming") return 5;
    if (!restored.remove("STREAMING") || restored.find("Streaming").has_value()) return 6;
    directory.deleteRecursively();
    std::cout << "ProfileStore tests passed\n";
    return 0;
}
