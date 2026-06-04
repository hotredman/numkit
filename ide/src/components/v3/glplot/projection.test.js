import { describe, it, expect } from 'vitest';
import { makeProjection, projectPoint, unprojectX, unprojectY } from './projection';

describe('makeProjection — linear', () => {
  const p = makeProjection({ xMin: 0, xMax: 10, yMin: -5, yMax: 5 });
  it('maps xMin→-1, xMax→+1, midpoint→0', () => {
    expect(projectPoint(p, 0, 0)[0]).toBeCloseTo(-1, 12);
    expect(projectPoint(p, 10, 0)[0]).toBeCloseTo(1, 12);
    expect(projectPoint(p, 5, 0)[0]).toBeCloseTo(0, 12);
  });
  it('maps yMin→-1, yMax→+1 (clip-y up)', () => {
    expect(projectPoint(p, 0, -5)[1]).toBeCloseTo(-1, 12);
    expect(projectPoint(p, 0, 5)[1]).toBeCloseTo(1, 12);
    expect(projectPoint(p, 0, 0)[1]).toBeCloseTo(0, 12);
  });
});

describe('makeProjection — reversed axes', () => {
  it('flips x: xMin→+1, xMax→-1', () => {
    const p = makeProjection({ xMin: 0, xMax: 10, yMin: 0, yMax: 1, xRev: true });
    expect(projectPoint(p, 0, 0)[0]).toBeCloseTo(1, 12);
    expect(projectPoint(p, 10, 0)[0]).toBeCloseTo(-1, 12);
  });
  it('flips y: yMin→+1, yMax→-1', () => {
    const p = makeProjection({ xMin: 0, xMax: 1, yMin: 0, yMax: 10, yRev: true });
    expect(projectPoint(p, 0, 0)[1]).toBeCloseTo(1, 12);
    expect(projectPoint(p, 0, 10)[1]).toBeCloseTo(-1, 12);
  });
});

describe('makeProjection — log axis', () => {
  const p = makeProjection({ xMin: 1, xMax: 1000, yMin: 1, yMax: 1, xLog: true });
  it('maps endpoints and the geometric mid', () => {
    expect(projectPoint(p, 1, 1)[0]).toBeCloseTo(-1, 12);
    expect(projectPoint(p, 1000, 1)[0]).toBeCloseTo(1, 12);
    // sqrt(1*1000) ≈ 31.62 is the log-space midpoint → clip 0
    expect(projectPoint(p, Math.sqrt(1000), 1)[0]).toBeCloseTo(0, 12);
  });
});

describe('unproject — round trip', () => {
  it('linear and log invert projectPoint', () => {
    const p = makeProjection({ xMin: -3, xMax: 7, yMin: 2, yMax: 200, yLog: true });
    const [cx, cy] = projectPoint(p, 4.2, 50);
    expect(unprojectX(p, cx)).toBeCloseTo(4.2, 9);
    expect(unprojectY(p, cy)).toBeCloseTo(50, 6);
  });
});

describe('makeProjection — degenerate range guard', () => {
  it('produces finite numbers when min === max', () => {
    const p = makeProjection({ xMin: 5, xMax: 5, yMin: 0, yMax: 1 });
    const [cx, cy] = projectPoint(p, 5, 0.5);
    expect(Number.isFinite(cx)).toBe(true);
    expect(Number.isFinite(cy)).toBe(true);
  });
});
