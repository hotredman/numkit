// Pure plot math (rule R5): viewport limits, series style decode,
// path builders. Framework-free; imports the shared MATLAB-parity
// tick ladder from the IDE's framework-free helpers (R7: no forks).
import { niceTicks } from '@ide-plot/plotTicks.js';
import { T } from '../tokens.js';

export { niceTicks };

const SKIPPED_TYPES = new Set(['imagesc', 'image-rgb', 'text']);

export function datasets(ax) {
  return ax?.datasets ?? [];
}

export function seriesData(ax) {
  return datasets(ax).filter(
    (d) => d.type === 'line' || d.type === undefined || d.type === 'comet'
      || d.type === 'animatedline');
}

export function viewport(ax) {
  const cfg = ax?.config ?? {};
  let [x0, x1] = cfg.xlim ?? [Infinity, -Infinity];
  let [y0, y1] = cfg.ylim ?? [Infinity, -Infinity];
  for (const ds of seriesData(ax)) {
    const xs = ds.x ?? [], ys = ds.y ?? [];
    for (let k = 0; k < Math.min(xs.length, ys.length); k++)
      if (Number.isFinite(xs[k]) && Number.isFinite(ys[k])) {
        if (!cfg.xlim) { x0 = Math.min(x0, xs[k]); x1 = Math.max(x1, xs[k]); }
        if (!cfg.ylim) { y0 = Math.min(y0, ys[k]); y1 = Math.max(y1, ys[k]); }
      }
  }
  if (!(x1 > x0)) { x0 = 0; x1 = 1; }
  if (!(y1 > y0)) { y0 -= 1; y1 += 1; }
  const pad = (hi, lo) => (hi - lo) * 0.05;
  if (!cfg.xlim) { x0 -= pad(x1, x0); x1 += pad(x1, x0); }
  if (!cfg.ylim) { y0 -= pad(y1, y0); y1 += pad(y1, y0); }
  return { x0, x1, y0, y1 };
}

export function seriesColor(style, idx) {
  if (typeof style === 'string' && style.startsWith('#')) return style;
  if (typeof style === 'string' && T.lineColor[style[0]])
    return T.lineColor[style[0]];
  return T.series[idx % T.series.length];
}

export function seriesDash(style) {
  if (typeof style !== 'string') return '';
  if (style.includes('--')) return '6 4';
  if (style.includes(':')) return '2 3';
  if (style.includes('-.')) return '8 3 2 3';
  return '';
}

// Path builder over finite runs (null/NaN separators break the pen —
// the wire carries series breaks as non-finite points).
export function buildPath(xs, ys, fx, fy) {
  let d = '';
  let pen = false;
  for (let k = 0; k < Math.min(xs.length, ys.length); k++) {
    if (!Number.isFinite(xs[k]) || !Number.isFinite(ys[k])) { pen = false; continue; }
    d += (pen ? 'L' : 'M') + fx(xs[k], ys[k]);
    pen = true;
  }
  return d;
}

// Polar scaling: theta (rad) + r against the max |r| and the box.
export function polarModel(ax, box) {
  let rMax = 0;
  for (const ds of seriesData(ax)) {
    for (const v of ds.y ?? [])
      if (Number.isFinite(v)) rMax = Math.max(rMax, Math.abs(v));
  }
  if (!(rMax > 0)) rMax = 1;
  const cx = box.x + box.w / 2;
  const cy = box.y + box.h / 2;
  const scale = (Math.min(box.w, box.h) / 2) - 6;
  return {
    rMax, cx, cy,
    R: (r) => (Math.abs(r) / rMax) * scale,
    X: (th, r) => cx + (Math.abs(r) / rMax) * scale * Math.cos(th),
    Y: (th, r) => cy - (Math.abs(r) / rMax) * scale * Math.sin(th),
  };
}
