# Releasing InputRack

## Windows installer

The `Windows release` GitHub Actions workflow builds the statically linked x64
desktop application, runs all tests, creates an Inno Setup executable, and emits
a SHA-256 checksum. A manual workflow run stores both files as a downloadable
workflow artifact. A semantic version tag publishes them as GitHub Release
assets.

```powershell
git tag v0.1.0
git push origin main
git push origin v0.1.0
```

Tags must follow `vMAJOR.MINOR.PATCH`. The corresponding installer is named
`InputRack-MAJOR.MINOR.PATCH-Windows-x64-Setup.exe`.

For a local package build, first build the release preset and then run:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
.\installer\windows\build-installer.ps1 -Version 0.1.0
```

## In-app updates

The application checks for newer releases itself, so a tagged release reaches an
installed copy without a manual download. `UpdateChecker` (`update/`) queries the
public releases API at most once a day, records the timestamp in
`%APPDATA%\InputRack\update-check.json`, and offers an install button in the
status strip. A second button there starts the check on demand.

This constrains what a release may look like:

- **The repository must stay public.** The check is unauthenticated; a private
  repository answers `404` and no update is ever found.
- **Every release needs both assets.** A release carrying the installer without
  its `.sha256` is deliberately ignored, because the download could not be
  verified. `build-installer.ps1` emits both, so this holds as long as releases
  come from the workflow.
- **Tags stay `vMAJOR.MINOR.PATCH`.** Versions are compared segment by segment,
  so `0.1.10` correctly supersedes `0.1.9`.

The installer is fetched, checked against the published SHA-256 and only then
executed with `/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS`. `AppMutex` in
`InputRack.iss` lets the restart manager close the running application before
the executable is replaced, and the install location under `{localappdata}`
means no elevation prompt appears.

The checksum protects against a corrupted or intercepted download. It does not
protect against a compromised GitHub account, because installer and checksum
share one origin; only a code signature would. Updates are Windows-only, since
`JUCE_USE_CURL=0` leaves the Linux build without TLS.

## Publishing route

Releases ship the application only. The processed chain reaches other
applications through a virtual audio cable the user installs separately, as
described in [VIRTUAL_CABLE.md](VIRTUAL_CABLE.md). No cable is bundled: each is
licensed by its vendor, and redistributing one requires an agreement with them.

The installer therefore has no driver component and needs no elevation beyond a
normal application install.

## Signing boundary

The current GitHub installer is an application preview. It does not bundle the
development/test-signed virtual microphone driver. Public x64 Windows systems
require a Microsoft-trusted kernel driver signature; enabling global test mode
inside a consumer installer is not an acceptable release path.

Free code signing does not close this gap. The SignPath Foundation signs open
source applications, not kernel drivers, and names itself as the publisher.
Azure Trusted Signing supports neither EV certificates nor drivers. Partner
Center anchors an account to an EV certificate, which is issued only to a
registry-verifiable legal entity.

Before calling a release production-ready:

1. Register the publisher in Microsoft Partner Center and associate the required
   code-signing identity.
2. Submit the driver package for Microsoft attestation or HLK signing.
3. Add the returned signed INF/CAT/SYS package to the installer and verify clean
   install, update, rollback, and uninstall on Windows 11 with Secure Boot on.
4. Authenticode-sign the application and final installer to establish publisher
   identity and reduce SmartScreen warnings.

The developer-only test driver remains available through
`driver/windows/manage-driver.ps1` as documented in `WINDOWS_VIRTUAL_MIC.md`.
