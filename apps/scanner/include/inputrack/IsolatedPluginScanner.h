#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace inputrack {
/**
 * Loads one plug-in module in a short-lived child process. A crash or hang is
 * reported to KnownPluginList as a scan failure, which puts that identifier on
 * its persistent blacklist instead of taking down the desktop application.
 */
class IsolatedPluginScanner final : public juce::KnownPluginList::CustomScanner {
public:
    explicit IsolatedPluginScanner(juce::File helperExecutable,
                                   int timeoutMilliseconds = 30000);

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
