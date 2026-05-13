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

/** Per-series rows for fit ▾. When `rows.length` is over `threshold`
 *  (default 5), wraps the list in a side-opening submenu so the
 *  popover doesn't grow tall. Below the threshold, renders inline.
 *  Each row is a JSX element produced by the caller.
 *
 *  Submenu uses position:fixed with coords computed from the trigger
 *  button's getBoundingClientRect — bypasses the parent .fw-pop's
 *  overflow:auto (which would otherwise clip the absolute-positioned
 *  child and surface a scrollbar instead of opening). */
function FwPopRowsOrSubmenu({ rows, label, threshold = 5 }) {
  const [open, setOpen] = useState(false);
  const [coords, setCoords] = useState(null);
  const triggerRef = useRef(null);
  useLayoutEffect(() => {
    if (!open) return;
    const el = triggerRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    setCoords({ left: r.right + 2, top: r.top - 4 });
  }, [open]);
  if (!Array.isArray(rows) || rows.length === 0) return null;
  if (rows.length <= threshold) {
    return <>{rows}</>;
  }
  return (
    <div className={`fw-pop-sub-wrap ${open ? 'is-open' : ''}`}
         onMouseEnter={() => setOpen(true)}
         onMouseLeave={() => setOpen(false)}>
      <button ref={triggerRef}
              className="fw-pop-sub-trigger"
              onClick={(e) => { e.stopPropagation(); setOpen((o) => !o); }}>
        <span>{label}</span>
        <span className="fw-pop-sub-arrow">▶</span>
      </button>
      {open && coords && (
        <div className="fw-pop fw-pop-sub"
             style={{ position: 'fixed', left: coords.left, top: coords.top }}>
          {rows}
        </div>
      )}
    </div>
  );
}

/** Toggle row for the display ▾ popover. Two-column grid:
 *  [ label | ✓ ]. No per-item icon (button-level icon already telegraphs
 *  the menu's purpose). No active colour tint — only the ✓ marker. */
function DisplayToggle({ label, active, disabled = false, disabledHint = '', onClick }) {
  return (
    <button className="fw-pop-toggle"
            disabled={disabled}
            title={disabled ? disabledHint : ''}
            onClick={onClick}>
      <span>{label}</span>
      <span className="fw-pop-check">{active ? '✓' : ''}</span>
    </button>
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
  // Major / minor grid state mirrors the script's. Both come from the
  // engine as independent strings ("on" / "off"); we keep local state
  // so the toolbar buttons can override without re-running the script,
  // but re-sync to the script value whenever the underlying figure
  // changes (so `grid minor` in a re-run propagates).
  //   2-D figures default to no grid (matches MATLAB).
  //   3-D figures: adapter forces grid='on' by default since a plain
  //   wireframe sits in space with no frame otherwise.
  const [showMajor, setShowMajor] = useState(figure.grid === 'on');
  const [showMinor, setShowMinor] = useState(figure.gridMinor === 'on');
  useEffect(() => { setShowMajor(figure.grid === 'on'); }, [figure.grid]);
  useEffect(() => { setShowMinor(figure.gridMinor === 'on'); }, [figure.gridMinor]);
  // Legend initial state mirrors what the script asked for: show only
  // when `legend(...)` (or legend Location) was set — MATLAB parity.
  // Toolbar toggle then lets the user override.
  const legendUserAsked = (Array.isArray(figure.legend) && figure.legend.length > 0)
                       || (figure.legendLocation && figure.legendLocation !== 'none');
  const [showLegend, setShowLegend] = useState(!!legendUserAsked);
  useEffect(() => { setShowLegend(!!legendUserAsked); },
            // eslint-disable-next-line react-hooks/exhaustive-deps
            [figure.id, figure.legend, figure.legendLocation]);
  // Log axis toggles — lifted from Heatmap so the toolbar can flip them
  // alongside grid/minor and the ПКМ menu inside the panel can mirror.
  // Initialised from figure.xscale/yscale (set by xscale('log') /
  // yscale('log')) and re-synced when those props change at script time.
  const [xLog, setXLog] = useState(figure.xscale === 'log');
  const [yLog, setYLog] = useState(figure.yscale === 'log');
  // zLog: 3-D-only. Wired through to Composite3DPlot; no script-level
  // initialiser yet (zscale('log') unimplemented), so state starts off.
  const [zLog, setZLog] = useState(false);
  useEffect(() => { setXLog(figure.xscale === 'log'); }, [figure.xscale]);
  useEffect(() => { setYLog(figure.yscale === 'log'); }, [figure.yscale]);

  // Colorbar visibility — defaults to "shown" only when the script
  // actually called colorbar() (which sets figure.colorbarLocation to a
  // non-empty placement string). Without an explicit call we leave it
  // off, matching MATLAB. Toolbar/ПКМ toggle then lets the user flip.
  const colorbarUserAsked = !!figure.colorbarLocation
                          && figure.colorbarLocation !== 'off';
  const [showColorbar, setShowColorbar] = useState(colorbarUserAsked);
  useEffect(() => { setShowColorbar(colorbarUserAsked); },
            // eslint-disable-next-line react-hooks/exhaustive-deps
            [figure.id, figure.colorbarLocation]);

  // Visibility toggles for title / xlabel / ylabel / zlabel. State is
  // local — we don't mutate figure data, so the underlying script-set
  // text survives a re-show. Initialise true: all labels visible by
  // default, matching MATLAB's "if you set it, you see it" behaviour.
  const [showTitle,  setShowTitle]  = useState(true);
  const [showXLabel, setShowXLabel] = useState(true);
  const [showYLabel, setShowYLabel] = useState(true);
  const [showZLabel, setShowZLabel] = useState(true);
  // Re-default to visible on figure-id swap so an earlier "hide" doesn't
  // leak across unrelated figures landing on the same modal.
  useEffect(() => {
    setShowTitle(true); setShowXLabel(true);
    setShowYLabel(true); setShowZLabel(true);
  }, [figure.id]);

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

  // Toggle that also auto-clamps the viewport's lo bound to a positive
  // value when entering log mode. Two paths:
  //   - heatmap: clamp to half a cell width (smallest meaningful unit
  //     that still lands inside the data grid).
  //   - line/scatter/etc: clamp to xRange[1]/1e4 (gives ~4 decades of
  //     visible range — a sane default for plot(1:1000) where the
  //     adapter's 4% padding pushed viewport.x[0] negative).
  // Without the clamp, xLogActive in CompositePlot stays false (its
  // guard is xMin > 0 && xMax > 0) and the toggle has no visual effect.
  function toggleAxisLog(axis) {
    if (axis === 'x') {
      const next = !xLog;
      if (next && (viewport.x[0] <= 0 || viewport.x[1] <= 0)) {
        let lo;
        if (isHeatmap) {
          const fullCols = heatmapLayer?.originalCols || 1;
          const cellW = (figure.xRange[1] - figure.xRange[0]) / fullCols;
          lo = Math.max(cellW * 0.5, 1e-6);
        } else {
          // Use the highest-positive data extent we know about as the
          // anchor — fall back to figure.xRange or viewport upper.
          const hi = Math.max(figure.xRange?.[1] || viewport.x[1], 1e-6);
          lo = Math.max(hi / 1e4, 1e-6);
        }
        const hiClamped = Math.max(lo * 10, figure.xRange?.[1] || viewport.x[1]);
        setViewport({ ...viewport, x: [lo, hiClamped] });
      }
      setXLog(next);
    } else {
      const next = !yLog;
      if (next && (viewport.y[0] <= 0 || viewport.y[1] <= 0)) {
        let lo;
        if (isHeatmap) {
          const fullRows = heatmapLayer?.originalRows || 1;
          const cellH = (figure.yRange[1] - figure.yRange[0]) / fullRows;
          lo = Math.max(cellH * 0.5, 1e-6);
        } else {
          const hi = Math.max(figure.yRange?.[1] || viewport.y[1], 1e-6);
          lo = Math.max(hi / 1e4, 1e-6);
        }
        const hiClamped = Math.max(lo * 10, figure.yRange?.[1] || viewport.y[1]);
        setViewport({ ...viewport, y: [lo, hiClamped] });
      }
      setYLog(next);
    }
  }
  const [fitOpen, setFitOpen]     = useState(false);
  // fitSignal fires every toolbar Fit click. SubplotGrid watches it
  // (counter-based change detection) and resets every cell's viewport
  // along the requested axis. Non-subplot figures use the legacy
  // applyFit path directly; this signal is only consumed by SubplotGrid.
  const [fitSignal, setFitSignal] = useState({ axis: '', n: 0 });
  function fitAllCells(axis) {
    setFitSignal((prev) => ({ axis, n: prev.n + 1 }));
  }
  // displayResetSignal — when toolbar Reset / display ▾ Reset fires we
  // also need SubplotGrid to drop every per-cell display override that
  // the per-cell ПКМ might have set, so the cells follow the (now-reset)
  // figure-wide toggles again.
  const [displayResetSignal, setDisplayResetSignal] = useState({ n: 0 });
  function fanDisplayReset() {
    setDisplayResetSignal((prev) => ({ n: prev.n + 1 }));
  }
  // ── Reset helpers ────────────────────────────────────────────────
  // The toolbar exposes three flavours of reset:
  //   • fit ▾ → reset    — viewport only (zoom/pan back to defaults)
  //   • display ▾ → reset — display state only (grid/log/labels back
  //                         to script defaults, legend re-synced)
  //   • 🏠 Reset toolbar  — both (viewport + display state)
  // For subplot the viewport reset is fanned out via fitSignal; the
  // display-state reset re-syncs the figure-wide show* flags and
  // propagates through props to every cell automatically.
  function displayReset() {
    setShowMajor(figure.grid === 'on');
    setShowMinor(figure.gridMinor === 'on');
    setShowTitle(true);
    setShowXLabel(true);
    setShowYLabel(true);
    setShowZLabel(true);
    setXLog(figure.xscale === 'log');
    setYLog(figure.yscale === 'log');
    setZLog(false);
    setShowLegend(!!legendUserAsked);
    setShowColorbar(colorbarUserAsked);
    // Drop per-cell ПКМ overrides too so the subplot grid follows the
    // reset figure-wide values rather than holding onto user tweaks.
    fanDisplayReset();
  }
  function viewportReset() {
    if (isSubplot) {
      fitAllCells('both');
    } else if (is3D) {
      if (bbox3d) {
        setViewport({
          x: [bbox3d.xMin, bbox3d.xMax],
          y: [bbox3d.yMin, bbox3d.yMax],
          z: [bbox3d.zMin, bbox3d.zMax],
        });
      }
    } else if (isHeatmap) {
      setViewport(figDefault);
      setColorOverride(null);
      setColormapOverride(null);
    } else {
      setViewport(figDefault);
    }
  }
  function resetAll() {
    viewportReset();
    displayReset();
  }
  const [displayOpen, setDisplayOpen] = useState(false);
  const [cmapOpen, setCmapOpen]   = useState(false);
  const [saveOpen, setSaveOpen]   = useState(false);
  const [viewOpen, setViewOpen]   = useState(false);
  const [maximized, setMaximized] = useState(false);
  const fitRef  = useRef(null);
  const displayRef = useRef(null);
  const cmapRef = useRef(null);
  const saveRef = useRef(null);
  const viewRef = useRef(null);

  // ── display-menu disabled rules ──────────────────────────────────────
  // For non-subplot figures we look at top-level fields. For subplots,
  // we scan cells — a toggle is enabled if AT LEAST one cell satisfies
  // the precondition (the toggle is global; per-cell handlers ignore
  // cells that can't apply).
  const cellsList = isSubplot && Array.isArray(figure.cells) ? figure.cells : [figure];
  const anyCellHas = (pred) => cellsList.some(pred);
  // Find the first heatmap layer ANYWHERE in the figure (top-level or
  // inside a subplot cell). Drives the toolbar Colormap ▾ visibility +
  // its "script default" anchor for marking the active palette.
  const findFirstHeatmap = () => {
    for (const c of cellsList) {
      const layers = (c && Array.isArray(c.layers)) ? c.layers : [];
      const hm = layers.find((l) => l && l.kind === 'heatmap');
      if (hm) return hm;
    }
    return null;
  };
  const anyHeatmap = findFirstHeatmap();
  const xLogEnabled = anyCellHas((c) => Array.isArray(c.xRange) && c.xRange[1] > 0);
  const yLogEnabled = anyCellHas((c) => Array.isArray(c.yRange) && c.yRange[1] > 0);
  // zLog/zLabel: 3-D only. For subplot, enabled if any cell is 3-D.
  const has3DCell = isSubplot
    ? cellsList.some((c) => c.kind === 'composite3d')
    : is3D;
  // titleEnabled — script-set titles only. The adapter substitutes a
  // default "Figure N" when the script didn't call title(); titleAuto
  // marks that case so the toggle stays disabled.
  const titleEnabled  = anyCellHas((c) => typeof c.title  === 'string' && c.title.length  > 0
                                          && !c.titleAuto);
  const xLabelEnabled = anyCellHas((c) => typeof c.xLabel === 'string' && c.xLabel.length > 0);
  const yLabelEnabled = anyCellHas((c) => typeof c.yLabel === 'string' && c.yLabel.length > 0);
  const zLabelEnabled = has3DCell
    && anyCellHas((c) => typeof c.zLabel === 'string' && c.zLabel.length > 0);
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
      if (fitRef.current     && !fitRef.current.contains(e.target))     setFitOpen(false);
      if (displayRef.current && !displayRef.current.contains(e.target)) setDisplayOpen(false);
      if (cmapRef.current    && !cmapRef.current.contains(e.target))    setCmapOpen(false);
      if (saveRef.current    && !saveRef.current.contains(e.target))    setSaveOpen(false);
      if (viewRef.current    && !viewRef.current.contains(e.target))    setViewOpen(false);
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
    const targetPx = (mmWidth / 25.4) * dpi;
    if (is3D) {
      // 3-D path now resizes the renderer's drawing buffer to the
      // target pixel count, renders once at high res, snapshots, and
      // restores. The on-screen canvas CSS size is untouched.
      const canvas = threeRef.current?.getCanvas?.();
      // We don't expose a getCanvas helper today — derive scale from
      // the live CSS width via the imperative handle's own caller-
      // visible size hint. Fall back to scale=2 if unknown.
      const ctxW = (threeRef.current?.getCanvasCssSize?.()?.width)
                    || (typeof window !== 'undefined' && window.innerWidth)
                    || 800;
      const scale = Math.max(1, targetPx / ctxW);
      const url = threeRef.current?.getCanvasDataURL?.(scale);
      if (!url) return;
      utilDownloadBlob(dataUrlToBlob(url), `figure_${figure.id}_${mmWidth}mm.png`);
      return;
    }
    const g = gatherFigureSvg();
    if (!g) return;
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
          {/* 🏠 Reset — full reset of viewport AND display state. For
              subplot it fans out to every cell. Standalone toolbar
              button (not a popover) for one-click access. */}
          <button className="ve-btn"
                  onClick={resetAll}
                  title="Reset viewport + display state"
                  data-fw-reset="all">
            <svg width="11" height="11" viewBox="0 0 12 12">
              <path d="M1 6l5-5 5 5 M2 5v6h8V5"
                    stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinejoin="round"/>
            </svg>
            reset
          </button>
          {/* fit ▾ — always shown, applies to EVERY cell in subplot mode.
              Per-series rows live in the right-click menu only; the
              toolbar version is global by design. */}
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
                  <button onClick={() => { viewportReset(); setFitOpen(false); }}>reset</button>
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">data extent</div>
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('both'); else applyFit('all', 'both');
                    setFitOpen(false);
                  }}>both axes</button>
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('x'); else applyFit('all', 'x');
                    setFitOpen(false);
                  }}>X only</button>
                  <button onClick={() => {
                    if (isSubplot) fitAllCells('y'); else applyFit('all', 'y');
                    setFitOpen(false);
                  }}>Y only</button>
                  <button
                    disabled={!is3D && !isSubplot}
                    title={(!is3D && !isSubplot) ? 'no Z axis on this figure' : ''}
                    onClick={() => {
                      if (isSubplot) fitAllCells('z'); else applyFit('all', 'z');
                      setFitOpen(false);
                    }}>Z only</button>
                </div>
                {/* Heatmap colour-fit affordance only for non-subplot
                    heatmap figures — the toolbar Fit Z covers the per-
                    cell colour reset isn't a user-facing concept yet. */}
                {!isSubplot && isHeatmap && (
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
                )}
              </div>
            )}
          </div>

          {/* View presets — 3-D only. Six standard MATLAB camera
              orientations: top (XY plane down), bottom (XY plane up),
              front (XZ), back, left (YZ), right, plus iso (default
              -37.5° / 30°). Each calls the Composite3DPlot.setView
              imperative method without re-emitting the figure JSON,
              so script-set view(az, el) is preserved as the "iso"
              fallback. */}
          {is3D && !isSubplot && (
            <div className="ve-tools-group" ref={viewRef}>
              <button className="ve-btn"
                      onClick={() => setViewOpen((o) => !o)}
                      title="Camera presets">
                view ▾
              </button>
              {viewOpen && (
                <div className="fw-pop">
                  <div className="fw-pop-section">
                    <div className="fw-pop-head">presets</div>
                    <button data-fw-view="iso" onClick={() => {
                      threeRef.current?.setView?.(-37.5, 30);
                      setViewOpen(false);
                    }}>iso (default)</button>
                    <button data-fw-view="top" onClick={() => {
                      threeRef.current?.setView?.(0, 90);
                      setViewOpen(false);
                    }}>top (XY)</button>
                    <button data-fw-view="bottom" onClick={() => {
                      threeRef.current?.setView?.(0, -90);
                      setViewOpen(false);
                    }}>bottom (XY-)</button>
                    <button data-fw-view="front" onClick={() => {
                      threeRef.current?.setView?.(0, 0);
                      setViewOpen(false);
                    }}>front (XZ)</button>
                    <button data-fw-view="back" onClick={() => {
                      threeRef.current?.setView?.(180, 0);
                      setViewOpen(false);
                    }}>back (XZ-)</button>
                    <button data-fw-view="right" onClick={() => {
                      threeRef.current?.setView?.(90, 0);
                      setViewOpen(false);
                    }}>right (YZ)</button>
                    <button data-fw-view="left" onClick={() => {
                      threeRef.current?.setView?.(-90, 0);
                      setViewOpen(false);
                    }}>left (YZ-)</button>
                  </div>
                </div>
              )}
            </div>
          )}

          {/* Range inputs (X / Y / Z / r) live in the footer status
              bar now — see below. The toolbar keeps fit / grid / log /
              save / export buttons only. */}

          <div className="ve-tools-group" ref={displayRef}>
            <button className="ve-btn"
                    onClick={() => setDisplayOpen((o) => !o)}
                    title="Toggle grid / scale / labels visibility">
              <svg width="11" height="11" viewBox="0 0 12 12">
                <path d="M0 4h12 M0 8h12 M4 0v12 M8 0v12"
                      stroke="currentColor" strokeWidth="1.2" fill="none"/>
              </svg>
              display ▾
            </button>
            {displayOpen && (
              <div className="fw-pop">
                <div className="fw-pop-section">
                  <div className="fw-pop-head">grid</div>
                  <DisplayToggle label="grid"  active={showMajor}
                                 onClick={() => setShowMajor((g) => !g)} />
                  <DisplayToggle label="minor" active={showMinor}
                                 onClick={() => setShowMinor((g) => !g)} />
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">scale</div>
                  <DisplayToggle label="xlog" active={xLog}
                                 disabled={!xLogEnabled}
                                 disabledHint="X range has no positive max — log scale undefined"
                                 onClick={() => toggleAxisLog('x')} />
                  <DisplayToggle label="ylog" active={yLog}
                                 disabled={!yLogEnabled}
                                 disabledHint="Y range has no positive max — log scale undefined"
                                 onClick={() => toggleAxisLog('y')} />
                  {/* zlog stays enabled even on 2-D figures: it's a no-op
                      there, but the user explicitly asked we don't gate
                      Z controls behind a 3-D check in the toolbar. */}
                  <DisplayToggle label="zlog" active={zLog}
                                 onClick={() => setZLog((v) => !v)} />
                </div>
                <div className="fw-pop-section">
                  <div className="fw-pop-head">labels</div>
                  <DisplayToggle label="title" active={showTitle}
                                 disabled={!titleEnabled}
                                 disabledHint="not set"
                                 onClick={() => setShowTitle((v) => !v)} />
                  <DisplayToggle label="xlabel" active={showXLabel}
                                 disabled={!xLabelEnabled}
                                 disabledHint="not set"
                                 onClick={() => setShowXLabel((v) => !v)} />
                  <DisplayToggle label="ylabel" active={showYLabel}
                                 disabled={!yLabelEnabled}
                                 disabledHint="not set"
                                 onClick={() => setShowYLabel((v) => !v)} />
                  {/* Same: zlabel stays enabled even on 2-D figures. */}
                  <DisplayToggle label="zlabel" active={showZLabel}
                                 onClick={() => setShowZLabel((v) => !v)} />
                  <DisplayToggle label="legend" active={showLegend}
                                 onClick={() => setShowLegend((v) => !v)} />
                  <DisplayToggle label="colorbar" active={showColorbar}
                                 onClick={() => setShowColorbar((v) => !v)} />
                </div>
                <div className="fw-pop-section">
                  <button onClick={() => { displayReset(); setDisplayOpen(false); }}>reset</button>
                </div>
              </div>
            )}
          {/* colormap ▾ — figure-wide picker. Visible whenever there's
              ANY heatmap layer in the figure (top-level OR inside a
              subplot cell). Click sets the figure-wide override; any
              per-cell ПКМ overrides are dropped (toolbar wins). */}
          {anyHeatmap && (
            <div className="ve-tools-group" ref={cmapRef}>
              <button className="ve-btn"
                      onClick={() => setCmapOpen((o) => !o)}
                      title="Colormap (figure-wide; overrides per-cell picks)">
                <svg width="11" height="11" viewBox="0 0 12 12">
                  <rect x="1" y="3" width="10" height="6" fill="none"
                        stroke="currentColor" strokeWidth="1"/>
                  <rect x="1" y="3" width="2" height="6" fill="currentColor" opacity="0.25"/>
                  <rect x="3" y="3" width="2" height="6" fill="currentColor" opacity="0.5"/>
                  <rect x="5" y="3" width="2" height="6" fill="currentColor" opacity="0.75"/>
                  <rect x="7" y="3" width="2" height="6" fill="currentColor"/>
                  <rect x="9" y="3" width="2" height="6" fill="currentColor" opacity="0.5"/>
                </svg>
                colormap ▾
              </button>
              {cmapOpen && (
                <div className="fw-pop">
                  <div className="fw-pop-section">
                    <div className="fw-pop-head">palette</div>
                    {['parula', 'jet', 'hot', 'cool', 'gray', 'bone', 'copper',
                      'spring', 'summer', 'autumn', 'winter', 'hsv', 'viridis']
                      .map((cm) => {
                        const scriptDefault = anyHeatmap.colormap || 'parula';
                        const active = (colormapOverride || scriptDefault) === cm;
                        return (
                          <button key={cm}
                                  className="fw-pop-toggle"
                                  onClick={() => {
                                    setColormapOverride(cm === scriptDefault ? null : cm);
                                    setCmapOpen(false);
                                  }}>
                            <span>{cm}</span>
                            <span className="fw-pop-check">{active ? '✓' : ''}</span>
                          </button>
                        );
                      })}
                  </div>
                </div>
              )}
            </div>
          )}
            {/* Legend toggle hidden for pure heatmap (colorbar IS the legend),
                shown when at least one series layer exists or the figure is
                a legacy line/polar shape. */}
            {/* Legend toolbar toggle — shown only when the script
                actually asked for a legend (so the IDE doesn't dangle
                a button that toggles nothing on a plain plot()). 3-D
                figures hide it because the WebGL renderer doesn't
                draw a legend block. */}
            {!is3D && legendUserAsked && (hasSeries || (!isHeatmap && !isComposite)) && (
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
              xLog, yLog, zLog,
              setXLog, setYLog,
              colorOverride, setColorOverride,
              colormapOverride, setColormapOverride,
              // showLegend gates BOTH the CompositePlot SVG-internal
              // legend block and the (now removed) HTML overlay. One
              // legend, controlled by the toolbar toggle.
              showLegend,
              // Visibility flags from display ▾. CompositePlot /
              // Composite3DPlot / SubplotGrid honour these by skipping
              // the corresponding <text> render path. State lives here
              // (not in figure JSON) so a script re-run doesn't reset.
              showTitle, showXLabel, showYLabel, showZLabel,
              showColorbar,
              // Display setters — passed so the right-click menu inside
              // CompositePlot can surface a Display submenu that mirrors
              // the toolbar's display ▾ state.
              setShowMajor, setShowMinor,
              setShowTitle, setShowXLabel, setShowYLabel,
              setShowLegend, setShowColorbar,
              // Top-level Reset + Save/Export bridge for the context
              // menu. ПКМ surfaces these as a 🏠 Reset row + Save/Export
              // submenu. Handlers run with no extra wrapping — the menu
              // closes automatically after the click via ContextMenu's
              // own onClose path.
              onResetAll: resetAll,
              onExportSvg: exportSvg,
              onExportPng2x: () => exportPng(2),
              onExportPngPrint85:  () => exportPngPrint(85),
              onExportPngPrint170: () => exportPngPrint(170),
              onExportPngPrint210: () => exportPngPrint(210),
              onExportCsv: exportCsv,
              onExportTsv: exportTsv,
              onExportJson: exportJson,
              // fitSignal — incrementing counter consumed by SubplotGrid
              // to fit each cell's viewport when the toolbar Fit X/Y/Z
              // is clicked on a subplot figure.
              fitSignal,
              // displayResetSignal — clears per-cell ПКМ display overrides
              // when the toolbar Reset (or display ▾ reset) fires.
              displayResetSignal,
              // 3-D specific — Composite3DPlot ignores these for non-3-D.
              // Skip the override on the very first render when viewport
              // is still the [-1,1] placeholder cube (otherwise computeBBox
              // would clamp data to the placeholder, onBBox would echo it
              // back, and the auto-fill loop would deadlock at -1 / 1).
              viewport3d: (is3D && !isPlaceholder3D(viewport)) ? viewport : null,
              onBBox: is3D ? onComposite3DBBox : null,
            }, threeRef)}
            {/* The HTML-overlay legend that lived here used to double-
                draw on top of CompositePlot's SVG legend whenever the
                script called legend(...). Removed — CompositePlot's
                internal block (gated on showLegend prop) is the single
                source of truth now. */}
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
                <NumberInput value={viewport.r[0]}
                  onCommit={(n) => setViewport({ ...viewport, r: [n, viewport.r[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={viewport.r[1]}
                  onCommit={(n) => setViewport({ ...viewport, r: [viewport.r[0], n] })} />
                {/* θ in degrees, MATLAB convention. defaultPolarViewport
                    fills [0, 360] when the script didn't set thetalim;
                    user can narrow the sweep here for a pie-wedge plot. */}
                <span className="ve-label" style={{ marginLeft: 6 }}>θ°</span>
                <NumberInput value={(viewport.theta || [0, 360])[0]}
                  onCommit={(n) => setViewport({ ...viewport,
                    theta: [n, (viewport.theta || [0, 360])[1]] })} />
                <span className="fw-range-sep">→</span>
                <NumberInput value={(viewport.theta || [0, 360])[1]}
                  onCommit={(n) => setViewport({ ...viewport,
                    theta: [(viewport.theta || [0, 360])[0], n] })} />
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
            <>
              <span>r ∈ [{fmtVp(viewport.r[0])}, {fmtVp(viewport.r[1])}]</span>
              {Array.isArray(viewport.theta) && (
                <>
                  <span className="ve-sep" />
                  <span>θ ∈ [{fmtVp(viewport.theta[0])}°, {fmtVp(viewport.theta[1])}°]</span>
                </>
              )}
            </>
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
