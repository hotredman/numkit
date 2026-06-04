import { describe, it, expect } from 'vitest';
import { packXY } from './pack';

describe('packXY', () => {
  it('interleaves a gap-free series into one segment', () => {
    const out = packXY([1, 2, 3], [4, 5, 6]);
    expect(Array.from(out.data)).toEqual([1, 4, 2, 5, 3, 6]);
    expect(out.count).toBe(3);
    expect(out.segments).toEqual([{ offset: 0, count: 3 }]);
  });

  it('splits at a non-finite point into separate segments', () => {
    const out = packXY([1, NaN, 3, 4], [1, 2, 3, 4]);
    // NaN at index 1 drops that vertex and breaks the strip.
    expect(Array.from(out.data)).toEqual([1, 1, 3, 3, 4, 4]);
    expect(out.count).toBe(3);
    expect(out.segments).toEqual([{ offset: 0, count: 1 }, { offset: 1, count: 2 }]);
  });

  it('treats Inf as a gap too', () => {
    const out = packXY([1, 2, Infinity, 4], [0, 0, 0, 0]);
    expect(out.count).toBe(3);
    expect(out.segments).toEqual([{ offset: 0, count: 2 }, { offset: 2, count: 1 }]);
  });

  it('uses the shorter of x / y and handles empty', () => {
    expect(packXY([1, 2, 3], [9]).count).toBe(1);
    expect(packXY([], []).count).toBe(0);
    expect(packXY([], []).segments).toEqual([]);
  });

  it('returns a Float32Array trimmed to the finite count', () => {
    const out = packXY([1, NaN, 3], [1, 2, 3]);
    expect(out.data).toBeInstanceOf(Float32Array);
    expect(out.data.length).toBe(out.count * 2);
  });
});
