import { useState, useMemo, useEffect, useRef, useCallback } from 'react';
import { useTheme } from '../../theme';
import { pathToMatlabLValue, valueToMatlabRHS, isValidIdentifier } from './inspectorOps';
import ContextMenu from './ContextMenu';
import ValueTable from './ValueTable';
import StatsBar, { useStatChooser, StatChooserButton } from './StatsBar';
import { aggregateStats, toNumericCell, VALUE_COLUMNS, loadVisibleColumns, saveVisibleColumns } from './valueColumns';
import { useChooser, ChooserButton } from './chooser';
import { classify } from './adapters';

/* ======================================================================== */
/* Type metadata + tone palette                                             */
/* ======================================================================== */
const KIND_META = {
  scalar: { label: 'scalar', glyph: '∙', tone: 'cyan'   },
  vector: { label: 'vector', glyph: '⟶', tone: 'green'  },
  matrix: { label: 'matrix', glyph: '▦', tone: 'amber'  },
  string: { label: 'string', glyph: '"', tone: 'violet' },
  struct: { label: 'struct', glyph: '⊞', tone: 'pink'   },
  cell:   { label: 'cell',   glyph: '{}', tone: 'pink'   },
};

const TONE = {
  cyan:   { fg: '#6ec3b8', bg: 'rgba(124,224,211,0.07)', border: 'rgba(124,224,211,0.20)',
            fgL: '#0969da', bgL: '#ddf4ff', borderL: '#54aeff66' },
  green:  { fg: '#6fc28a', bg: 'rgba(127,217,154,0.07)', border: 'rgba(127,217,154,0.20)',
            fgL: '#1a7f37', bgL: '#dafbe1', borderL: '#4ac26b66' },
  amber:  { fg: '#d0a360', bg: 'rgba(233,184,112,0.07)', border: 'rgba(233,184,112,0.20)',
            fgL: '#9a6700', bgL: '#fff8c5', borderL: '#d4a72c66' },
  violet: { fg: '#9d89db', bg: 'rgba(182,156,242,0.07)', border: 'rgba(182,156,242,0.20)',
            fgL: '#6639ba', bgL: '#fbefff', borderL: '#c297ff66' },
  pink:   { fg: '#d877b8', bg: 'rgba(224,112,192,0.07)', border: 'rgba(224,112,192,0.20)',
            fgL: '#a040a0', bgL: '#ffeffb', borderL: '#e070c066' },
};

function pickTone(t, themeName) {
  const isLight = themeName === 'light';
  return isLight
    ? { fg: t.fgL, bg: t.bgL, border: t.borderL }
    : { fg: t.fg,  bg: t.bg,  border: t.border  };
}

function fmt(v, opts = {}) {
  if (typeof v === 'string') return v;
  if (v === 0) return '0';
  if (Number.isInteger(v) && Math.abs(v) < 10000) return String(v);
  const abs = Math.abs(v);
  if (abs >= 1e5 || (abs > 0 && abs < 1e-3)) return v.toExponential(opts.exp ?? 3);
  return v.toFixed(opts.fix ?? 4);
}

function heatColor(v, min, max) {
  if (min === max) return 'transparent';
  const t = (v - min) / (max - min);
  const hue = (1 - t) * 220 + t * 20;
  return `oklch(0.55 0.05 ${hue} / ${0.18 + 0.22 * Math.abs(t - 0.5) * 2})`;
}

/* ======================================================================== */
/* Card / row                                                               */
/* ======================================================================== */
//
// One mouse click opens the Variable Editor — the same gesture that opens
// a figure card in the Figures pane. There's no persistent selection
// state on purpose: a "selected" workspace variable used to swallow Enter
// keystrokes meant for the editor / console (the pane held a window-level
// keydown listener), so a click in the workspace would silently break
// newline insertion elsewhere. Hover highlight only.

// One card for both contexts — a workspace variable or a struct field.
// `row` is the normalized shape EntityBrowser uses:
//   { key, name, value, size, klass, kind, stats, drill }
function EntityCard({ row, nameCell, onOpen, onContextMenu }) {
  const { themeName } = useTheme();
  const meta = KIND_META[row.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, themeName);
  return (
    <div
      className="var-card"
      onClick={row.drill !== false ? () => onOpen?.(row) : undefined}
      onContextMenu={onContextMenu ? (e) => { e.preventDefault(); e.stopPropagation(); onContextMenu(row, e); } : undefined}
      role="button"
      aria-label={`Open ${row.name}`}
    >
      <div className="var-card-head">
        <span className="var-name">{nameCell ? nameCell(row) : row.name}</span>
        <span className="var-size">{row.size}</span>
        <span className="var-type-pill" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
          <span className="var-glyph">{meta.glyph}</span>{row.klass}
        </span>
      </div>
      <div className="var-card-body">
        <span className="var-preview">{row.value}</span>
      </div>
    </div>
  );
}

/* ======================================================================== */
/* Workspace toolbar                                                        */
/* ======================================================================== */
// Numeric size for sorting: bytes when known (variables), else element
// count parsed from the "R×C" string (struct fields).
function sizeMetric(row) {
  if (Number.isFinite(row.bytes)) return row.bytes;
  const m = String(row.size || '').match(/(\d+)\s*[x×]\s*(\d+)/);
  return m ? (+m[1]) * (+m[2]) : 0;
}

const VIEW_OPTS = ['cards', 'list'];
const SORT_OPTS = ['name', 'size', 'type'];

// Unified tabular browser — the one widget behind BOTH the Workspace
// panel and the struct/cell inspector's field list. Toolbar (filter ·
// sort · Σ▾ column chooser · cards/list toggle) over a cards grid or a
// ValueTable. The caller supplies normalized rows + open / context-menu
// handlers + an optional footer (e.g. the struct "+ new field" row);
// view & sort persist per `viewKey`/`sortKey`, columns via the shared
// `cols`/`setCols` chooser state. Filter & sort are display-only.
function EntityBrowser({
  rows, nameHeader = 'Name', countNoun = 'item', defaultView = 'cards',
  viewKey, sortKey, cols, setCols,
  nameCell, onOpen, onRowContextMenu, onAreaContextMenu, footer,
}) {
  const [query, setQuery] = useState('');
  const [sort, setSort] = useState(() => loadPref(sortKey, SORT_OPTS, 'name'));
  const [view, setView] = useState(() => loadPref(viewKey, VIEW_OPTS, defaultView));
  useEffect(() => { try { localStorage.setItem(viewKey, view); } catch { /* ignore */ } }, [viewKey, view]);
  useEffect(() => { try { localStorage.setItem(sortKey, sort); } catch { /* ignore */ } }, [sortKey, sort]);

  const filtered = useMemo(() => {
    const q = query.toLowerCase();
    const list = rows.filter((r) => r.name.toLowerCase().includes(q));
    list.sort((a, b) => {
      if (sort === 'size') return sizeMetric(b) - sizeMetric(a);
      if (sort === 'type') return String(a.klass || '').localeCompare(String(b.klass || ''));
      return a.name.localeCompare(b.name);
    });
    return list;
  }, [rows, query, sort]);

  const plural = (n) => `${n} ${countNoun}${n === 1 ? '' : 's'}`;

  return (
    <div className="entity-browser">
      <div className="ws-toolbar">
        <div className="ws-toolbar-left">
          <span className="ws-count">{plural(filtered.length)}</span>
          <span className="ws-sep" />
          <div className="ws-search">
            <svg width="11" height="11" viewBox="0 0 12 12" aria-hidden="true">
              <circle cx="5" cy="5" r="3.2" stroke="currentColor" strokeWidth="1.2" fill="none" />
              <path d="M7.4 7.4L10 10" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
            </svg>
            <input value={query} onChange={(e) => setQuery(e.target.value)}
              placeholder={`filter ${countNoun}s…`} spellCheck={false} />
          </div>
        </div>
        <div className="ws-toolbar-right">
          {view === 'list' && cols && (
            <ChooserButton className="ws-cols-btn" title="choose columns"
              label={<>Σ <span className="ve-caret">▾</span></>}
              defs={VALUE_COLUMNS} lockedLabel={nameHeader}
              visible={cols} setVisible={setCols} />
          )}
          <div className="ws-segmented" role="tablist" aria-label="Sort">
            {SORT_OPTS.map((k) => (
              <button key={k} role="tab" aria-selected={sort === k}
                className={sort === k ? 'is-active' : ''}
                onClick={() => setSort(k)}>sort: {k}</button>
            ))}
          </div>
          <div className="ws-segmented" role="tablist" aria-label="Layout">
            <button aria-selected={view === 'cards'} className={view === 'cards' ? 'is-active' : ''}
              onClick={() => setView('cards')} title="Cards view">
              <svg width="12" height="12" viewBox="0 0 12 12">
                <rect x="1" y="1"   width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
                <rect x="6.5" y="1" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
                <rect x="1" y="6.5" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
                <rect x="6.5" y="6.5" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
              </svg>
            </button>
            <button aria-selected={view === 'list'} className={view === 'list' ? 'is-active' : ''}
              onClick={() => setView('list')} title="List view">
              <svg width="12" height="12" viewBox="0 0 12 12">
                <rect x="1" y="2"   width="10" height="1.4" rx="0.5" fill="currentColor"/>
                <rect x="1" y="5.3" width="10" height="1.4" rx="0.5" fill="currentColor"/>
                <rect x="1" y="8.6" width="10" height="1.4" rx="0.5" fill="currentColor"/>
              </svg>
            </button>
          </div>
        </div>
      </div>

      {view === 'cards' ? (
        <div className="ws-grid" onContextMenu={onAreaContextMenu}>
          {filtered.map((r) => (
            <EntityCard key={r.key} row={r} nameCell={nameCell}
              onOpen={onOpen} onContextMenu={onRowContextMenu} />
          ))}
          {filtered.length === 0 && <div className="ws-empty">nothing matches “{query}”</div>}
        </div>
      ) : (
        <div className="ws-list" onContextMenu={onAreaContextMenu}>
          <ValueTable
            rows={filtered}
            nameHeader={nameHeader}
            nameCell={nameCell}
            visible={cols} setVisible={setCols}
            onRowClick={onOpen}
            onRowContextMenu={onRowContextMenu}
            emptyLabel={`nothing matches “${query}”`}
          />
        </div>
      )}
      {footer}
    </div>
  );
}

/* ======================================================================== */
/* Workspace panel (the main exported component for the bottom-dock tab)    */
/* ======================================================================== */
// localStorage keys for the Workspace display preferences. Same
// `numkit.ide.*` namespace + lazy-init / write-on-change pattern as
// the editor-pane layout in IDE.jsx, so the user's chosen view (cards
// vs table) and sort order survive restarts. The search `query` is
// intentionally NOT persisted — a stale filter on restart would hide
// variables for no visible reason.
const WS_VIEW_KEY = 'numkit.ide.workspace.view';
const WS_SORT_KEY = 'numkit.ide.workspace.sort';

function loadPref(key, allowed, fallback) {
  try {
    const v = localStorage.getItem(key);
    if (v && allowed.includes(v)) return v;
  } catch { /* private mode / unavailable */ }
  return fallback;
}

export function WorkspacePanel({ variables, onOpen }) {
  // Column visibility — shared key with the struct inspector's table.
  const [cols, setCols] = useChooser('numkit.ide.valuecols', loadVisibleColumns, saveVisibleColumns);
  const byName = useMemo(() => {
    const m = new Map();
    for (const v of variables) m.set(v.name, v);
    return m;
  }, [variables]);
  const rows = useMemo(() => variables.map((v) => ({
    key: v.name, name: v.name, value: v.preview, size: v.size,
    klass: v.type, kind: v.kind, bytes: v.bytes, stats: v.stats || null, drill: true,
  })), [variables]);

  return (
    <div className="workspace">
      <EntityBrowser
        rows={rows}
        nameHeader="Name"
        countNoun="variable"
        viewKey={WS_VIEW_KEY} sortKey={WS_SORT_KEY}
        cols={cols} setCols={setCols}
        onOpen={(row) => { const v = byName.get(row.name); if (v) onOpen(v); }}
        footer={(
          <div className="ws-hint">
            <kbd>click</kbd> open · <kbd>Esc</kbd> close editor
          </div>
        )}
      />
    </div>
  );
}

/* ======================================================================== */
/* Save-as menu (used inside Variable Editor)                               */
/* ======================================================================== */
function downloadBlob(filename, blob) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename;
  document.body.appendChild(a); a.click();
  setTimeout(() => { URL.revokeObjectURL(url); a.remove(); }, 100);
}

function exportData(variable, data, format) {
  const name = variable.name;
  if (format === 'csv') {
    const csv = data.map((row) => row.map((v) => typeof v === 'number' ? v : `"${v}"`).join(',')).join('\n');
    downloadBlob(`${name}.csv`, new Blob([csv], { type: 'text/csv' }));
  } else if (format === 'tsv') {
    const tsv = data.map((row) => row.map((v) => typeof v === 'number' ? v : `"${v}"`).join('\t')).join('\n');
    downloadBlob(`${name}.tsv`, new Blob([tsv], { type: 'text/tab-separated-values' }));
  } else if (format === 'json') {
    const obj = { name, size: variable.size, type: variable.type, data };
    downloadBlob(`${name}.json`, new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }));
  } else if (format === 'mat') {
    const text = `% Numkit save\n${name} = [\n${data.map((r) => '  ' + r.join(' ')).join(';\n')}\n];\n`;
    downloadBlob(`${name}.n`, new Blob([text], { type: 'text/plain' }));
  } else if (format === 'npy') {
    const rows = data.length, cols = data[0]?.length || 0;
    const flat = new Float64Array(rows * cols);
    for (let i = 0; i < rows; i++) for (let j = 0; j < cols; j++) flat[i * cols + j] = +data[i][j] || 0;
    const header = `{'descr': '<f8', 'fortran_order': False, 'shape': (${rows}, ${cols}), }`;
    const padded = header + ' '.repeat(63 - ((10 + header.length) % 64)) + '\n';
    const headerBytes = new TextEncoder().encode(padded);
    const magic = new Uint8Array([0x93, 0x4E, 0x55, 0x4D, 0x50, 0x59, 1, 0]);
    const lenBytes = new Uint8Array(2);
    new DataView(lenBytes.buffer).setUint16(0, headerBytes.length, true);
    const buf = new Uint8Array(magic.length + lenBytes.length + headerBytes.length + flat.byteLength);
    buf.set(magic, 0);
    buf.set(lenBytes, magic.length);
    buf.set(headerBytes, magic.length + lenBytes.length);
    buf.set(new Uint8Array(flat.buffer), magic.length + lenBytes.length + headerBytes.length);
    downloadBlob(`${name}.npy`, new Blob([buf], { type: 'application/octet-stream' }));
  }
}

function SaveAsMenu({ onPick, onClose }) {
  useEffect(() => {
    function onDoc(e) {
      if (!e.target.closest?.('.ve-saveas-menu') && !e.target.closest?.('.ve-saveas-trigger')) onClose();
    }
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, [onClose]);

  const items = [
    { id: 'csv',  label: 'CSV',           hint: 'comma-separated · spreadsheet' },
    { id: 'tsv',  label: 'TSV',           hint: 'tab-separated' },
    { id: 'json', label: 'JSON',          hint: 'with metadata' },
    { id: 'mat',  label: 'Numkit script', hint: '.n — re-runnable' },
    { id: 'npy',  label: 'NumPy .npy',    hint: 'float64 binary' },
  ];

  return (
    <div className="ve-saveas-menu" role="menu">
      <div className="ve-saveas-head">save as…</div>
      {items.map((it) => (
        <button key={it.id} role="menuitem" className="ve-saveas-item" onClick={() => onPick(it.id)}>
          <span className="ve-saveas-label">{it.label}</span>
          <span className="ve-saveas-hint">{it.hint}</span>
        </button>
      ))}
    </div>
  );
}

/* ======================================================================== */
/* Inline plot (Variable Editor: small chart of selected rows/cols)         */
/* ======================================================================== */
function PlotChart({ curves, xMin, xMax, yMin, yMax, xLabel }) {
  const wrapRef = useRef(null);
  const [W, setW] = useState(480);
  const H = 220;

  useEffect(() => {
    if (!wrapRef.current) return;
    const ro = new ResizeObserver((entries) => {
      const w = entries[0].contentRect.width;
      if (w && Math.abs(w - W) > 1) setW(Math.max(280, Math.round(w)));
    });
    ro.observe(wrapRef.current);
    return () => ro.disconnect();
  }, [W]);

  const padL = 48, padR = 14, padT = 10, padB = 32;
  const w = W - padL - padR, h = H - padT - padB;
  const sx = (v) => padL + ((v - xMin) / (xMax - xMin)) * w;
  const sy = (v) => padT + h - ((v - yMin) / (yMax - yMin)) * h;

  function niceTicks(min, max, target = 5) {
    const range = max - min;
    if (range <= 0) return [min];
    const rough = range / target;
    const pow = Math.pow(10, Math.floor(Math.log10(rough)));
    const norm = rough / pow;
    let step;
    if (norm < 1.5) step = 1 * pow;
    else if (norm < 3) step = 2 * pow;
    else if (norm < 7) step = 5 * pow;
    else step = 10 * pow;
    const start = Math.ceil(min / step) * step;
    const ticks = [];
    for (let v = start; v <= max + step * 1e-6; v += step) ticks.push(+v.toFixed(12));
    return ticks;
  }
  function fmtTick(v) {
    const a = Math.abs(v);
    if (a !== 0 && (a < 1e-3 || a >= 1e5)) return v.toExponential(1);
    if (a >= 100) return v.toFixed(0);
    if (a >= 10)  return v.toFixed(1);
    if (a >= 1)   return v.toFixed(2);
    return v.toFixed(3);
  }
  const xTicks = niceTicks(xMin, xMax, Math.max(3, Math.floor(w / 90)));
  const yTicks = niceTicks(yMin, yMax, 5);

  return (
    <div ref={wrapRef} className="ve-plot-chart-wrap">
      <svg width={W} height={H} className="ve-plot-svg" style={{ display: 'block', fontFamily: 'JetBrains Mono, monospace' }}>
        <rect x={padL} y={padT} width={w} height={h} fill="var(--plot-bg)" />
        {xTicks.map((v, i) => (
          <line key={`gx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + h} stroke="var(--plot-grid-min)" />
        ))}
        {yTicks.map((v, i) => (
          <line key={`gy${i}`} x1={padL} x2={padL + w} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid-min)" />
        ))}
        {yMin < 0 && yMax > 0 && (
          <line x1={padL} x2={padL + w} y1={sy(0)} y2={sy(0)} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
        )}
        {curves.map((c) => {
          const d = c.x.map((xv, i) => `${i === 0 ? 'M' : 'L'}${sx(xv).toFixed(1)},${sy(c.y[i]).toFixed(1)}`).join(' ');
          return <path key={c.name} d={d} stroke={c.color} strokeWidth="1.4" fill="none" strokeLinejoin="round" strokeLinecap="round" opacity="0.95" />;
        })}
        <rect x={padL} y={padT} width={w} height={h} fill="none" stroke="var(--plot-frame)" />
        {yTicks.map((v, i) => {
          const y = sy(v);
          if (y < padT - 1 || y > padT + h + 1) return null;
          return (
            <g key={`yl${i}`}>
              <line x1={padL - 3} x2={padL} y1={y} y2={y} stroke="var(--plot-tick)" />
              <text x={padL - 6} y={y + 3} fill="var(--plot-text)" fontSize="9.5" textAnchor="end">{fmtTick(v)}</text>
            </g>
          );
        })}
        {xTicks.map((v, i) => {
          const x = sx(v);
          if (x < padL - 1 || x > padL + w + 1) return null;
          return (
            <g key={`xl${i}`}>
              <line x1={x} x2={x} y1={padT + h} y2={padT + h + 3} stroke="var(--plot-tick)" />
              <text x={x} y={padT + h + 13} fill="var(--plot-text)" fontSize="9.5" textAnchor="middle">{fmtTick(v)}</text>
            </g>
          );
        })}
        <text x={padL + w / 2} y={H - 6} fill="var(--plot-text)" fontSize="9.5" textAnchor="middle">x: {xLabel}</text>
      </svg>
    </div>
  );
}

function MultiPickerControls({ mAxis, setMAxis, mSel, setMSel, rows, cols,
                               mXMode, setMXMode, mXSrc, setMXSrc,
                               pickerQuery, setPickerQuery }) {
  const limit = mAxis === 'col' ? cols : rows;
  const [open, setOpen] = useState(false);
  const ref = useRef(null);
  useEffect(() => {
    if (!open) return;
    function onDoc(e) { if (ref.current && !ref.current.contains(e.target)) setOpen(false); }
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, [open]);

  const items = useMemo(() => {
    const q = pickerQuery.trim().toLowerCase();
    const arr = [];
    for (let i = 0; i < limit; i++) {
      const label = `${mAxis} ${i + 1}`;
      if (!q || label.toLowerCase().includes(q) || String(i + 1).includes(q)) arr.push({ idx: i, label });
    }
    return arr;
  }, [pickerQuery, mAxis, limit]);

  function toggle(i) {
    const next = new Set(mSel);
    if (next.has(i)) next.delete(i); else next.add(i);
    setMSel(next);
  }
  function selectAll()  { setMSel(new Set(items.map((it) => it.idx))); }
  function selectNone() { setMSel(new Set()); }
  function selectRange(spec) {
    const next = new Set(mSel);
    spec.split(',').forEach((part) => {
      const m = part.trim().match(/^(\d+)\s*-\s*(\d+)$/);
      if (m) {
        const a = +m[1], b = +m[2];
        for (let i = Math.min(a, b); i <= Math.max(a, b); i++) if (i >= 1 && i <= limit) next.add(i - 1);
      } else {
        const n = +part.trim();
        if (Number.isFinite(n) && n >= 1 && n <= limit) next.add(n - 1);
      }
    });
    setMSel(next);
  }

  const selectedSorted = [...mSel].filter((k) => k < limit).sort((a, b) => a - b);

  return (
    <>
      <div className="ve-plot-subhead ve-plot-row">
        <span className="ve-plot-lbl ve-plot-lbl-fixed">curves by</span>
        <div className="ve-segmented sm">
          <button className={mAxis === 'col' ? 'is-active' : ''} onClick={() => { setMAxis('col'); setMSel(new Set([0])); setMXSrc({ axis: 'col', idx: 0 }); }}>col</button>
          <button className={mAxis === 'row' ? 'is-active' : ''} onClick={() => { setMAxis('row'); setMSel(new Set([0])); setMXSrc({ axis: 'row', idx: 0 }); }}>row</button>
        </div>
      </div>

      <div className="ve-plot-subhead ve-plot-row">
        <span className="ve-plot-lbl ve-plot-lbl-fixed">X</span>
        <div className="ve-segmented sm">
          <button className={mXMode === 'index' ? 'is-active' : ''} onClick={() => setMXMode('index')}>index</button>
          <button className={mXMode === 'src' ? 'is-active' : ''} onClick={() => setMXMode('src')}>{mAxis === 'col' ? 'a col' : 'a row'}</button>
        </div>
        {mXMode === 'src' && (
          <span className="ve-plot-src ve-x-stepper">
            <span className="ve-plot-lbl">{mAxis}</span>
            <button className="ve-step-btn"
              onClick={() => setMXSrc({ axis: mAxis, idx: Math.max(0, (mXSrc.idx ?? 0) - 1) })}
              disabled={(mXSrc.idx ?? 0) <= 0}
              aria-label="prev">
              <svg width="8" height="8" viewBox="0 0 8 8"><path d="M5.5 1L2.5 4L5.5 7" stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinecap="round" strokeLinejoin="round"/></svg>
            </button>
            <input type="text" inputMode="numeric"
              className="ve-step-input"
              value={(mXSrc.idx ?? 0) + 1}
              onChange={(e) => {
                const n = parseInt(e.target.value.replace(/[^\d]/g, '') || '1', 10);
                setMXSrc({ axis: mAxis, idx: Math.max(0, Math.min(limit - 1, n - 1)) });
              }} />
            <span className="ve-step-total">/ {limit}</span>
            <button className="ve-step-btn"
              onClick={() => setMXSrc({ axis: mAxis, idx: Math.min(limit - 1, (mXSrc.idx ?? 0) + 1) })}
              disabled={(mXSrc.idx ?? 0) >= limit - 1}
              aria-label="next">
              <svg width="8" height="8" viewBox="0 0 8 8"><path d="M2.5 1L5.5 4L2.5 7" stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinecap="round" strokeLinejoin="round"/></svg>
            </button>
          </span>
        )}
      </div>

      <div className="ve-plot-subhead ve-plot-row">
        <span className="ve-plot-lbl ve-plot-lbl-fixed">Y</span>
        <div className="ve-multi-picker" ref={ref}>
          <button className="ve-multi-trigger" onClick={() => setOpen((o) => !o)}>
            <span className="ve-multi-count">{selectedSorted.length}</span>
            <span className="ve-multi-label">of {limit} {mAxis}s</span>
            <svg width="8" height="8" viewBox="0 0 8 8"><path d="M1 2.5L4 5.5L7 2.5" stroke="currentColor" strokeWidth="1.2" fill="none"/></svg>
          </button>
          {open && (
            <div className="ve-multi-pop">
              <div className="ve-multi-pop-head">
                <input className="ve-multi-search" placeholder="search or 1-10, 12 …"
                  value={pickerQuery}
                  onChange={(e) => setPickerQuery(e.target.value)}
                  onKeyDown={(e) => { if (e.key === 'Enter' && /[\d,-]/.test(pickerQuery)) { selectRange(pickerQuery); setPickerQuery(''); } }}
                />
                <button className="ve-multi-mini" onClick={selectAll}  title="Select all (filtered)">all</button>
                <button className="ve-multi-mini" onClick={selectNone}>none</button>
              </div>
              <div className="ve-multi-grid">
                {items.map((it) => (
                  <button key={it.idx}
                    className={`ve-multi-chip ${mSel.has(it.idx) ? 'is-on' : ''}`}
                    style={mSel.has(it.idx) ? { borderColor: 'rgba(127, 217, 154, 0.5)' } : null}
                    onClick={() => toggle(it.idx)}>
                    {it.label}
                  </button>
                ))}
                {items.length === 0 && <span className="ve-multi-empty">no matches</span>}
              </div>
              <div className="ve-multi-pop-foot">
                <span>{selectedSorted.length} selected</span>
                <span className="ve-plot-spacer" />
                <span className="ve-multi-hint">type “1-10, 15” + ↵ to add range</span>
              </div>
            </div>
          )}
        </div>
      </div>
    </>
  );
}

function PlotControls({ rows, cols,
                        mAxis, setMAxis, mSel, setMSel,
                        mXMode, setMXMode, mXSrc, setMXSrc,
                        pickerQuery, setPickerQuery,
                        onClose }) {
  return (
    <>
      <div className="ve-plot-head">
        <span className="ve-plot-title">inline plot</span>
        <span className="ve-plot-spacer" />
        <button className="ve-plot-close" onClick={onClose} title="Hide plot">×</button>
      </div>
      <MultiPickerControls mAxis={mAxis} setMAxis={setMAxis} mSel={mSel} setMSel={setMSel}
        rows={rows} cols={cols}
        mXMode={mXMode} setMXMode={setMXMode} mXSrc={mXSrc} setMXSrc={setMXSrc}
        pickerQuery={pickerQuery} setPickerQuery={setPickerQuery} />
    </>
  );
}

function InlinePlot({ getSlice, rows, cols, onClose }) {
  // Auto-pick the slice axis from the matrix shape:
  //   1×N row vector → 'row' (the single row, cols entries)
  //   N×1 col vector → 'col' (the single col, rows entries)
  //   M×N matrix     → axis whose slice is LONGER.
  //                    'row' → cols entries per slice
  //                    'col' → rows entries per slice
  //                    cols ≥ rows → 'row' (rows are at least as long)
  // Mirrors what a MATLAB user expects: a few long curves, not many tiny ones.
  const defaultAxis = (r, c) => (c >= r ? 'row' : 'col');

  const [mAxis, setMAxis] = useState(() => defaultAxis(rows, cols));
  const [mSel, setMSel]   = useState(() => new Set([0]));
  const [mXMode, setMXMode] = useState('index');
  const [mXSrc, setMXSrc]   = useState(() => ({ axis: defaultAxis(rows, cols), idx: 0 }));
  const [pickerQuery, setPickerQuery] = useState('');

  useEffect(() => {
    const def = defaultAxis(rows, cols);
    setMAxis(def);
    setMSel(new Set([0]));
    setMXMode('index');
    setMXSrc({ axis: def, idx: 0 });
    setPickerQuery('');
  }, [rows, cols]);

  const palette = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a', '#d4a5e6', '#f2a37e', '#6fcfbf'];

  // Slice fetcher provided by the parent — synchronous in full mode
  // (just reads the data array), tile-mode returns the slice from the
  // engine via a single column/row tile fetch.
  function sliceArr(src) {
    return getSlice(src.axis, src.idx) || [];
  }

  const limit = mAxis === 'col' ? cols : rows;
  const ids = [...mSel].filter((k) => k < limit).sort((a, b) => a - b);
  const xShared = mXMode === 'src' ? sliceArr(mXSrc).map(Number) : null;
  const xLabel  = mXMode === 'src'
    ? `${mXSrc.axis} ${mXSrc.idx + 1}`
    : 'index';
  const curves = ids.map((k, i) => {
    const ys = (getSlice(mAxis, k) || []).map(Number);
    const length = ys.length;
    const xs = xShared ? xShared.slice(0, length) : ys.map((_, j) => j + 1);
    const x = [], y = [];
    for (let j = 0; j < length; j++) if (Number.isFinite(xs[j]) && Number.isFinite(ys[j])) { x.push(xs[j]); y.push(ys[j]); }
    return { name: `${mAxis} ${k + 1}`, x, y, color: palette[i % palette.length] };
  });

  if (curves.length === 0 || curves.every((c) => c.y.length === 0)) {
    return (
      <div className="ve-plot">
        <PlotControls rows={rows} cols={cols}
          mAxis={mAxis} setMAxis={setMAxis} mSel={mSel} setMSel={setMSel}
          mXMode={mXMode} setMXMode={setMXMode} mXSrc={mXSrc} setMXSrc={setMXSrc}
          pickerQuery={pickerQuery} setPickerQuery={setPickerQuery}
          onClose={onClose} />
        <div className="ve-plot-empty">no numeric data to plot — pick at least one {mAxis}</div>
      </div>
    );
  }

  let xMin = Infinity, xMax = -Infinity, yMin = Infinity, yMax = -Infinity;
  curves.forEach((c) => {
    c.x.forEach((v) => { if (v < xMin) xMin = v; if (v > xMax) xMax = v; });
    c.y.forEach((v) => { if (v < yMin) yMin = v; if (v > yMax) yMax = v; });
  });
  if (xMin === xMax) { xMin -= 0.5; xMax += 0.5; }
  if (yMin === yMax) { yMin -= 0.5; yMax += 0.5; }

  return (
    <div className="ve-plot">
      <PlotControls rows={rows} cols={cols}
        mAxis={mAxis} setMAxis={setMAxis} mSel={mSel} setMSel={setMSel}
        mXMode={mXMode} setMXMode={setMXMode} mXSrc={mXSrc} setMXSrc={setMXSrc}
        pickerQuery={pickerQuery} setPickerQuery={setPickerQuery}
        onClose={onClose} />
      <PlotChart curves={curves} xMin={xMin} xMax={xMax} yMin={yMin} yMax={yMax} xLabel={xLabel} />
      <div className="ve-plot-legend">
        {curves.slice(0, 8).map((c) => (
          <span key={c.name} className="ve-plot-legend-item"><i style={{ background: c.color }} />{c.name} <em>n={c.y.length}</em></span>
        ))}
        {curves.length > 8 && <span className="ve-plot-legend-more">+{curves.length - 8} more</span>}
      </div>
    </div>
  );
}

/* ======================================================================== */
/* Virtualised Variable Editor table                                        */
/* ======================================================================== */
//
// Renders only the cells currently visible in the scroll viewport. With a
// 512×512 matrix that drops the live DOM-cell count from 262144 to ~30×10
// regardless of total size, which is the difference between an unusable
// editor and instant precision-slider response.
//
// Sizing is fixed per cell (ROW_H × COL_W) so we can compute the visible
// window from scroll offsets alone — no per-row measurement needed. Spacer
// rows (top/bottom) and spacer cells (left/right) preserve the natural
// scroll height/width so the browser's native scrollbar still tracks the
// full extent.

const ROW_H    = 22;     // matches CSS .ve-table tbody td natural height
const COL_W    = 88;     // matches CSS min-width: 88px
const HEADER_H = 26;     // sticky thead row height
const CORNER_W = 60;     // corner cell + row-head width
const OVERSCAN = 6;      // extra rows/cols above/below to make scroll look continuous

// Tile-based fetch state for huge matrices. The cache is keyed by
// `${tileR},${tileC}` (in tile units, not cell coords).
const TILE = 64;

function VirtualTable({
  tableRef, rows, cols, getCellValue,
  activeCell, setActiveCell,
  editing, setEditing, editVal, setEditVal, commitEdit, inputRef,
  heatmap, stats, format,
  readOnly = false,
}) {
  const [scroll, setScroll] = useState({ top: 0, left: 0, viewW: 800, viewH: 400 });

  // Scroll + resize listeners on the wrap. ResizeObserver catches both modal
  // resize and the inline-plot toggle that shrinks the table area.
  useEffect(() => {
    const wrap = tableRef.current;
    if (!wrap) return;
    let raf = 0;
    const update = () => {
      cancelAnimationFrame(raf);
      raf = requestAnimationFrame(() => {
        setScroll({
          top:   wrap.scrollTop,
          left:  wrap.scrollLeft,
          viewW: wrap.clientWidth,
          viewH: wrap.clientHeight,
        });
      });
    };
    update();
    wrap.addEventListener('scroll', update, { passive: true });
    const ro = (typeof ResizeObserver !== 'undefined') ? new ResizeObserver(update) : null;
    ro?.observe(wrap);
    return () => {
      cancelAnimationFrame(raf);
      wrap.removeEventListener('scroll', update);
      ro?.disconnect();
    };
  }, [tableRef]);

  // Compute visible row/col window. Below 200 rows / 200 cols just render
  // everything — the virtualisation overhead isn't worth it.
  const fullRender = rows < 200 && cols < 200;

  const firstRow = fullRender ? 0
    : Math.max(0, Math.floor(scroll.top / ROW_H) - OVERSCAN);
  const lastRow  = fullRender ? rows - 1
    : Math.min(rows - 1, Math.ceil((scroll.top + scroll.viewH) / ROW_H) + OVERSCAN);

  const colScrollLeft = Math.max(0, scroll.left - CORNER_W);
  const firstCol = fullRender ? 0
    : Math.max(0, Math.floor(colScrollLeft / COL_W) - OVERSCAN);
  const lastCol  = fullRender ? cols - 1
    : Math.min(cols - 1, Math.ceil((colScrollLeft + scroll.viewW) / COL_W) + OVERSCAN);

  const topPadH    = firstRow * ROW_H;
  const bottomPadH = (rows - 1 - lastRow) * ROW_H;
  const leftPadW   = firstCol * COL_W;
  const rightPadW  = (cols - 1 - lastCol) * COL_W;

  const visibleRows = [];
  for (let r = firstRow; r <= lastRow; r++) visibleRows.push(r);
  const visibleCols = [];
  for (let c = firstCol; c <= lastCol; c++) visibleCols.push(c);

  return (
    <div className="ve-table-wrap" ref={tableRef}>
      <table className="ve-table">
        <thead>
          <tr>
            <th className="ve-corner">{rows}×{cols}</th>
            {leftPadW > 0 && <th aria-hidden="true" style={{ minWidth: leftPadW, padding: 0, border: 'none' }} />}
            {visibleCols.map((c) => (
              <th key={c} className={c === activeCell.c ? 'is-active' : ''}
                style={{ minWidth: COL_W }}>{c + 1}</th>
            ))}
            {rightPadW > 0 && <th aria-hidden="true" style={{ minWidth: rightPadW, padding: 0, border: 'none' }} />}
          </tr>
        </thead>
        <tbody>
          {topPadH > 0 && (
            <tr aria-hidden="true" style={{ height: topPadH }}>
              <td colSpan={visibleCols.length + 3} style={{ padding: 0, border: 'none' }} />
            </tr>
          )}
          {visibleRows.map((r) => (
            <tr key={r} style={{ height: ROW_H }}>
              <th className={`ve-rowhead ${r === activeCell.r ? 'is-active' : ''}`}>{r + 1}</th>
              {leftPadW > 0 && <td aria-hidden="true" style={{ minWidth: leftPadW, padding: 0, border: 'none' }} />}
              {visibleCols.map((c) => {
                const v = getCellValue(r, c);
                const isActive  = activeCell.r === r && activeCell.c === c;
                const isEditing = editing && editing.r === r && editing.c === c;
                // Logical cells arrive as JS booleans → coerce to 1/0 so the
                // heatmap colours them (numbers pass through, NaN is skipped).
                const vNum = toNumericCell(v);
                const bg = (heatmap && stats && Number.isFinite(vNum))
                  ? heatColor(vNum, stats.min, stats.max) : undefined;
                return (
                  <td
                    key={c}
                    className={isActive ? 'is-active' : ''}
                    style={{ background: bg, minWidth: COL_W }}
                    onClick={() => setActiveCell({ r, c })}
                    onDoubleClick={() => {
                      setActiveCell({ r, c });
                      if (readOnly) return;
                      // Same loaded-value guard as Enter / F2.
                      if (v === null || v === undefined) return;
                      setEditing({ r, c });
                      setEditVal(typeof v === 'number' ? String(v) : '');
                    }}
                  >
                    {isEditing ? (
                      <input
                        ref={inputRef}
                        value={editVal}
                        onChange={(e) => setEditVal(e.target.value)}
                        onBlur={commitEdit}
                        className="ve-cell-input"
                      />
                    ) : (
                      <span className={typeof v !== 'number' ? 've-string' : ''}>{format(v)}</span>
                    )}
                  </td>
                );
              })}
              {rightPadW > 0 && <td aria-hidden="true" style={{ minWidth: rightPadW, padding: 0, border: 'none' }} />}
            </tr>
          ))}
          {bottomPadH > 0 && (
            <tr aria-hidden="true" style={{ height: bottomPadH }}>
              <td colSpan={visibleCols.length + 3} style={{ padding: 0, border: 'none' }} />
            </tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

/* ======================================================================== */
/* MatrixPanel — the polished matrix viewer/editor body                     */
/* ======================================================================== */
// Presentation + display-state only. The data layer (how rows/cols/values
// are fetched) is supplied by the caller via props, so the SAME panel
// serves top-level matrix variables (name-addressed fetch in
// VariableEditor) and drilled struct/cell matrix fields (inline data in
// StructInspector). No toolbar duplication.
//
// Props:
//   rows, cols, name, type
//   getCellValue(r,c)  — raw value
//   getSlice(axis,idx) — for the inline plot
//   stats              — { min, max, mean, n } | null   (heatmap + status)
//   readOnly           — disables cell editing
//   onCommit(r,c,rhs)  — owner performs the write + refresh; rhs is a
//                        ready-to-interpolate MATLAB literal (number now;
//                        type-aware string in Phase B)
//   onEscape()         — Esc when not editing (close modal / pop breadcrumb)
//   onSave(format)     — save-as handler; null → no save-as button
//   saveDisabled       — gray out save-as (e.g. tile-mode huge matrix)
//   fontScale
function MatrixPanel({
  rows, cols, name, type,             
  getCellValue, getSlice, stats,
  readOnly = false,
  onCommit, onEscape, onSave,
  saveDisabled = false,
}) {
  const [precision, setPrecision] = useState(4);
  const [notation, setNotation]   = useState('fixed');
  const [heatmap, setHeatmap]     = useState(false);
  const [statsVisible, setStatsVisible] = useStatChooser();
  const [showPlot, setShowPlot]   = useState(false);
  const [saveOpen, setSaveOpen]   = useState(false);
  const [plotWidth, setPlotWidth] = useState(() => {
    const stored = parseInt(localStorage.getItem('numkit.ve.plotWidth') || '', 10);
    return Number.isFinite(stored) && stored >= 200 && stored <= 1600 ? stored : 520;
  });
  useEffect(() => { localStorage.setItem('numkit.ve.plotWidth', String(plotWidth)); }, [plotWidth]);

  const [activeCell, setActiveCell] = useState({ r: 0, c: 0 });
  const [editing, setEditing]       = useState(null);
  const [editVal, setEditVal]       = useState('');
  const veBodyRef = useRef(null);
  const tableRef  = useRef(null);
  const inputRef  = useRef(null);

  // Reset the active cell when the data source changes shape (e.g. the
  // inspector drilled into a different field).
  useEffect(() => { setActiveCell({ r: 0, c: 0 }); setEditing(null); }, [rows, cols, name]);

  function startDragDivider(e) {
    e.preventDefault();
    const body = veBodyRef.current;
    if (!body) return;
    const onMove = (ev) => {
      const rect = body.getBoundingClientRect();
      const desired = rect.right - ev.clientX;
      const maxW = Math.max(220, rect.width - 200);
      setPlotWidth(Math.max(200, Math.min(maxW, desired)));
    };
    const onUp = () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  }

  function formatNum(n) {
    if (!Number.isFinite(n)) return String(n);
    if (notation === 'exp')   return n.toExponential(precision);
    if (notation === 'fixed') return n.toFixed(precision);
    return fmt(n, { fix: precision, exp: precision });
  }
  const COMPLEX_RE = /^\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*([+-])\s*(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)i\s*$/;
  function format(v) {
    if (v === null || v === undefined) return '—';
    if (typeof v === 'number') return formatNum(v);
    if (typeof v === 'string') {
      const m = v.match(COMPLEX_RE);
      if (m) {
        const re = parseFloat(m[1]);
        const im = parseFloat(m[3]) * (m[2] === '-' ? -1 : 1);
        return `${formatNum(re)}${im >= 0 ? '+' : '-'}${formatNum(Math.abs(im))}i`;
      }
    }
    return String(v);
  }

  // Collect the edit, hand a type-aware MATLAB literal + JS mirror value
  // to the owner. valueToMatlabRHS escapes/validates per type (numeric /
  // logical / char / string); null → invalid input, abort the edit.
  const commitEdit = useCallback(() => {
    if (!editing) return;
    const out = valueToMatlabRHS(editVal, type);
    if (!out) { setEditing(null); return; }
    onCommit?.(editing.r, editing.c, out.rhs, out.value);
    setEditing(null);
  }, [editing, editVal, onCommit, type]);

  useEffect(() => {
    if (editing && inputRef.current) { inputRef.current.focus(); inputRef.current.select(); }
  }, [editing]);

  const handleKey = useCallback((e) => {
    if (editing) {
      if (e.key === 'Enter')  { e.preventDefault(); commitEdit(); }
      if (e.key === 'Escape') { e.preventDefault(); setEditing(null); }
      return;
    }
    if (e.key === 'Escape') { onEscape?.(); return; }
    if (!readOnly && (e.key === 'Enter' || e.key === 'F2')) {
      const v = getCellValue(activeCell.r, activeCell.c);
      if (v === null || v === undefined) return;
      setEditing({ ...activeCell });
      setEditVal(typeof v === 'number' ? String(v) : '');
      e.preventDefault();
      return;
    }
    let { r, c } = activeCell;
    if (e.key === 'ArrowUp')    r = Math.max(0, r - 1);
    if (e.key === 'ArrowDown')  r = Math.min(rows - 1, r + 1);
    if (e.key === 'ArrowLeft')  c = Math.max(0, c - 1);
    if (e.key === 'ArrowRight') c = Math.min(cols - 1, c + 1);
    if (e.key === 'Home')       c = 0;
    if (e.key === 'End')        c = cols - 1;
    if (e.key === 'PageUp')     r = Math.max(0, r - 10);
    if (e.key === 'PageDown')   r = Math.min(rows - 1, r + 10);
    if (r !== activeCell.r || c !== activeCell.c) { setActiveCell({ r, c }); e.preventDefault(); }
  }, [activeCell, rows, cols, editing, getCellValue, onEscape, readOnly, commitEdit]);

  useEffect(() => {
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [handleKey]);

  return (
    <>
      <div className="ve-toolbar">
        <div className="ve-tools-group">
          <span className="ve-label">notation</span>
          <div className="ve-segmented">
            {['fixed', 'exp', 'auto'].map((n) => (
              <button key={n} className={notation === n ? 'is-active' : ''} onClick={() => setNotation(n)}>{n}</button>
            ))}
          </div>
        </div>
        <div className="ve-tools-group">
          <span className="ve-label">precision</span>
          <input type="range" min={0} max={8} step={1}
            value={precision} onChange={(e) => setPrecision(parseInt(e.target.value, 10))} />
          <span className="ve-precision-num">{precision}</span>
        </div>
        <div className="ve-tools-group">
          {stats && <StatChooserButton visible={statsVisible} setVisible={setStatsVisible} />}
          <button className={`ve-btn ${heatmap ? 'is-active' : ''}`}
            onClick={() => setHeatmap((h) => !h)} title="Toggle value heatmap">
            <svg width="11" height="11" viewBox="0 0 12 12">
              <rect x="0.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 240)"/>
              <rect x="3.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 180)"/>
              <rect x="6.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 60)"/>
              <rect x="9.5" y="0.5" width="2" height="11" fill="oklch(0.6 0.1 30)"/>
            </svg>
            heatmap
          </button>
          <button className="ve-btn" title="Copy as CSV" onClick={() => {
            const lines = [];
            for (let r = 0; r < rows; r++) {
              const row = [];
              for (let c = 0; c < cols; c++) {
                const v = getCellValue(r, c);
                row.push(typeof v === 'number' ? v : `"${v ?? ''}"`);
              }
              lines.push(row.join(','));
            }
            navigator.clipboard?.writeText(lines.join('\n'));
          }}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <rect x="2" y="2" width="7" height="8" rx="1" stroke="currentColor" fill="none"/>
              <rect x="3.5" y="0.5" width="7" height="8" rx="1" stroke="currentColor" fill="none"/>
            </svg>
            copy csv
          </button>
          <button className={`ve-btn ${showPlot ? 'is-active' : ''}`}
            title="Toggle inline plot" onClick={() => setShowPlot((p) => !p)}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <polyline points="1,9 4,5 7,7 11,2" stroke="currentColor" fill="none" strokeWidth="1.4"/>
            </svg>
            plot
          </button>
          {onSave && (
            <div className="ve-saveas-wrap">
              <button className="ve-btn ve-saveas-trigger"
                title={saveDisabled ? 'save disabled for huge matrices' : 'Save variable to file'}
                disabled={saveDisabled}
                onClick={() => setSaveOpen((s) => !s)}>
                <svg width="11" height="11" viewBox="0 0 12 12">
                  <path d="M2 2h6l2 2v6H2z M4 2v3h4V2 M4 8h4v2H4z" stroke="currentColor" fill="none"/>
                </svg>
                save as
                <svg width="8" height="8" viewBox="0 0 8 8" style={{ marginLeft: 2 }}>
                  <path d="M1 2.5 L4 5.5 L7 2.5" stroke="currentColor" fill="none" strokeWidth="1.2" strokeLinecap="round"/>
                </svg>
              </button>
              {saveOpen && (
                <SaveAsMenu onClose={() => setSaveOpen(false)}
                  onPick={(f) => { onSave(f); setSaveOpen(false); }} />
              )}
            </div>
          )}
        </div>
        <div className="ve-tools-spacer" />
      </div>

      {/* Aggregate statistics over the whole matrix, on their own row. The
          chooser (Σ ▾) lives in the toolbar above; this row collapses to
          nothing for non-numeric values OR when no statistic is selected. */}
      <StatsBar stats={stats} visible={statsVisible} />

      <div className="ve-address">
        <span className="ve-cell-ref">{name}({activeCell.r + 1}, {activeCell.c + 1})</span>
        <span className="ve-eq">=</span>
        <span className="ve-cell-val">{format(getCellValue(activeCell.r, activeCell.c))}</span>
      </div>

      <div className={`ve-body ${showPlot ? 'has-plot' : ''}`}
           ref={veBodyRef}
           style={showPlot ? { gridTemplateColumns: `1fr 6px ${plotWidth}px` } : undefined}>
        <VirtualTable
          tableRef={tableRef}
          rows={rows} cols={cols}
          getCellValue={getCellValue}
          activeCell={activeCell} setActiveCell={setActiveCell}
          editing={editing} setEditing={setEditing}
          editVal={editVal} setEditVal={setEditVal}
          commitEdit={commitEdit}
          inputRef={inputRef}
          heatmap={heatmap} stats={stats}
          format={format}
          readOnly={readOnly}
        />
        {showPlot && (
          <>
            <div className="ve-divider" role="separator" aria-orientation="vertical"
              aria-label="Resize plot pane"
              onMouseDown={startDragDivider}
              onDoubleClick={() => setPlotWidth(520)}
              title="Drag to resize · double-click to reset" />
            <InlinePlot getSlice={getSlice} rows={rows} cols={cols}
              onClose={() => setShowPlot(false)} />
          </>
        )}
      </div>

      <div className="ve-status">
        <span>cell ({activeCell.r + 1},{activeCell.c + 1})</span>
        <span className="ve-sep" />
        <span>↑↓←→ navigate</span>
        <span className="ve-sep" />
        <span>{readOnly ? 'read-only' : '↵ / F2 edit'}</span>
        <span className="ve-sep" />
        <span>Esc {onEscape ? 'back' : 'close'}</span>
        <span className="ve-spacer" />
        <span>UTF-8</span>
        <span className="ve-sep" />
        <span>{readOnly ? 'read-only' : 'read/write'}</span>
      </div>
    </>
  );
}

/* ======================================================================== */
/* Variable Editor — modal table with notation/precision/heatmap/plot       */
/* ======================================================================== */
// Switch to tile-mode for matrices with more cells than this. A 500×500
// matrix is the rough boundary where full-fetch JSON becomes expensive
// (~250k values, several MB of JSON, sluggish parsing); above it we
// only fetch what's visible.
const TILE_MODE_THRESHOLD = 250000;

/* ======================================================================== */
/* Struct / cell inspector — MATLAB-style drill-in                          */
/* ======================================================================== */
// Navigable inspector over engine.inspectPath(name, pathStr). The engine
// resolves a ';'-delimited typed path ('' = root) and returns one of:
//   { kind:'struct', rows, cols, numel, fields:[], elems:[[cell]] }
//   { kind:'cell',   rows, cols, elems:[cell] }   (column-major)
//   { kind:'matrix', type, rows, cols, data | truncated }
// where cell = { type, size, summary, drill, label? }.
//
// A single struct (numel 1) renders as a field list; a struct array as an
// element×field table; a cell as an R×C grid; a matrix reuses VirtualTable.
// Double-clicking a `drill` cell pushes a path step; the breadcrumb pops
// back. Mounted with key={variable.name} so a variable swap resets nav.

function InspectorBreadcrumb({ nav, onJump }) {
  return (
    <div className="ve-crumbs">
      {nav.map((c, i) => (
        <span key={i} className="ve-crumb-seg">
          {i > 0 && <span className="ve-crumb-sep">›</span>}
          <button className="ve-crumb" disabled={i === nav.length - 1}
            onClick={() => onJump(i)}>{c.label}</button>
        </span>
      ))}
    </div>
  );
}

// A field name that becomes an inline text input on double-click, for
// renaming. Stops click/double-click propagation so it doesn't trigger
// the row's drill. Enter commits, Esc cancels.
function EditableFieldName({ name, onRename, className, editing: cEditing, setEditing: cSetEditing }) {
  // Editing is optionally controlled: when the parent passes editing /
  // setEditing (so a context-menu "Rename" can start it), use those;
  // otherwise self-manage on double-click (struct-array header usage).
  const [iEditing, iSetEditing] = useState(false);
  const editing = cEditing !== undefined ? cEditing : iEditing;
  const setEditing = cSetEditing || iSetEditing;
  const [val, setVal] = useState(name);
  // Reset the draft whenever edit mode opens (covers the menu-triggered
  // path, which can't pre-seed val like the double-click handler did).
  useEffect(() => { if (editing) setVal(name); }, [editing, name]);
  if (!editing) {
    return (
      <span className={className}
        onDoubleClick={(e) => { e.stopPropagation(); setEditing(true); }}
        title="double-click to rename">{name}</span>
    );
  }
  const commit = () => { setEditing(false); if (val !== name) onRename(name, val); };
  return (
    <input className="ve-rename-input" autoFocus value={val}
      onClick={(e) => e.stopPropagation()}
      onChange={(e) => setVal(e.target.value)}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === 'Enter') { e.preventDefault(); commit(); }
        else if (e.key === 'Escape') { setEditing(false); }
      }} />
  );
}

// Inline "+ new field" control — validates the name as a MATLAB
// identifier and only enables Add when valid. Shared by the single-
// struct list and the struct-array table.
function AddFieldRow({ onAdd }) {
  const [name, setName] = useState('');
  const valid = isValidIdentifier(name);
  const submit = () => { if (valid) { onAdd(name); setName(''); } };
  return (
    <div className="ve-addfield">
      <input className="ve-addfield-input" placeholder="+ new field"
        value={name}
        onChange={(e) => setName(e.target.value)}
        onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); submit(); } }} />
      <button className="ve-addfield-btn" disabled={!valid} onClick={submit}>add</button>
    </div>
  );
}

// Single struct (numel 1): vertical field list. Click a drillable field
// to descend; × deletes a field; the add-row appends a new one.
function StructFieldList({ payload, onDrill, onAddField, onDeleteField,
                          onRenameField, onDuplicateField, onInsertField }) {
  const cells = payload.elems[0] || [];
  const [menu, setMenu] = useState(null);          // { x, y, name, drill }
  const [renaming, setRenaming] = useState(null);  // field name in rename mode
  // Columns shared with the Workspace (same key); view/sort are per the
  // struct context so it can differ from the Workspace's layout.
  const [cols, setCols] = useChooser('numkit.ide.valuecols', loadVisibleColumns, saveVisibleColumns);
  const open = (name) => onDrill([{ k: 'f', name, label: `.${name}` }]);
  const rows = payload.fields.map((name, f) => {
    const cell = cells[f] || {};
    return {
      key: name, name, value: cell.summary, size: cell.size,
      klass: cell.type, kind: classify(cell.size, cell.type),
      stats: cell.stats || null, drill: !!cell.drill,
    };
  });
  const nameCell = (row) => (
    <EditableFieldName name={row.name} onRename={onRenameField}
      editing={renaming === row.name}
      setEditing={(v) => setRenaming(v ? row.name : null)} />
  );
  return (
    <>
      <EntityBrowser
        rows={rows}
        nameHeader="Field"
        countNoun="field"
        defaultView="list"
        viewKey="numkit.ide.struct.view" sortKey="numkit.ide.struct.sort"
        cols={cols} setCols={setCols}
        nameCell={nameCell}
        onOpen={(row) => open(row.name)}
        onRowContextMenu={(row, e) =>
          setMenu({ x: e.clientX, y: e.clientY, name: row.name, drill: row.drill })}
        onAreaContextMenu={(e) => {
          // Right-click anywhere in the table area (not on a row — those
          // stop propagation): the field-agnostic menu (Insert field).
          e.preventDefault();
          setMenu({ x: e.clientX, y: e.clientY, name: null, drill: false });
        }}
      />
      {menu && (
        <ContextMenu x={menu.x} y={menu.y} onClose={() => setMenu(null)} items={
          menu.name
            ? [
              { label: 'Open',         disabled: !menu.drill, onClick: () => open(menu.name) },
              { label: 'Rename',       onClick: () => setRenaming(menu.name) },
              { label: 'Duplicate',    onClick: () => onDuplicateField(menu.name) },
              { label: 'Insert field', onClick: () => onInsertField() },
              { separator: true },
              { label: 'Delete',       onClick: () => onDeleteField(menu.name) },
            ]
            : [
              // Empty table area: no field target → just add a new one.
              { label: 'Insert field', onClick: () => onInsertField() },
            ]
        } />
      )}
    </>
  );
}

// Struct array: rows = elements (1),(2),…; cols = fields. Click a
// drillable cell to open s(e).field; × on a column header deletes that
// field across all elements; the add-row appends a field column.
function StructArrayTable({ payload, onDrill, onAddField, onDeleteField, onRenameField }) {
  return (
    <div className="ve-arr-wrap">
      <table className="ve-arr-table">
        <thead>
          <tr>
            <th className="ve-arr-corner">{payload.rows}×{payload.cols}</th>
            {payload.fields.map((f) => (
              <th key={f}>
                <EditableFieldName name={f} onRename={onRenameField} />
                <button className="ve-arr-delcol" title="delete field"
                  onClick={() => onDeleteField(f)}>×</button>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {payload.elems.map((row, e) => (
            <tr key={e}>
              <th className="ve-arr-rowhead">({e + 1})</th>
              {row.map((cell, f) => (
                <td key={f}
                  className={cell.drill ? 'is-drillable' : ''}
                  title={cell.summary}
                  onClick={cell.drill
                    ? () => onDrill([
                        { k: 'e', idx: e },
                        { k: 'f', name: payload.fields[f], label: `(${e + 1}).${payload.fields[f]}` },
                      ]) : undefined}>
                  {cell.summary}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
      <AddFieldRow onAdd={onAddField} />
    </div>
  );
}

// Cell array: R×C grid of element previews (column-major linear order).
function CellGrid({ payload, onDrill }) {
  const { rows, cols, elems } = payload;
  return (
    <div className="ve-arr-wrap">
      <table className="ve-arr-table">
        <tbody>
          {Array.from({ length: rows }, (_, r) => (
            <tr key={r}>
              <th className="ve-arr-rowhead">{r + 1}</th>
              {Array.from({ length: cols }, (_, c) => {
                const i = c * rows + r;             // column-major
                const cell = elems[i] || {};
                return (
                  <td key={c}
                    className={cell.drill ? 'is-drillable' : ''}
                    title={cell.summary}
                    onClick={cell.drill
                      ? () => onDrill([{ k: 'c', idx: i, label: cell.label || `{${r + 1},${c + 1}}` }])
                      : undefined}>
                    {cell.summary}
                  </td>
                );
              })}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

// Drilled-into matrix — render through the shared MatrixPanel so a
// struct/cell matrix field gets the same polished toolbar (notation /
// precision / heatmap / inline-plot / stats / copy-csv) as a top-level
// matrix variable. Data comes inline from the inspect payload.
//   name    — breadcrumb leaf label, shown in the address bar
//   onBack  — Esc / back to the parent path
//   onCommit(r,c,rhs) — write-back (Phase B); absent → read-only
function DrilledMatrix({ payload, name, onBack, onCommit }) {
  const data = payload.data || [];
  // Full stat set (min/max/mean/median/mode/var/std/n) over the inline
  // data, for the StatsBar — mirrors the engine's getVarStatsJSON.
  const stats = useMemo(() => aggregateStats(data.flat()), [data]);
  if (payload.truncated) {
    return (
      <div className="ve-struct-empty">
        Field too large to display inline ({payload.rows}×{payload.cols}).
      </div>
    );
  }
  const getCellValue = (r, c) => data[r]?.[c];
  const getSlice = (axis, idx) =>
    axis === 'col' ? data.map((row) => row[idx]) : (data[idx] || []).slice();
  return (
    <MatrixPanel
      rows={payload.rows} cols={payload.cols} name={name} type={payload.type}
      getCellValue={getCellValue} getSlice={getSlice} stats={stats}
      readOnly={!onCommit} onCommit={onCommit} onEscape={onBack} onSave={null}
    />
  );
}

function StructInspector({ variable, engine }) {
  const [nav, setNav] = useState([{ label: variable.name, path: '' }]);
  const [payload, setPayload] = useState(null);
  const [error, setError] = useState(null);
  const [refreshTick, setRefreshTick] = useState(0);  // bump → refetch after a write
  const cur = nav[nav.length - 1];

  useEffect(() => {
    setError(null);
    if (!engine || typeof engine.inspectPath !== 'function') {
      setError('struct inspection needs a full WASM rebuild (binding missing)');
      setPayload(null);
      return;
    }
    const p = engine.inspectPath(variable.name, cur.path);
    if (p === null) {
      setError('struct inspection needs a full WASM rebuild (binding missing)');
      setPayload(null);
    } else if (p.error) {
      setError(p.error);
      setPayload(null);
    } else {
      setPayload(p);
    }
  }, [variable.name, engine, cur.path, refreshTick]);

  // Write-back for an edited matrix-field cell. Builds a MATLAB lvalue
  // from the current path (e.g. `car.engine.data(2,5)`) and runs the
  // same engine.execute assignment as the top-level editor — no new
  // engine API. Refetch the path afterwards to show the new value.
  const commitFieldCell = useCallback((r, c, rhs) => {
    if (!engine || typeof engine.execute !== 'function') return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    try {
      engine.execute(`${lvalue}(${r + 1},${c + 1}) = ${rhs};`);
    } catch (e) {
      console.warn('[StructInspector] field write-back failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path]);

  // Add a field to the struct at the current path. For a struct array,
  // assign via element 1 — MATLAB then adds the field (empty) to every
  // element. New fields default to [] so the user can drill in and fill.
  const addField = useCallback((fieldName) => {
    if (!engine?.execute || !isValidIdentifier(fieldName)) return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    const target = (payload && payload.numel > 1) ? `${lvalue}(1)` : lvalue;
    try {
      engine.execute(`${target}.${fieldName} = [];`);
    } catch (e) {
      console.warn('[StructInspector] add-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path, payload]);

  // Remove a field via rmfield (drops it from every struct-array element).
  const deleteField = useCallback((fieldName) => {
    if (!engine?.execute || !isValidIdentifier(fieldName)) return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    try {
      engine.execute(`${lvalue} = rmfield(${lvalue}, '${fieldName}');`);
    } catch (e) {
      console.warn('[StructInspector] delete-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path]);

  // Duplicate a field to a fresh, collision-free "<name>_copy" name. The
  // `[lvalue.copy] = lvalue.name` bracket form works for a single struct
  // and distributes the per-element CSL across a struct array. Names are
  // identifiers, so the expression is injection-safe.
  const duplicateField = useCallback((fieldName) => {
    if (!engine?.execute || !isValidIdentifier(fieldName)) return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    const existing = new Set(payload?.fields || []);
    let copy = `${fieldName}_copy`;
    for (let i = 2; existing.has(copy); i++) copy = `${fieldName}_copy${i}`;
    try {
      engine.execute(`[${lvalue}.${copy}] = ${lvalue}.${fieldName};`);
    } catch (e) {
      console.warn('[StructInspector] duplicate-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path, payload]);

  // Insert a fresh empty field with a collision-free default name
  // ("unnamed", "unnamed1", …) — mirrors MATLAB's context-menu Insert.
  const insertField = useCallback(() => {
    const existing = new Set(payload?.fields || []);
    let name = 'unnamed';
    for (let i = 1; existing.has(name); i++) name = `unnamed${i}`;
    addField(name);
  }, [payload, addField]);

  // Rename = copy the field's value to the new name, drop the old, then
  // re-pin the field order so the renamed field keeps its original slot.
  // The `[lvalue.new] = lvalue.old` bracket form works for a single
  // struct and distributes the per-element CSL across a struct array.
  // Without the final orderfields() the new field would land at the END
  // (copy-append semantics); the 2-arg orderfields(s, {names...}) restores
  // position. Guards: valid identifier, no-op on same name, refuse to
  // clobber an existing field.
  const renameField = useCallback((oldName, newName) => {
    if (!engine?.execute || !isValidIdentifier(newName)) return;
    if (newName === oldName) return;
    if (payload?.fields?.includes(newName)) return;   // collision
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    // Desired order = current fields with oldName swapped to newName in
    // place. Field names are identifiers, so the {'a','b',...} cell
    // literal is injection-safe.
    const order = (payload?.fields || []).map((f) => (f === oldName ? newName : f));
    const orderCell = '{' + order.map((f) => `'${f}'`).join(',') + '}';
    try {
      let expr = `[${lvalue}.${newName}] = ${lvalue}.${oldName}; `
               + `${lvalue} = rmfield(${lvalue}, '${oldName}');`;
      if (order.length) expr += ` ${lvalue} = orderfields(${lvalue}, ${orderCell});`;
      engine.execute(expr);
    } catch (e) {
      console.warn('[StructInspector] rename-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path, payload]);

  // Push a path: steps is an array of { k, name?/idx?, label }. Only the
  // last step's label becomes the breadcrumb segment (e.g. drilling into
  // a struct-array cell is two steps `e:`+`f:` but one crumb "(2).field").
  const drill = useCallback((steps) => {
    const parts = steps.map((s) => (s.k === 'f' ? `f:${s.name}` : `${s.k}:${s.idx}`));
    setNav((prev) => {
      const base = prev[prev.length - 1].path;
      const newPath = (base ? `${base};` : '') + parts.join(';');
      return [...prev, { label: steps[steps.length - 1].label, path: newPath }];
    });
  }, []);
  const jump = useCallback((i) => setNav((prev) => prev.slice(0, i + 1)), []);

  let body;
  if (error) body = <div className="ve-struct-empty">{error}</div>;
  else if (!payload) body = <div className="ve-struct-empty">loading…</div>;
  else if (payload.kind === 'struct') {
    body = payload.numel === 1
      ? <StructFieldList payload={payload} onDrill={drill}
          onAddField={addField} onDeleteField={deleteField} onRenameField={renameField}
          onDuplicateField={duplicateField} onInsertField={insertField} />
      : <StructArrayTable payload={payload} onDrill={drill}
          onAddField={addField} onDeleteField={deleteField} onRenameField={renameField} />;
  } else if (payload.kind === 'cell') {
    body = <CellGrid payload={payload} onDrill={drill} />;
  } else if (payload.kind === 'matrix') {
    const leaf = (cur.label || variable.name).replace(/^\./, '');
    body = (
      <DrilledMatrix
        payload={payload}
        name={leaf}
        onBack={nav.length > 1 ? () => jump(nav.length - 2) : undefined}
        onCommit={commitFieldCell}
      />
    );
  } else {
    body = <div className="ve-struct-empty">unsupported value</div>;
  }

  return (
    <div className="ve-inspector">
      <InspectorBreadcrumb nav={nav} onJump={jump} />
      <div className="ve-inspector-body">{body}</div>
    </div>
  );
}

export function VariableEditor({ variable, onClose, engine }) {
  // Struct / cell variables get the drill-in inspector instead of the
  // numeric table — the matrix toolbar (notation / precision / heatmap
  // / plot / cell-address) doesn't apply to them. Gated on kind, which
  // adapters.classify() now reports as 'struct' / 'cell'. StructInspector
  // owns its own path state + engine fetch.
  const isStructLike = variable.kind === 'struct' || variable.kind === 'cell';

  // Display state (notation / precision / heatmap / plot / activeCell /
  // editing) lives in MatrixPanel now. VariableEditor keeps only the
  // window chrome + the name-addressed DATA layer below.
  const [maximized, setMaximized] = useState(false);

  // dimensions: { rows, cols, tileMode } — populated from getVarShape
  const initialShape = (() => {
    const r = variable.data?.length || 1;
    const c = variable.data?.[0]?.length || 1;
    return { rows: r, cols: c, tileMode: false };
  })();
  const [shape, setShape] = useState(initialShape);
  // Full-mode data (small matrices). For tile-mode we don't use this.
  const [data, setData]           = useState(variable.data);
  // Tile cache + pending set (tile-mode only). Map<"tR,tC", number[][] | 'pending' | 'error'>
  const tileCache = useRef(new Map());
  const [, setTileBump] = useState(0);  // bump to re-render after tile arrives
  const [loading, setLoading]     = useState(false);
  const [loadError, setLoadError] = useState(null);

  // Struct / cell fetching is owned by StructInspector (it manages its
  // own path state + engine.inspectPath calls). VariableEditor just
  // skips the matrix fetch below when isStructLike.

  // On open (and on variable swap), pick a fetch strategy:
  //   - small matrix → full fetch (existing path) — responsive precision/heatmap
  //   - huge matrix  → tile mode — only the viewport is read from the engine
  useEffect(() => {
    if (isStructLike) return;   // struct path handled by the effect above
    setData(variable.data);
    // activeCell now lives in MatrixPanel (it resets itself on a
    // rows/cols/name change), so VariableEditor no longer touches it.
    setLoadError(null);
    tileCache.current = new Map();
    sliceCache.current = new Map();
    if (!engine || typeof engine.getVarData !== 'function') {
      setShape({ rows: variable.data?.length || 1, cols: variable.data?.[0]?.length || 1, tileMode: false });
      return;
    }
    setLoading(true);
    const handle = setTimeout(() => {
      try {
        // Cheap dimension probe first.
        const sh = (typeof engine.getVarShape === 'function')
          ? engine.getVarShape(variable.name)
          : null;
        if (sh && !sh.error) {
          const numel = sh.rows * sh.cols;
          const tileMode = numel > TILE_MODE_THRESHOLD
                        && typeof engine.getVarTile === 'function';
          setShape({ rows: sh.rows, cols: sh.cols, tileMode });
          if (tileMode) {
            // Don't full-fetch. The virtual table will request tiles on demand.
            setLoading(false);
            return;
          }
        }
        // Small enough: full fetch.
        const r = engine.getVarData(variable.name);
        if (!r) { setLoading(false); return; }
        if (r.error) { setLoadError(r.error); setLoading(false); return; }
        if (Array.isArray(r.data) && r.data.length > 0) {
          setData(r.data);
          setShape({
            rows: r.rows ?? r.data.length,
            cols: r.cols ?? (r.data[0]?.length || 0),
            tileMode: false,
          });
        }
        setLoading(false);
      } catch (e) {
        setLoadError(e?.message || String(e));
        setLoading(false);
      }
    }, 0);
    return () => clearTimeout(handle);
  }, [variable, engine, isStructLike]);

  /* ─── slice cache (for InlinePlot in both modes) ─── */
  // Keyed by `${axis}:${idx}`. In tile-mode each miss triggers a single
  // 10000×1 (or 1×10000) tile fetch — fast because numkit storage is
  // column-major, so a column slice is a single contiguous read.
  const sliceCache = useRef(new Map());
  const getSlice = useCallback((axis, idx) => {
    if (!shape.tileMode) {
      return axis === 'row' ? (data[idx] || []) : data.map((r) => r[idx]);
    }
    const key = `${axis}:${idx}`;
    const cached = sliceCache.current.get(key);
    if (cached) return cached;
    if (!engine || typeof engine.getVarTile !== 'function') return [];
    let res;
    if (axis === 'row') {
      res = engine.getVarTile(variable.name, idx, 0, 1, shape.cols);
    } else {
      res = engine.getVarTile(variable.name, 0, idx, shape.rows, 1);
    }
    if (!res || res.error || !Array.isArray(res.data)) return [];
    // Flatten — tile.data is rows×cols; slice is one row or one col.
    const out = (axis === 'row')
      ? (res.data[0] || [])
      : res.data.map((r) => r[0]);
    sliceCache.current.set(key, out);
    return out;
  }, [shape.tileMode, shape.rows, shape.cols, data, engine, variable.name]);

  /* ─── tile-mode cell accessor ─── */
  // Returns the value at (r, c). For full-mode this is just data[r][c].
  // For tile-mode it consults the tile cache, kicking off a fetch if the
  // tile is missing. While the tile is in flight we return null and the
  // cell renders "—".
  const getCellValue = useCallback((r, c) => {
    if (!shape.tileMode) return data[r]?.[c];
    const tR = Math.floor(r / TILE);
    const tC = Math.floor(c / TILE);
    const key = `${tR},${tC}`;
    const tile = tileCache.current.get(key);
    if (tile && tile !== 'pending' && tile !== 'error') {
      return tile[r - tR * TILE]?.[c - tC * TILE];
    }
    if (tile === undefined) {
      // First time we ask for this tile — kick off fetch. Mark pending so
      // we don't re-trigger on every cell render.
      tileCache.current.set(key, 'pending');
      const r0 = tR * TILE, c0 = tC * TILE;
      // Defer to a microtask so React's render pass isn't blocked.
      Promise.resolve().then(() => {
        try {
          const res = engine.getVarTile(variable.name, r0, c0, TILE, TILE);
          if (!res || res.error || !Array.isArray(res.data)) {
            tileCache.current.set(key, 'error');
          } else {
            tileCache.current.set(key, res.data);
          }
        } catch {
          tileCache.current.set(key, 'error');
        }
        // Bump state to trigger re-render.
        setTileBump((n) => n + 1);
      });
    }
    return null;
  }, [shape.tileMode, data, engine, variable.name]);

  // Dimensions come from `shape` (set on open via getVarShape) so tile-mode
  // matrices size their grid correctly even before any tile arrives.
  const rows = shape.rows;
  const cols = shape.cols;

  // Tile-mode stats are computed natively in the engine via getVarStats.
  // Stored in state so the heatmap can light up as soon as the result
  // arrives (typically <100 ms even for 100M cells).
  const [tileStats, setTileStats] = useState(null);
  useEffect(() => {
    if (!shape.tileMode || !engine || typeof engine.getVarStats !== 'function') {
      setTileStats(null);
      return;
    }
    let cancelled = false;
    setTimeout(() => {
      const s = engine.getVarStats(variable.name);
      if (cancelled) return;
      if (s && !s.error) setTileStats(s);
    }, 0);
    return () => { cancelled = true; };
  }, [shape.tileMode, engine, variable.name]);

  // Full-mode stats: the complete set over the in-memory data (same helper
  // as the drilled-matrix path). Tile-mode (huge matrices) uses the
  // engine's getVarStats, which now returns the full set too.
  const stats = useMemo(() => {
    if (shape.tileMode) return tileStats;
    return aggregateStats(data.flat());
  }, [data, shape.tileMode, tileStats]);

  // Write-back for a committed cell edit. MatrixPanel hands us (r, c,
  // rhs) where rhs is a ready-to-interpolate MATLAB literal. The engine's
  // parser handles 1-based indexing, type coercion, and persistence; we
  // then invalidate the affected cache so the cell repaints fresh.
  const onCommit = useCallback((r, c, rhs, value) => {
    if (engine && typeof engine.execute === 'function') {
      try {
        engine.execute(`${variable.name}(${r + 1},${c + 1}) = ${rhs};`);
      } catch (e) {
        console.warn('[VariableEditor] write-back failed:', e);
      }
    }
    if (shape.tileMode) {
      const tR = Math.floor(r / TILE), tC = Math.floor(c / TILE);
      tileCache.current.delete(`${tR},${tC}`);
      sliceCache.current.delete(`col:${c}`);
      sliceCache.current.delete(`row:${r}`);
      setTileBump((n) => n + 1);
    } else {
      // Full mode: mirror the JS value locally so the cell repaints
      // immediately without a refetch.
      setData((d) => {
        const copy = d.map((row) => row.slice());
        if (copy[r]) copy[r][c] = value;
        return copy;
      });
    }
  }, [engine, variable.name, shape.tileMode]);

  const { themeName: veThemeName } = useTheme();
  const meta = KIND_META[variable.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, veThemeName);

  // Shared title-right (maximise / close) — identical for the matrix
  // and struct layouts, so it lives in one const.
  const titleButtons = (
    <div className="ve-title-right">
      <button className="ve-close" onClick={() => setMaximized((m) => !m)}
        title={maximized ? 'Restore' : 'Maximise'} aria-label="Maximise">
        {maximized ? (
          <svg width="13" height="13" viewBox="0 0 12 12" fill="none">
            <rect x="1.5" y="3.5" width="7" height="7"
              stroke="currentColor" strokeWidth="1.2" fill="var(--bg-2)"/>
            <rect x="3.5" y="1.5" width="7" height="7"
              stroke="currentColor" strokeWidth="1.2" fill="var(--bg-2)"/>
          </svg>
        ) : (
          <svg width="13" height="13" viewBox="0 0 12 12" fill="none">
            <rect x="1.5" y="1.5" width="9" height="9" stroke="currentColor" strokeWidth="1.2"/>
          </svg>
        )}
      </button>
      <button className="ve-close" onClick={onClose} aria-label="Close">×</button>
    </div>
  );

  // ── Struct / cell layout — drill-in inspector, no matrix toolbar ──
  if (isStructLike) {
    return (
      <div className="ve-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
        <div className={`ve-window ve-window-struct ${maximized ? 'is-max' : ''}`}
          role="dialog" aria-label={`Variable Editor: ${variable.name}`}>
          <div className="ve-titlebar">
            <div className="ve-title-left">
              <span className="ve-tag" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
                {meta.glyph} {variable.type}
              </span>
              <span className="ve-name">{variable.name}</span>
              <span className="ve-dim">{variable.size}</span>
            </div>
            {titleButtons}
          </div>
          <div className="ve-struct-body">
            <StructInspector key={variable.name} variable={variable} engine={engine} />
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="ve-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className={`ve-window ${maximized ? 'is-max' : ''}`}
        role="dialog" aria-label={`Variable Editor: ${variable.name}`}>
        <div className="ve-titlebar">
          <div className="ve-title-left">
            <span className="ve-tag" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
              {meta.glyph} {variable.type}
            </span>
            <span className="ve-name">{variable.name}</span>
            <span className="ve-dim">{variable.size}</span>
            <span className="ve-meta" title={`${variable.bytes} B · ${rows * cols} elements`}>
              {variable.bytes} B · {rows * cols} elements
            </span>
            {loading && (
              <span className="ve-meta" style={{ color: 'var(--accent)' }}>
                loading…
              </span>
            )}
            {loadError && (
              <span className="ve-meta" style={{ color: 'var(--danger)' }}
                title={loadError}>
                preview only
              </span>
            )}
          </div>
          {titleButtons}
        </div>

        <MatrixPanel
          rows={rows} cols={cols} name={variable.name} type={variable.type}
          getCellValue={getCellValue} getSlice={getSlice} stats={stats}
          readOnly={false}
          onCommit={onCommit}
          onEscape={onClose}
          onSave={shape.tileMode ? null : (f) => exportData(variable, data, f)}
          saveDisabled={shape.tileMode}
        />
      </div>
    </div>
  );
}
