# InputRack

InputRack is a Windows 11 and Fedora desktop application that processes a
physical microphone through a user-defined chain of 64-bit VST3 effects and
publishes the processed stereo signal through an output other applications can
use as a microphone.

## Download

Windows x64 setup executables are published on the
[GitHub Releases page](https://github.com/jona1502/vstexe/releases). Each setup
has a matching SHA-256 checksum file.

Publishing the chain requires a virtual audio cable such as
[VB-CABLE](https://vb-audio.com/Cable/), installed separately. InputRack's own
kernel driver has no Microsoft release signature yet and therefore cannot be
installed on a machine with Secure Boot enabled. See
[docs/VIRTUAL_CABLE.md](docs/VIRTUAL_CABLE.md) for the routing, and
[docs/WINDOWS_VIRTUAL_MIC.md](docs/WINDOWS_VIRTUAL_MIC.md) for the driver.

## MVP

- Windows 11 and Fedora Linux
- mono microphone input with a stereo, 48 kHz processing and output path
- VST3 discovery and hosting
- add, remove, reorder, bypass and edit plug-ins
- versioned InputRack presets including complete plug-in state
- publishing through any virtual audio cable
- PipeWire virtual source on Fedora
- WDM/WaveRT virtual capture endpoint on Windows 11 (driver not release-signed)

## Build

Requirements: CMake 3.25+, Ninja, a C++20 compiler and platform audio
development packages. JUCE is fetched at its pinned release unless
`INPUTRACK_JUCE_SOURCE_DIR` points to a local checkout.

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

On Windows, CMake and the MSVC toolchain ship inside Visual Studio and are not
on the global `PATH`. Dot-source the helper once per shell, then run the
equivalent presets:

```powershell
. .\scripts\dev-shell.ps1
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

A Visual Studio Developer PowerShell already has that environment loaded and
needs no helper.

Routing the chain into a virtual audio cable, the supported path today, is
documented in [docs/VIRTUAL_CABLE.md](docs/VIRTUAL_CABLE.md).

The Windows virtual microphone is a separate kernel driver. Development build,
installation, diagnostics, Discord selection, and removal are documented in
[docs/WINDOWS_VIRTUAL_MIC.md](docs/WINDOWS_VIRTUAL_MIC.md).

Platform delivery milestones are described in
[docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md).
Release creation and signing requirements are described in
[docs/RELEASING.md](docs/RELEASING.md).
