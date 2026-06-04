/**
 * plotTicks.js — pure tick generation & number formatting for the 2-D
 * plot renderers. Extracted from CompositePlot (which defined these
 * inline) so they're independently testable and reusable. No DOM, no
 * React, no component state.
 */

/**
 * "Nice" linear ticks over [min, max] aiming for ~target major ticks,
 * snapped to the 1/2/5×10ⁿ ladder. Minor ticks at step/5 (excluding the
 * majors). Returns { major: number[], minor: number[] }.
 */
export function niceTicks(min, max, target = 6) {
  const range = max - min;
  if (range <= 0) return { major: [min], minor: [] };
  const rough = range / target;
  const pow = Math.pow(10, Math.floor(Math.log10(rough)));
  const norm = rough / pow;
  const step = norm < 1.5 ? pow : norm < 3 ? 2 * pow : norm < 7 ? 5 * pow : 10 * pow;
  const start = Math.ceil(min / step) * step;
  const majorArr = [];
  for (let v = start; v <= max + step * 1e-6; v += step) majorArr.push(+v.toFixed(12));
  const minorStep = step / 5;
  const minorArr = [];
  for (let v = Math.ceil(min / minorStep) * minorStep; v <= max + minorStep * 1e-6; v += minorStep) {
    if (Math.abs(((v - start) / step) - Math.round((v - start) / step)) > 1e-6) minorArr.push(+v.toFixed(12));
  }
  return { major: majorArr, minor: minorArr };
}

/**
 * Log-axis ticks: powers of 10 as major, the 2..9 multiples between them
 * as minor. Returns empty arrays for a non-positive / degenerate range.
 */
export function logTicks(min, max) {
  if (min <= 0 || max <= 0 || max <= min) return { major: [], minor: [] };
  const lmin = Math.floor(Math.log10(min));
  const lmax = Math.ceil(Math.log10(max));
  const major = [], minor = [];
  for (let p = lmin; p <= lmax; p++) {
    const base = Math.pow(10, p);
    if (base >= min && base <= max) major.push(base);
    for (let m = 2; m <= 9; m++) {
      const v = base * m;
      if (v >= min && v <= max) minor.push(v);
    }
  }
  return { major, minor };
}

/**
 * Apply a single-value sprintf-style tick format — the common subset of
 * MATLAB's xtickformat/ytickformat: %d, %f, %.Nf, %e, %.Ne, %g, %.Ng.
 * Returns null for an unsupported / empty format (caller falls back).
 */
export function applyTickFormat(fmt, v) {
  if (!fmt) return null;
  const m = fmt.match(/^%(?:\.(\d+))?([defg])$/);
  if (!m) return null;
  const prec = m[1] !== undefined ? Number(m[1]) : 6;
  const conv = m[2];
  if (conv === 'd') return String(Math.round(v));
  if (conv === 'f') return v.toFixed(prec);
  if (conv === 'e') return v.toExponential(prec);
  if (conv === 'g') return v.toPrecision(prec);
  return null;
}

/**
 * Default auto tick label — magnitude-aware precision, exponential for
 * very small / very large values. Used when no custom labels / format.
 */
export function fmtTick(v) {
  const a = Math.abs(v);
  if (a !== 0 && (a < 1e-3 || a >= 1e5)) return v.toExponential(1);
  if (a >= 100) return v.toFixed(0);
  if (a >= 10)  return v.toFixed(1);
  if (a >= 1)   return v.toFixed(2);
  return v.toFixed(3);
}
