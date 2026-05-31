// @vitest-environment jsdom
import { describe, it, expect, beforeEach } from 'vitest';
import {
  VALUE_COLUMNS, DEFAULT_VISIBLE, loadVisibleColumns, saveVisibleColumns,
  toggleColumn, statValue, fmtStat,
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
