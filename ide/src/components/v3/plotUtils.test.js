import { describe, it, expect } from 'vitest';
import { logClampRange, previewViewport } from './plotUtils';

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
