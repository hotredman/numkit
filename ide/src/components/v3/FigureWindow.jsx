import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import InteractivePlot from './InteractivePlot';
import Heatmap from './Heatmap';
import PolarPlot, { defaultPolarViewport, nicePolarMax } from './PolarPlot';
import SubplotGrid from './SubplotGrid';
import { computeFitViewport,
  composeSvgsToString, exportSvgString, exportPngString,
  downloadBlob as utilDownloadBlob } from './plotUtils';

function renderFigure(figure, props) {
  if (figure.kind === 'subplot') return <SubplotGrid figure={figure} {...props} />;
  if (figure.kind === 'heatmap') return <Heatmap figure={figure} {...props} />;
  if (figure.kind === 'polar')   return <PolarPlot figure={figure} {...props} />;
  return <InteractivePlot figure={figure} {...props} />;
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

export default function FigureWindow({ figure, onClose, engine = null }) {
  const isPolar   = figure.kind === 'polar';
  const isSubplot = figure.kind === 'subplot';
  const isHeatmap = figure.kind === 'heatmap';
  // Polar plots use {r:[lo,hi]}; cartesian use {x:[…], y:[…]}; subplots have
  // per-cell viewports managed inside SubplotGrid, so the top-level viewport
  // is just a placeholder that the toolbar's range/status helpers branch off.
  const figDefault = isSubplot
    ? null
    : isPolar
      ? defaultPolarViewport(figure)
      : (figure.xRange && figure.yRange)
        ? { x: figure.xRange.slice(), y: figure.yRange.slice() }
        : { x: [-1, 1], y: [-1, 1] };
  const [viewport, setViewport]   = useState(figDefault);
  // Major grid defaults to on (a plot without it is unreadable). Minor grid
  // follows the engine's gridMode strictly: only on when the script called
  // `grid minor`, off after `grid on` / no call. Matches MATLAB.
  const [showMajor, setShowMajor] = useState(true);
  const [showMinor, setShowMinor] = useState(figure.grid === 'minor');
  const [showLegend, setShowLegend] = useState(true);
  // Log axis toggles — lifted from Heatmap so the toolbar can flip them
  // alongside grid/minor and the ПКМ menu inside the panel can mirror.
  // Initialised from figure.xscale/yscale (set by xscale('log') /
  // yscale('log')) and re-synced when those props change at script time.
  const [xLog, setXLog] = useState(figure.xscale === 'log');
  const [yLog, setYLog] = useState(figure.yscale === 'log');
  useEffect(() => { setXLog(figure.xscale === 'log'); }, [figure.xscale]);
  useEffect(() => { setYLog(figure.yscale === 'log'); }, [figure.yscale]);

  // Toggle that also auto-clamps the viewport's lo bound to the smallest
  // positive cell-centre when entering log mode (yRange[0] is typically
  // -cellH/2 due to imagesc padding — log of that is undefined).
  function toggleAxisLog(axis) {
    if (!isHeatmap) return;          // log toggles only meaningful for heatmap toolbar
    if (axis === 'x') {
      const next = !xLog;
      if (next && (viewport.x[0] <= 0 || viewport.x[1] <= 0)) {
        const fullCols = figure.originalCols || 1;
        const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
        const lo = Math.max(cellW * 0.5, 1e-6);
        setViewport({ ...viewport, x: [lo, Math.max(lo * 10, figure.xRange[1])] });
      }
      setXLog(next);
    } else {
      const next = !yLog;
      if (next && (viewport.y[0] <= 0 || viewport.y[1] <= 0)) {
        const fullRows = figure.originalRows || 1;
        const cellH = (figure.yRange[1] - figure.yRange[0]) / fullRows;
        const lo = Math.max(cellH * 0.5, 1e-6);
        setViewport({ ...viewport, y: [lo, Math.max(lo * 10, figure.yRange[1])] });
      }
      setYLog(next);
    }
  }
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
      if (e.key === '0' && figDefault) setViewport(figDefault);
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

  // Local downloadBlob for CSV/TSV/JSON paths below — image exports go through
  // plotUtils helpers so they share the light-theme + variable-resolution
  // logic with the per-plot ПКМ menus.
  const downloadBlob = utilDownloadBlob;

  /**
   * For subplot figures, gather every cell <svg> + its position relative to
   * the canvas wrap, and compose them into a single SVG string. Otherwise
   * just serialise the one SVG inside the wrap. Returns { xml, w, h } or
   * null if nothing's there yet.
   */
  function gatherFigureSvg() {
    const wrap = wrapRef.current;
    if (!wrap) return null;
    if (figure.kind === 'subplot') {
      const svgs = wrap.querySelectorAll('svg');
      if (svgs.length === 0) return null;
      const wrapRect = wrap.getBoundingClientRect();
      const layouts = Array.from(svgs).map((s) => {
        const r = s.getBoundingClientRect();
        return {
          x: r.left - wrapRect.left, y: r.top - wrapRect.top,
          w: r.width, h: r.height,
        };
      });
      return {
        xml: composeSvgsToString(svgs, layouts, wrapRect.width, wrapRect.height),
        w: wrapRect.width, h: wrapRect.height,
      };
    }
    const svg = wrap.querySelector('svg');
    if (!svg) return null;
    return {
      xml: new XMLSerializer().serializeToString(svg),
      w: size.w, h: size.h,
    };
  }
  function exportSvg() {
    const g = gatherFigureSvg();
    if (!g) return;
    exportSvgString(g.xml, `figure_${figure.id}.svg`);
  }
  function exportPng(scale = 2, suffix = '') {
    const g = gatherFigureSvg();
    if (!g) return;
    exportPngString(g.xml, g.w, g.h, scale, `figure_${figure.id}${suffix}.png`);
  }
  function exportPngPrint(mmWidth, dpi = 300) {
    const g = gatherFigureSvg();
    if (!g) return;
    const targetPx = (mmWidth / 25.4) * dpi;
    const scale = targetPx / g.w;
    exportPngString(g.xml, g.w, g.h, scale, `figure_${figure.id}_${mmWidth}mm.png`);
  }
  // Build a CSV/TSV "name<sep>x<sep>y" body for one series-bearing figure.
  function seriesBody(fig, sep) {
    const rows = [`name${sep}x${sep}y`];
    (fig.series || []).forEach((s) => {
      const xs = s.x || s.theta || [];
      const ys = s.y || s.rho   || [];
      for (let i = 0; i < xs.length; i++) rows.push(`${s.name}${sep}${xs[i]}${ys[i] != null ? sep + ys[i] : ''}`);
    });
    return rows.join('\n');
  }
  function exportCsv() {
    if (figure.kind === 'heatmap') {
      const rows = figure.z.map((row) => row.map((v) => v == null ? '' : v).join(','));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    if (figure.kind === 'subplot') {
      // One CSV section per cell, blank-line separated and prefixed by a tag.
      const parts = figure.cells.map((c, i) => {
        const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
        if (c.kind === 'heatmap') return `${tag}\n` + c.z.map((row) => row.join(',')).join('\n');
        return `${tag}\n` + seriesBody(c, ',');
      });
      downloadBlob(new Blob([parts.join('\n\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    downloadBlob(new Blob([seriesBody(figure, ',')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
  }
  function exportTsv() {
    if (figure.kind === 'heatmap') {
      const rows = figure.z.map((row) => row.map((v) => v == null ? '' : v).join('\t'));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    if (figure.kind === 'subplot') {
      const parts = figure.cells.map((c, i) => {
        const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
        if (c.kind === 'heatmap') return `${tag}\n` + c.z.map((row) => row.join('\t')).join('\n');
        return `${tag}\n` + seriesBody(c, '\t');
      });
      downloadBlob(new Blob([parts.join('\n\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    downloadBlob(new Blob([seriesBody(figure, '\t')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
  }
  function exportJson() {
    if (figure.kind === 'heatmap') {
      const obj = { id: figure.id, kind: 'heatmap', title: figure.title,
        xRange: figure.xRange, yRange: figure.yRange,
        cmin: figure.cmin, cmax: figure.cmax, colormap: figure.colormap, z: figure.z };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
      return;
    }
    if (figure.kind === 'subplot') {
      const obj = {
        id: figure.id, kind: 'subplot', title: figure.title, grid: figure.grid,
        cells: figure.cells.map((c) => {
          if (c.kind === 'heatmap') {
            return { subplotIndex: c.subplotIndex, kind: 'heatmap', title: c.title,
              xRange: c.xRange, yRange: c.yRange, z: c.z };
          }
          return { subplotIndex: c.subplotIndex, kind: c.kind, title: c.title,
            xLabel: c.xLabel, yLabel: c.yLabel,
            series: (c.series || []).map((s) => ({
              name: s.name, color: s.color, x: s.x ?? s.theta, y: s.y ?? s.rho,
            })),
          };
        }),
      };
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
    if (isSubplot) return;                 // subplot fit lives per-cell; close menu
    if (isHeatmap) {
      // Heatmap has no series — its X/Y data extent is figure.xRange/yRange.
      // Under a log axis (figure.xscale/yscale === 'log') the natural extent
      // straddles zero (cellH/2 padding) and would silently flip the axis
      // back to linear. Clamp the lo bound to half-cell-width when log.
      const xLog = figure.xscale === 'log';
      const yLog = figure.yscale === 'log';
      const next = { x: viewport.x.slice(), y: viewport.y.slice() };
      if (axisMode === 'both' || axisMode === 'x') {
        if (xLog) {
          const fullCols = figure.originalCols || 1;
          const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
          const lo = Math.max(cellW * 0.5, 1e-6);
          next.x = [lo, Math.max(lo * 10, figure.xRange[1])];
        } else {
          next.x = figure.xRange.slice();
        }
      }
      if (axisMode === 'both' || axisMode === 'y') {
        if (yLog) {
          const fullRows = figure.originalRows || 1;
          const cellH = (figure.yRange[1] - figure.yRange[0]) / fullRows;
          const lo = Math.max(cellH * 0.5, 1e-6);
          next.y = [lo, Math.max(lo * 10, figure.yRange[1])];
        } else {
          next.y = figure.yRange.slice();
        }
      }
      setViewport(next);
      setFitOpen(false);
      return;
    }
    if (!figure.series) return;            // no-op for heatmap (now handled above)
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
                : figure.kind === 'subplot'
                  ? `subplot ${figure.grid[0]}×${figure.grid[1]} · ${figure.cells.length} axes`
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
          {!isSubplot && (
          <div className="ve-tools-group" ref={fitRef}>
            <button className="ve-btn" onClick={() => setFitOpen((o) => !o)} title="Fit viewport">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M2 2L10 10 M2 6V2H6 M10 6v4H6" stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinecap="round"/>
              </svg>
              fit ▾
            </button>
            {fitOpen && (isHeatmap ? (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { setViewport(figDefault); setFitOpen(false); }}>reset to default</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">data extent</div>
                  <button onClick={() => applyFit('all', 'both')}>both axes</button>
                  <button onClick={() => applyFit('all', 'x')}>X only</button>
                  <button onClick={() => applyFit('all', 'y')}>Y only</button>
                </div>
              </div>
            ) : isPolar ? (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { setViewport(figDefault); setFitOpen(false); }}>reset to default</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">all curves</div>
                  <button onClick={() => applyFit('all', 'both')}>fit r-range</button>
                </div>
                {Array.isArray(figure.series) && figure.series.length > 1 && (
                  <div className="fw-pop-section">
                    <div className="fw-pop-head">single curve</div>
                    {figure.series.map((s) => (
                      <div key={s.name} className="fw-pop-row">
                        <span className="fw-pop-name"><i style={{ background: s.color }} />{s.name}</span>
                        <button onClick={() => applyFit(s.name, 'both')}>fit r</button>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            ) : (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { setViewport(figDefault); setFitOpen(false); }}>reset to default</button>
                </div>
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
              </div>
            ))}
          </div>
          )}

          {isSubplot ? null : isPolar ? (
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
            {isHeatmap && (
              <>
                <button
                  className={`ve-btn ${xLog ? 'is-active' : ''}`}
                  onClick={() => toggleAxisLog('x')}
                  disabled={figure.xRange[1] <= 0}
                  title="Log X axis (also: ПКМ → Axes → X axis · log)">x log</button>
                <button
                  className={`ve-btn ${yLog ? 'is-active' : ''}`}
                  onClick={() => toggleAxisLog('y')}
                  disabled={figure.yRange[1] <= 0}
                  title="Log Y axis (also: ПКМ → Axes → Y axis · log)">y log</button>
              </>
            )}
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
                  <div className="fw-pop-head">image · screen</div>
                  <button onClick={() => { exportSvg(); setSaveOpen(false); }}>SVG (vector)</button>
                  <button onClick={() => { exportPng(2); setSaveOpen(false); }}>PNG @2×</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">image · print (300 DPI)</div>
                  <button onClick={() => { exportPngPrint(85);  setSaveOpen(false); }}>PNG · 1 column (85 mm)</button>
                  <button onClick={() => { exportPngPrint(170); setSaveOpen(false); }}>PNG · 2 columns (170 mm)</button>
                  <button onClick={() => { exportPngPrint(210); setSaveOpen(false); }}>PNG · A4 width (210 mm)</button>
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
              engine,
              xLog, yLog,
              setXLog, setYLog,
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
          {isSubplot ? (
            <span>{figure.cells.length} axes · per-cell pan/zoom</span>
          ) : isPolar ? (
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
          <span>{isPolar ? 'wheel · zoom' : 'wheel · zoom xy · ⌃ x · ⇧ y'}</span>
          <span className="ve-sep" />
          <span>dbl-click · reset</span>
          <span className="ve-sep" />
          <span>0 · reset · Esc · close</span>
        </div>
      </div>
    </div>
  );
}
