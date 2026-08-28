#include <inputrack/IsolatedPluginScanner.h>
#include <chrono>
#include <memory>
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
    // Comfortably more than the pipe buffer Windows hands out by default, so a
    // caller that only reads after the exit would deadlock here instead.
    if (identifier == "chatty")
        for (int i = 0; i < 4000; ++i)
            std::cout << "helper chatter line " << i << std::endl;

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
    const auto timeouts = std::make_shared<inputrack::ScanTimeouts>();
    inputrack::IsolatedPluginScanner scanner(executable, 1000, timeouts);

    juce::OwnedArray<juce::PluginDescription> found;
    expect(scanner.scan("VST3", "safe", found) == inputrack::ScanOutcome::described,
           "a healthy helper scan succeeds");
    expect(found.size() == 1 && found[0]->name == "Isolated test effect",
           "descriptions cross the process boundary");

    found.clear();
    expect(scanner.scan("VST3", "crash", found) == inputrack::ScanOutcome::crashed,
           "a crashing helper is contained");

    found.clear();
    inputrack::IsolatedPluginScanner patient(executable, 30000, timeouts);
    expect(patient.scan("VST3", "chatty", found) == inputrack::ScanOutcome::described,
           "a helper that fills the output pipe still finishes");

    inputrack::IsolatedPluginScanner impatient(executable, 50, timeouts);
    expect(impatient.scan("VST3", "hang", found) == inputrack::ScanOutcome::timedOut,
           "a hanging helper is terminated");
    expect(timeouts->files() == juce::StringArray{"hang"},
           "the module that ran out of time is recorded for the caller");

    inputrack::IsolatedPluginScanner missing(
        executable.getSiblingFile("missing-inputrack-scanner"), 50, timeouts);
    expect(missing.scan("VST3", "safe", found) == inputrack::ScanOutcome::unavailable,
           "a missing helper fails safely");

    // Only a crash may reach the persistent blacklist: a slow module and a
    // broken installation both have to stay retryable.
    juce::VST3PluginFormat format;
    found.clear();
    expect(!scanner.findPluginTypesFor(format, found, "crash"),
           "a crash is reported to KnownPluginList as a failure");
    expect(impatient.findPluginTypesFor(format, found, "hang"),
           "a timeout is not reported as a failure");
    expect(missing.findPluginTypesFor(format, found, "safe"),
           "a missing helper is not reported as a failure");

    if (failures == 0) std::cout << "IsolatedPluginScannerTests passed\n";
    return failures == 0 ? 0 : 1;
}
