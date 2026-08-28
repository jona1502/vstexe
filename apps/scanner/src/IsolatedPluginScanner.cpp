#include <inputrack/IsolatedPluginScanner.h>

namespace inputrack {
void ScanTimeouts::add(const juce::String& fileOrIdentifier)
{
    const juce::ScopedLock guard(lock);
    timedOut.addIfNotAlreadyThere(fileOrIdentifier);
}

juce::StringArray ScanTimeouts::files() const
{
    const juce::ScopedLock guard(lock);
    return timedOut;
}

void ScanTimeouts::clear()
{
    const juce::ScopedLock guard(lock);
    timedOut.clear();
}

IsolatedPluginScanner::IsolatedPluginScanner(juce::File helperExecutable,
                                             int timeoutMilliseconds,
                                             std::shared_ptr<ScanTimeouts> timeoutLog)
    : helper(std::move(helperExecutable)),
      timeoutMs(juce::jmax(1, timeoutMilliseconds)),
      timeouts(std::move(timeoutLog))
{
}

/*
 * KnownPluginList reads a false return as "this module is unsafe" and puts it
 * on the persistent blacklist, which bars it from every later scan as well.
 * Only a crash deserves that. A timeout says the module is slow, and a missing
 * helper says nothing about the module at all, so both report success with
 * nothing found: the module stays off the blacklist and the next scan retries
 * it.
 */
bool IsolatedPluginScanner::findPluginTypesFor(
    juce::AudioPluginFormat& format,
    juce::OwnedArray<juce::PluginDescription>& result,
    const juce::String& fileOrIdentifier)
{
    return scan(format.getName(), fileOrIdentifier, result) != ScanOutcome::crashed;
}

ScanOutcome IsolatedPluginScanner::scan(
    const juce::String& formatName,
    const juce::String& fileOrIdentifier,
    juce::OwnedArray<juce::PluginDescription>& result) const
{
    if (!helper.existsAsFile() || formatName.isEmpty() || fileOrIdentifier.isEmpty())
        return ScanOutcome::unavailable;

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
        return ScanOutcome::unavailable;

    const auto startedAt = juce::Time::getMillisecondCounterHiRes();
    while (child.isRunning()) {
        const auto timedOut = juce::Time::getMillisecondCounterHiRes() - startedAt >= timeoutMs;
        if (timedOut || juce::Thread::currentThreadShouldExit()) {
            child.kill();
            output.deleteFile();
            if (timedOut && timeouts != nullptr)
                timeouts->add(fileOrIdentifier);
            return timedOut ? ScanOutcome::timedOut : ScanOutcome::unavailable;
        }
        juce::Thread::sleep(10);
    }

    // Drain the redirected handles before destroying the process object. The
    // helper emits output only on errors, but leaving unread handles behind is
    // still avoidable resource leakage on Windows.
    child.readAllProcessOutput();
    if (child.getExitCode() != 0) {
        output.deleteFile();
        return ScanOutcome::crashed;
    }

    const auto xml = juce::XmlDocument::parse(output);
    output.deleteFile();
    if (xml == nullptr)
        return ScanOutcome::crashed;

    juce::KnownPluginList scanned;
    scanned.recreateFromXml(*xml);
    for (const auto& description : scanned.getTypes())
        result.add(new juce::PluginDescription(description));
    return ScanOutcome::described;
}
}
