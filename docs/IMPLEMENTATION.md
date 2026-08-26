# Implementation milestones

## M1 — desktop audio graph (current foundation)

- C++20/CMake/JUCE application
- fixed mono 48 kHz graph
- VST3 scan, load, remove, reorder, bypass and native editor
- versioned JSON presets with opaque plug-in state
- microphone device selector

Before M1 is considered complete, build and run it on Fedora and Windows 11,
move scanning to a helper executable, add meters and make bypass toggle state
visible in the list.

## M2 — Fedora virtual microphone

- implement `VirtualMicrophone` with native PipeWire streams
- publish a stable `Audio/Source` node named `InputRack Virtual Microphone`
- route processed blocks directly to it, not through a monitor output
- reconnect after PipeWire restart and publish silence while capture is absent
- package as AppImage and RPM for the first Fedora release

Acceptance: Firefox, Discord and OBS can select the source and receive the same
processed signal for a two-hour soak test without xruns.

## M3 — Windows virtual microphone

- fork the Microsoft SYSVAD componentized sample into a dedicated WDK solution
- expose one mono 48 kHz WaveRT capture endpoint
- connect the user-mode engine through a versioned KS packet transport and a
  bounded kernel ring buffer
- enforce explicit buffer ownership, sequence counters and silence on disconnect
- create INF/CAT packaging, installer rollback and clean uninstall
- validate with Driver Verifier and the applicable HLK audio playlist

Acceptance: signed install, update and uninstall work on clean Windows 11 VMs;
Discord, OBS, Teams and Chromium receive processed audio after reboot.

## M4 — release hardening

- out-of-process scanner and persistent quarantine
- missing-plug-in placeholders in presets
- input/output meters, global bypass and clipping protection
- crash recovery, device hotplug and preset migration tests
- CI builds on Fedora and Windows; signed release pipeline

## Known build limitation

The repository creation environment did not contain CMake or a C++ compiler, so
the initial source tree could not be compiled there. The first developer setup
must run configure, build and tests before platform work starts.
