import { describe, it, expect } from 'vitest';
import { logClampRange, previewViewport, previewStride } from './plotUtils';

describe('previewStride', () => {
  it('never subsamples in interactive windows', () => {
    expect(previewStride(1000000, true)).toBe(1);
  });
  it('keeps small series whole in previews', () => {
    expect(previewStride(2000, false)).toBe(1);
    expect(previewStride(500, false)).toBe(1);
  });
  it('strides big series down to ~max for previews', () => {
    expect(previewStride(200000, false)).toBe(100);   // 200k / 2000
    expect(previewStride(50000, false)).toBe(25);
    expect(previewStride(2001, false)).toBe(2);
  });
  it('honours a custom max and tolerates bad n', () => {
    expect(previewStride(10000, false, 1000)).toBe(10);
    expect(previewStride(NaN, false)).toBe(1);
  });
});

describe('logClampRange', () => {
  it('leaves already-positive ranges untouched', () => {
    expect(logClampRange(1, 1000)).toEqual([1, 1000]);
    expect(logClampRange(0.01, 5)).toEqual([0.01, 5]);
  });

  it('clamps a range whose lower bound dipped to/below 0', () => {
    // data 1..1000 padded 4% → [-39, 1040]; preview must not render linear.
    const [lo, hi] = logClampRange(-39, 1040);
    expect(lo).toBeGreaterThan(0);
    expect(hi).toBeGreaterThanOrEqual(lo * 10);
    expect(hi).toBeGreaterThanOrEqual(1040);
  });

  it('handles both bounds non-positive', () => {
    const [lo, hi] = logClampRange(-5, 0);
    expect(lo).toBeGreaterThan(0);
    expect(hi).toBeGreaterThan(lo);
  });

  it('minPositive overrides the default hi/1e4 anchor (heatmap half-cell)', () => {
    // Heatmap straddling zero: clamp the lo bound to half a cell width
    // instead of hi/1e4. e.g. 10-col grid over [-0.5, 9.5] → cellW = 1.
    const [lo, hi] = logClampRange(-0.5, 9.5, 0.5);
    expect(lo).toBe(0.5);
    expect(hi).toBeGreaterThanOrEqual(9.5);
  });

  it('minPositive is ignored when the range is already positive', () => {
    // The early return wins: positive ranges pass through verbatim.
    expect(logClampRange(2, 100, 0.5)).toEqual([2, 100]);
  });

  it('minPositive still floored at 1e-6', () => {
    const [lo] = logClampRange(-1, 1, 0);
    expect(lo).toBe(1e-6);
  });
});

describe('previewViewport', () => {
  it('linear figure → raw ranges', () => {
    const vp = previewViewport({ kind: 'composite', xscale: 'linear', yscale: 'linear',
      xRange: [-39, 1040], yRange: [0, 10] });
    expect(vp).toEqual({ x: [-39, 1040], y: [0, 10] });
  });

  it('log x-axis → x clamped positive, y untouched', () => {
    const vp = previewViewport({ kind: 'composite', xscale: 'log', yscale: 'linear',
      xRange: [-39, 1040], yRange: [-5, 5] });
    expect(vp.x[0]).toBeGreaterThan(0);
    expect(vp.y).toEqual([-5, 5]);
  });

  it('loglog → both axes clamped', () => {
    const vp = previewViewport({ kind: 'composite', xscale: 'log', yscale: 'log',
      xRange: [-39, 1040], yRange: [-2, 100] });
    expect(vp.x[0]).toBeGreaterThan(0);
    expect(vp.y[0]).toBeGreaterThan(0);
  });

  it('log axis with already-positive range stays put', () => {
    const vp = previewViewport({ kind: 'composite', xscale: 'log', yscale: 'linear',
      xRange: [1, 1000], yRange: [0, 1] });
    expect(vp.x).toEqual([1, 1000]);
  });

  it('missing ranges fall back to [-1,1]', () => {
    const vp = previewViewport({ kind: 'composite', xscale: 'linear', yscale: 'linear' });
    expect(vp).toEqual({ x: [-1, 1], y: [-1, 1] });
  });
});
