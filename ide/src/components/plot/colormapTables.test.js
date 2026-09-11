import { describe, it, expect } from 'vitest';
import { tableOf } from './colormapTables.js';
import { getColormap, getLUT, makeCustomColormap } from './colormaps.js';

// MATLAB R2025b ground truth (probed 2026-09-11): parula(256) sampled
// at five anchors, jet/hot/gray at the mid entry. The old control-point
// parula diverged from MATLAB by up to 50/255 in the interior (blue
// channel at t=0.5); these anchors pin the dumped tables.
const near = (got, want, tol = 1) =>
  Math.abs(got - want) <= tol;

describe('colormap tables (MATLAB R2025b dump)', () => {
  const parulaAnchors = [
    [0.00, [0.2422, 0.1504, 0.6603]],
    [0.25, [0.2006, 0.4803, 0.9906]],
    [0.50, [0.0770, 0.7468, 0.7224]],
    [0.75, [0.7829, 0.7579, 0.1608]],
    [1.00, [0.9769, 0.9839, 0.0805]],
  ];
  it.each(parulaAnchors)('parula anchor t=%s', (t, rgb) => {
    const tb = tableOf('parula');
    const i = Math.round(t * 255) * 3;
    expect(near(tb[i], rgb[0] * 255)).toBe(true);
    expect(near(tb[i + 1], rgb[1] * 255)).toBe(true);
    expect(near(tb[i + 2], rgb[2] * 255)).toBe(true);
  });

  it('jet/hot/gray mid entries match MATLAB', () => {
    const jet = tableOf('jet'), hot = tableOf('hot'), gray = tableOf('gray');
    const i = 128 * 3;
    expect([jet[i], jet[i + 1], jet[i + 2]]).toEqual([131, 255, 124]);
    expect([hot[i], hot[i + 1], hot[i + 2]]).toEqual([255, 88, 0]);
    expect([gray[i], gray[i + 1], gray[i + 2]]).toEqual([128, 128, 128]);
  });

  it('tables have 256 triplets and are monotone at the ends for gray', () => {
    const gray = tableOf('gray');
    expect(gray.length).toBe(256 * 3);
    expect(gray[0]).toBe(0);
    expect(gray[255 * 3]).toBe(255);
  });

  it('getColormap resolves named maps and falls back to parula', () => {
    expect(getColormap('parula')(0)).toBe('rgb(62,38,168)');
    expect(getColormap('turbo')(0)).toBe('rgb(48,18,59)');
    expect(getColormap('no-such-map')(0)).toBe('rgb(62,38,168)');
  });

  it('getLUT samples an interpolator into 256×3', () => {
    const lut = getLUT(getColormap('parula'));
    expect(lut.length).toBe(768);
    expect(Array.from(lut.slice(0, 3))).toEqual([62, 38, 168]);
  });

  it('makeCustomColormap honours endpoints of an N×3 matrix', () => {
    const f = makeCustomColormap([[0, 0, 0], [1, 1, 1]]);
    expect(f(0)).toBe('rgb(0,0,0)');
    expect(f(1)).toBe('rgb(255,255,255)');
    expect(f(0.5)).toBe('rgb(128,128,128)');
  });
});
