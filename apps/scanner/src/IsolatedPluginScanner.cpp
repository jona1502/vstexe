#include <inputrack/IsolatedPluginScanner.h>

namespace inputrack {
namespace {
/*
 * The helper inherits a single pipe for stdout and stderr, and Windows gives
 * that pipe a few kilobytes of buffer. A plug-in that logs while it loads fills
 * the buffer and then blocks on its next write, with nobody at the other end:
 * the wait below would see a process that never finishes and call it a hang.
 * The output itself is of no interest, but somebody has to keep reading it.
 */
class ChildOutputDrain final : private juce::Thread {
public:
    explicit ChildOutputDrain(juce::ChildProcess& processToDrain)
        : juce::Thread("VST3 scan helper output"), child(processToDrain)
    {
        startThread();
    }

    ~ChildOutputDrain() override { stopThread(2000); }

private:
    // Returns once the helper has exited and the pipe is empty, which is why
    // this is only ever destroyed after the process is gone.
    void run() override { child.readAllProcessOutput(); }

    juce::ChildProcess& child;
};
}

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

    // Declared after the process so it is joined before the process is torn
    // down, on every path out of this function.
    ChildOutputDrain drain(child);

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
