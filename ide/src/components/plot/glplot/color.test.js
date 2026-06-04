import { describe, it, expect } from 'vitest';
import { cssColorToRGBA } from './color';

describe('cssColorToRGBA', () => {
  it('parses #rrggbb', () => {
    const c = cssColorToRGBA('#1f77b4');
    expect(c[0]).toBeCloseTo(31 / 255, 6);
    expect(c[1]).toBeCloseTo(119 / 255, 6);
    expect(c[2]).toBeCloseTo(180 / 255, 6);
    expect(c[3]).toBe(1);
  });
  it('expands #rgb shorthand', () => {
    expect(cssColorToRGBA('#abc')).toEqual([170 / 255, 187 / 255, 204 / 255, 1]);
  });
  it('parses the alpha in #rrggbbaa', () => {
    expect(cssColorToRGBA('#00000080')[3]).toBeCloseTo(128 / 255, 6);
  });
  it('parses rgb() and rgba()', () => {
    expect(cssColorToRGBA('rgb(255, 0, 0)')).toEqual([1, 0, 0, 1]);
    expect(cssColorToRGBA('rgba(0, 0, 0, 0.5)')).toEqual([0, 0, 0, 0.5]);
  });
  it('falls back to opaque black on anything unrecognized', () => {
    expect(cssColorToRGBA('chartreuse')).toEqual([0, 0, 0, 1]);
    expect(cssColorToRGBA(null)).toEqual([0, 0, 0, 1]);
  });
});
