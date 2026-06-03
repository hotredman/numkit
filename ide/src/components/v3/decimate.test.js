import { describe, it, expect } from 'vitest';
import {
  visibleRange, decimateM4, decimateM2, decimateLTTB, decimateSeries,
  buildPyramid, decimateLOD, isMonotonicX,
} from './decimate';

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

describe('decimateM2', () => {
  it('preserves global min/max and emits ≤ 2*width points', () => {
    const N = 100000, W = 800;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 50));
    y[40000] = 77; y[90000] = -55;
    const out = decimateM2(x, y, 0, N - 1, W);
    expect(Math.max(...out.y)).toBe(77);
    expect(Math.min(...out.y)).toBe(-55);
    expect(out.x.length).toBeLessThanOrEqual(2 * W);
    for (let i = 1; i < out.x.length; i++) {
      expect(out.x[i]).toBeGreaterThanOrEqual(out.x[i - 1]);
    }
  });
  it('is lighter than M4 on the same data', () => {
    const N = 100000, W = 800;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 3) + (i % 7));   // dense extrema
    const m2 = decimateM2(x, y, 0, N - 1, W);
    const m4 = decimateM4(x, y, 0, N - 1, W);
    expect(m2.x.length).toBeLessThanOrEqual(m4.x.length);
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

describe('buildPyramid', () => {
  it('level 0 is the raw series; each level is no larger than half the prior', () => {
    const N = 200000;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 100));
    const levels = buildPyramid(x, y, 8000);
    expect(levels[0].x).toBe(x);              // raw, same reference
    expect(levels.length).toBeGreaterThan(1);
    for (let i = 1; i < levels.length; i++) {
      expect(levels[i].x.length).toBeLessThanOrEqual(levels[i - 1].x.length / 2 + 4);
    }
    expect(levels[levels.length - 1].x.length).toBeLessThanOrEqual(8000);
  });

  it('preserves global extrema all the way to the coarsest level', () => {
    const N = 200000;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 100));
    y[12345] = 50;     // spike must survive every pooling step
    y[60000] = -40;
    const levels = buildPyramid(x, y, 4000);
    const coarse = levels[levels.length - 1];
    expect(Math.max(...coarse.y)).toBe(50);
    expect(Math.min(...coarse.y)).toBe(-40);
  });
});

describe('decimateLOD', () => {
  it('bounds output points and stays in range at full zoom-out', () => {
    const N = 1000000, W = 1000;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 500));
    const levels = buildPyramid(x, y);
    const out = decimateLOD(levels, 0, N - 1, W, 'm4');
    expect(out.x.length).toBeLessThanOrEqual(4 * W);
    expect(out.x.length).toBeGreaterThan(0);
  });

  it('keeps a spike visible when fully zoomed out (coarse level)', () => {
    const N = 1000000, W = 1000;
    const x = ramp(N);
    const y = x.map(() => 0);
    y[500000] = 99;
    const levels = buildPyramid(x, y);
    const out = decimateLOD(levels, 0, N - 1, W, 'm4');
    expect(Math.max(...out.y)).toBe(99);
  });

  it('restricts to the visible range when zoomed in', () => {
    const N = 1000000, W = 1000;
    const x = ramp(N);
    const y = x.map((i) => i);
    const levels = buildPyramid(x, y);
    const out = decimateLOD(levels, 400000, 401000, W, 'm4');
    expect(out.x[0]).toBeGreaterThanOrEqual(399000);
    expect(out.x[out.x.length - 1]).toBeLessThanOrEqual(402000);
    expect(out.x.length).toBeLessThanOrEqual(4 * W);
  });
});

describe('non-monotonic x (voronoi / triplot / parametric)', () => {
  // Regression: a multi-segment line whose x doubles back used to be sliced by
  // the ascending-x binary search in visibleRange, dropping most segments — a
  // voronoi diagram rendered with nearly all its cell edges missing. Such a
  // series must now be handed back RAW (no x-window, no decimation).
  const X = [0, 1, 2, 3, 9, 0.5, 1.5, 2.5];     // doubles back at index 5
  const Y = [0, 1, 0, 1, 5, 2,   3,   2];

  it('isMonotonicX flags ascending (with gaps) vs doubling-back', () => {
    expect(isMonotonicX([0, 1, 2, 3])).toBe(true);
    expect(isMonotonicX([0, 1, NaN, 2, 3])).toBe(true);    // gaps ignored
    expect(isMonotonicX([5, 5, 5])).toBe(true);            // equal is fine
    expect(isMonotonicX([0, 1, NaN, 0.5, 3])).toBe(false); // 0.5 < 1
    expect(isMonotonicX(X)).toBe(false);
    expect(isMonotonicX([3, 1])).toBe(false);
  });

  it('decimateSeries returns the WHOLE series, not a windowed slice', () => {
    // visibleRange(X,-1,5) is [0,5] for this data → the old code sliced an
    // [0,1,2,3,9] prefix and dropped the last three points.
    const out = decimateSeries(X, Y, -1, 5, 700, 'm4');
    expect(out.decimated).toBe(false);
    expect(out.x).toEqual(X);
    expect(out.y).toEqual(Y);
  });

  it('buildPyramid keeps only the raw level (cannot pyramid non-monotonic x)', () => {
    const levels = buildPyramid(X, Y, 4);   // tiny target would force pooling if monotonic
    expect(levels.length).toBe(1);
    expect(levels[0].x).toBe(X);
  });

  it('decimateLOD renders the raw series unchanged', () => {
    const levels = buildPyramid(X, Y);
    const out = decimateLOD(levels, -1, 5, 700, 'm4');
    expect(out.x).toEqual(X);
    expect(out.y).toEqual(Y);
  });

  it('still decimates a normal ascending series (no behaviour change)', () => {
    const N = 100000, W = 800;
    const x = ramp(N);
    const y = x.map((i) => Math.sin(i / 50));
    const out = decimateSeries(x, y, 0, N - 1, W, 'm4');
    expect(out.decimated).toBe(true);                 // monotonic path intact
    expect(out.x.length).toBeLessThanOrEqual(4 * W);
  });
});
