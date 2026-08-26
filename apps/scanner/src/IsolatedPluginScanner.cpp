#include <inputrack/IsolatedPluginScanner.h>

namespace inputrack {
IsolatedPluginScanner::IsolatedPluginScanner(juce::File helperExecutable,
                                             int timeoutMilliseconds)
    : helper(std::move(helperExecutable)),
      timeoutMs(juce::jmax(1, timeoutMilliseconds))
{
}

bool IsolatedPluginScanner::findPluginTypesFor(
    juce::AudioPluginFormat& format,
    juce::OwnedArray<juce::PluginDescription>& result,
    const juce::String& fileOrIdentifier)
{
    return scan(format.getName(), fileOrIdentifier, result);
}

bool IsolatedPluginScanner::scan(
    const juce::String& formatName,
    const juce::String& fileOrIdentifier,
    juce::OwnedArray<juce::PluginDescription>& result) const
{
    if (!helper.existsAsFile() || formatName.isEmpty() || fileOrIdentifier.isEmpty())
        return false;

    const auto output = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("inputrack-plugin-scan", ".xml", false);
    output.deleteFile();

    juce::StringArray arguments;
    arguments.add(helper.getFullPathName());
    arguments.add("--format");
    arguments.add(formatName);
    arguments.add("--plugin");
    arguments.add(fileOrIdentifier);
    arguments.add("--output");
    arguments.add(output.getFullPathName());

    juce::ChildProcess child;
    if (!child.start(arguments))
        return false;

    const auto startedAt = juce::Time::getMillisecondCounterHiRes();
    while (child.isRunning()) {
        const auto timedOut = juce::Time::getMillisecondCounterHiRes() - startedAt >= timeoutMs;
        if (timedOut || juce::Thread::currentThreadShouldExit()) {
            child.kill();
            output.deleteFile();
            return false;
        }
        juce::Thread::sleep(10);
    }

    // Drain the redirected handles before destroying the process object. The
    // helper emits output only on errors, but leaving unread handles behind is
    // still avoidable resource leakage on Windows.
    child.readAllProcessOutput();
    if (child.getExitCode() != 0) {
        output.deleteFile();
        return false;
    }

    const auto xml = juce::XmlDocument::parse(output);
    output.deleteFile();
    if (xml == nullptr)
        return false;

    juce::KnownPluginList scanned;
    scanned.recreateFromXml(*xml);
    for (const auto& description : scanned.getTypes())
        result.add(new juce::PluginDescription(description));
    return true;
}
}
