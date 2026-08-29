/**
 * Single source of truth for every string the marketing site renders.
 * Claims here must stay verifiable against the desktop app and docs/.
 */
export const site = {
  name: 'InputRack',
  tagline: 'Your input. Your effects. Everywhere.',
  description:
    'Route your physical microphone through your own 64-bit VST3 effects and send the processed signal wherever you need it, through a virtual audio cable.',
  storeUrl: import.meta.env.PUBLIC_INPUTRACK_STORE_URL ?? '',
  /**
   * Windows lets only a driver publish a microphone, and ours is not
   * release-signed yet. Until then the chain is published through a virtual
   * audio cable the user installs separately, so the page has to say so.
   */
  previewNotice:
    'Publishing needs a virtual audio cable such as VB-CABLE, installed separately. The bundled InputRack microphone driver is not release-signed yet.',
  cableUrl: 'https://vb-audio.com/Cable/',
} as const;

export const downloadUrl = site.storeUrl || '#pricing';

export const nav = [
  { label: 'Features', href: '#features' },
  { label: 'Signal path', href: '#signal-path' },
  { label: 'Pricing', href: '#pricing' },
] as const;

export const pricing = [
  {
    name: 'Free', price: '€0', billing: 'forever', featured: false,
    detail: 'Build and route a complete microphone effect chain without an InputRack account.',
    features: ['VST3 hosting and scanning', 'Rack preset import/export', 'Meters and routing assistant'],
  },
  {
    name: 'Pro', price: '€29.99', billing: 'one time', featured: true,
    detail: 'Try every Pro workflow free for 14 days. No subscription; all 1.x updates included.',
    features: ['Named profiles', 'Automatic app profiles', 'Global hotkeys', 'Start with Windows'],
  },
] as const;

/**
 * Verifiable capability statements. There are no users to quote yet, so the
 * marquee carries facts instead of invented testimonials.
 */
export const facts = [
  { icon: 'vst3', title: '64-bit VST3 hosting', detail: 'Scan and run the plug-ins already installed on your machine.' },
  { icon: 'shield', title: 'Local audio processing', detail: 'Every sample stays on your machine. No account, no upload.' },
  { icon: 'waveform', title: '48 kHz stereo effects', detail: 'Mono microphone input with a true stereo processing and output path.' },
  { icon: 'preset', title: 'Versioned chain presets', detail: 'Order, bypass and complete plug-in state in one JSON file.' },
  { icon: 'microphone', title: 'Native plug-in editors', detail: 'Open each plug-in in its own window, exactly as its vendor built it.' },
  { icon: 'virtual-mic', title: 'Works with any cable', detail: 'VB-CABLE, VoiceMeeter, Sonar and Wave Link are all recognised.' },
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
    detail: 'Save the whole chain as a versioned .inputrack.json file.',
  },
  {
    icon: 'virtual-mic',
    title: 'Any capture client',
    detail: 'Routed through a virtual audio cable, the chain reaches Discord, OBS and the rest.',
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
      'No bundled effects to learn and no subscription. InputRack hosts the 64-bit VST3 effects already installed on your system.',
  },
  {
    title: 'Keep processing on your machine',
    detail:
      'Capture, effects and publishing all run locally. There is no InputRack account or upload; only Store purchase and restore use the network.',
  },
  {
    title: 'Use one processed signal everywhere',
    detail:
      'Build the chain once and select the cable wherever you would pick a normal input device. Every application reads the same processed signal.',
  },
] as const;

/** Stages of the audio path described in docs/ARCHITECTURE.md. */
export const signalStages = [
  { label: 'Physical microphone', meta: 'mono input · stereo effects · 48 kHz', kind: 'source' },
  { label: 'Noise suppression', meta: 'your VST3 plug-in', kind: 'effect' },
  { label: 'EQ → Compressor → De-esser', meta: 'ordered effect nodes', kind: 'effect' },
  { label: 'Virtual audio cable', meta: 'capture endpoint', kind: 'sink' },
] as const;

export const signalPoints = [
  'The graph is rebuilt on the message thread, so reordering never interrupts capture.',
  'Bypass keeps a plug-in loaded and its state intact; only its processing is skipped.',
  'The cable output preserves the processed left and right channels.',
  'Local monitoring is a separate switch, so you can hear the chain without echo.',
] as const;

export const footerLinks = [
  {
    heading: 'Product',
    links: [
      { label: 'Features', href: '#features' },
      { label: 'Signal path', href: '#signal-path' },
      { label: 'Pricing', href: '#pricing' },
      { label: 'Download', href: '#download' },
    ],
  },
  {
    heading: 'Download',
    links: [
      { label: site.storeUrl ? 'Microsoft Store' : 'Store launch pending', href: downloadUrl, external: Boolean(site.storeUrl) },
    ],
  },
  {
    heading: 'Legal',
    links: [
      { label: 'Impressum', href: '/impressum' },
      { label: 'Datenschutz', href: '/privacy' },
    ],
  },
] as const;

/**
 * Imprint details required by § 5 DDG. Every TODO value must be replaced with
 * real data before the site goes live — the legal pages render a visible
 * warning while any placeholder is left in place.
 */
export const legal = {
  operator: 'TODO: Vor- und Nachname',
  street: 'TODO: Straße und Hausnummer',
  city: 'TODO: PLZ und Ort',
  country: 'Deutschland',
  email: 'TODO: kontakt@example.com',
  /** Only required for commercial operation; leave empty for a private project. */
  vatId: '',
} as const;

export const isLegalDataComplete = !Object.values(legal).some((value) => value.startsWith('TODO'));
