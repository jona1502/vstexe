/**
 * Single source of truth for every string the marketing site renders.
 * Claims here must stay verifiable against the desktop app and docs/.
 */
export const site = {
  name: 'VocalChain',
  tagline: 'Your voice. Your chain. Everywhere.',
  description:
    'Route your physical microphone through your own 64-bit VST3 effects and use the processed signal as a virtual microphone.',
  repo: 'https://github.com/jona1502/vstexe',
  releases: 'https://github.com/jona1502/vstexe/releases',
  releasesApi: 'https://api.github.com/repos/jona1502/vstexe/releases/latest',
  /** The Windows kernel driver is not release-signed yet, so downloads are previews. */
  previewNotice: 'Preview build — the Windows virtual microphone driver is not release-signed yet.',
} as const;

export const nav = [
  { label: 'Features', href: '#features' },
  { label: 'Signal path', href: '#signal-path' },
  { label: 'Presets', href: '#presets' },
  { label: 'Open source', href: site.repo, external: true },
] as const;

/** Concepts that float around the hero headline. */
export const heroOrbit = [
  { icon: 'microphone', label: 'Physical microphone', position: 'left-[4%] top-[14%]', delay: '0s' },
  { icon: 'vst3', label: 'VST3 plug-ins', position: 'left-[13%] bottom-[16%]', delay: '-2.4s' },
  { icon: 'waveform', label: '48 kHz mono path', position: 'right-[5%] top-[20%]', delay: '-1.2s' },
  { icon: 'preset', label: 'Chain presets', position: 'right-[14%] bottom-[13%]', delay: '-3.6s' },
  { icon: 'virtual-mic', label: 'Virtual microphone', position: 'left-1/2 -top-2 -translate-x-1/2', delay: '-4.8s' },
] as const;

/**
 * Verifiable capability statements. There are no users to quote yet, so the
 * marquee carries facts instead of invented testimonials.
 */
export const facts = [
  { icon: 'vst3', title: '64-bit VST3 hosting', detail: 'Scan and run the plug-ins already installed on your machine.' },
  { icon: 'shield', title: 'Local audio processing', detail: 'Every sample stays on your machine. No account, no upload.' },
  { icon: 'waveform', title: '48 kHz mono signal path', detail: 'One fixed rate and channel count from capture to output.' },
  { icon: 'preset', title: 'Versioned chain presets', detail: 'Order, bypass and complete plug-in state in one JSON file.' },
  { icon: 'microphone', title: 'Native plug-in editors', detail: 'Open each plug-in in its own window, exactly as its vendor built it.' },
  { icon: 'virtual-mic', title: 'Windows 11 and Fedora', detail: 'A WaveRT capture endpoint on Windows, a PipeWire source on Fedora.' },
] as const;

export const featureMatrix = [
  {
    icon: 'vst3',
    title: 'VST3 effects',
    detail: 'Scan your VST3 folders once; the result is cached between launches.',
  },
  {
    icon: 'preset',
    title: 'Reorder & bypass',
    detail: 'Move any effect up or down and bypass it without losing its state.',
  },
  {
    icon: 'waveform',
    title: 'Presets',
    detail: 'Save the whole chain as a versioned .vocalchain.json file.',
  },
  {
    icon: 'virtual-mic',
    title: 'Virtual microphone',
    detail: 'The processed signal appears as a capture device for other apps.',
  },
  {
    icon: 'shield',
    title: 'Local processing',
    detail: 'Audio never leaves the machine, so nothing depends on a service.',
  },
] as const;

export const featureBenefits = [
  {
    title: 'Use plug-ins you already own',
    detail:
      'No bundled effects to learn and no subscription. VocalChain hosts the 64-bit VST3 effects already installed on your system.',
  },
  {
    title: 'Keep processing on your machine',
    detail:
      'Capture, effects and publishing all run in one local process. There is no account, no upload and no network dependency.',
  },
  {
    title: 'Use one processed signal everywhere',
    detail:
      'Build the chain once and select the VocalChain virtual microphone wherever you would pick a normal input device.',
  },
] as const;

/** Stages of the audio path described in docs/ARCHITECTURE.md. */
export const signalStages = [
  { label: 'Physical microphone', meta: 'mono · 48 kHz capture', kind: 'source' },
  { label: 'Noise suppression', meta: 'your VST3 plug-in', kind: 'effect' },
  { label: 'EQ → Compressor → De-esser', meta: 'ordered effect nodes', kind: 'effect' },
  { label: 'VocalChain Virtual Microphone', meta: 'capture endpoint', kind: 'sink' },
] as const;

export const signalPoints = [
  'The graph is rebuilt on the message thread, so reordering never interrupts capture.',
  'Bypass keeps a plug-in loaded and its state intact; only its processing is skipped.',
  'The publisher receives already processed mono float samples and never allocates.',
  'Local monitoring is a separate switch, so you can hear the chain without echo.',
] as const;

export const presetPoints = [
  {
    title: 'The complete plug-in state travels with the preset',
    detail:
      'Each entry stores the opaque VST3 state chunk base64 encoded, so a restored chain sounds exactly like the one you saved.',
  },
  {
    title: 'Order and bypass are part of the file',
    detail: 'Children are written in processing order and every entry carries its own bypassed flag.',
  },
  {
    title: 'Versioned and inspectable',
    detail:
      'A schemaVersion guards the format and a preset from a newer VocalChain is rejected instead of silently misread.',
  },
] as const;

/** Build commands from README.md, mirrored here so the page cannot drift silently. */
export const buildCommands = [
  { prompt: '$', command: 'git clone https://github.com/jona1502/vstexe' },
  { prompt: '$', command: 'cmake --preset windows-release' },
  { prompt: '$', command: 'cmake --build --preset windows-release' },
] as const;
