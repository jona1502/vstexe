#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace inputrack {
/**
 * Loads one plug-in module in a short-lived child process. A crash or hang is
 * reported to KnownPluginList as a scan failure, which puts that identifier on
 * its persistent blacklist instead of taking down the desktop application.
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
                                   int timeoutMilliseconds = defaultTimeoutMilliseconds);

    bool findPluginTypesFor(juce::AudioPluginFormat&,
                            juce::OwnedArray<juce::PluginDescription>&,
                            const juce::String& fileOrIdentifier) override;

    /** Public for the process-boundary tests; production calls this through JUCE. */
    bool scan(const juce::String& formatName,
              const juce::String& fileOrIdentifier,
              juce::OwnedArray<juce::PluginDescription>& result) const;

private:
    juce::File helper;
    int timeoutMs;
};
}
