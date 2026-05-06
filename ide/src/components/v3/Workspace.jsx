import { useState, useMemo, useEffect, useRef, useCallback } from 'react';
import { useTheme } from '../../theme';

/* ======================================================================== */
/* Type metadata + tone palette                                             */
/* ======================================================================== */
const KIND_META = {
  scalar: { label: 'scalar', glyph: '∙', tone: 'cyan'   },
  vector: { label: 'vector', glyph: '⟶', tone: 'green'  },
  matrix: { label: 'matrix', glyph: '▦', tone: 'amber'  },
  string: { label: 'string', glyph: '"', tone: 'violet' },
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
function VariableCard({ v, selected, onSelect, onOpen }) {
  const { themeName } = useTheme();
  const meta = KIND_META[v.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, themeName);
  return (
    <div
      className={`var-card ${selected ? 'is-selected' : ''}`}
      onClick={onSelect}
      onDoubleClick={onOpen}
      role="button"
      tabIndex={0}
      onKeyDown={(e) => { if (e.key === 'Enter') onOpen(); }}
    >
      <div className="var-card-head">
        <span className="var-name">{v.name}</span>
        <span className="var-size">{v.size}</span>
        <span className="var-type-pill" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
          <span className="var-glyph">{meta.glyph}</span>{v.type}
        </span>
      </div>
      <div className="var-card-body">
        <span className="var-preview">{v.preview}</span>
      </div>
      <button
        className="var-card-open"
        title="Open in Variable Editor (Enter)"
        onClick={(e) => { e.stopPropagation(); onOpen(); }}
        aria-label={`Open ${v.name}`}
      >
        <svg width="12" height="12" viewBox="0 0 12 12" aria-hidden="true">
          <path d="M2 2h4M2 2v4M10 10H6M10 10V6" stroke="currentColor" strokeWidth="1.4" fill="none" strokeLinecap="square" />
        </svg>
      </button>
    </div>
  );
}

function VariableRow({ v, selected, onSelect, onOpen }) {
  const { themeName } = useTheme();
  const meta = KIND_META[v.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, themeName);
  return (
    <div
      className={`var-row ${selected ? 'is-selected' : ''}`}
      onClick={onSelect}
      onDoubleClick={onOpen}
      tabIndex={0}
    >
      <span className="var-row-name">{v.name}</span>
      <span className="var-row-size">{v.size}</span>
      <span className="var-row-type" style={{ color: tone.fg }}>{meta.glyph}  {v.type}</span>
      <span className="var-row-preview">{v.preview}</span>
      <span className="var-row-bytes">{v.bytes} B</span>
      <span className="var-row-stat">μ {v.mean != null ? fmt(v.mean) : '—'}</span>
      <span className="var-row-stat">min {v.min != null ? fmt(v.min) : '—'}</span>
      <span className="var-row-stat">max {v.max != null ? fmt(v.max) : '—'}</span>
    </div>
  );
}

/* ======================================================================== */
/* Workspace toolbar                                                        */
/* ======================================================================== */
function WorkspaceToolbar({ count, query, setQuery, sort, setSort, view, setView }) {
  return (
    <div className="ws-toolbar">
      <div className="ws-toolbar-left">
        <span className="ws-count">{count} variable{count === 1 ? '' : 's'}</span>
        <span className="ws-sep" />
        <div className="ws-search">
          <svg width="11" height="11" viewBox="0 0 12 12" aria-hidden="true">
            <circle cx="5" cy="5" r="3.2" stroke="currentColor" strokeWidth="1.2" fill="none" />
            <path d="M7.4 7.4L10 10" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
          </svg>
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="filter variables…"
            spellCheck={false}
          />
        </div>
      </div>
      <div className="ws-toolbar-right">
        <div className="ws-segmented" role="tablist" aria-label="Sort">
          {['name', 'size', 'type'].map((k) => (
            <button
              key={k}
              role="tab"
              aria-selected={sort === k}
              className={sort === k ? 'is-active' : ''}
              onClick={() => setSort(k)}
            >sort: {k}</button>
          ))}
        </div>
        <div className="ws-segmented" role="tablist" aria-label="Layout">
          <button aria-selected={view === 'cards'} className={view === 'cards' ? 'is-active' : ''} onClick={() => setView('cards')} title="Cards view">
            <svg width="12" height="12" viewBox="0 0 12 12">
              <rect x="1" y="1"   width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
              <rect x="6.5" y="1" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
              <rect x="1" y="6.5" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
              <rect x="6.5" y="6.5" width="4.5" height="4.5" rx="0.5" fill="currentColor"/>
            </svg>
          </button>
          <button aria-selected={view === 'list'} className={view === 'list' ? 'is-active' : ''} onClick={() => setView('list')} title="List view">
            <svg width="12" height="12" viewBox="0 0 12 12">
              <rect x="1" y="2"   width="10" height="1.4" rx="0.5" fill="currentColor"/>
              <rect x="1" y="5.3" width="10" height="1.4" rx="0.5" fill="currentColor"/>
              <rect x="1" y="8.6" width="10" height="1.4" rx="0.5" fill="currentColor"/>
            </svg>
          </button>
        </div>
      </div>
    </div>
  );
}

/* ======================================================================== */
/* Workspace panel (the main exported component for the bottom-dock tab)    */
/* ======================================================================== */
export function WorkspacePanel({ variables, onOpen }) {
  const [query, setQuery]       = useState('');
  const [sort, setSort]         = useState('name');
  const [view, setView]         = useState('cards');
  const [selected, setSelected] = useState(null);

  const filtered = useMemo(() => {
    let list = variables.filter((v) => v.name.toLowerCase().includes(query.toLowerCase()));
    list.sort((a, b) => {
      if (sort === 'size') return (b.bytes || 0) - (a.bytes || 0);
      if (sort === 'type') return a.kind.localeCompare(b.kind);
      return a.name.localeCompare(b.name);
    });
    return list;
  }, [variables, query, sort]);

  useEffect(() => {
    function onKey(e) {
      if (e.key === 'Enter' && selected) {
        const v = variables.find((x) => x.name === selected);
        if (v) onOpen(v);
      }
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [selected, variables, onOpen]);

  return (
    <div className="workspace">
      <WorkspaceToolbar
        count={filtered.length}
        query={query} setQuery={setQuery}
        sort={sort} setSort={setSort}
        view={view} setView={setView}
      />
      {view === 'cards' ? (
        <div className="ws-grid">
          {filtered.map((v) => (
            <VariableCard
              key={v.name}
              v={v}
              selected={selected === v.name}
              onSelect={() => setSelected(v.name)}
              onOpen={() => onOpen(v)}
            />
          ))}
        </div>
      ) : (
        <div className="ws-list">
          <div className="var-row var-row-head">
            <span>name</span><span>size</span><span>type</span><span>preview</span>
            <span>bytes</span><span>mean</span><span>min</span><span>max</span>
          </div>
          {filtered.map((v) => (
            <VariableRow
              key={v.name}
              v={v}
              selected={selected === v.name}
              onSelect={() => setSelected(v.name)}
              onOpen={() => onOpen(v)}
            />
          ))}
        </div>
      )}
      {filtered.length === 0 && (
        <div className="ws-empty">no variables match “{query}”</div>
      )}
      <div className="ws-hint">
        <kbd>↵</kbd> open · <kbd>dbl-click</kbd> open · <kbd>Esc</kbd> close editor
      </div>
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
                  onKeyDown={(e) => { if (e.key === 'Enter' && /[\d,\-]/.test(pickerQuery)) { selectRange(pickerQuery); setPickerQuery(''); } }}
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

function InlinePlot({ data, rows, cols, onClose }) {
  const [mAxis, setMAxis] = useState('col');
  const [mSel, setMSel]   = useState(() => new Set([0]));
  const [mXMode, setMXMode] = useState('index');
  const [mXSrc, setMXSrc]   = useState({ axis: 'col', idx: 0 });
  const [pickerQuery, setPickerQuery] = useState('');

  useEffect(() => {
    const def = cols >= rows ? 'col' : 'row';
    setMAxis(def);
    setMSel(new Set([0]));
    setMXMode('index');
    setMXSrc({ axis: def, idx: 0 });
    setPickerQuery('');
  }, [rows, cols]);

  const palette = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a', '#d4a5e6', '#f2a37e', '#6fcfbf'];

  function sliceArr(src) {
    if (src.axis === 'row') return data[src.idx] || [];
    return data.map((r) => r[src.idx]);
  }

  const limit = mAxis === 'col' ? cols : rows;
  const ids = [...mSel].filter((k) => k < limit).sort((a, b) => a - b);
  const xShared = mXMode === 'src' ? sliceArr(mXSrc).map(Number) : null;
  const xLabel  = mXMode === 'src'
    ? `${mXSrc.axis} ${mXSrc.idx + 1}`
    : 'index';
  const curves = ids.map((k, i) => {
    const ys = (mAxis === 'col' ? data.map((r) => r[k]) : (data[k] || [])).map(Number);
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
/* Variable Editor — modal table with notation/precision/heatmap/plot       */
/* ======================================================================== */
export function VariableEditor({ variable, onClose, engine }) {
  const [precision, setPrecision] = useState(4);
  const [notation, setNotation]   = useState('fixed');
  const [heatmap, setHeatmap]     = useState(false);
  const [showPlot, setShowPlot]   = useState(false);
  const [saveOpen, setSaveOpen]   = useState(false);
  const [activeCell, setActiveCell] = useState({ r: 0, c: 0 });
  const [editing, setEditing]     = useState(null);
  const [editVal, setEditVal]     = useState('');
  const [data, setData]           = useState(variable.data);
  const [loading, setLoading]     = useState(false);
  const [loadError, setLoadError] = useState(null);
  const tableRef = useRef(null);
  const inputRef = useRef(null);

  // On open (and on variable swap), seed with the cheap preview, then ask
  // the engine for the variable's full numeric data and replace. The
  // preview renders instantly; the full fetch keeps the table responsive
  // for big arrays without blocking the UI.
  useEffect(() => {
    setData(variable.data);
    setActiveCell({ r: 0, c: 0 });
    setLoadError(null);
    if (!engine || typeof engine.getVarData !== 'function') return;
    setLoading(true);
    // Defer to next microtask so the modal paints first with the preview,
    // then the WASM fetch swaps in the full data. Avoid requestIdleCallback
    // — it can take seconds in some headless environments.
    const handle = setTimeout(() => {
      try {
        const r = engine.getVarData(variable.name);
        if (!r) { setLoading(false); return; }
        if (r.error) { setLoadError(r.error); setLoading(false); return; }
        if (Array.isArray(r.data) && r.data.length > 0) setData(r.data);
        setLoading(false);
      } catch (e) {
        setLoadError(e?.message || String(e));
        setLoading(false);
      }
    }, 0);
    return () => clearTimeout(handle);
  }, [variable, engine]);

  const rows = data.length;
  const cols = data[0]?.length || 0;

  const stats = useMemo(() => {
    let min = Infinity, max = -Infinity, sum = 0, n = 0;
    for (const row of data) for (const v of row) {
      if (typeof v === 'number') { if (v < min) min = v; if (v > max) max = v; sum += v; n++; }
    }
    return n ? { min, max, mean: sum / n, n } : null;
  }, [data]);

  // Format a single number with the active notation/precision settings.
  function formatNum(n) {
    if (!Number.isFinite(n)) return String(n);
    if (notation === 'exp')   return n.toExponential(precision);
    if (notation === 'fixed') return n.toFixed(precision);
    return fmt(n, { fix: precision, exp: precision });
  }

  // Complex values arrive as "a+bi" strings from the WASM binding (it keeps
  // doubleData precision in real/imag separately, but JS doesn't have a
  // native complex type so we render as a string). Parse, reformat each
  // half through formatNum, reassemble. Falls through to the original
  // string if parsing fails so unrecognized cells aren't garbled.
  const COMPLEX_RE = /^\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*([+-])\s*(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)i\s*$/;
  function format(v) {
    if (typeof v === 'number') return formatNum(v);
    if (typeof v === 'string') {
      const m = v.match(COMPLEX_RE);
      if (m) {
        const re = parseFloat(m[1]);
        const im = parseFloat(m[3]) * (m[2] === '-' ? -1 : 1);
        const reStr = formatNum(re);
        const imStr = formatNum(Math.abs(im));
        return `${reStr}${im >= 0 ? '+' : '-'}${imStr}i`;
      }
    }
    return String(v);
  }

  function commitEdit() {
    if (!editing) return;
    const parsed = parseFloat(editVal);
    if (!Number.isNaN(parsed)) {
      setData((d) => {
        const copy = d.map((r) => r.slice());
        copy[editing.r][editing.c] = parsed;
        return copy;
      });
    }
    setEditing(null);
  }

  useEffect(() => {
    if (editing && inputRef.current) {
      inputRef.current.focus();
      inputRef.current.select();
    }
  }, [editing]);

  const handleKey = useCallback((e) => {
    if (editing) {
      if (e.key === 'Enter')  { e.preventDefault(); commitEdit(); }
      if (e.key === 'Escape') { e.preventDefault(); setEditing(null); }
      return;
    }
    if (e.key === 'Escape') { onClose(); return; }
    if (e.key === 'Enter' || e.key === 'F2') {
      const v = data[activeCell.r][activeCell.c];
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
    if (r !== activeCell.r || c !== activeCell.c) {
      setActiveCell({ r, c });
      e.preventDefault();
    }
  }, [activeCell, rows, cols, editing, data, onClose]);

  useEffect(() => {
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [handleKey]);

  const { themeName: veThemeName } = useTheme();
  const meta = KIND_META[variable.kind] || KIND_META.matrix;
  const tone = pickTone(TONE[meta.tone] || TONE.amber, veThemeName);

  return (
    <div className="ve-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="ve-window" role="dialog" aria-label={`Variable Editor: ${variable.name}`}>
        <div className="ve-titlebar">
          <div className="ve-title-left">
            <span className="ve-tag" style={{ color: tone.fg, background: tone.bg, borderColor: tone.border }}>
              {meta.glyph} {variable.type}
            </span>
            <span className="ve-name">{variable.name}</span>
            <span className="ve-dim">{variable.size}</span>
          </div>
          <div className="ve-title-right">
            {loading && (
              <span className="ve-meta" style={{ color: 'var(--accent)' }}>
                loading full data…
              </span>
            )}
            {loadError && (
              <span className="ve-meta" style={{ color: 'var(--danger)' }}
                title={loadError}>
                preview only
              </span>
            )}
            <span className="ve-meta">{variable.bytes} B · {rows * cols} elements</span>
            <button className="ve-close" onClick={onClose} aria-label="Close">×</button>
          </div>
        </div>

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
            <input
              type="range" min={0} max={8} step={1}
              value={precision}
              onChange={(e) => setPrecision(parseInt(e.target.value, 10))}
            />
            <span className="ve-precision-num">{precision}</span>
          </div>
          <div className="ve-tools-group">
            <button className={`ve-btn ${heatmap ? 'is-active' : ''}`}
              onClick={() => setHeatmap((h) => !h)}
              title="Toggle value heatmap">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <rect x="0.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 240)"/>
                <rect x="3.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 180)"/>
                <rect x="6.5" y="0.5" width="3" height="11" fill="oklch(0.6 0.1 60)"/>
                <rect x="9.5" y="0.5" width="2" height="11" fill="oklch(0.6 0.1 30)"/>
              </svg>
              heatmap
            </button>
            <button className="ve-btn" title="Copy as CSV" onClick={() => {
              const csv = data.map((row) => row.map((v) => typeof v === 'number' ? v : `"${v}"`).join(',')).join('\n');
              navigator.clipboard?.writeText(csv);
            }}>
              <svg width="11" height="11" viewBox="0 0 12 12">
                <rect x="2" y="2" width="7" height="8" rx="1" stroke="currentColor" fill="none"/>
                <rect x="3.5" y="0.5" width="7" height="8" rx="1" stroke="currentColor" fill="none"/>
              </svg>
              copy csv
            </button>
            <button className={`ve-btn ${showPlot ? 'is-active' : ''}`}
              title="Toggle inline plot"
              onClick={() => setShowPlot((p) => !p)}>
              <svg width="11" height="11" viewBox="0 0 12 12">
                <polyline points="1,9 4,5 7,7 11,2" stroke="currentColor" fill="none" strokeWidth="1.4"/>
              </svg>
              plot
            </button>
            <div className="ve-saveas-wrap">
              <button className="ve-btn ve-saveas-trigger"
                title="Save variable to file"
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
                <SaveAsMenu
                  onClose={() => setSaveOpen(false)}
                  onPick={(f) => { exportData(variable, data, f); setSaveOpen(false); }}
                />
              )}
            </div>
          </div>
          <div className="ve-tools-spacer" />
          {stats && (
            <div className="ve-stats">
              <span><b>min</b> {fmt(stats.min)}</span>
              <span><b>max</b> {fmt(stats.max)}</span>
              <span><b>μ</b> {fmt(stats.mean)}</span>
              <span><b>n</b> {stats.n}</span>
            </div>
          )}
        </div>

        <div className="ve-address">
          <span className="ve-cell-ref">{variable.name}({activeCell.r + 1}, {activeCell.c + 1})</span>
          <span className="ve-eq">=</span>
          <span className="ve-cell-val">{format(data[activeCell.r]?.[activeCell.c])}</span>
        </div>

        <div className={`ve-body ${showPlot ? 'has-plot' : ''}`}>
          <div className="ve-table-wrap" ref={tableRef}>
            <table className="ve-table">
              <thead>
                <tr>
                  <th className="ve-corner">{rows}×{cols}</th>
                  {Array.from({ length: cols }, (_, c) => (
                    <th key={c} className={c === activeCell.c ? 'is-active' : ''}>{c + 1}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {data.map((row, r) => (
                  <tr key={r}>
                    <th className={`ve-rowhead ${r === activeCell.r ? 'is-active' : ''}`}>{r + 1}</th>
                    {row.map((v, c) => {
                      const isActive  = activeCell.r === r && activeCell.c === c;
                      const isEditing = editing && editing.r === r && editing.c === c;
                      const bg = (heatmap && stats && typeof v === 'number')
                        ? heatColor(v, stats.min, stats.max) : undefined;
                      return (
                        <td
                          key={c}
                          className={isActive ? 'is-active' : ''}
                          style={{ background: bg }}
                          onClick={() => setActiveCell({ r, c })}
                          onDoubleClick={() => {
                            setActiveCell({ r, c });
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
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          {showPlot && (
            <InlinePlot
              data={data}
              rows={rows}
              cols={cols}
              onClose={() => setShowPlot(false)}
            />
          )}
        </div>

        <div className="ve-status">
          <span>cell ({activeCell.r + 1},{activeCell.c + 1})</span>
          <span className="ve-sep" />
          <span>↑↓←→ navigate</span>
          <span className="ve-sep" />
          <span>↵ / F2 edit</span>
          <span className="ve-sep" />
          <span>Esc close</span>
          <span className="ve-spacer" />
          <span>UTF-8</span>
          <span className="ve-sep" />
          <span>read/write</span>
        </div>
      </div>
    </div>
  );
}
