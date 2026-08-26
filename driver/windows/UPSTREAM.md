# Windows driver upstream

The initial Windows driver base is vendored from Microsoft's
`Windows-driver-samples/audio/sysvad` tree.

- Repository: <https://github.com/microsoft/Windows-driver-samples>
- Upstream commit: `717778a20ba4dd2440fe609f69153a1f8a64f597`
- Imported subtree: `audio/sysvad`
- License: Microsoft Public License (`sysvad/LICENSE.MS-PL`)

The unmodified import is intentionally isolated under `sysvad/`. InputRack
changes must retain Microsoft's notices and should be kept as reviewable commits.
Before updating the import, compare the WDK version, INF model, PortCls changes,
and all security-relevant diffs against the pinned commit.

The first downstream reduction will remove unrelated render, Bluetooth, USB,
HDMI, keyword, and APO endpoints. The target package exposes only one root-
enumerated mono capture endpoint named `InputRack Microphone`.
