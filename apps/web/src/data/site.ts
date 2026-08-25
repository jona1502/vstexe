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
