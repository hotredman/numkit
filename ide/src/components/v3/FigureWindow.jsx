import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import CompositePlot from './CompositePlot';
import Composite3DPlot from './Composite3DPlot';
import FigureErrorBoundary from './FigureErrorBoundary';
import PolarPlot, { defaultPolarViewport, nicePolarMax } from './PolarPlot';
import SubplotGrid from './SubplotGrid';
import { computeFitViewport,
  composeSvgsToString, exportSvgString, exportPngString,
  downloadBlob as utilDownloadBlob } from './plotUtils';

function renderFigure(figure, props, threeRef) {
  if (figure.kind === 'subplot')     return <SubplotGrid     figure={figure} {...props} />;
  if (figure.kind === 'composite3d') {
    return (
      <FigureErrorBoundary label="composite3d-modal" figureId={figure.id}
        width={props.width} height={props.height}>
        <Composite3DPlot ref={threeRef} figure={figure} {...props} />
      </FigureErrorBoundary>
    );
  }
  if (figure.kind === 'composite')   return <CompositePlot   figure={figure} {...props} />;
  if (figure.kind === 'polar')       return <PolarPlot       figure={figure} {...props} />;
  return <CompositePlot figure={figure} {...props} />;
}

/** True when the 3-D viewport is still the (−1, 1) cube placeholder
 *  set up at mount before onBBox reports the real data extent. */
function isPlaceholder3D(v) {
  if (!v || !v.x || !v.y || !v.z) return true;
  return v.x[0] === -1 && v.x[1] === 1
      && v.y[0] === -1 && v.y[1] === 1
      && v.z[0] === -1 && v.z[1] === 1;
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
  const isComposite = figure.kind === 'composite';
  const is3D = figure.kind === 'composite3d';
  // Imperative handle on Composite3DPlot — exposed when the modal
  // hosts a 3-D figure. FigureWindow uses it for fit-3D, X/Y/Z input
  // wiring, and PNG / CSV export (canvas geometry has no SVG to
  // serialise).
  const threeRef = useRef(null);
  // Composite figures carry a heterogeneous layers[] array. Heatmap-specific
  // toolbar bits (color autoscale, colormap select, log toggle) gate on the
  // presence of a heatmap layer; the rest of the toolbar (fit, legend, range)
  // works off the series layers.
  const compositeLayers = isComposite && Array.isArray(figure.layers) ? figure.layers : [];
  const heatmapLayer = compositeLayers.find((l) => l.kind === 'heatmap') || null;
  const seriesLayers = compositeLayers.filter((l) => l.kind === 'series');
  const isHeatmap = !!heatmapLayer;
  const hasSeries = seriesLayers.length > 0;
  // Polar plots use {r:[lo,hi]}; cartesian use {x:[…], y:[…]}; subplots have
  // per-cell viewports managed inside SubplotGrid, so the top-level viewport
  // is just a placeholder that the toolbar's range/status helpers branch off.
  // For 3-D the data extent isn't on the figure prop directly — it's
  // computed by Composite3DPlot via its bbox helper. We start with a
  // safe placeholder ([-1, 1] cube) and fill in the real extent
  // through the onBBox callback below.
  const figDefault = isSubplot
    ? null
    : is3D
      ? { x: [-1, 1], y: [-1, 1], z: [-1, 1] }
      : isPolar
        ? defaultPolarViewport(figure)
        : (figure.xRange && figure.yRange)
          ? { x: figure.xRange.slice(), y: figure.yRange.slice() }
          : { x: [-1, 1], y: [-1, 1] };
  const [viewport, setViewport]   = useState(figDefault);
  // 3-D bbox cache — Composite3DPlot reports it via onBBox each
  // figure rebuild. Used as the "fit to data" target.
  const [bbox3d, setBbox3d] = useState(null);
  // Reset 3-D viewport on ACTUAL figure swap (a different figure.id)
  // so stale lims from a previous figure don't carry over. Skip the
  // initial mount — Composite3DPlot's onBBox would otherwise lose to
  // this reset (child effects run before parent effects, so the auto-
  // fill viewport gets clobbered back to the placeholder before the
  // user sees anything).
  const lastFigureIdRef = useRef(figure.id);
  useEffect(() => {
    if (!is3D) return;
    if (lastFigureIdRef.current !== figure.id) {
      setViewport({ x: [-1, 1], y: [-1, 1], z: [-1, 1] });
      lastFigureIdRef.current = figure.id;
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [figure.id]);
  // First bbox report fills the viewport with the actual data extent
  // — without this the X/Y/Z inputs would show -1 / 1 instead of the
  // data range until the user touched the Fit menu.
  function onComposite3DBBox(bbox) {
    setBbox3d(bbox);
    setViewport((cur) => {
      // Only auto-fill if the viewport is still the placeholder; once
      // the user committed an explicit input, leave it alone.
      if (!isPlaceholder3D(cur)) return cur;
      return {
        x: [bbox.xMin, bbox.xMax],
        y: [bbox.yMin, bbox.yMax],
        z: [bbox.zMin, bbox.zMax],
      };
    });
  }
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

  // Color-limit override for heatmap window/level autoscale. Lifted so the
  // toolbar fit menu and the panel's ПКМ menu share one state. null = use
  // figure.cmin/cmax directly.
  const [colorOverride, setColorOverride] = useState(null);
  // Colormap override — null falls back to figure.colormap. The toolbar
  // combo lets the user switch palettes at runtime without changing the
  // script.
  const [colormapOverride, setColormapOverride] = useState(null);
  // Reset both on figure identity change — old overrides don't apply to a
  // freshly emitted dataset.
  useEffect(() => {
    setColorOverride(null);
    setColormapOverride(null);
  }, [figure._raw?.id, figure.id]);

  // Compute new color override from a coarse-LOD scan of the visible
  // source-rect. Mirrors Heatmap.fitColorsToVisible. Lives here so the
  // toolbar fit menu can invoke it without lifting / via callback ref.
  function fitColorsToVisible() {
    if (!engine || typeof engine.getFigureTile !== 'function') return;
    if (!heatmapLayer) return;
    const figId = heatmapLayer._figId;
    if (typeof figId !== 'number' || figId < 0) return;
    const fullCols = heatmapLayer.originalCols;
    const fullRows = heatmapLayer.originalRows;
    if (!fullCols || !fullRows) return;
    const xExt = figure.xRange[1] - figure.xRange[0];
    const yExt = figure.yRange[1] - figure.yRange[0];
    const colsPerUnit = fullCols / (xExt || 1);
    const rowsPerUnit = fullRows / (yExt || 1);
    const xMin = viewport.x[0], xMax = viewport.x[1];
    const yMin = viewport.y[0], yMax = viewport.y[1];
    const c0 = Math.max(0, Math.floor((Math.min(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const c1 = Math.min(fullCols, Math.ceil((Math.max(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const r0 = Math.max(0, Math.floor((Math.min(yMin, yMax) - figure.yRange[0]) * rowsPerUnit));
    const r1 = Math.min(fullRows, Math.ceil((Math.max(yMin, yMax) - figure.yRange[0]) * rowsPerUnit));
    const tileW = c1 - c0, tileH = r1 - r0;
    if (tileW <= 0 || tileH <= 0) return;
    const lod = Math.max(1, Math.ceil(Math.max(tileH, tileW) / 256));
    const tile = engine.getFigureTile(figId, heatmapLayer._axIdx, heatmapLayer._dsIdx,
                                      r0, c0, tileH, tileW, lod);
    if (!tile || tile.error || !tile.data) return;
    let idxMn = 256, idxMx = -1;
    for (let i = 0; i < tile.data.length; i++) {
      const idx = tile.data[i];
      if (idx === 255) continue;
      if (idx < idxMn) idxMn = idx;
      if (idx > idxMx) idxMx = idx;
    }
    if (idxMn > 254 || idxMx < 0 || idxMn === idxMx) return;
    const cminOrig = heatmapLayer.cminOrig ?? heatmapLayer.cmin;
    const cmaxOrig = heatmapLayer.cmaxOrig ?? heatmapLayer.cmax;
    const range = cmaxOrig - cminOrig;
    setColorOverride({
      cmin: cminOrig + (idxMn / 254) * range,
      cmax: cminOrig + (idxMx / 254) * range,
    });
  }

  // Toggle that also auto-clamps the viewport's lo bound to the smallest
  // positive cell-centre when entering log mode (yRange[0] is typically
  // -cellH/2 due to imagesc padding — log of that is undefined).
  function toggleAxisLog(axis) {
    if (!isHeatmap) return;          // log toggles only meaningful for heatmap toolbar
    if (axis === 'x') {
      const next = !xLog;
      if (next && (viewport.x[0] <= 0 || viewport.x[1] <= 0)) {
        const fullCols = heatmapLayer?.originalCols || 1;
        const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
        const lo = Math.max(cellW * 0.5, 1e-6);
        setViewport({ ...viewport, x: [lo, Math.max(lo * 10, figure.xRange[1])] });
      }
      setXLog(next);
    } else {
      const next = !yLog;
      if (next && (viewport.y[0] <= 0 || viewport.y[1] <= 0)) {
        const fullRows = heatmapLayer?.originalRows || 1;
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
    if (is3D) return;   // SVG export not available for WebGL geometry
    const g = gatherFigureSvg();
    if (!g) return;
    exportSvgString(g.xml, `figure_${figure.id}.svg`);
  }
  function dataUrlToBlob(dataUrl) {
    // data:image/png;base64,...
    const idx = dataUrl.indexOf(',');
    const meta = dataUrl.substring(5, idx);                    // image/png;base64
    const mime = meta.split(';')[0];
    const b64  = dataUrl.substring(idx + 1);
    const bin  = atob(b64);
    const arr  = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i);
    return new Blob([arr], { type: mime });
  }
  function exportPng(scale = 2, suffix = '') {
    if (is3D) {
      const url = threeRef.current?.getCanvasDataURL?.(scale);
      if (!url) return;
      utilDownloadBlob(dataUrlToBlob(url), `figure_${figure.id}${suffix}.png`);
      return;
    }
    const g = gatherFigureSvg();
    if (!g) return;
    exportPngString(g.xml, g.w, g.h, scale, `figure_${figure.id}${suffix}.png`);
  }
  function exportPngPrint(mmWidth, dpi = 300) {
    if (is3D) {
      // 3-D path can't resize the WebGL canvas on the fly without a
      // re-mount, so print sizes use the same screen-resolution dump.
      // Resolution scaling is a follow-up (offscreen render at target).
      const url = threeRef.current?.getCanvasDataURL?.(1);
      if (!url) return;
      utilDownloadBlob(dataUrlToBlob(url), `figure_${figure.id}_${mmWidth}mm.png`);
      return;
    }
    const g = gatherFigureSvg();
    if (!g) return;
    const targetPx = (mmWidth / 25.4) * dpi;
    const scale = targetPx / g.w;
    exportPngString(g.xml, g.w, g.h, scale, `figure_${figure.id}_${mmWidth}mm.png`);
  }
  // Build a CSV/TSV "name<sep>x<sep>y[<sep>z]" body from a series
  // source. Accepts either a polar figure (`series` with theta/rho),
  // an array of 2-D series layers (`x`, `y`), or 3-D series with z[].
  function seriesBody(source, sep) {
    const list = Array.isArray(source) ? source : (source.series || []);
    const has3D = list.some((s) => Array.isArray(s.z));
    const rows = [`name${sep}x${sep}y${has3D ? sep + 'z' : ''}`];
    list.forEach((s) => {
      const xs = s.x || s.theta || [];
      const ys = s.y || s.rho   || [];
      const zs = Array.isArray(s.z) ? s.z : null;
      for (let i = 0; i < xs.length; i++) {
        let row = `${s.name}${sep}${xs[i]}`;
        if (ys[i] != null) row += sep + ys[i];
        if (zs && zs[i] != null) row += sep + zs[i];
        rows.push(row);
      }
    });
    return rows.join('\n');
  }
  function get3DRows() {
    return threeRef.current?.getCsvData?.() || [];
  }
  // Composite cell exporter — pulls heatmap layer's z if present, else series.
  function compositeCellBody(cell, sep) {
    const layers = cell.layers || [];
    const hl = layers.find((l) => l.kind === 'heatmap');
    if (hl) return hl.z.map((row) => row.map((v) => v == null ? '' : v).join(sep)).join('\n');
    return seriesBody(layers.filter((l) => l.kind === 'series'), sep);
  }
  function exportCsv() {
    if (is3D) {
      downloadBlob(new Blob([seriesBody(get3DRows(), ',')], { type: 'text/csv' }),
                   `figure_${figure.id}.csv`);
      return;
    }
    if (isHeatmap) {
      const z = heatmapLayer.z;
      const rows = z.map((row) => row.map((v) => v == null ? '' : v).join(','));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    if (isSubplot) {
      const parts = figure.cells.map((c, i) => {
        const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
        if (c.kind === 'composite') return `${tag}\n` + compositeCellBody(c, ',');
        return `${tag}\n` + seriesBody(c, ',');
      });
      downloadBlob(new Blob([parts.join('\n\n')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    if (isComposite) {
      downloadBlob(new Blob([seriesBody(seriesLayers, ',')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
      return;
    }
    downloadBlob(new Blob([seriesBody(figure, ',')], { type: 'text/csv' }), `figure_${figure.id}.csv`);
  }
  function exportTsv() {
    if (is3D) {
      downloadBlob(new Blob([seriesBody(get3DRows(), '\t')], { type: 'text/tab-separated-values' }),
                   `figure_${figure.id}.tsv`);
      return;
    }
    if (isHeatmap) {
      const z = heatmapLayer.z;
      const rows = z.map((row) => row.map((v) => v == null ? '' : v).join('\t'));
      downloadBlob(new Blob([rows.join('\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    if (isSubplot) {
      const parts = figure.cells.map((c, i) => {
        const tag = `# subplot ${c.subplotIndex || i + 1} — ${c.title || c.kind}`;
        if (c.kind === 'composite') return `${tag}\n` + compositeCellBody(c, '\t');
        return `${tag}\n` + seriesBody(c, '\t');
      });
      downloadBlob(new Blob([parts.join('\n\n')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    if (isComposite) {
      downloadBlob(new Blob([seriesBody(seriesLayers, '\t')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
      return;
    }
    downloadBlob(new Blob([seriesBody(figure, '\t')], { type: 'text/tab-separated-values' }), `figure_${figure.id}.tsv`);
  }
  function exportJson() {
    if (is3D) {
      const obj = {
        id: figure.id, kind: 'composite3d',
        title: figure.title,
        xLabel: figure.xLabel, yLabel: figure.yLabel, zLabel: figure.zLabel,
        view: figure.view,
        series: get3DRows(),
      };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }),
                   `figure_${figure.id}.json`);
      return;
    }
    if (isHeatmap) {
      const obj = {
        id: figure.id, kind: 'heatmap', title: figure.title,
        xRange: figure.xRange, yRange: figure.yRange,
        cmin: heatmapLayer.cmin, cmax: heatmapLayer.cmax,
        colormap: heatmapLayer.colormap, z: heatmapLayer.z,
      };
      downloadBlob(new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' }), `figure_${figure.id}.json`);
      return;
    }
    if (isSubplot) {
      const obj = {
        id: figure.id, kind: 'subplot', title: figure.title, grid: figure.grid,
        cells: figure.cells.map((c) => {
          if (c.kind === 'composite') {
            const layers = c.layers || [];
            return {
              subplotIndex: c.subplotIndex, kind: 'composite', title: c.title,
              xLabel: c.xLabel, yLabel: c.yLabel,
              xRange: c.xRange, yRange: c.yRange,
              layers: layers.map((ly) => {
                if (ly.kind === 'heatmap') return { kind: 'heatmap', z: ly.z, cmin: ly.cmin, cmax: ly.cmax };
                if (ly.kind === 'series')  return { kind: 'series', mode: ly.mode, name: ly.name, color: ly.color, x: ly.x, y: ly.y };
                return { ...ly };
              }),
            };
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
    if (isComposite) {
      const obj = {
        id: figure.id, kind: 'composite', title: figure.title,
        xLabel: figure.xLabel, yLabel: figure.yLabel,
        xRange: figure.xRange, yRange: figure.yRange,
        layers: compositeLayers.map((ly) => {
          if (ly.kind === 'heatmap') return { kind: 'heatmap', z: ly.z, cmin: ly.cmin, cmax: ly.cmax, colormap: ly.colormap };
          if (ly.kind === 'series')  return { kind: 'series', mode: ly.mode, name: ly.name, color: ly.color, x: ly.x, y: ly.y };
          return { ...ly };
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
    if (is3D) {
      // 3-D fit pulls the data bbox from Composite3DPlot's imperative
      // handle (Composite3DPlot reports it via onBBox each rebuild;
      // bbox3d is the cached copy). Each axis is reset independently
      // so "X only" leaves Y/Z lims untouched.
      const b = bbox3d || (threeRef.current?.getBBox?.() ?? null);
      if (!b) { setFitOpen(false); return; }
      const next = {
        x: viewport?.x?.slice() || [-1, 1],
        y: viewport?.y?.slice() || [-1, 1],
        z: viewport?.z?.slice() || [-1, 1],
      };
      if (axisMode === 'both' || axisMode === 'x') next.x = [b.xMin, b.xMax];
      if (axisMode === 'both' || axisMode === 'y') next.y = [b.yMin, b.yMax];
      if (axisMode === 'both' || axisMode === 'z') next.z = [b.zMin, b.zMax];
      setViewport(next);
      setFitOpen(false);
      return;
    }
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
    if (isComposite && isHeatmap && !hasSeries) {
      // Pure-heatmap composite: data extent is figure.xRange/yRange. Under
      // a log axis the natural extent straddles zero (cellH/2 padding) and
      // would silently flip back to linear. Clamp lo bound to half-cell.
      const xLogNow = figure.xscale === 'log';
      const yLogNow = figure.yscale === 'log';
      const next = { x: viewport.x.slice(), y: viewport.y.slice() };
      if (axisMode === 'both' || axisMode === 'x') {
        if (xLogNow) {
          const fullCols = heatmapLayer.originalCols || 1;
          const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
          const lo = Math.max(cellW * 0.5, 1e-6);
          next.x = [lo, Math.max(lo * 10, figure.xRange[1])];
        } else {
          next.x = figure.xRange.slice();
        }
      }
      if (axisMode === 'both' || axisMode === 'y') {
        if (yLogNow) {
          const fullRows = heatmapLayer.originalRows || 1;
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
    if (isComposite) {
      // Series composite (with or without heatmap underlay): fit to selected
      // series. computeFitViewport accepts the series list — layer.x/y match
      // the legacy series shape closely enough.
      if (seriesLayers.length === 0) { setFitOpen(false); return; }
      setViewport(computeFitViewport(seriesLayers, mode, axisMode, viewport, figDefault));
      setFitOpen(false);
      return;
    }
    if (!figure.series) return;
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
              {isSubplot
                ? `subplot ${figure.grid[0]}×${figure.grid[1]} · ${figure.cells.length} axes`
                : isPolar
                  ? `${figure.series?.length ?? 0} series · ${(figure.series || []).reduce((s, x) => s + (x.theta?.length ?? 0), 0)} points`
                  : isHeatmap
                    ? `${heatmapLayer.z?.length ?? 0} × ${heatmapLayer.z?.[0]?.length ?? 0} cells · range [${Number(heatmapLayer.cmin).toPrecision(3)} … ${Number(heatmapLayer.cmax).toPrecision(3)}]${hasSeries ? ` · ${seriesLayers.length} overlay${seriesLayers.length === 1 ? '' : 's'}` : ''}`
                    : `${seriesLayers.length} series · ${seriesLayers.reduce((s, x) => s + (x.x?.length ?? 0), 0)} points`}
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
            {fitOpen && (is3D ? (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => {
                    if (bbox3d) {
                      setViewport({
                        x: [bbox3d.xMin, bbox3d.xMax],
                        y: [bbox3d.yMin, bbox3d.yMax],
                        z: [bbox3d.zMin, bbox3d.zMax],
                      });
                    }
                    setFitOpen(false);
                  }}>reset to data extent</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">data extent</div>
                  <button onClick={() => applyFit('all', 'both')}>all axes</button>
                  <button onClick={() => applyFit('all', 'x')}>X only</button>
                  <button onClick={() => applyFit('all', 'y')}>Y only</button>
                  <button onClick={() => applyFit('all', 'z')}>Z only</button>
                </div>
              </div>
            ) : isHeatmap ? (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <button onClick={() => { setViewport(figDefault); setColorOverride(null); setFitOpen(false); }}>reset to default</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">data extent</div>
                  <button onClick={() => applyFit('all', 'both')}>both axes</button>
                  <button onClick={() => applyFit('all', 'x')}>X only</button>
                  <button onClick={() => applyFit('all', 'y')}>Y only</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">colors</div>
                  <button
                    onClick={() => { fitColorsToVisible(); setFitOpen(false); }}
                    disabled={!engine || typeof engine.getFigureTile !== 'function'
                              || !heatmapLayer || heatmapLayer._figId < 0}>
                    fit to visible
                  </button>
                  <button
                    onClick={() => { setColorOverride(null); setFitOpen(false); }}
                    disabled={!colorOverride}>
                    reset colors
                  </button>
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
                {seriesLayers.length > 1 && (
                  <div className="fw-pop-section">
                    <div className="fw-pop-head">single curve</div>
                    {seriesLayers.map((s) => (
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

          {/* Range inputs (X / Y / Z / r) live in the footer status
              bar now — see below. The toolbar keeps fit / grid / log /
              save / export buttons only. */}

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
                <select
                  className="ve-btn fw-cmap-select"
                  value={colormapOverride ?? heatmapLayer.colormap ?? 'parula'}
                  onChange={(e) => {
                    const v = e.target.value;
                    setColormapOverride(v === (heatmapLayer.colormap ?? 'parula') ? null : v);
                  }}
                  title="Colormap (overrides script-level colormap())">
                  {['parula', 'jet', 'hot', 'cool', 'gray', 'bone', 'copper',
                    'spring', 'summer', 'autumn', 'winter', 'hsv', 'viridis']
                    .map((cm) => <option key={cm} value={cm}>{cm}</option>)}
                </select>
              </>
            )}
            {/* Legend toggle hidden for pure heatmap (colorbar IS the legend),
                shown when at least one series layer exists or the figure is
                a legacy line/polar shape. */}
            {!is3D && (hasSeries || (!isHeatmap && !isComposite)) && (
              <button className={`ve-btn ${showLegend ? 'is-active' : ''}`} onClick={() => setShowLegend((g) => !g)}>legend</button>
            )}
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
                  <button
                    onClick={() => { exportSvg(); setSaveOpen(false); }}
                    disabled={is3D}
                    title={is3D ? 'SVG export not available for 3-D figures (WebGL geometry has no vector form). Use PNG.' : ''}>
                    SVG (vector){is3D ? ' · n/a for 3-D' : ''}
                  </button>
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
              colorOverride, setColorOverride,
              colormapOverride,
              // 3-D specific — Composite3DPlot ignores these for non-3-D.
              // Skip the override on the very first render when viewport
              // is still the [-1,1] placeholder cube (otherwise computeBBox
              // would clamp data to the placeholder, onBBox would echo it
              // back, and the auto-fill loop would deadlock at -1 / 1).
              viewport3d: (is3D && !isPlaceholder3D(viewport)) ? viewport : null,
              onBBox: is3D ? onComposite3DBBox : null,
            }, threeRef)}
            {showLegend && (() => {
              const list = isComposite
                ? seriesLayers
                : (Array.isArray(figure.series) ? figure.series : []);
              if (list.length === 0) return null;
              return (
                <div className="fw-legend">
                  {list.map((s) => (
                    <div key={s.name} className="fw-legend-item">
                      <i style={{ background: s.color }} />
                      <span>{s.name}</span>
                    </div>
                  ))}
                </div>
              );
            })()}
          </div>
        </div>

        {/* Range-input row — was in the toolbar, moved to the footer
            so it sits next to the live viewport readout. 3-D figures
            get a Z row in addition to X / Y. */}
        {!isSubplot && (
          <div className="fw-range-row">
            {isPolar ? (
              <div className="ve-tools-group fw-range-group">
                <span className="ve-label">r</span>
                <NumberInput value={viewport.r[0]} onCommit={(n) => setViewport({ r: [n, viewport.r[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.r[1]} onCommit={(n) => setViewport({ r: [viewport.r[0], n] })} />
              </div>
            ) : is3D ? (
              <div className="ve-tools-group fw-range-group">
                <span className="ve-label">x</span>
                <NumberInput value={viewport.x[0]} onCommit={(n) => setViewport({ ...viewport, x: [n, viewport.x[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.x[1]} onCommit={(n) => setViewport({ ...viewport, x: [viewport.x[0], n] })} />
                <span className="ve-label" style={{ marginLeft: 6 }}>y</span>
                <NumberInput value={viewport.y[0]} onCommit={(n) => setViewport({ ...viewport, y: [n, viewport.y[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.y[1]} onCommit={(n) => setViewport({ ...viewport, y: [viewport.y[0], n] })} />
                <span className="ve-label" style={{ marginLeft: 6 }}>z</span>
                <NumberInput value={viewport.z[0]} onCommit={(n) => setViewport({ ...viewport, z: [n, viewport.z[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.z[1]} onCommit={(n) => setViewport({ ...viewport, z: [viewport.z[0], n] })} />
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
          </div>
        )}

        <div className="fw-status">
          {isSubplot ? (
            <span>{figure.cells.length} axes · per-cell pan/zoom</span>
          ) : isPolar ? (
            <span>r ∈ [{fmtVp(viewport.r[0])}, {fmtVp(viewport.r[1])}]</span>
          ) : is3D ? (
            <>
              <span>x ∈ [{fmtVp(viewport.x[0])}, {fmtVp(viewport.x[1])}]</span>
              <span className="ve-sep" />
              <span>y ∈ [{fmtVp(viewport.y[0])}, {fmtVp(viewport.y[1])}]</span>
              <span className="ve-sep" />
              <span>z ∈ [{fmtVp(viewport.z[0])}, {fmtVp(viewport.z[1])}]</span>
            </>
          ) : (
            <>
              <span>x ∈ [{fmtVp(viewport.x[0])}, {fmtVp(viewport.x[1])}]</span>
              <span className="ve-sep" />
              <span>y ∈ [{fmtVp(viewport.y[0])}, {fmtVp(viewport.y[1])}]</span>
            </>
          )}
          <span className="ve-spacer" />
          <span>{isPolar ? 'drag · zoom rMax' : is3D ? 'drag · orbit · wheel · dolly' : 'drag · pan'}</span>
          {!is3D && (
            <>
              <span className="ve-sep" />
              <span>{isPolar ? 'wheel · zoom' : 'wheel · zoom xy · ⌃ x · ⇧ y'}</span>
            </>
          )}
          <span className="ve-sep" />
          <span>dbl-click · reset</span>
          <span className="ve-sep" />
          <span>0 · reset · Esc · close</span>
        </div>
      </div>
    </div>
  );
}
