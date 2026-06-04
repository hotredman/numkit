// InlinePlot.jsx — the variable-editor's inline plotting panel: SaveAsMenu,
// PlotChart, the multi-axis picker controls, and the InlinePlot host that
// ties them together. Used by MatrixPanel.
import { useState, useMemo, useEffect, useRef } from 'react';

export function SaveAsMenu({ onPick, onClose }) {
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

export function InlinePlot({ getSlice, rows, cols, onClose }) {
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
