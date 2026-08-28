import { site } from '../data/site';

interface ReleaseAsset {
  name: string;
  browser_download_url: string;
  size: number;
}

/**
 * An asset is only followed when GitHub itself serves it from this release's
 * own download path. Matching the path rather than a hard-coded repository URL
 * keeps the check strict without breaking when the repository is renamed.
 */
const isReleaseAsset = (asset: ReleaseAsset, expectedName: string, version: string) => {
  if (asset.name !== expectedName) return false;

  let url: URL;
  try {
    url = new URL(asset.browser_download_url);
  } catch {
    return false;
  }

  return (
    url.origin === 'https://github.com' &&
    url.pathname.endsWith(`/releases/download/v${version}/${expectedName}`)
  );
};

/**
 * Every download button is server-rendered pointing at the releases page and
 * upgraded here to the newest installer, so the first click always downloads
 * something. Any failure leaves the releases page in place; nothing falls back
 * to an anchor that only scrolls.
 */
export const resolveLatestDownload = async () => {
  if (site.storeUrl) return;
  const links = document.querySelectorAll<HTMLAnchorElement>('[data-download-link]');
  if (links.length === 0) return;

  const response = await fetch(site.releasesApi, { headers: { Accept: 'application/vnd.github+json' } });
  if (!response.ok) return;

  const release = (await response.json()) as {
    tag_name?: string;
    draft?: boolean;
    prerelease?: boolean;
    assets?: ReleaseAsset[];
  };
  const version = release.tag_name?.match(/^v(\d+\.\d+\.\d+)$/)?.[1];
  if (!version || release.draft || release.prerelease) return;

  const installerName = `InputRack-${version}-Windows-x64-Setup.exe`;
  const checksumName = `${installerName}.sha256`;
  const setups = release.assets?.filter((asset) => isReleaseAsset(asset, installerName, version)) ?? [];
  const checksums = release.assets?.filter((asset) => isReleaseAsset(asset, checksumName, version)) ?? [];
  if (setups.length !== 1 || checksums.length !== 1) return;
  const setup = setups[0];

  for (const link of links) {
    link.href = setup.browser_download_url;
    // The installer downloads in place; a new tab would just be left blank.
    link.removeAttribute('target');
  }

  for (const label of document.querySelectorAll<HTMLElement>('[data-download-label]')) {
    label.textContent = `Download for Windows · v${version}`;
  }

  const meta = document.querySelector<HTMLElement>('[data-download-meta]');
  if (meta) {
    const megabytes = (setup.size / 1024 / 1024).toFixed(1);
    meta.textContent = `${setup.name} · ${megabytes} MB · SHA-256 checksum on the releases page`;
  }
};

/** Windows is the only platform with an installer; say so to everyone else. */
export const revealNonWindowsNote = () => {
  const other = document.querySelector<HTMLElement>('[data-download-other]');
  if (!other) return;

  const platform =
    (navigator as Navigator & { userAgentData?: { platform?: string } }).userAgentData?.platform ??
    navigator.userAgent;
  if (!/win/i.test(platform)) other.hidden = false;
};
