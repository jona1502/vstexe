/** Windows is the only platform with an installer; say so to everyone else. */
export const revealNonWindowsNote = () => {
  const other = document.querySelector<HTMLElement>('[data-download-other]');
  if (!other) return;

  const platform =
    (navigator as Navigator & { userAgentData?: { platform?: string } }).userAgentData?.platform ??
    navigator.userAgent;
  if (!/win/i.test(platform)) other.hidden = false;
};
