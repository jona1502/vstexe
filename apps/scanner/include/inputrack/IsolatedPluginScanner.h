#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace inputrack {
/**
 * Modules that were still being described when their deadline passed.
 *
 * KnownPluginList takes ownership of the scanner, so the record of what timed
 * out has to live somewhere the caller still holds after handing the scanner
 * over. Written from the scan thread and read from the message thread.
 */
class ScanTimeouts final {
public:
    void add(const juce::String& fileOrIdentifier);
    juce::StringArray files() const;
    void clear();

private:
    juce::CriticalSection lock;
    juce::StringArray timedOut;
};

/** What became of one attempt to describe a module in the helper process. */
enum class ScanOutcome {
    described,    ///< The helper finished; the result holds whatever it found.
    crashed,      ///< The helper died or wrote unreadable output: blame the module.
    timedOut,     ///< The helper was still working when the deadline passed.
    unavailable   ///< The helper could not be started: not the module's fault.
};

/**
 * Loads one plug-in module in a short-lived child process, so that a module
 * which crashes on load takes down the helper rather than the desktop
 * application.
 *
 * The timeout is generous because a VST3 shell is not one plug-in. A shell
 * module houses many effects and carries no manifest listing them, so JUCE has
 * to instantiate every housed class to describe it. Waves' WaveShell needs
 * roughly half a minute for a modest licence and minutes for a large one; a
 * timeout tuned to single-effect modules mistakes that for a hang.
 */
class IsolatedPluginScanner final : public juce::KnownPluginList::CustomScanner {
public:
    static constexpr int defaultTimeoutMilliseconds = 180000;

    explicit IsolatedPluginScanner(juce::File helperExecutable,
                                   int timeoutMilliseconds = defaultTimeoutMilliseconds,
                                   std::shared_ptr<ScanTimeouts> = nullptr);

    bool findPluginTypesFor(juce::AudioPluginFormat&,
                            juce::OwnedArray<juce::PluginDescription>&,
                            const juce::String& fileOrIdentifier) override;

    /** Public for the process-boundary tests; production calls this through JUCE. */
    ScanOutcome scan(const juce::String& formatName,
                     const juce::String& fileOrIdentifier,
                     juce::OwnedArray<juce::PluginDescription>& result) const;

private:
    juce::File helper;
    int timeoutMs;
    std::shared_ptr<ScanTimeouts> timeouts;
};
}
