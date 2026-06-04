import { describe, it, expect } from 'vitest';
import { pageCount, pageToSubs, subsToPage } from './sliceNav';

describe('pageCount', () => {
  it('is 1 for a 2-D array (no higher dims)', () => {
    expect(pageCount([4, 5])).toBe(1);
    expect(pageCount([1, 1])).toBe(1);
  });
  it('equals dim-3 for a 3-D array', () => {
    expect(pageCount([4, 5, 3])).toBe(3);
  });
  it('is the product of dims[2..] for N-D', () => {
    expect(pageCount([2, 3, 4, 5])).toBe(20);
    expect(pageCount([2, 2, 2, 2, 2])).toBe(8);
  });
  it('tolerates a non-array', () => {
    expect(pageCount(undefined)).toBe(1);
  });
});

describe('pageToSubs / subsToPage', () => {
  it('3-D: page is the single dim-3 subscript', () => {
    const dims = [4, 5, 3];
    expect(pageToSubs(0, dims)).toEqual([0]);
    expect(pageToSubs(2, dims)).toEqual([2]);
    expect(subsToPage([2], dims)).toBe(2);
  });

  it('N-D: column-major over dims[2..]', () => {
    const dims = [2, 3, 4, 5];   // 20 pages, dim3=4, dim4=5
    // page = k3 + k4*4
    expect(pageToSubs(0, dims)).toEqual([0, 0]);
    expect(pageToSubs(4, dims)).toEqual([0, 1]);   // 4 % 4 = 0, 4/4 = 1
    expect(pageToSubs(7, dims)).toEqual([3, 1]);   // 7 % 4 = 3, 7/4 = 1
    expect(subsToPage([0, 1], dims)).toBe(4);
    expect(subsToPage([3, 1], dims)).toBe(7);
  });

  it('round-trips every page for a 4-D shape', () => {
    const dims = [1, 1, 3, 4];   // 12 pages
    for (let p = 0; p < pageCount(dims); p++) {
      expect(subsToPage(pageToSubs(p, dims), dims)).toBe(p);
    }
  });

  it('subsToPage clamps out-of-range subscripts into the valid slice', () => {
    const dims = [2, 2, 3];
    expect(subsToPage([99], dims)).toBe(2);   // clamps to dim3-1 = 2
    expect(subsToPage([-1], dims)).toBe(0);
  });
});
