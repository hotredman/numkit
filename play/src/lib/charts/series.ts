// Pure series math (rule R5): which datasets are line-series, style
// decode, path building against ANY projection. Chart components stay
// markup; stages pass their own projection — one LineChart serves
// cartesian AND polar (the D1 duplication this split removes).
import type { WireAxes, LineDataset } from './wire.js';
import { T } from '../tokens.js';
import { niceTicks as ideNiceTicks } from '@ide-plot/plotTicks.js';

/** IDE ladder returns {major, minor} — the TYPED array form the
 * stages consume. (TypeScript caught D1 calling the object form as an
 * array — a latent render-time crash the wire smokes never hit.) */
export function ticks(lo: number, hi: number, target = 10): number[] {
  const t = ideNiceTicks(lo, hi, target) as unknown as
    | number[] | { major: number[] };
  return Array.isArray(t) ? t : (t.major ?? []);
}

const LINE_TYPES = new Set(['line', undefined, 'comet', 'animatedline']);
export const lineSeries = (ax: WireAxes | null): LineDataset[] =>
  (ax?.datasets ?? []).filter(
    (d): d is LineDataset => LINE_TYPES.has((d as LineDataset).type),
  );

export function seriesColor(style: string | undefined, idx: number): string {
  if (typeof style === 'string' && style.startsWith('#')) return style;
  if (typeof style === 'string' && T.lineColor[style[0]!])
    return T.lineColor[style[0]]!;
  return T.series[idx % T.series.length]!;
}

export function seriesDash(style: string | undefined): string {
  if (typeof style !== 'string') return '';
  if (style.includes('--')) return '6 4';
  if (style.includes(':')) return '2 3';
  if (style.includes('-.')) return '8 3 2 3';
  return '';
}

// Path over finite runs (non-finite points break the pen — the wire's
// series separators). `project(x, y)` returns "px py" screen coords.
export function buildPath(
  xs: number[], ys: number[],
  project: (x: number, y: number) => string,
): string {
  let d = '';
  let pen = false;
  for (let k = 0; k < Math.min(xs.length, ys.length); k++) {
    if (!Number.isFinite(xs[k]!) || !Number.isFinite(ys[k]!)) { pen = false; continue; }
    d += (pen ? 'L' : 'M') + project(xs[k]!, ys[k]!);
    pen = true;
  }
  return d;
}
