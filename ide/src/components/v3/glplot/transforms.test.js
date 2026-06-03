import { describe, it, expect } from 'vitest';
import { polarToCartesian } from './transforms';

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
