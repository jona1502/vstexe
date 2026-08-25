#include <vocalchain/UpdateChecker.h>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

juce::String release(const juce::String& tag, const juce::String& assets)
{
    return "{\"tag_name\":\"" + tag + "\",\"assets\":[" + assets + "]}";
}

juce::String asset(const juce::String& name, const juce::String& url)
{
    return "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + url + "\"}";
}

const juce::String setupAsset =
    asset("VocalChain-0.1.5-Windows-x64-Setup.exe",
          "https://github.com/jona1502/vstexe/releases/download/v0.1.5/VocalChain-0.1.5-Windows-x64-Setup.exe");
const juce::String checksumAsset =
    asset("VocalChain-0.1.5-Windows-x64-Setup.exe.sha256",
          "https://github.com/jona1502/vstexe/releases/download/v0.1.5/VocalChain-0.1.5-Windows-x64-Setup.exe.sha256");
}

int main()
{
    using Checker = vocalchain::UpdateChecker;

    // Segment-wise numeric comparison. A string comparison claims 0.1.9 is the
    // newer of these two, which would strand everyone on an old build.
    expect(Checker::compareVersions("0.1.10", "0.1.9") > 0, "0.1.10 is newer than 0.1.9");
    expect(Checker::compareVersions("0.1.9", "0.1.10") < 0, "0.1.9 is older than 0.1.10");
    expect(Checker::compareVersions("0.1.4", "0.1.4") == 0, "equal versions compare equal");
    expect(Checker::compareVersions("1.0.0", "0.9.9") > 0, "a major bump wins over minor and patch");
    expect(Checker::compareVersions("v0.2.0", "0.2.0") == 0, "a leading v is ignored");
    expect(Checker::compareVersions("0.2", "0.2.0") == 0, "missing segments count as zero");

    // A newer release with both assets is the only case that offers an update.
    const auto found = Checker::parseLatestRelease(
        release("v0.1.5", setupAsset + "," + checksumAsset), "0.1.4");
    expect(found.has_value(), "a newer release is offered");
    if (found.has_value()) {
        expect(found->version == "0.1.5", "the tag becomes the version without its v");
        expect(found->downloadUrl.endsWith("Setup.exe"), "the installer asset is picked");
        expect(found->checksumUrl.endsWith(".sha256"), "the checksum asset is picked");
    }

    expect(!Checker::parseLatestRelease(
               release("v0.1.4", setupAsset + "," + checksumAsset), "0.1.4").has_value(),
           "the running version is not offered to itself");
    expect(!Checker::parseLatestRelease(
               release("v0.1.3", setupAsset + "," + checksumAsset), "0.1.4").has_value(),
           "an older release is not offered");

    // Without a checksum the download could not be verified, so no update is
    // better than an unverifiable one.
    expect(!Checker::parseLatestRelease(release("v0.1.5", setupAsset), "0.1.4").has_value(),
           "a release without a checksum is refused");
    expect(!Checker::parseLatestRelease(release("v0.1.5", checksumAsset), "0.1.4").has_value(),
           "a release without an installer is refused");

    // A tampered payload must not be able to redirect the download.
    const auto offSite =
        asset("VocalChain-0.1.5-Windows-x64-Setup.exe", "https://example.com/evil.exe");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5", offSite + "," + checksumAsset), "0.1.4").has_value(),
           "an asset hosted off GitHub is refused");

    expect(!Checker::parseLatestRelease("not json at all", "0.1.4").has_value(),
           "malformed payloads yield no update");
    expect(!Checker::parseLatestRelease("{}", "0.1.4").has_value(),
           "a payload without a tag yields no update");

    if (failures == 0) std::cout << "UpdateCheckerTests passed\n";
    return failures == 0 ? 0 : 1;
}
