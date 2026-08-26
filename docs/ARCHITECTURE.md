# Architecture

## Audio path

```text
physical microphone (mono, 48 kHz)
  -> mono-to-stereo adapter
  -> JUCE AudioProcessorGraph
  -> ordered mono or stereo VST3 effect nodes
  -> selected output device (virtual audio cable) or virtual microphone driver
  -> Discord / OBS / browser / other capture client
```

Two publishing routes share that last stage. A virtual audio cable is the route
that works on a stock Windows installation and is described in
[VIRTUAL_CABLE.md](VIRTUAL_CABLE.md); the bundled kernel driver in
[WINDOWS_VIRTUAL_MIC.md](WINDOWS_VIRTUAL_MIC.md) needs a release signature
before it can take over.

The audio callback owns no UI or persistence work. Graph mutations happen on
the message thread and JUCE applies them at processing boundaries. Plug-in
state is captured only when saving a preset.

## Components

- `PluginChainEngine` owns device capture, VST3 formats and the processing graph.
- `ChainState` is the versioned persistence model. JSON remains inspectable while
  opaque VST3 state chunks are base64 encoded without interpretation.
- Mono-only VST3 effects are surrounded by stereo-to-mono and mono-to-stereo
  adapters. Bypassing the effect also bypasses both adapters, preserving stereo.
- `VirtualMicrophone` is the narrow real-time publisher boundary for the legacy
  driver route. Platform code receives the processed left channel as mono.
- The desktop app manages microphone selection, plug-in scanning and editors.

## Real-time contract

`VirtualMicrophone::push` and all processing callbacks must not allocate, log,
touch files, wait on locks, call UI code or invoke control IPC. Platform
implementations must preallocate bounded lock-free buffers. An underrun produces
silence; an overrun drops the oldest complete block and records a counter outside
the audio thread.

## Security and stability

Production scanning runs in a separate helper process with a dead-man file.
Plug-ins that crash the scanner are quarantined. A later isolation milestone can
move processing into a supervised audio service; this is intentionally not
pretended by the initial in-process graph.
