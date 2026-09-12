// Design tokens (rule R4) — the ONLY place raw colors/sizes live.
// Components import named tokens; pages never see a color.
export interface Tokens {
  bg: string; bgPanel: string; bgHover: string; bgActive: string;
  border: string; text: string; textDim: string; textFaint: string;
  accent: string; accentSoft: string; danger: string; dangerBg: string;
  dangerBorder: string; plotBg: string; plotFrame: string;
  plotGrid: string; plotText: string; series: string[];
  lineColor: Record<string, string>; font: string; fontMono: string;
}

export const T: Tokens = {
  // NOTE: <style> blocks in Svelte are STATIC (no JS interpolation) —
  // components reference tokens there as var(--nk-<name>). injectCssVars()
  // (called once in main.js) publishes every token as a CSS variable;
  // markup attributes keep using the JS constants directly.

  bg: '#14161a',
  bgPanel: '#191c22',
  bgHover: '#232732',
  bgActive: '#243b2f',
  border: '#262a32',
  text: '#e8eaf0',
  textDim: '#8b93a3',
  textFaint: '#6b7280',
  accent: '#7fd99a',
  accentSoft: '#a4e3b8',
  danger: '#e26a6a',
  dangerBg: '#2a1d1d',
  dangerBorder: '#3a2626',
  plotBg: '#ffffff',
  plotFrame: '#404044',
  plotGrid: '#e6e6ea',
  plotText: '#282830',
  series: ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a'],
  lineColor: { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', c: '#60d0f0',
               m: '#e070c0', y: '#e8d060', k: '#282830', w: '#ffffff' },
  font: "'Segoe UI', system-ui, sans-serif",
  fontMono: "'JetBrains Mono', 'Cascadia Code', monospace",
};

// Publish T as :root CSS variables (--nk-bg, --nk-plot-frame, …).
export function injectCssVars() {
  const css = [':root {'];
  for (const [k, v] of Object.entries(T))
    if (typeof v === 'string')
      css.push(`--nk-${k.replace(/[A-Z]/g, (c) => '-' + c.toLowerCase())}: ${v};`);
  css.push('}');
  const style = document.createElement('style');
  style.textContent = css.join('\n');
  document.head.appendChild(style);
}
