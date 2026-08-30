# Releasing InputRack

## Local development installer and CI validation

The `Windows validation` GitHub Actions workflow is manual. It builds the
statically linked x64 desktop application, runs all tests, and stores only a
development-identity MSIX as a workflow artifact for internal QA. It never
uploads the direct installer or publishes GitHub Release assets. Production
distribution is exclusively through the Microsoft Store, so the durable Pro
entitlement cannot be bypassed through a downloadable CI installer.

Local non-Store builds intentionally keep Pro enabled for development. A direct
installer can still be created locally when needed, but it must not be uploaded
as a public or Actions artifact.

For a local package build, first build the release preset and then run:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
.\installer\windows\build-installer.ps1 -Version 0.1.0
. .\scripts\dev-shell.ps1
cmake --preset windows-store
cmake --build --preset windows-store
ctest --preset windows-store
.\installer\windows\build-msix.ps1 -Version 0.1.0
```

## Microsoft Store MSIX

`build-msix.ps1` creates an unsigned, full-trust Windows 11 MSIX containing
`InputRack.exe` and its crash-isolated scanner. The package deliberately has no
kernel driver. Microsoft signs the package after Store certification.

The `windows-store` preset defines `INPUTRACK_STORE_BUILD=ON`. This removes the
GitHub release checker and installer launcher from the packaged application;
Store updates replace them. User state remains in `%APPDATA%\InputRack`, outside
the immutable package, so settings, scan results, recovery state and presets
survive an MSIX update and can be shared with an earlier direct installation.

The manifest identity must exactly match the values assigned in Partner Center.
Set these GitHub Actions repository variables before submitting a CI package:

- `INPUTRACK_STORE_IDENTITY_NAME`
- `INPUTRACK_STORE_PUBLISHER`

The defaults (`InputRack.Dev` and `CN=InputRack Development`) only make local
package validation reproducible; they are not a production Store identity.

The commercial configuration is versioned in `store/product.json`: the app is
free and Pro is a durable, one-time add-on with offer token `inputrack.pro`.
The regular target price is EUR 29.99, with EUR 19.99 for the first 30 days
after launch. Partner Center maps these targets to its regional price tiers and
handles taxes, refunds and Microsoft's Store fee.

Because Partner Center exposes its native free-trial selector only for
subscriptions, the 14-day Pro trial is application-managed. It starts only on
request and stores its first-use timestamp for the current Windows user under
`HKCU\Software\InputRack`. The marker survives a normal app uninstall, but this
is intentionally lightweight licensing: preventing deliberate registry resets
would require an account-backed licensing service. The permanent purchase and
restore remain authoritative Microsoft Store entitlements.

Create the app and add-on in Partner Center, then set these repository variables:

- `INPUTRACK_STORE_IDENTITY_NAME`
- `INPUTRACK_STORE_PUBLISHER`
- `INPUTRACK_STORE_PRODUCT_ID`
- `PUBLIC_INPUTRACK_STORE_URL` (`https://apps.microsoft.com/detail/PRODUCT_ID`)

Complete the legal operator data in `apps/web/src/data/site.ts`. The manual
**Microsoft Store submission package** workflow then checks all identifiers,
builds and tests the Store variant, verifies the Store-configured website, and
uploads the MSIX artifact for Partner Center. Run the same gate locally with:

```powershell
.\scripts\check-store-release.ps1
```

The repository intentionally cannot supply the publisher identity, Product ID,
legal address or final regional Partner Center prices; those are external
account data and the gate fails clearly until they are provided.

## Legacy direct-build updates

Direct builds retain the legacy GitHub update checker for compatibility with the
public previews released through version 0.1.9. No newer consumer installers are
published there. `UpdateChecker` (`update/`) queries the public releases API at
most once a day, records the timestamp in
`%APPDATA%\InputRack\update-check.json`, and offers an install button in the
status strip. A second button there starts the check on demand.

This constrains what a release may look like:

- **The repository must stay public.** The check is unauthenticated; a private
  repository answers `404` and no update is ever found.
- **Every release needs both assets.** A release carrying the installer without
  its `.sha256` is deliberately ignored, because the download could not be
  verified. `build-installer.ps1` emits both, so this holds as long as releases
  come from the workflow.
- **Names, tag and URLs must agree exactly.** Both the app and website accept
  only `InputRack-VERSION-Windows-x64-Setup.exe` and its `.sha256` from the
  matching `vVERSION` GitHub release path. Drafts, prereleases, duplicate assets
  and unrelated executables are ignored.
- **Tags stay `vMAJOR.MINOR.PATCH`.** Versions are compared segment by segment,
  so `0.1.10` correctly supersedes `0.1.9`.

The installer is fetched, checked against the published SHA-256 and only then
executed with `/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS`. `AppMutex` in
`InputRack.iss` lets the restart manager close the running application before
the executable is replaced, and the install location under `{localappdata}`
means no elevation prompt appears.

The checksum protects against a corrupted or intercepted download. It does not
protect against a compromised GitHub account, because installer and checksum
share one origin; only a code signature would.

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
