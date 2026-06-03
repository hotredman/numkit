// glplot/color.js — parse a CSS color into a [r,g,b,a] tuple in 0..1 for GL
// uniforms. Handles #rgb / #rrggbb / #rrggbbaa and rgb()/rgba(). Falls back
// to opaque black on anything unrecognized (a visible, safe default).

export function cssColorToRGBA(css) {
  if (typeof css !== 'string') return [0, 0, 0, 1];
  const s = css.trim();

  if (s[0] === '#') {
    let h = s.slice(1);
    if (h.length === 3) h = h.split('').map((c) => c + c).join('');
    if (h.length === 6 || h.length === 8) {
      const r = parseInt(h.slice(0, 2), 16) / 255;
      const g = parseInt(h.slice(2, 4), 16) / 255;
      const b = parseInt(h.slice(4, 6), 16) / 255;
      const a = h.length === 8 ? parseInt(h.slice(6, 8), 16) / 255 : 1;
      if ([r, g, b, a].every(Number.isFinite)) return [r, g, b, a];
    }
  }

  const m = s.match(/^rgba?\(([^)]+)\)$/i);
  if (m) {
    const p = m[1].split(',').map((v) => parseFloat(v));
    if (p.length >= 3 && p.slice(0, 3).every(Number.isFinite)) {
      return [p[0] / 255, p[1] / 255, p[2] / 255, p.length >= 4 ? p[3] : 1];
    }
  }

  return [0, 0, 0, 1];
}
