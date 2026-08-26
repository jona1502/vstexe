#include <inputrack/UpdateChecker.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_events/juce_events.h>

namespace inputrack {
namespace {
constexpr const char* releasesApi =
    "https://api.github.com/repos/jona1502/vstexe/releases/latest";
constexpr int connectionTimeoutMs = 15000;
constexpr juce::int64 checkIntervalMs = 24 * 60 * 60 * 1000;

/*
 * A release payload is attacker-influenced the moment the account behind it is,
 * so a download URL is only accepted while it stays on GitHub. Without this a
 * tampered response could point the installer download at any host.
 */
bool isTrustedDownloadUrl(const juce::String& url)
{
    return url.startsWith("https://github.com/");
}

juce::String fetch(const juce::String& url, juce::String& error)
{
    // GitHub rejects API requests without a user agent, so it is not optional.
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(connectionTimeoutMs)
                             .withExtraHeaders("User-Agent: InputRack");
    if (auto stream = juce::URL(url).createInputStream(options))
        return stream->readEntireStreamAsString();
    error = "Could not reach " + juce::URL(url).getDomain() + ".";
    return {};
}
}

UpdateChecker::UpdateChecker(juce::String version, juce::File directory)
    : Thread("InputRack update checker"),
      currentVersion(std::move(version)),
      stateDirectory(std::move(directory))
{
}

UpdateChecker::~UpdateChecker()
{
    signalThreadShouldExit();
    stopThread(connectionTimeoutMs + 1000);
}

bool UpdateChecker::isBusy() const noexcept { return isThreadRunning(); }

juce::File UpdateChecker::stateFile() const
{
    return stateDirectory.getChildFile("update-check.json");
}

bool UpdateChecker::isDueForAutomaticCheck() const
{
    const auto state = juce::JSON::parse(stateFile());
    const auto last = static_cast<juce::int64>(state.getProperty("lastCheckMs", 0));
    return juce::Time::currentTimeMillis() - last >= checkIntervalMs;
}

void UpdateChecker::recordCheckTime() const
{
    stateDirectory.createDirectory();
    juce::DynamicObject::Ptr state = new juce::DynamicObject();
    state->setProperty("lastCheckMs", juce::Time::currentTimeMillis());
    stateFile().replaceWithText(juce::JSON::toString(juce::var(state.get())));
}

int UpdateChecker::compareVersions(const juce::String& a, const juce::String& b)
{
    juce::StringArray left, right;
    left.addTokens(a.trimCharactersAtStart("vV"), ".", {});
    right.addTokens(b.trimCharactersAtStart("vV"), ".", {});
    for (int i = 0; i < juce::jmax(left.size(), right.size()); ++i) {
        const auto leftPart = i < left.size() ? left[i].getIntValue() : 0;
        const auto rightPart = i < right.size() ? right[i].getIntValue() : 0;
        if (leftPart != rightPart) return leftPart < rightPart ? -1 : 1;
    }
    return 0;
}

std::optional<AvailableUpdate> UpdateChecker::parseLatestRelease(const juce::String& json,
                                                                 const juce::String& currentVersion)
{
    const auto release = juce::JSON::parse(json);
    const auto tag = release.getProperty("tag_name", {}).toString();
    if (tag.isEmpty()) return {};

    const auto version = tag.trimCharactersAtStart("vV");
    if (compareVersions(version, currentVersion) <= 0) return {};

    const auto* assets = release.getProperty("assets", {}).getArray();
    if (assets == nullptr) return {};

    AvailableUpdate update;
    update.version = version;
    const auto installerName = "InputRack-" + version + "-Windows-x64-Setup.exe";
    const auto checksumName = installerName + ".sha256";
    for (const auto& asset : *assets) {
        const auto name = asset.getProperty("name", {}).toString();
        const auto url = asset.getProperty("browser_download_url", {}).toString();
        if (!isTrustedDownloadUrl(url)) continue;
        if (name == checksumName) update.checksumUrl = url;
        else if (name == installerName) update.downloadUrl = url;
    }

    // Without both halves the download could not be verified, so a release
    // missing either is reported as no update rather than an unchecked one.
    if (update.downloadUrl.isEmpty() || update.checksumUrl.isEmpty()) return {};
    return update;
}

void UpdateChecker::check(CheckCallback callback)
{
    if (isThreadRunning()) return;
    checkCallback = std::move(callback);
    job = Job::check;
    startThread(juce::Thread::Priority::low);
}

void UpdateChecker::download(const AvailableUpdate& update, DownloadCallback callback)
{
    if (isThreadRunning()) return;
    pending = update;
    downloadCallback = std::move(callback);
    job = Job::download;
    startThread(juce::Thread::Priority::low);
}

void UpdateChecker::run()
{
    if (job == Job::check) runCheck();
    else if (job == Job::download) runDownload();
}

void UpdateChecker::runCheck()
{
    juce::String error;
    const auto payload = fetch(releasesApi, error);
    if (threadShouldExit()) return;

    std::optional<AvailableUpdate> found;
    if (error.isEmpty()) {
        found = parseLatestRelease(payload, currentVersion);
        recordCheckTime();
    }

    juce::MessageManager::callAsync([callback = checkCallback, found, error] {
        if (callback) callback(found, error);
    });
}

void UpdateChecker::runDownload()
{
    const auto report = [this](juce::File file, juce::String message) {
        juce::MessageManager::callAsync([callback = downloadCallback, file, message] {
            if (callback) callback(file, message);
        });
    };

    juce::String error;
    const auto checksumLine = fetch(pending.checksumUrl, error);
    if (error.isNotEmpty()) { report({}, error); return; }

    const auto expected = checksumLine.upToFirstOccurrenceOf(" ", false, false).trim();
    if (expected.isEmpty()) { report({}, "The release published no usable checksum."); return; }
    if (threadShouldExit()) return;

    stateDirectory.createDirectory();
    const auto setup = stateDirectory.getChildFile("InputRack-" + pending.version + "-Setup.exe");
    setup.deleteFile();

    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(connectionTimeoutMs)
                             .withExtraHeaders("User-Agent: InputRack");
    auto stream = juce::URL(pending.downloadUrl).createInputStream(options);
    if (stream == nullptr) { report({}, "The installer could not be downloaded."); return; }

    {
        juce::FileOutputStream output(setup);
        if (output.failedToOpen()) {
            report({}, "Could not write to " + setup.getFullPathName() + ".");
            return;
        }
        output.writeFromInputStream(*stream, -1);
    }
    if (threadShouldExit()) { setup.deleteFile(); return; }

    // An installer is executable code, so a mismatch removes the file rather
    // than leaving something unverified sitting on disk.
    if (!juce::SHA256(setup).toHexString().equalsIgnoreCase(expected)) {
        setup.deleteFile();
        report({}, "The downloaded installer failed its checksum and was discarded.");
        return;
    }
    report(setup, {});
}

bool UpdateChecker::launchInstaller(const juce::File& setup)
{
#if JUCE_WINDOWS
    // Silent, with the restart manager closing and reopening InputRack around
    // the file replacement. The install location needs no elevation.
    return setup.startAsProcess("/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /NOCANCEL");
#else
    juce::ignoreUnused(setup);
    return false;
#endif
}
}
