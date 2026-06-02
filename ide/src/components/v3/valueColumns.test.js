// @vitest-environment jsdom
import { describe, it, expect, beforeEach } from 'vitest';
import {
  VALUE_COLUMNS, DEFAULT_VISIBLE, loadVisibleColumns, saveVisibleColumns,
  toggleColumn, statValue, fmtStat,
  STAT_BAR, DEFAULT_STAT_BAR, loadStatBar, saveStatBar, statBarValue, aggregateStats,
  toNumericCell, heatColor, heatmapCellBackground,
} from './valueColumns';

beforeEach(() => { try { localStorage.clear(); } catch { /* none */ } });

describe('column set persistence', () => {
  it('defaults to value/size/class when nothing is stored', () => {
    expect([...loadVisibleColumns('k')].sort()).toEqual([...DEFAULT_VISIBLE].sort());
  });
  it('round-trips a saved set', () => {
    saveVisibleColumns('k', new Set(['value', 'mean', 'std']));
    expect([...loadVisibleColumns('k')].sort()).toEqual(['mean', 'std', 'value']);
  });
  it('drops unknown keys on load', () => {
    localStorage.setItem('k', JSON.stringify(['value', 'bogus', 'std']));
    expect([...loadVisibleColumns('k')].sort()).toEqual(['std', 'value']);
  });
  it('falls back to default on malformed JSON', () => {
    localStorage.setItem('k', '{not json');
    expect([...loadVisibleColumns('k')].sort()).toEqual([...DEFAULT_VISIBLE].sort());
  });
});

describe('toggleColumn', () => {
  it('adds a missing key and removes a present one (immutably)', () => {
    const a = new Set(['value']);
    const b = toggleColumn(a, 'mean');
    expect([...b].sort()).toEqual(['mean', 'value']);
    expect([...a]).toEqual(['value']);          // original untouched
    expect([...toggleColumn(b, 'mean')]).toEqual(['value']);
  });
});

describe('statValue', () => {
  const stats = { min: 2, max: 10, mean: 5, median: 4, mode: 2, var: 8, std: 2.83 };
  it('reads a direct stat', () => expect(statValue(stats, 'mean')).toBe(5));
  it('derives range = max - min', () => expect(statValue(stats, 'range')).toBe(8));
  it('returns null when stats absent', () => expect(statValue(null, 'mean')).toBeNull());
  it('returns null when a stat is missing', () => expect(statValue({ min: 1 }, 'max')).toBeNull());
});

describe('fmtStat', () => {
  it('— for null/NaN', () => { expect(fmtStat(null)).toBe('—'); expect(fmtStat(NaN)).toBe('—'); });
  it('integers verbatim', () => expect(fmtStat(8489)).toBe('8489'));
  it('precision-trimmed floats', () => expect(fmtStat(2.8284271)).toBe('2.8284'));
  it('exponential for tiny / huge', () => {
    expect(fmtStat(0.0001)).toBe('1.00e-4');
    expect(fmtStat(1.5e7)).toBe('1.50e+7');
  });
});

describe('VALUE_COLUMNS', () => {
  it('covers the MATLAB stat set and excludes the name column', () => {
    const keys = VALUE_COLUMNS.map((c) => c.key);
    expect(keys).toEqual(['value', 'size', 'class', 'min', 'max', 'range',
      'mean', 'median', 'mode', 'var', 'std']);
  });
});

describe('stats bar (matrix context)', () => {
  it('STAT_BAR adds n to the stat set; default shows min/max/mean/n', () => {
    expect(STAT_BAR.map((d) => d.key)).toEqual(
      ['min', 'max', 'range', 'mean', 'median', 'mode', 'var', 'std', 'n']);
    expect(DEFAULT_STAT_BAR).toEqual(['min', 'max', 'mean', 'n']);
  });
  it('loadStatBar defaults + round-trips via saveStatBar', () => {
    expect([...loadStatBar('b')].sort()).toEqual([...DEFAULT_STAT_BAR].sort());
    saveStatBar('b', new Set(['mean', 'std', 'n']));
    expect([...loadStatBar('b')].sort()).toEqual(['mean', 'n', 'std']);
  });
  it('statBarValue reads n and derives range', () => {
    const s = { min: 2, max: 8, mean: 5, n: 6 };
    expect(statBarValue(s, 'n')).toBe(6);
    expect(statBarValue(s, 'range')).toBe(6);
    expect(statBarValue(null, 'n')).toBeNull();
  });
});

describe('aggregateStats', () => {
  it('matches the engine formulas for [2 4 4 4 6 8]', () => {
    const s = aggregateStats([2, 4, 4, 4, 6, 8]);
    expect(s.min).toBe(2);
    expect(s.max).toBe(8);
    expect(s.n).toBe(6);
    expect(s.mean).toBeCloseTo(4.6667, 3);
    expect(s.median).toBe(4);
    expect(s.mode).toBe(4);
    expect(s.var).toBeCloseTo(4.2667, 3);   // sample (N-1)
    expect(s.std).toBeCloseTo(2.0656, 3);
  });
  it('ignores non-numbers / non-finite; null when empty', () => {
    expect(aggregateStats(['a', NaN, Infinity, null])).toBeNull();
    expect(aggregateStats([3, 'x', NaN, 5]).n).toBe(2);
  });
  it('even count → median is the mean of the two middles', () => {
    expect(aggregateStats([1, 2, 3, 4]).median).toBe(2.5);
  });
  it('treats logical booleans as 1 / 0 (so logical matrices get stats + heatmap)', () => {
    const s = aggregateStats([true, false, true, true]);
    expect(s.min).toBe(0);
    expect(s.max).toBe(1);
    expect(s.n).toBe(4);
    expect(s.mean).toBe(0.75);
  });
});

describe('toNumericCell', () => {
  it('passes finite numbers through unchanged', () => {
    expect(toNumericCell(3.5)).toBe(3.5);
    expect(toNumericCell(0)).toBe(0);
    expect(toNumericCell(-2)).toBe(-2);
  });
  it('maps logical booleans to 1 / 0', () => {
    expect(toNumericCell(true)).toBe(1);
    expect(toNumericCell(false)).toBe(0);
  });
  it('returns NaN for char-cell strings, null, undefined', () => {
    expect(toNumericCell('x')).toBeNaN();
    expect(toNumericCell(null)).toBeNaN();
    expect(toNumericCell(undefined)).toBeNaN();
  });
});

describe('heatColor', () => {
  it('is transparent for a degenerate range (min === max)', () => {
    expect(heatColor(5, 5, 5)).toBe('transparent');
  });
  it('produces an oklch ramp colour for an in-range value', () => {
    expect(heatColor(0.5, 0, 1)).toMatch(/^oklch\(/);
    expect(heatColor(0, 0, 1)).not.toBe(heatColor(1, 0, 1));  // ends differ
  });
});

describe('heatmapCellBackground', () => {
  const stats = { min: 0, max: 1 };

  it('is undefined when the heatmap is off', () => {
    expect(heatmapCellBackground(true, stats, false)).toBeUndefined();
    expect(heatmapCellBackground(1, stats, false)).toBeUndefined();
  });

  it('is undefined when stats are not ready yet', () => {
    expect(heatmapCellBackground(1, null, true)).toBeUndefined();
  });

  // The bug: logical cells arrive as JS booleans and used to be skipped by
  // the `typeof v === 'number'` guard, so a logical matrix never coloured.
  it('colours logical booleans (true/false → 1/0) at opposite ramp ends', () => {
    const hi = heatmapCellBackground(true, stats, true);
    const lo = heatmapCellBackground(false, stats, true);
    expect(hi).toBeTruthy();
    expect(lo).toBeTruthy();
    expect(hi).not.toBe(lo);
  });

  it('colours a boolean identically to its 0/1 number', () => {
    expect(heatmapCellBackground(true, stats, true)).toBe(heatmapCellBackground(1, stats, true));
    expect(heatmapCellBackground(false, stats, true)).toBe(heatmapCellBackground(0, stats, true));
  });

  it('leaves non-numeric cells (char strings / null) unpainted', () => {
    expect(heatmapCellBackground('x', stats, true)).toBeUndefined();
    expect(heatmapCellBackground(null, stats, true)).toBeUndefined();
  });

  it('is transparent when every cell shares one value (min === max)', () => {
    expect(heatmapCellBackground(1, { min: 1, max: 1 }, true)).toBe('transparent');
  });
});
