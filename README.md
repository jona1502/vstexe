# VocalChain

VocalChain is a Windows 11 and Fedora desktop application that processes a
physical microphone through a user-defined chain of 64-bit VST3 effects and
publishes the processed mono signal as a virtual microphone.

## MVP

- Windows 11 and Fedora Linux
- mono, 48 kHz audio path
- VST3 discovery and hosting
- add, remove, reorder, bypass and edit plug-ins
- versioned Vocal Chain presets including complete plug-in state
- PipeWire virtual source on Fedora
- WDM/WaveRT virtual capture endpoint on Windows 11

## Build

Requirements: CMake 3.25+, Ninja, a C++20 compiler and platform audio
development packages. JUCE is fetched at its pinned release unless
`VOCALCHAIN_JUCE_SOURCE_DIR` points to a local checkout.

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

On Windows, run the equivalent presets from a Visual Studio Developer PowerShell:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The Windows virtual microphone is a separate kernel driver. Development build,
installation, diagnostics, Discord selection, and removal are documented in
[docs/WINDOWS_VIRTUAL_MIC.md](docs/WINDOWS_VIRTUAL_MIC.md).

Platform delivery milestones are described in
[docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md).
