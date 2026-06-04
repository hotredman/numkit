import { describe, it, expect } from 'vitest';
import { niceTicks, logTicks, applyTickFormat, fmtTick } from './plotTicks';

describe('niceTicks', () => {
  it('snaps majors to a 1/2/5 ladder', () => {
    const { major } = niceTicks(0, 10, 5);
    expect(major[0]).toBe(0);
    expect(major).toContain(2);
    expect(major[major.length - 1]).toBeLessThanOrEqual(10);
  });
  it('degenerate range → single major', () => {
    expect(niceTicks(5, 5)).toEqual({ major: [5], minor: [] });
  });
  it('minor ticks exclude majors', () => {
    const { major, minor } = niceTicks(0, 10, 5);
    for (const m of minor) expect(major).not.toContain(m);
  });
});

describe('logTicks', () => {
  it('powers of 10 are majors, 2..9 multiples minors', () => {
    const { major, minor } = logTicks(1, 1000);
    expect(major).toEqual([1, 10, 100, 1000]);
    expect(minor).toContain(20);
    expect(minor).toContain(200);
  });
  it('empty for non-positive / degenerate', () => {
    expect(logTicks(-1, 100)).toEqual({ major: [], minor: [] });
    expect(logTicks(0, 10)).toEqual({ major: [], minor: [] });
    expect(logTicks(10, 10)).toEqual({ major: [], minor: [] });
  });
});

describe('applyTickFormat', () => {
  it('%d rounds', () => expect(applyTickFormat('%d', 3.7)).toBe('4'));
  it('%.2f fixed', () => expect(applyTickFormat('%.2f', 3.14159)).toBe('3.14'));
  it('%.1e exponential', () => expect(applyTickFormat('%.1e', 12345)).toBe('1.2e+4'));
  it('%.3g precision', () => expect(applyTickFormat('%.3g', 3.14159)).toBe('3.14'));
  it('null for empty / unsupported', () => {
    expect(applyTickFormat('', 5)).toBeNull();
    expect(applyTickFormat('%x', 5)).toBeNull();
    expect(applyTickFormat(undefined, 5)).toBeNull();
  });
});

describe('fmtTick', () => {
  it('exponential for tiny / huge', () => {
    expect(fmtTick(0.0001)).toBe('1.0e-4');
    expect(fmtTick(1e6)).toBe('1.0e+6');
  });
  it('magnitude-aware precision', () => {
    expect(fmtTick(123)).toBe('123');
    expect(fmtTick(12.3)).toBe('12.3');
    expect(fmtTick(1.23)).toBe('1.23');
    expect(fmtTick(0.123)).toBe('0.123');
  });
  it('zero', () => expect(fmtTick(0)).toBe('0.000'));
});
