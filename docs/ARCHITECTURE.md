# Architecture

## Audio path

```text
physical microphone (mono, 48 kHz)
  -> JUCE AudioProcessorGraph
  -> ordered VST3 effect nodes
  -> platform virtual microphone publisher
  -> Discord / OBS / browser / other capture client
```

The audio callback owns no UI or persistence work. Graph mutations happen on
the message thread and JUCE applies them at processing boundaries. Plug-in
state is captured only when saving a preset.

## Components

- `PluginChainEngine` owns device capture, VST3 formats and the processing graph.
- `ChainState` is the versioned persistence model. JSON remains inspectable while
  opaque VST3 state chunks are base64 encoded without interpretation.
- `VirtualMicrophone` is the narrow real-time publisher boundary. Platform code
  receives already processed mono float samples.
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
