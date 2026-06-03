import { describe, it, expect } from 'vitest';
import { visibleRange, decimateM4, decimateLTTB, decimateSeries } from './decimate';

const ramp = (n) => Array.from({ length: n }, (_, i) => i);

describe('visibleRange', () => {
  it('covers the window padded by one sample each side', () => {
    const x = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    const [i0, i1] = visibleRange(x, 3, 6);
    expect(x[i0]).toBeLessThanOrEqual(3);       // padded left so the line enters
    expect(x[i1 - 1]).toBeGreaterThanOrEqual(6); // padded right so it exits
  });
  it('clamps to the array bounds', () => {
    const x = [0, 1, 2, 3, 4];
    expect(visibleRange(x, -100, 100)).toEqual([0, 5]);
    expect(visibleRange([], 0, 1)).toEqual([0, 0]);
  });
});

describe('decimateM4', () => {
  it('preserves the global min and max (spikes survive)', () => {
    const N = 1000;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 7));
    y[500] = 99;    // spike
    y[123] = -99;   // dip
    const out = decimateM4(x, y, 0, N - 1, 50);
    expect(Math.max(...out.y)).toBe(99);
    expect(Math.min(...out.y)).toBe(-99);
  });

  it('emits at most 4*width points and keeps x ascending', () => {
    const N = 100000, W = 800;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 50));
    const out = decimateM4(x, y, 0, N - 1, W);
    expect(out.x.length).toBeLessThanOrEqual(4 * W);
    expect(out.x.length).toBeGreaterThan(0);
    for (let i = 1; i < out.x.length; i++) {
      expect(out.x[i]).toBeGreaterThanOrEqual(out.x[i - 1]);
    }
  });

  it('restricts to the visible x-range (zoom)', () => {
    const N = 1000;
    const x = ramp(N);
    const y = x.slice();
    const out = decimateM4(x, y, 100, 200, 50);
    expect(out.x[0]).toBeGreaterThanOrEqual(99);            // padded one left
    expect(out.x[out.x.length - 1]).toBeLessThanOrEqual(201); // padded one right
  });
});

describe('decimateLTTB', () => {
  it('keeps both endpoints and exactly `threshold` points', () => {
    const N = 1000;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 11));
    const out = decimateLTTB(x, y, 0, N - 1, 100);
    expect(out.x[0]).toBe(0);
    expect(out.x[out.x.length - 1]).toBe(N - 1);
    expect(out.x.length).toBe(100);
  });

  it('returns the raw slice when threshold >= N', () => {
    const x = ramp(10), y = ramp(10);
    const out = decimateLTTB(x, y, 0, 9, 100);
    expect(out.x.length).toBe(10);
  });
});

describe('decimateSeries', () => {
  it('passes through unchanged when the series fits the width', () => {
    const x = [0, 1, 2, 3, 4], y = [0, 1, 2, 3, 4];
    const out = decimateSeries(x, y, 0, 4, 100, 'm4');
    expect(out.decimated).toBe(false);
    expect(out.y).toEqual([0, 1, 2, 3, 4]);
  });

  it('decimates when N >> width and reports the source size', () => {
    const N = 100000, W = 800;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 50));
    const out = decimateSeries(x, y, 0, N - 1, W, 'm4');
    expect(out.decimated).toBe(true);
    expect(out.x.length).toBeLessThanOrEqual(4 * W);
    expect(out.n).toBe(N);
  });

  it("'none' returns the raw visible slice", () => {
    const N = 10000;
    const x = ramp(N), y = ramp(N);
    const out = decimateSeries(x, y, 0, N - 1, 100, 'none');
    expect(out.decimated).toBe(false);
    expect(out.x.length).toBe(N);
  });

  it('lttb path decimates a large series', () => {
    const N = 50000, W = 600;
    const x = ramp(N);
    const y = x.map((i) => i + (i % 2));
    const out = decimateSeries(x, y, 0, N - 1, W, 'lttb');
    expect(out.decimated).toBe(true);
    expect(out.x.length).toBeLessThanOrEqual(W + 2);
  });
});
