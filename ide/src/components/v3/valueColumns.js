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

/** Build the ContextMenu items for a chooser (Select all · Clear all ·
 *  separator · optional locked row · per-`defs` checkbox toggles). Shared
 *  by every chooser surface — the table header, the Workspace toolbar
 *  button, and the matrix StatsBar button — so there's one definition.
 *  `defs` is a list of { key, label }; `lockedLabel` (optional) renders a
 *  disabled always-checked row (e.g. the table's Name column). */
export function buildChooserItems({ defs, visible, setVisible, lockedLabel }) {
  const items = [
    { label: 'Select all', keepOpen: true, onClick: () => setVisible(new Set(defs.map((d) => d.key))) },
    { label: 'Clear all',  keepOpen: true, onClick: () => setVisible(new Set()) },
    { separator: true },
  ];
  if (lockedLabel) items.push({ label: `✓ ${lockedLabel}`, disabled: true });
  for (const d of defs) {
    items.push({
      label: `${visible.has(d.key) ? '✓' : ' '} ${d.label}`,
      keepOpen: true,
      onClick: () => setVisible((prev) => toggleColumn(prev, d.key)),
    });
  }
  return items;
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

/* ──────────────────────────────────────────────────────────────────────
 * Matrix StatsBar — the same statistics, but as an aggregate row over a
 * whole matrix (not per-row columns). Adds the element count `n`. The
 * chooser/persistence mirror the table column chooser so all three
 * contexts (workspace · struct · matrix) share one stats vocabulary.
 * ────────────────────────────────────────────────────────────────────── */
export const STAT_BAR = [
  { key: 'min',    label: 'min' },
  { key: 'max',    label: 'max' },
  { key: 'range',  label: 'range' },
  { key: 'mean',   label: 'mean' },
  { key: 'median', label: 'median' },
  { key: 'mode',   label: 'mode' },
  { key: 'var',    label: 'var' },
  { key: 'std',    label: 'std' },
  { key: 'n',      label: 'n' },
];
const STAT_BAR_KNOWN = new Set(STAT_BAR.map((d) => d.key));
export const DEFAULT_STAT_BAR = ['min', 'max', 'mean', 'n'];

export function loadStatBar(storageKey) {
  try {
    const raw = localStorage.getItem(storageKey);
    if (!raw) return new Set(DEFAULT_STAT_BAR);
    const arr = JSON.parse(raw);
    if (!Array.isArray(arr)) return new Set(DEFAULT_STAT_BAR);
    return new Set(arr.filter((k) => STAT_BAR_KNOWN.has(k)));
  } catch {
    return new Set(DEFAULT_STAT_BAR);
  }
}

// Saving is shape-agnostic (a list of keys) — reuse one writer.
export const saveStatBar = saveVisibleColumns;

/** Resolve a stats-bar value: `n` reads the element count; everything
 *  else (incl. derived `range`) goes through statValue. */
export function statBarValue(stats, key) {
  if (!stats) return null;
  if (key === 'n') return typeof stats.n === 'number' ? stats.n : null;
  return statValue(stats, key);
}

/** Compute the full stat set over a flat array of values (client-side
 *  counterpart of the engine's computeValueStats — for drilled matrix
 *  fields whose data arrives inline). Non-numbers are ignored; returns
 *  null when no finite number remains. Sample (N−1) variance; mode is the
 *  smallest most-frequent value. */
export function aggregateStats(values) {
  const v = [];
  for (const x of values) if (typeof x === 'number' && Number.isFinite(x)) v.push(x);
  const n = v.length;
  if (n === 0) return null;
  let sum = 0, min = v[0], max = v[0];
  for (const x of v) { sum += x; if (x < min) min = x; if (x > max) max = x; }
  const mean = sum / n;
  const sorted = [...v].sort((a, b) => a - b);
  const median = n % 2 ? sorted[(n - 1) / 2] : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
  let acc = 0; for (const x of v) acc += (x - mean) ** 2;
  const variance = n >= 2 ? acc / (n - 1) : 0;
  let mode = sorted[0], bestCnt = 1, cur = 1;
  for (let i = 1; i < n; i++) {
    cur = sorted[i] === sorted[i - 1] ? cur + 1 : 1;
    if (cur > bestCnt) { bestCnt = cur; mode = sorted[i]; }
  }
  return { min, max, mean, median, mode, var: variance, std: Math.sqrt(variance), n };
}
