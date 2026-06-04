import { describe, it, expect } from 'vitest';
import { polarToCartesian, polarToScreen } from './transforms';

describe('polarToCartesian', () => {
  it('converts degrees on the unit circle', () => {
    const { x, y } = polarToCartesian([0, 90, 180], [1, 1, 1], { degrees: true });
    expect(x[0]).toBeCloseTo(1, 12);  expect(y[0]).toBeCloseTo(0, 12);
    expect(x[1]).toBeCloseTo(0, 12);  expect(y[1]).toBeCloseTo(1, 12);
    expect(x[2]).toBeCloseTo(-1, 12); expect(y[2]).toBeCloseTo(0, 12);
  });

  it('converts radians and scales by rho', () => {
    const { x, y } = polarToCartesian([0, Math.PI / 2], [2, 3]);
    expect(x[0]).toBeCloseTo(2, 12); expect(y[0]).toBeCloseTo(0, 12);
    expect(x[1]).toBeCloseTo(0, 12); expect(y[1]).toBeCloseTo(3, 12);
  });

  it('cw direction mirrors the angle', () => {
    const ccw = polarToCartesian([90], [1], { degrees: true });
    const cw  = polarToCartesian([90], [1], { degrees: true, direction: 'cw' });
    expect(cw.y[0]).toBeCloseTo(-ccw.y[0], 12);
  });

  it('applies thetaZero offset (radians)', () => {
    const { x, y } = polarToCartesian([0], [1], { thetaZero: Math.PI / 2 });
    expect(x[0]).toBeCloseTo(0, 12);
    expect(y[0]).toBeCloseTo(1, 12);
  });

  it('uses the shorter array and handles empty', () => {
    expect(polarToCartesian([0, 1, 2], [1]).x.length).toBe(1);
    expect(polarToCartesian([], []).x.length).toBe(0);
  });
});

describe('polarToScreen', () => {
  const layout = { cx: 100, cy: 100, radius: 50, rMin: 0, rMax: 1, zero: 0, dirSign: 1 };

  it('maps (θ,ρ) to PolarPlot screen pixels (cx+cos·r, cy-sin·r)', () => {
    const { x, y } = polarToScreen([0, Math.PI / 2], [1, 1], layout);
    expect(x[0]).toBeCloseTo(150, 4); expect(y[0]).toBeCloseTo(100, 4);  // θ=0  → right
    expect(x[1]).toBeCloseTo(100, 4); expect(y[1]).toBeCloseTo(50, 4);   // θ=90°→ up (screen −y)
  });

  it('scales ρ over [rMin,rMax] → [0,radius]', () => {
    const { x } = polarToScreen([0], [0.5], { ...layout, rMin: 0, rMax: 2 });
    expect(x[0]).toBeCloseTo(100 + (0.5 / 2) * 50, 4);   // r = 12.5 → x = 112.5
  });

  it('reflects negative ρ to (θ+π, |ρ|)', () => {
    const { x, y } = polarToScreen([0], [-1], layout);
    expect(x[0]).toBeCloseTo(50, 4);   // θ→π → left
    expect(y[0]).toBeCloseTo(100, 4);
  });

  it('honours dirSign (clockwise)', () => {
    const ccw = polarToScreen([Math.PI / 2], [1], layout);
    const cw  = polarToScreen([Math.PI / 2], [1], { ...layout, dirSign: -1 });
    expect(cw.y[0]).toBeCloseTo(150, 4);   // mirrored: down instead of up
    expect(ccw.y[0]).toBeCloseTo(50, 4);
  });

  it('emits NaN for non-finite samples (pack gaps them)', () => {
    const { x, y } = polarToScreen([0, NaN, 1], [1, 1, Infinity], layout);
    expect(Number.isNaN(x[1])).toBe(true);
    expect(Number.isNaN(y[2])).toBe(true);
    expect(Number.isFinite(x[0])).toBe(true);
  });
});
