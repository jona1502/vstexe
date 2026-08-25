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

Platform delivery milestones are described in
[docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md).
