import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import InteractivePlot from './InteractivePlot';
import Heatmap from './Heatmap';
import PolarPlot, { defaultPolarViewport, nicePolarMax } from './PolarPlot';

function renderFigure(figure, props) {
  if (figure.kind === 'heatmap') return <Heatmap figure={figure} {...props} />;
  if (figure.kind === 'polar')   return <PolarPlot figure={figure} {...props} />;
  return <InteractivePlot figure={figure} {...props} />;
}

function computeFitViewport(series, mode, axisMode, currentVp, figDefault) {
  const list = mode === 'all' ? series : series.filter((s) => s.name === mode);
  let xMin = Infinity, xMax = -Infinity, yMin = Infinity, yMax = -Infinity;

  const xLo = Math.min(currentVp.x[0], currentVp.x[1]);
  const xHi = Math.max(currentVp.x[0], currentVp.x[1]);
  const yLo = Math.min(currentVp.y[0], currentVp.y[1]);
  const yHi = Math.max(currentVp.y[0], currentVp.y[1]);

  list.forEach((s) => {
    for (let i = 0; i < s.x.length; i++) {
      const xv = s.x[i], yv = s.y[i];
      if (!Number.isFinite(xv) || !Number.isFinite(yv)) continue;
      const passYWindow = axisMode === 'x' ? (yv >= yLo && yv <= yHi) : true;
      if (passYWindow) {
        if (xv < xMin) xMin = xv;
        if (xv > xMax) xMax = xv;
      }
      const passXWindow = axisMode === 'y' ? (xv >= xLo && xv <= xHi) : true;
      if (passXWindow) {
        if (yv < yMin) yMin = yv;
        if (yv > yMax) yMax = yv;
      }
    }
  });

  const next = { x: currentVp.x.slice(), y: currentVp.y.slice() };
  if (axisMode === 'both' || axisMode === 'x') {
    if (Number.isFinite(xMin) && Number.isFinite(xMax)) {
      const padX = (xMax - xMin) * 0.04 || Math.abs(xMin) * 0.05 || 0.5;
      next.x = [xMin - padX, xMax + padX];
    } else next.x = figDefault.x.slice();
  }
  if (axisMode === 'both' || axisMode === 'y') {
    if (Number.isFinite(yMin) && Number.isFinite(yMax)) {
      const padY = (yMax - yMin) * 0.06 || Math.abs(yMin) * 0.05 || 0.5;
      next.y = [yMin - padY, yMax + padY];
    } else next.y = figDefault.y.slice();
  }
  return next;
}

function NumberInput({ value, onCommit, width = 88 }) {
  const [v, setV] = useState(value);
  useEffect(() => { setV(value); }, [value]);
  return (
    <input
      type="text"
      value={typeof v === 'number' ? Number(v.toFixed(6)).toString() : v}
      onChange={(e) => setV(e.target.value)}
      onKeyDown={(e) => {
        if (e.key === 'Enter')  { e.target.blur(); }
        if (e.key === 'Escape') { setV(value); e.target.blur(); }
      }}
      onBlur={() => {
        const n = parseFloat(v);
        if (Number.isFinite(n)) onCommit(n);
        else setV(value);
      }}
      className="fw-num-input"
      style={{ width }}
    />
  );
}

export default function FigureWindow({ figure, onClose }) {
  const isPolar = figure.kind === 'polar';
  // Polar plots use {r:[lo,hi]}; cartesian plots use {x:[…], y:[…]}. We never
  // mix the two — the toolbar branches on `isPolar` to render the right inputs.
  const figDefault = isPolar
    ? defaultPolarViewport(figure)
    : (figure.xRange && figure.yRange)
      ? { x: figure.xRange.slice(), y: figure.yRange.slice() }
      : { x: [-1, 1], y: [-1, 1] };
  const [viewport, setViewport]   = useState(figDefault);
  const [showMajor, setShowMajor] = useState(true);
  const [showMinor, setShowMinor] = useState(true);
  const [showLegend, setShowLegend] = useState(true);
  const [fitOpen, setFitOpen]     = useState(false);
  const [saveOpen, setSaveOpen]   = useState(false);
  const [maximized, setMaximized] = useState(false);
  const fitRef  = useRef(null);
  const saveRef = useRef(null);
  const wrapRef = useRef(null);
  const [size, setSize] = useState({ w: 1100, h: 600 });

  useEffect(() => {
    function onKey(e) {
      if (e.key === 'Escape') onClose();
      if (e.key === '0') setViewport(figDefault);
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose, figure]);

  useEffect(() => {
    function onDoc(e) {
      if (fitRef.current  && !fitRef.current.contains(e.target))  setFitOpen(false);
      if (saveRef.current && !saveRef.current.contains(e.target)) setSaveOpen(false);
    }
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, []);

  // Synchronous measure before paint so the SVG is sized correctly on the
  // very first frame the modal opens. `[]` deps so this only runs once at
  // mount — without it React would rerun the effect after every state
  // change, feeding setSize back into another render and tripping the
  // "Maximum update depth exceeded" guard.
  useLayoutEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    if (!r.width) return;
    setSize((prev) => {
      const w = Math.max(400, Math.round(r.width  - 32));
      const h = Math.max(300, Math.round(r.height - 32));
      return (Math.abs(prev.w - w) > 0.5 || Math.abs(prev.h - h) > 0.5) ? { w, h } : prev;
    });
  }, []);

  // Re-measure on resize signals: window resize (modal is 85vw / 80vh) plus
  // ResizeObserver in modern browsers.
  useEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const remeasure = () => {
      const r = el.getBoundingClientRect();
      if (!r.width) return;
      setSize((prev) => {
        const w = Math.max(400, Math.round(r.width  - 32));
        const h = Math.max(300, Math.round(r.height - 32));
        return (Math.abs(prev.w - w) > 0.5 || Math.abs(prev.h - h) > 0.5) ? { w, h } : prev;
      });
    };
    let ro = null;
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(remeasure);
      ro.observe(el);
    }
    window.addEventListener('resize', remeasure);
    return () => {
      ro?.disconnect();
      window.removeEventListener('resize', remeasure);
    };
  }, []);

  function downloadBlob(blob, name) {
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = name; a.click();
    setTimeout(() => URL.revokeObjectURL(url), 100);
  }
  function exportSvg() {
    const svg = wrapRef.current?.querySelector('svg');
    if (!svg) return;
    const xml = new XMLSerializer().serializeToString(svg);
    downloadBlob(new Blob([xml], { type: 'image/svg+xml' }), `figure_${figure.id}.svg`);
  }
  function exportPng(scale = 2) {
    const svg = wrapRef.current?.querySelector('svg');
    if (!svg) return;
    const xml = new XMLSerializer().serializeToString(svg);
    const w = size.w * scale, h = size.h * scale;
    const img = new Image();
    const url = URL.createObjectURL(new Blob([xml], { type: 'image/svg+xml' }));
    img.onload = () => {
      const c = document.createElement('canvas');
      c.width = w; c.height = h;
      const ctx = c.getContext('2d');
      ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--plot-bg') || '#1a1f24';
      ctx.fillRect(0, 0, w, h);
      ctx.drawImage(img, 0, 0, w, h);
      c.toBlob((b) => { downloadBlob(b, `figure_${figure.id}.png`); URL.revokeObjectURL(url); });
    };
    img.src = url;
  }
  // Heatmap exports its z-matrix as a CSV/TSV grid (not series-shape).
  function exportCsv() {
    if (figure.kind === 'heatmap') {
      const rows = figure.z.map((row) => row.map((v) => v == null ? '' : v).join(','));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    const rows = ['name,x,y'];
    (figure.series || []).forEach((s) => {
      const xs = s.x || s.theta || [];
      const ys = s.y || s.rho   || [];
      for (let i = 0; i < xs.length; i++) rows.push(`${s.name},${xs[i]},${ys[i]}`);
    });
    downloadBlob(new Blob([rows.join('\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
  }
  function exportTsv() {
    if (figure.kind === 'heatmap') {
      const rows = figure.z.map((row) => row.map((v) => v == null ? '' : v).join('\t'));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    const rows = ['name\tx\ty'];
    (figure.series || []).forEach((s) => {
      const xs = s.x || s.theta || [];
      const ys = s.y || s.rho   || [];
      for (let i = 0; i < xs.length; i++) rows.push(`${s.name}\t${xs[i]}\t${ys[i]}`);
    });
    downloadBlob(new Blob([rows.join('\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
  }
  function exportJson() {
    if (figure.kind === 'heatmap') {
      const obj = { id: figure.id, kind: 'heatmap', title: figure.title,
        xRange: figure.xRange, yRange: figure.yRange,
        cmin: figure.cmin, cmax: figure.cmax, colormap: figure.colormap, z: figure.z };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
      return;
    }
    const obj = {
      id: figure.id, kind: figure.kind, title: figure.title,
      xLabel: figure.xLabel, yLabel: figure.yLabel,
      series: (figure.series || []).map((s) => ({
        name: s.name, color: s.color,
        x: s.x ?? s.theta, y: s.y ?? s.rho,
      })),
    };
    downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
  }

  function applyFit(mode, axisMode) {
    if (!figure.series) return;            // no-op for heatmap
    if (isPolar) {
      // Polar fit: pick max |rho| across selected series, round up to a nice
      // multiple, keep rMin at 0 (or whatever the figure's existing inner
      // ring is). axisMode is ignored — there's only one axis.
      const list = mode === 'all' ? figure.series : figure.series.filter((s) => s.name === mode);
      let m = 0;
      list.forEach((s) => s.rho?.forEach((v) => {
        if (Number.isFinite(v) && Math.abs(v) > m) m = Math.abs(v);
      }));
      const lo = (Array.isArray(figure.rlim) && figure.rlim.length === 2) ? figure.rlim[0] : 0;
      setViewport({ r: [lo, nicePolarMax(m || 1)] });
      setFitOpen(false);
      return;
    }
    setViewport(computeFitViewport(figure.series, mode, axisMode, viewport, figDefault));
    setFitOpen(false);
  }

  const fmtVp = (n) => Number.isFinite(n) ? Number(n.toPrecision(5)).toString() : '—';

  return (
    <div className="fw-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className={`fw-window ${maximized ? 'is-max' : ''}`}
        role="dialog" aria-label={`Figure ${figure.id}`}>
        <div className="fw-titlebar">
          <div className="fw-title-left">
            <span className="ve-tag" style={{
              color: 'var(--accent)',
              background: 'rgba(127,217,154,0.10)',
              borderColor: 'rgba(127,217,154,0.30)',
            }}>▦ figure</span>
            <span className="fw-name">Figure {figure.id}</span>
            <span className="ve-dim">{figure.title}</span>
            <span className="fw-meta">
              {figure.kind === 'heatmap'
                ? `${figure.z?.length ?? 0} × ${figure.z?.[0]?.length ?? 0} cells · range [${Number(figure.cmin).toPrecision(3)} … ${Number(figure.cmax).toPrecision(3)}]`
                : `${figure.series?.length ?? 0} series · ${(figure.series || []).reduce((s, x) => s + (x.x?.length ?? x.theta?.length ?? 0), 0)} points`}
            </span>
          </div>
          <div className="fw-title-right">
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
        </div>

        <div className="fw-toolbar">
          <div className="ve-tools-group" ref={fitRef}>
            <button className="ve-btn" onClick={() => setFitOpen((o) => !o)} title="Fit viewport">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M2 2L10 10 M2 6V2H6 M10 6v4H6" stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinecap="round"/>
              </svg>
              fit ▾
            </button>
            {fitOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <div className="fw-pop-head">all curves</div>
                  <button onClick={() => applyFit('all', 'both')}>both axes</button>
                  <button onClick={() => applyFit('all', 'x')}>X only</button>
                  <button onClick={() => applyFit('all', 'y')}>Y only</button>
                </div>
                {Array.isArray(figure.series) && figure.series.length > 1 && (
                  <div className="fw-pop-section">
                    <div className="fw-pop-head">single curve</div>
                    {figure.series.map((s) => (
                      <div key={s.name} className="fw-pop-row">
                        <span className="fw-pop-name"><i style={{ background: s.color }} />{s.name}</span>
                        <button onClick={() => applyFit(s.name, 'both')}>xy</button>
                        <button onClick={() => applyFit(s.name, 'x')}>x</button>
                        <button onClick={() => applyFit(s.name, 'y')}>y</button>
                      </div>
                    ))}
                  </div>
                )}
                <div className="fw-pop-section">
                  <button onClick={() => { setViewport(figDefault); setFitOpen(false); }}>reset to default</button>
                </div>
              </div>
            )}
          </div>

          {isPolar ? (
            <div className="ve-tools-group fw-range-group">
              <span className="ve-label">r</span>
              <NumberInput value={viewport.r[0]} onCommit={(n) => setViewport({ r: [n, viewport.r[1]] })} />
              <span className="fw-range-sep">→</span>
              <NumberInput value={viewport.r[1]} onCommit={(n) => setViewport({ r: [viewport.r[0], n] })} />
            </div>
          ) : (
            <div className="ve-tools-group fw-range-group">
              <span className="ve-label">x</span>
              <NumberInput value={viewport.x[0]} onCommit={(n) => setViewport({ ...viewport, x: [n, viewport.x[1]] })} />
              <span className="fw-range-sep">→</span>
              <NumberInput value={viewport.x[1]} onCommit={(n) => setViewport({ ...viewport, x: [viewport.x[0], n] })} />
              <span className="ve-label" style={{ marginLeft: 6 }}>y</span>
              <NumberInput value={viewport.y[0]} onCommit={(n) => setViewport({ ...viewport, y: [n, viewport.y[1]] })} />
              <span className="fw-range-sep">→</span>
              <NumberInput value={viewport.y[1]} onCommit={(n) => setViewport({ ...viewport, y: [viewport.y[0], n] })} />
            </div>
          )}

          <div className="ve-tools-group">
            <button className={`ve-btn ${showMajor ? 'is-active' : ''}`} onClick={() => setShowMajor((g) => !g)} title="Major grid">grid</button>
            <button className={`ve-btn ${showMinor ? 'is-active' : ''}`} onClick={() => setShowMinor((g) => !g)} title="Minor grid">minor</button>
            <button className={`ve-btn ${showLegend ? 'is-active' : ''}`} onClick={() => setShowLegend((g) => !g)}>legend</button>
          </div>

          <div className="ve-tools-spacer" />

          <div className="ve-tools-group" ref={saveRef}>
            <button className="ve-btn" onClick={() => setSaveOpen((o) => !o)}>
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M6 1v8M3 6l3 3 3-3M2 11h8" stroke="currentColor" fill="none" strokeLinecap="round"/>
              </svg>
              save / export ▾
            </button>
            {saveOpen && (
              <div className="fw-pop fw-pop-right">
                <div className="fw-pop-section">
                  <div className="fw-pop-head">image</div>
                  <button onClick={() => { exportSvg(); setSaveOpen(false); }}>SVG (vector)</button>
                  <button onClick={() => { exportPng(2); setSaveOpen(false); }}>PNG @2×</button>
                  <button onClick={() => { exportPng(4); setSaveOpen(false); }}>PNG @4×</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">data</div>
                  <button onClick={() => { exportCsv(); setSaveOpen(false); }}>CSV</button>
                  <button onClick={() => { exportTsv(); setSaveOpen(false); }}>TSV</button>
                  <button onClick={() => { exportJson(); setSaveOpen(false); }}>JSON</button>
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="fw-canvas-wrap" ref={wrapRef}>
          <div style={{ position: 'relative', width: '100%', height: '100%' }}>
            {renderFigure(figure, {
              width: size.w, height: size.h,
              viewport, setViewport,
              major: showMajor, minor: showMinor,
              fontScale: 1.15,
            })}
            {showLegend && Array.isArray(figure.series) && figure.series.length > 0 && (
              <div className="fw-legend">
                {figure.series.map((s) => (
                  <div key={s.name} className="fw-legend-item">
                    <i style={{ background: s.color }} />
                    <span>{s.name}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>

        <div className="fw-status">
          {isPolar ? (
            <span>r ∈ [{fmtVp(viewport.r[0])}, {fmtVp(viewport.r[1])}]</span>
          ) : (
            <>
              <span>x ∈ [{fmtVp(viewport.x[0])}, {fmtVp(viewport.x[1])}]</span>
              <span className="ve-sep" />
              <span>y ∈ [{fmtVp(viewport.y[0])}, {fmtVp(viewport.y[1])}]</span>
            </>
          )}
          <span className="ve-spacer" />
          <span>{isPolar ? 'drag · zoom rMax' : 'drag · pan'}</span>
          <span className="ve-sep" />
          <span>wheel · zoom</span>
          <span className="ve-sep" />
          <span>dbl-click · reset</span>
          <span className="ve-sep" />
          <span>0 · reset · Esc · close</span>
        </div>
      </div>
    </div>
  );
}
