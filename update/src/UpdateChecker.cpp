#include <inputrack/UpdateChecker.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_events/juce_events.h>

namespace inputrack {
namespace {
constexpr const char* releasesApi =
    "https://api.github.com/repos/jona1502/inputrack/releases/latest";
constexpr const char* releaseDownloadRoot =
    "https://github.com/jona1502/inputrack/releases/download/v";
constexpr int connectionTimeoutMs = 15000;
constexpr juce::int64 checkIntervalMs = 24 * 60 * 60 * 1000;

/*
 * A release payload is attacker-influenced the moment the account behind it is,
 * so a download URL is only accepted while it stays on GitHub. Without this a
 * tampered response could point the installer download at any host.
 */
bool isSemanticVersion(const juce::String& version)
{
    juce::StringArray parts;
    parts.addTokens(version, ".", {});
    if (parts.size() != 3) return false;
    for (const auto& part : parts)
        if (part.isEmpty() || !part.containsOnly("0123456789")) return false;
    return true;
}

juce::String installerNameFor(const juce::String& version)
{
    return "InputRack-" + version + "-Windows-x64-Setup.exe";
}

bool isExpectedAssetUrl(const juce::String& url, const juce::String& version,
                        const juce::String& filename)
{
    return url == juce::String(releaseDownloadRoot) + version + "/" + filename;
}

bool isExpectedUpdate(const AvailableUpdate& update)
{
    if (!isSemanticVersion(update.version)) return false;
    const auto installer = installerNameFor(update.version);
    return isExpectedAssetUrl(update.downloadUrl, update.version, installer)
        && isExpectedAssetUrl(update.checksumUrl, update.version, installer + ".sha256");
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
    if (!tag.startsWithChar('v')
        || static_cast<bool>(release.getProperty("draft", false))
        || static_cast<bool>(release.getProperty("prerelease", false)))
        return {};

    const auto version = tag.substring(1);
    if (!isSemanticVersion(version)) return {};
    if (compareVersions(version, currentVersion) <= 0) return {};

    const auto* assets = release.getProperty("assets", {}).getArray();
    if (assets == nullptr) return {};

    AvailableUpdate update;
    update.version = version;
    const auto installerName = installerNameFor(version);
    const auto checksumName = installerName + ".sha256";
    for (const auto& asset : *assets) {
        const auto name = asset.getProperty("name", {}).toString();
        const auto url = asset.getProperty("browser_download_url", {}).toString();
        if (name == checksumName && isExpectedAssetUrl(url, version, checksumName)) {
            if (update.checksumUrl.isNotEmpty()) return {};
            update.checksumUrl = url;
        }
        else if (name == installerName && isExpectedAssetUrl(url, version, installerName)) {
            if (update.downloadUrl.isNotEmpty()) return {};
            update.downloadUrl = url;
        }
    }

    // Without both halves the download could not be verified, so a release
    // missing either is reported as no update rather than an unchecked one.
    if (update.downloadUrl.isEmpty() || update.checksumUrl.isEmpty()) return {};
    return update;
}

std::optional<juce::String> UpdateChecker::parseSha256(
    const juce::String& contents, const juce::String& expectedFilename)
{
    juce::StringArray lines;
    lines.addLines(contents);
    lines.removeEmptyStrings(true);
    if (lines.size() != 1) return {};

    juce::StringArray fields;
    fields.addTokens(lines[0], " \t", {});
    fields.removeEmptyStrings(true);
    if (fields.size() != 2 || fields[1] != expectedFilename) return {};

    const auto hash = fields[0].toLowerCase();
    if (hash.length() != 64 || !hash.containsOnly("0123456789abcdef")) return {};
    return hash;
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

    if (!isExpectedUpdate(pending)) {
        report({}, "The update metadata did not match an InputRack release.");
        return;
    }

    juce::String error;
    const auto checksumLine = fetch(pending.checksumUrl, error);
    if (error.isNotEmpty()) { report({}, error); return; }

    const auto releaseFilename = installerNameFor(pending.version);
    const auto expected = parseSha256(checksumLine, releaseFilename);
    if (!expected.has_value()) {
        report({}, "The release published no usable checksum.");
        return;
    }
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
    if (juce::SHA256(setup).toHexString().toLowerCase() != *expected) {
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
