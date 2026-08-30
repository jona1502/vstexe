# Implementation milestones

## M1 — desktop audio graph (current foundation)

- C++20/CMake/JUCE application
- mono input with a stereo 48 kHz processing and output graph
- VST3 scan, load, remove, reorder, bypass and native editor
- versioned JSON presets with opaque plug-in state
- microphone device selector
- input/output level meters

Before M1 is considered complete, build and run it on Windows 11. VST3
discovery now runs one module at a time in a timeout-controlled helper
process; failed modules are persisted in the plug-in blacklist.

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
- input/output meters, global bypass and clipping protection
- crash recovery (session-loss recovery from the last known-good rack) and
  device hotplug (reinitialising audio when the selected device disappears)
- preset schema-version guards, covered by migration tests
- CI builds on Windows

Still open before M4 is complete:

- signed release pipeline

## Known build limitation

The repository creation environment did not contain CMake or a C++ compiler, so
the initial source tree could not be compiled there. The first developer setup
must run configure, build and tests before platform work starts.
