#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <optional>

namespace inputrack {
/** A release newer than the running build, with everything needed to install it. */
struct AvailableUpdate {
    juce::String version;
    juce::String downloadUrl;
    juce::String checksumUrl;
};

/**
 * Finds newer GitHub releases and installs them. The pure parts are static so
 * they can be tested without a network, which is where this kind of code
 * usually goes wrong.
 */
class UpdateChecker final : private juce::Thread {
public:
    /** Reports the outcome on the message thread. Both fields empty means "up to date". */
    using CheckCallback = std::function<void(std::optional<AvailableUpdate>, juce::String error)>;
    /** Reports a verified installer, or an error describing why there is none. */
    using DownloadCallback = std::function<void(juce::File setup, juce::String error)>;

    UpdateChecker(juce::String currentVersion, juce::File stateDirectory);
    ~UpdateChecker() override;

    /** Starts a check unless one is already running. */
    void check(CheckCallback);
    /** Downloads the release, verifies its checksum and reports the installer. */
    void download(const AvailableUpdate&, DownloadCallback);
    bool isBusy() const noexcept;

    /** True when no automatic check has run within the last day. */
    bool isDueForAutomaticCheck() const;

    /**
     * Negative when a is older than b, zero when equal, positive when newer.
     * Compares segments numerically: 0.1.10 is newer than 0.1.9, which a
     * string comparison gets backwards.
     */
    static int compareVersions(const juce::String& a, const juce::String& b);

    /** Extracts an update from a GitHub release payload, or nothing when current. */
    static std::optional<AvailableUpdate> parseLatestRelease(const juce::String& json,
                                                             const juce::String& currentVersion);

    /** Hands the installer to Windows and asks the app to quit. */
    static bool launchInstaller(const juce::File& setup);

private:
    enum class Job { none, check, download };
    void run() override;
    void runCheck();
    void runDownload();
    void recordCheckTime() const;
    juce::File stateFile() const;

    const juce::String currentVersion;
    const juce::File stateDirectory;
    Job job{Job::none};
    AvailableUpdate pending;
    CheckCallback checkCallback;
    DownloadCallback downloadCallback;
};
}
