import { useState, useMemo, useEffect, useRef, useLayoutEffect } from 'react';
import FigureWindow from '../plot/FigureWindow';

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
        <span className="ve-plot-lbl ve-plot-lbl-fixed">
          <span className="ve-plot-lbl-full">curves by</span>
          <span className="ve-plot-lbl-short">by</span>
        </span>
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
            <span className="ve-multi-label">
              <span className="ve-multi-label-full">of {limit} {mAxis}s</span>
              <span className="ve-multi-label-short">/ {limit}</span>
            </span>
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
                        dimMode, setDimMode,
                        mAxis, setMAxis, mSel, setMSel,
                        mXMode, setMXMode, mXSrc, setMXSrc,
                        pickerQuery, setPickerQuery,
                        plotType, setPlotType,
                        onClose }) {
  return (
    <>
      <div className="ve-plot-head">
        <span className="ve-plot-title">
          <span className="ve-plot-title-full">inline plot</span>
          <span className="ve-plot-title-short" title="Inline Plot">📈</span>
        </span>
        <div className="ve-plot-tabs">
          {['1d', '2d', '3d'].map((mode) => (
            <button key={mode}
              className={`ve-btn ${dimMode === mode ? 'is-active' : ''}`}
              onClick={() => {
                setDimMode(mode);
                if (mode === '1d') setPlotType('line');
                if (mode === '2d') setPlotType('imagesc');
                if (mode === '3d') setPlotType('surf');
              }}
              style={{
                background: dimMode === mode ? 'var(--bg-3)' : 'transparent',
                color: dimMode === mode ? 'var(--fg-1)' : 'var(--fg-3)',
                border: 'none',
                padding: '2px 8px',
                borderRadius: '4px',
                fontSize: '11px',
                fontFamily: 'var(--font-mono)',
                cursor: 'pointer',
              }}>
              {mode}
            </button>
          ))}
        </div>
        <span className="ve-plot-spacer" />
        <div className="ve-plot-type-wrap" style={{ display: 'flex', alignItems: 'center', gap: '6px', marginRight: '8px' }}>
          <span className="ve-plot-lbl">type</span>
          <select
            className="ve-btn ve-plot-type-select"
            value={plotType}
            onChange={(e) => setPlotType(e.target.value)}
            style={{
              background: 'var(--bg-2)',
              color: 'var(--fg-0)',
              border: '1px solid var(--line)',
              borderRadius: 'var(--r-sm)',
              fontSize: '11px',
              padding: '2px 6px',
              fontFamily: 'var(--font-mono)',
              cursor: 'pointer',
              outline: 'none',
            }}
          >
            {dimMode === '1d' && (
              <>
                <option value="line">📈 line</option>
                <option value="stem">📍 stem</option>
                <option value="bar">📊 bar</option>
                <option value="scatter">🔴 scatter</option>
                <option value="area">🏔️ area</option>
                <option value="stairs">🪜 stairs</option>
              </>
            )}
            {dimMode === '2d' && (
              <>
                <option value="imagesc">🔲 imagesc</option>
                <option value="contour">〰️ contour</option>
                <option value="spy">🔍 spy</option>
              </>
            )}
            {dimMode === '3d' && (
              <>
                <option value="surf">🌊 surf</option>
                <option value="mesh">🌐 mesh</option>
              </>
            )}
          </select>
        </div>
        <button className="ve-plot-close" onClick={onClose} title="Hide plot">×</button>
      </div>
      {dimMode === '1d' && (
        <MultiPickerControls mAxis={mAxis} setMAxis={setMAxis} mSel={mSel} setMSel={setMSel}
          rows={rows} cols={cols}
          mXMode={mXMode} setMXMode={setMXMode} mXSrc={mXSrc} setMXSrc={setMXSrc}
          pickerQuery={pickerQuery} setPickerQuery={setPickerQuery} />
      )}
    </>
  );
}

export function InlinePlot({ getSlice, rows, cols, varName, engine, isSparse, page = 0, onClose }) {
  const defaultAxis = (r, c) => (c >= r ? 'row' : 'col');

  const [dimMode, setDimMode] = useState(() => isSparse ? '2d' : '1d');
  const [mAxis, setMAxis] = useState(() => defaultAxis(rows, cols));
  const [mSel, setMSel]   = useState(() => new Set([0]));
  const [mXMode, setMXMode] = useState('index');
  const [mXSrc, setMXSrc]   = useState(() => ({ axis: defaultAxis(rows, cols), idx: 0 }));
  const [pickerQuery, setPickerQuery] = useState('');
  const [plotType, setPlotType] = useState(() => isSparse ? 'spy' : 'line');

  const [engineFig, setEngineFig] = useState(null);
  const [engineLoading, setEngineLoading] = useState(false);

  useEffect(() => {
    if (!engine || typeof engine.getVarFigure !== 'function' || !varName) {
      setEngineFig(null);
      setEngineLoading(false);
      return;
    }
    let active = true;
    setEngineLoading(true);
    // Ask the C++ engine to generate the figure JSON (e.g., downsampling for 1D/2D/3D)
    const req = engine.getVarFigure(varName, {
      mode: plotType,
      dimMode,
      axis: mAxis,
      indices: [...mSel],
      xMode: mXMode,
      xSrc: mXSrc,
      page,
    });
    Promise.resolve(req).then((res) => {
      if (active) {
        if (res && !res.error) {
          setEngineFig(res);
        } else {
          setEngineFig(null);
        }
        setEngineLoading(false);
      }
    }).catch((err) => {
      if (active) {
        console.error('getVarFigure error:', err);
        setEngineFig(null);
        setEngineLoading(false);
      }
    });
    return () => { active = false; };
  }, [dimMode, plotType, engine, varName, mAxis, mSel, mXMode, mXSrc, page]);

  useEffect(() => {
    const def = defaultAxis(rows, cols);
    setMAxis(def);
    setMSel(new Set([0]));
    setMXMode('index');
    setMXSrc({ axis: def, idx: 0 });
    setPickerQuery('');
  }, [rows, cols]);

  const palette = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a', '#d4a5e6', '#f2a37e', '#6fcfbf'];

  function sliceArr(src) {
    return (getSlice && getSlice(src.axis, src.idx)) || [];
  }

  const limit = mAxis === 'col' ? cols : rows;
  const ids = [...mSel].filter((k) => k < limit).sort((a, b) => a - b);
  const xShared = mXMode === 'src' ? sliceArr(mXSrc).map(Number) : null;
  const xLabel  = mXMode === 'src'
    ? `${mXSrc.axis} ${mXSrc.idx + 1}`
    : 'index';
  const curves = ids.map((k, i) => {
    const ys = ((getSlice && getSlice(mAxis, k)) || []).map(Number);
    const length = ys.length;
    const xs = xShared ? xShared.slice(0, length) : ys.map((_, j) => j + 1);
    const x = [], y = [];
    for (let j = 0; j < length; j++) if (Number.isFinite(xs[j]) && Number.isFinite(ys[j])) { x.push(xs[j]); y.push(ys[j]); }
    return { name: `${mAxis} ${k + 1}`, x, y, color: palette[i % palette.length] };
  });

  const fig = useMemo(() => {
    let xMin = Infinity, xMax = -Infinity, yMin = Infinity, yMax = -Infinity;
    curves.forEach((c) => {
      c.x.forEach((v) => { if (v < xMin) xMin = v; if (v > xMax) xMax = v; });
      c.y.forEach((v) => { if (v < yMin) yMin = v; if (v > yMax) yMax = v; });
    });
    if (xMin === Infinity || xMax === -Infinity) { xMin = -1; xMax = 1; }
    if (yMin === Infinity || yMax === -Infinity) { yMin = -1; yMax = 1; }
    if (xMin === xMax) { xMin -= 0.5; xMax += 0.5; }
    if (yMin === yMax) { yMin -= 0.5; yMax += 0.5; }

    return {
      kind: 'composite',
      id: `1`,
      title: `${mAxis} ${[...mSel].map((k) => k + 1).join(', ')}`,
      xLabel: xLabel,
      yLabel: '',
      xRange: [xMin, xMax],
      yRange: [yMin, yMax],
      grid: true,
      layers: curves.map((c) => ({
        kind: 'series',
        mode: plotType,
        name: c.name,
        x: c.x,
        y: c.y,
        color: c.color,
      })),
    };
  }, [curves, plotType, xLabel, mAxis, mSel]);

  const [aspectStr, setAspectStr] = useState(() => {
    try {
      // Need loadSettings here without importing it at top level if not already imported
      const raw = localStorage.getItem('numkit.ide.settings');
      if (raw) return JSON.parse(raw).plotAspectRatio || '16:9';
    } catch (e) {}
    return '16:9';
  });

  useEffect(() => {
    const onSettings = (e) => setAspectStr(e.detail?.plotAspectRatio || '16:9');
    window.addEventListener('numkitSettingsChanged', onSettings);
    return () => window.removeEventListener('numkitSettingsChanged', onSettings);
  }, []);

  const aspect = (() => {
    if (aspectStr === '4:3') return 4 / 3;
    if (aspectStr === '16:10') return 16 / 10;
    return 16 / 9;
  })();

  let content = null;
  if (engineLoading) {
    content = <div className="ve-plot-empty">generating figure...</div>;
  } else if (engineFig) {
    content = <FigureWindow figure={engineFig} embedded={true} onClose={onClose} aspectRatio={aspect} />;
  } else if (dimMode === '1d') {
    if (curves.length === 0 || curves.every((c) => c.y.length === 0)) {
      content = <div className="ve-plot-empty">no numeric data to plot — pick at least one {mAxis}</div>;
    } else {
      content = <FigureWindow figure={fig} embedded={true} onClose={onClose} aspectRatio={aspect} />;
    }
  } else {
    content = <div className="ve-plot-empty">failed to generate {dimMode} figure</div>;
  }

  return (
    <div className="ve-plot" style={{ display: 'flex', flexDirection: 'column', width: '100%', maxWidth: '100%', minWidth: 0, height: '100%', minHeight: 0 }}>
      <PlotControls rows={rows} cols={cols}
        dimMode={dimMode} setDimMode={setDimMode}
        mAxis={mAxis} setMAxis={setMAxis} mSel={mSel} setMSel={setMSel}
        mXMode={mXMode} setMXMode={setMXMode} mXSrc={mXSrc} setMXSrc={setMXSrc}
        pickerQuery={pickerQuery} setPickerQuery={setPickerQuery}
        plotType={plotType} setPlotType={setPlotType}
        onClose={onClose} />
      <div style={{ flex: 1, minWidth: 0, minHeight: 0, position: 'relative', overflow: 'hidden', width: '100%', maxWidth: '100%' }}>
        {content}
      </div>
    </div>
  );
}
