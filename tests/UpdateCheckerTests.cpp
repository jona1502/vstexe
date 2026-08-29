#include <inputrack/UpdateChecker.h>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

juce::String release(const juce::String& tag, const juce::String& assets,
                     bool draft = false, bool prerelease = false)
{
    return "{\"tag_name\":\"" + tag + "\",\"draft\":"
        + (draft ? "true" : "false") + ",\"prerelease\":"
        + (prerelease ? "true" : "false") + ",\"assets\":[" + assets + "]}";
}

juce::String asset(const juce::String& name, const juce::String& url)
{
    return "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + url + "\"}";
}

const juce::String setupAsset =
    asset("InputRack-0.1.5-Windows-x64-Setup.exe",
          "https://github.com/jona1502/inputrack/releases/download/v0.1.5/InputRack-0.1.5-Windows-x64-Setup.exe");
const juce::String checksumAsset =
    asset("InputRack-0.1.5-Windows-x64-Setup.exe.sha256",
          "https://github.com/jona1502/inputrack/releases/download/v0.1.5/InputRack-0.1.5-Windows-x64-Setup.exe.sha256");
}

int main()
{
    using Checker = inputrack::UpdateChecker;

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

    // GitHub redirects the repository's old vstexe name to inputrack, but the
    // release payload contains canonical URLs. Keep a real release-shaped
    // payload here so a future repository rename cannot silently look current.
    const auto canonicalSetup = asset(
        "InputRack-0.1.9-Windows-x64-Setup.exe",
        "https://github.com/jona1502/inputrack/releases/download/v0.1.9/InputRack-0.1.9-Windows-x64-Setup.exe");
    const auto canonicalChecksum = asset(
        "InputRack-0.1.9-Windows-x64-Setup.exe.sha256",
        "https://github.com/jona1502/inputrack/releases/download/v0.1.9/InputRack-0.1.9-Windows-x64-Setup.exe.sha256");
    const auto canonicalRelease = Checker::parseLatestRelease(
        release("v0.1.9", canonicalSetup + "," + canonicalChecksum), "0.1.8");
    expect(canonicalRelease.has_value(), "the canonical InputRack release URL is accepted");
    if (canonicalRelease.has_value()) {
        expect(canonicalRelease->downloadUrl.contains("/jona1502/inputrack/releases/"),
               "the accepted installer comes from the canonical repository");
    }

    expect(!Checker::parseLatestRelease(
               release("v0.1.4", setupAsset + "," + checksumAsset), "0.1.4").has_value(),
           "the running version is not offered to itself");
    expect(!Checker::parseLatestRelease(
               release("v0.1.3", setupAsset + "," + checksumAsset), "0.1.4").has_value(),
           "an older release is not offered");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5-beta.1", setupAsset + "," + checksumAsset), "0.1.4").has_value(),
           "a non-semantic release tag is refused");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5", setupAsset + "," + checksumAsset, true), "0.1.4").has_value(),
           "a draft release is refused");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5", setupAsset + "," + checksumAsset, false, true), "0.1.4").has_value(),
           "a prerelease is refused by the stable update channel");

    // Without a checksum the download could not be verified, so no update is
    // better than an unverifiable one.
    expect(!Checker::parseLatestRelease(release("v0.1.5", setupAsset), "0.1.4").has_value(),
           "a release without a checksum is refused");
    expect(!Checker::parseLatestRelease(release("v0.1.5", checksumAsset), "0.1.4").has_value(),
           "a release without an installer is refused");

    juce::String metadataError;
    Checker::parseLatestRelease(release("v0.1.5", setupAsset), "0.1.4", &metadataError);
    expect(metadataError.isNotEmpty(), "missing release assets report a metadata error");
    metadataError.clear();
    Checker::parseLatestRelease(release("v0.1.4", setupAsset + "," + checksumAsset),
                                "0.1.4", &metadataError);
    expect(metadataError.isEmpty(), "being up to date is not reported as a metadata error");

    // A tampered payload must not be able to redirect the download.
    const auto offSite =
        asset("InputRack-0.1.5-Windows-x64-Setup.exe", "https://example.com/evil.exe");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5", offSite + "," + checksumAsset), "0.1.4").has_value(),
           "an asset hosted off GitHub is refused");

    const auto wrongReleasePath =
        asset("InputRack-0.1.5-Windows-x64-Setup.exe",
              "https://github.com/jona1502/inputrack/releases/download/v0.1.4/InputRack-0.1.5-Windows-x64-Setup.exe");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5", wrongReleasePath + "," + checksumAsset), "0.1.4").has_value(),
           "an asset from a different release path is refused");

    const auto otherSetup =
        asset("OtherProduct-0.1.5-Windows-x64-Setup.exe",
              "https://github.com/jona1502/inputrack/releases/download/v0.1.5/OtherProduct-0.1.5-Windows-x64-Setup.exe");
    const auto otherChecksum =
        asset("OtherProduct-0.1.5-Windows-x64-Setup.exe.sha256",
              "https://github.com/jona1502/inputrack/releases/download/v0.1.5/OtherProduct-0.1.5-Windows-x64-Setup.exe.sha256");
    expect(!Checker::parseLatestRelease(
               release("v0.1.5", otherSetup + "," + otherChecksum), "0.1.4").has_value(),
           "assets for a different product are ignored");

    expect(!Checker::parseLatestRelease("not json at all", "0.1.4").has_value(),
           "malformed payloads yield no update");
    metadataError.clear();
    Checker::parseLatestRelease("not json at all", "0.1.4", &metadataError);
    expect(metadataError.isNotEmpty(), "malformed payloads report a metadata error");
    expect(!Checker::parseLatestRelease("{}", "0.1.4").has_value(),
           "a payload without a tag yields no update");

    const juce::String hash(juce::String::repeatedString("a", 64));
    const auto parsedHash = Checker::parseSha256(
        hash + "  InputRack-0.1.5-Windows-x64-Setup.exe\n",
        "InputRack-0.1.5-Windows-x64-Setup.exe");
    expect(parsedHash.has_value() && *parsedHash == hash,
           "the workflow checksum format is accepted");
    expect(!Checker::parseSha256(
               "abcd  InputRack-0.1.5-Windows-x64-Setup.exe\n",
               "InputRack-0.1.5-Windows-x64-Setup.exe").has_value(),
           "a short checksum is refused");
    expect(!Checker::parseSha256(
               hash + "  OtherProduct.exe\n",
               "InputRack-0.1.5-Windows-x64-Setup.exe").has_value(),
           "a checksum naming another file is refused");
    expect(!Checker::parseSha256(
               hash + "  InputRack-0.1.5-Windows-x64-Setup.exe\nextra\n",
               "InputRack-0.1.5-Windows-x64-Setup.exe").has_value(),
           "a checksum file with extra content is refused");

    if (failures == 0) std::cout << "UpdateCheckerTests passed\n";
    return failures == 0 ? 0 : 1;
}
