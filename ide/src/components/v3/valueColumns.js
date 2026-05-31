/**
 * valueColumns.js — pure column model + persistence for the ValueTable
 * shared by the Workspace list view and the struct/cell inspector.
 *
 * The "name" column (Field / Name) is always shown and is NOT in this
 * toggle list. Every other column can be toggled via the header's
 * right-click chooser, and the visible set persists to localStorage.
 *
 * Stat columns read from `row.stats` (min/max/mean/median/mode/var/std,
 * emitted by the engine for numeric values); `range` is derived (max−min).
 */

export const VALUE_COLUMNS = [
  { key: 'value',  label: 'Value' },
  { key: 'size',   label: 'Size',   align: 'right' },
  { key: 'class',  label: 'Class' },
  { key: 'min',    label: 'Min',    stat: 'min',    align: 'right' },
  { key: 'max',    label: 'Max',    stat: 'max',    align: 'right' },
  { key: 'range',  label: 'Range',  stat: 'range',  align: 'right' },
  { key: 'mean',   label: 'Mean',   stat: 'mean',   align: 'right' },
  { key: 'median', label: 'Median', stat: 'median', align: 'right' },
  { key: 'mode',   label: 'Mode',   stat: 'mode',   align: 'right' },
  { key: 'var',    label: 'Var',    stat: 'var',    align: 'right' },
  { key: 'std',    label: 'Std',    stat: 'std',    align: 'right' },
];

const KNOWN = new Set(VALUE_COLUMNS.map((c) => c.key));

// MATLAB-like default: identity columns on, statistics off.
export const DEFAULT_VISIBLE = ['value', 'size', 'class'];

/** Load the persisted visible-column set, falling back to the default.
 *  Unknown keys (e.g. from a renamed column) are dropped. */
export function loadVisibleColumns(storageKey) {
  try {
    const raw = localStorage.getItem(storageKey);
    if (!raw) return new Set(DEFAULT_VISIBLE);
    const arr = JSON.parse(raw);
    if (!Array.isArray(arr)) return new Set(DEFAULT_VISIBLE);
    return new Set(arr.filter((k) => KNOWN.has(k)));
  } catch {
    return new Set(DEFAULT_VISIBLE);
  }
}

export function saveVisibleColumns(storageKey, set) {
  try { localStorage.setItem(storageKey, JSON.stringify([...set])); } catch { /* ignore */ }
}

/** Pure toggle — returns a NEW Set with `key` flipped. */
export function toggleColumn(set, key) {
  const next = new Set(set);
  if (next.has(key)) next.delete(key); else next.add(key);
  return next;
}

/** Resolve a column's numeric value from a row's stats (range derived). */
export function statValue(stats, statKey) {
  if (!stats) return null;
  if (statKey === 'range') {
    return (typeof stats.min === 'number' && typeof stats.max === 'number')
      ? stats.max - stats.min : null;
  }
  const v = stats[statKey];
  return typeof v === 'number' ? v : null;
}

/** Compact numeric formatting for a stat cell (— when absent). */
export function fmtStat(n) {
  if (n == null || !Number.isFinite(n)) return '—';
  const a = Math.abs(n);
  // Very small / very large → exponential (keeps narrow columns readable),
  // even for integers like 1.5e7.
  if (a !== 0 && (a < 1e-3 || a >= 1e6)) return n.toExponential(2);
  if (Number.isInteger(n)) return String(n);
  return Number(n.toPrecision(5)).toString();
}
