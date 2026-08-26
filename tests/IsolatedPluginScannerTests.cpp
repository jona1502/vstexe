#include <inputrack/IsolatedPluginScanner.h>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

juce::String valueAfter(const juce::StringArray& arguments, const juce::String& option)
{
    const auto index = arguments.indexOf(option);
    return juce::isPositiveAndBelow(index, arguments.size() - 1)
        ? arguments[index + 1] : juce::String{};
}

int actAsHelper(const juce::StringArray& arguments)
{
    const auto identifier = valueAfter(arguments, "--plugin");
    if (identifier == "crash") return 9;
    if (identifier == "hang") {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 0;
    }

    juce::PluginDescription description;
    description.name = "Isolated test effect";
    description.pluginFormatName = valueAfter(arguments, "--format");
    description.fileOrIdentifier = identifier;
    description.uniqueId = 1234;
    juce::KnownPluginList list;
    list.addType(description);
    const auto output = juce::File(valueAfter(arguments, "--output"));
    return output.replaceWithText(list.createXml()->toString()) ? 0 : 8;
}
}

int main(int argc, char* argv[])
{
    juce::StringArray arguments;
    for (int i = 1; i < argc; ++i)
        arguments.add(juce::String::fromUTF8(argv[i]));
    if (arguments.contains("--plugin"))
        return actAsHelper(arguments);

    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    inputrack::IsolatedPluginScanner scanner(executable, 1000);

    juce::OwnedArray<juce::PluginDescription> found;
    expect(scanner.scan("VST3", "safe", found), "a healthy helper scan succeeds");
    expect(found.size() == 1 && found[0]->name == "Isolated test effect",
           "descriptions cross the process boundary");

    found.clear();
    expect(!scanner.scan("VST3", "crash", found), "a crashing helper is contained");

    inputrack::IsolatedPluginScanner impatient(executable, 50);
    expect(!impatient.scan("VST3", "hang", found), "a hanging helper is terminated");

    inputrack::IsolatedPluginScanner missing(
        executable.getSiblingFile("missing-inputrack-scanner"), 50);
    expect(!missing.scan("VST3", "safe", found), "a missing helper fails safely");

    if (failures == 0) std::cout << "IsolatedPluginScannerTests passed\n";
    return failures == 0 ? 0 : 1;
}
