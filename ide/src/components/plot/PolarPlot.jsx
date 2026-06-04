/**
 * Polar plot — `polarplot(theta, rho)`. Concentric-circle radial grid, radial
 * spokes every 30°, optional `thetaDir` (clockwise/counterclockwise) and
 * `thetaZeroLocation` (top/bottom/left/right) from the engine config.
 *
 * Figure shape (built by adapters.adaptFigure when cfg.polar=true):
 *   {
 *     id, title,
 *     thetaDir,          // 'counterclockwise' (default) | 'clockwise'
 *     thetaZeroLocation, // 'right' (default) | 'top' | 'left' | 'bottom'
 *     rlim,              // [rmin, rmax] — auto-computed when undefined
 *     series: [{ name, theta:Number[], rho:Number[], color, width? }],
 *   }
 *
 * Viewport (controlled from the parent so fit / range inputs can mutate it):
 *   { r: [rMin, rMax] }
 */
import { useEffect, useMemo, useRef, useState } from 'react';
import ContextMenu from '../ui/ContextMenu';
import { fitCellViewport, exportSvgNode, exportPngNode, exportPngForPrint, previewStride } from './plotUtils';
import GLChart from './glplot/GLChart';
import { isWebGL2Available } from './glplot/glcontext';
import { makeProjection } from './glplot/projection';
import {
  glRoutable, glFlagFromStorage, selectGLPolarSeries, SCATTER_MIN,
} from './glplot/route';
import { renderGLPreviewDataURL } from './glplot/previewRaster';

const PALETTE = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a',
                 '#d4a5e6', '#f2a37e', '#6fcfbf'];

function thetaZeroOffset(loc) {
  switch (loc) {
    case 'top':    return Math.PI / 2;
    case 'left':   return Math.PI;
    case 'bottom': return -Math.PI / 2;
    default:       return 0;
  }
}

/**
 * Pick a "nice" step for splitting a range into ~target divisions, using the
 * 1 / 2 / 2.5 / 5 / 10 multiplier set. The 2.5 step is what produces the
 * 0.25 / 0.5 / 0.75 / 1 series readers expect on a polar grid.
 */
function niceStep(range, target = 4) {
  if (range <= 0) return 1;
  const rough = range / target;
  const pow = Math.pow(10, Math.floor(Math.log10(rough)));
  const norm = rough / pow;
  if (norm < 1.5)  return 1   * pow;
  if (norm < 2.25) return 2   * pow;
  if (norm < 3.75) return 2.5 * pow;
  if (norm < 7)    return 5   * pow;
  return 10 * pow;
}

/** Round `v` upwards to the nearest "nice" rMax — 0.25 / 0.5 / 1 / 2.5 / 5 / 10 / … */
export function nicePolarMax(v) {
  if (!Number.isFinite(v) || v <= 0) return 1;
  const step = niceStep(v, 4);
  return Math.ceil(v / step) * step;
}

/** Default viewport for a figure — polar uses {r:[…], theta:[…]},
 *  cartesian {x,y}. theta is in DEGREES so the FigureWindow inputs
 *  show user-friendly numbers (0..360); the renderer converts to
 *  radians at draw time. */
export function defaultPolarViewport(figure) {
  const r = (Array.isArray(figure?.rlim) && figure.rlim.length === 2)
    ? figure.rlim.slice()
    : (() => {
        let m = 0;
        figure?.series?.forEach((s) => s.rho?.forEach((v) => {
          if (Number.isFinite(v) && Math.abs(v) > m) m = Math.abs(v);
        }));
        return [0, nicePolarMax(m || 1)];
      })();
  const theta = (Array.isArray(figure?.thetalim) && figure.thetalim.length === 2)
    ? figure.thetalim.slice()
    : [0, 360];
  return { r, theta };
}

export default function PolarPlot({
  figure, width, height,
  viewport, setViewport,
  major = true,
  minor = true,
  // Per-axis polar grid props (MATLAB RGrid / ThetaGrid). When the
  // parent (FigureWindow) wires them, they take precedence over the
  // combined `major` flag. Default (undefined) → fall back to `major`
  // so preview cards and standalone callers keep current behaviour.
  rGrid: rGridProp,
  thetaGrid: thetaGridProp,
  // Per-axis polar MINOR-grid props (MATLAB RMinorGrid / ThetaMinorGrid).
  // Same per-axis-prop-wins rule as the major pair above; absent →
  // fall back to the combined `minor` flag.
  rMinor: rMinorProp,
  thetaMinor: thetaMinorProp,
  // ПКМ Grid ▶ setters — when provided (modal context), the right-
  // click menu surfaces a polar-specialised Grid submenu mirroring
  // the toolbar grid ▾ Polar section. Absent → submenu omitted.
  setShowMajor = null,
  setShowMinor = null,
  setRGrid     = null,
  setThetaGrid = null,
  setRMinor     = null,
  setThetaMinor = null,
  // Decoration props — title / legend visibility + legend Location.
  // PolarPlot now mirrors CompositePlot's Decoration ▶ submenu so the
  // ПКМ surface is symmetric across plot kinds. Defaults: title shown
  // when figure.title is set; legend hidden unless the script called
  // legend(...) (figure.legend non-empty OR legendLocation set).
  showTitle    = true,
  showLegend   = true,
  legendLocation: legendLocationProp,
  setShowTitle      = null,
  setShowLegend     = null,
  setLegendLocation = null,
  // Top-level Reset + figure-wide displayReset (matches CompositePlot's
  // ПКМ bridge). FigureWindow wires `resetAll` / `displayReset`.
  onResetAll     = null,
  onDisplayReset = null,
  fontScale = 1,
  interactive = true,
}) {
  // Resolve per-axis grid: per-axis prop wins, otherwise combined.
  const rGridOn     = (rGridProp     !== undefined) ? !!rGridProp     : !!major;
  const thetaGridOn = (thetaGridProp !== undefined) ? !!thetaGridProp : !!major;
  const rMinorOn     = (rMinorProp     !== undefined) ? !!rMinorProp     : !!minor;
  const thetaMinorOn = (thetaMinorProp !== undefined) ? !!thetaMinorProp : !!minor;
  const svgRef  = useRef(null);
  const dragRef = useRef(null);
  const [ctxMenu, setCtxMenu] = useState(null);
  const dirSign = figure.thetaDir === 'clockwise' ? -1 : 1;
  const zero    = thetaZeroOffset(figure.thetaZeroLocation);

  // Resolve viewport: prefer controlled prop, else fall back to figure.rlim,
  // else auto from data extent.
  const fallback = useMemo(() => defaultPolarViewport(figure), [figure]);
  const vp = (viewport && Array.isArray(viewport.r) && viewport.r.length === 2)
    ? {
        r: viewport.r,
        theta: (Array.isArray(viewport.theta) && viewport.theta.length === 2)
          ? viewport.theta : (fallback.theta || [0, 360]),
      }
    : fallback;
  const [rMin, rMax] = vp.r;
  // thetalim in DEGREES — convert to radians for trig.
  const [thMinDeg, thMaxDeg] = vp.theta || [0, 360];
  const thMin = (thMinDeg * Math.PI) / 180;
  const thMax = (thMaxDeg * Math.PI) / 180;
  const isFullSweep = Math.abs((thMaxDeg - thMinDeg) - 360) < 1e-6;

  // padTop reserves space for the title strip — drop the reserve
  // when title is either unset OR toggled off via Decoration ▶, so
  // the polar disk gets the freed pixels back.
  const padTop = ((figure.title && showTitle) ? 28 : 12) * fontScale;
  const padBot = 12 * fontScale;
  const padX   = 12 * fontScale;
  const cx = width / 2;
  const cy = padTop + (height - padTop - padBot) / 2;
  const radius = Math.max(20, Math.min(width / 2 - padX - 30, (height - padTop - padBot) / 2 - 28));

  const span = (rMax - rMin) || 1;
  const rScale = (rho) => ((rho - rMin) / span) * radius;

  // ── WebGL overlay (polar) — large line / scatter series draw on GL in
  // SCREEN space (polarToScreen → exactly the SVG's pixels), so it lands on the
  // grid. Default-on (opt out 'numkit.plot.gl'=0). The interactive window draws
  // a live canvas; preview cards rasterize the SAME series to an SVG <image>
  // (previewRaster, one shared context) so they match the window. Small series
  // and bubble/compass/bar/rose stay on SVG. polarscatter is the big win.
  const glUsable = glRoutable(
    glFlagFromStorage(typeof localStorage !== 'undefined' ? localStorage : null),
    isWebGL2Available(),
  );
  const glPolar = useMemo(() => {
    if (!glUsable || !Array.isArray(figure.series)) return { routed: new Set(), series: [] };
    const resolved = figure.series.map((s, i) => ({
      ...s, color: s.color || PALETTE[i % PALETTE.length],
    }));
    return selectGLPolarSeries(resolved, { cx, cy, radius, rMin, rMax, zero, dirSign },
      { lineMin: 50000, scatterMin: SCATTER_MIN });
  }, [glUsable, figure.series, cx, cy, radius, rMin, rMax, zero, dirSign]);
  const glActive = glPolar.series.length > 0;
  const glLive = glActive && interactive;        // live GL canvas overlay
  const glPreview = glActive && !interactive;    // rasterize to an SVG <image>
  // Polar data is already in viewBox px (y down) → screen→clip with y flipped.
  const glProj = useMemo(() => makeProjection({
    xMin: 0, xMax: width, yMin: 0, yMax: height, yRev: true,
  }), [width, height]);
  // Clip the overlay to the polar disc (like the SVG clipPath) so zoomed-in
  // data never spills past the outer ring. Memoized for a stable draw dep.
  const glClip = useMemo(() => [cx, cy, radius], [cx, cy, radius]);
  const glDpr = typeof window !== 'undefined' ? (window.devicePixelRatio || 1) : 1;
  // Preview cards rasterize the GL series to a PNG (shared context) embedded as
  // an SVG <image> — matches the live overlay, export-safe, contextless.
  const [previewImg, setPreviewImg] = useState(null);
  useEffect(() => {
    if (!glPreview) { setPreviewImg(null); return; }
    setPreviewImg(renderGLPreviewDataURL({
      series: glPolar.series, proj: glProj, pxW: width, pxH: height, dpr: glDpr, clip: glClip,
    }));
  }, [glPreview, glPolar, glProj, width, height, glDpr, glClip]);

  // Major rings on every nice step (skipping the centre at rMin) plus minor
  // rings at step/5 spacing — matches InteractivePlot's tick split.
  // Custom `figure.rticks` overrides auto-generated majors when set.
  const { rTicksMajor, rTicksMinor } = useMemo(() => {
    // Custom rticks (MATLAB `rticks([0 1 2 3])`) — use exactly what
    // the user asked, dropping any outside [rMin, rMax]. Minor rings
    // are disabled to honour the explicit choice.
    if (Array.isArray(figure?.rticks) && figure.rticks.length > 0) {
      const custom = figure.rticks
        .filter((v) => Number.isFinite(v) && v >= rMin && v <= rMax)
        .map((v) => +Number(v).toFixed(12));
      return { rTicksMajor: custom, rTicksMinor: [] };
    }
    const step = niceStep(span, 4);
    const majorArr = [];
    const start = Math.ceil(rMin / step) * step;
    for (let v = start; v <= rMax + step * 1e-6; v += step) {
      if (Math.abs(v - rMin) > step * 1e-6) majorArr.push(+v.toFixed(12));
    }
    const minorStep = step / 5;
    const minorArr = [];
    for (let v = Math.ceil(rMin / minorStep) * minorStep; v <= rMax + minorStep * 1e-6; v += minorStep) {
      if (Math.abs(((v - start) / step) - Math.round((v - start) / step)) > 1e-6
        && Math.abs(v - rMin) > minorStep * 1e-6) {
        minorArr.push(+v.toFixed(12));
      }
    }
    return { rTicksMajor: majorArr, rTicksMinor: minorArr };
  }, [rMin, rMax, span, figure?.rticks]);

  function fmtR(v, idx) {
    // Custom rticklabels (MATLAB `rticklabels({...})`) take precedence
    // over numeric formatting when the labels array length matches
    // the active rticks array.
    if (Array.isArray(figure?.rticklabels)
     && Array.isArray(figure?.rticks)
     && figure.rticklabels.length === figure.rticks.length
     && idx != null && figure.rticklabels[idx] != null) {
      return String(figure.rticklabels[idx]);
    }
    const a = Math.abs(v);
    if (a !== 0 && (a < 1e-3 || a >= 1e5)) return v.toExponential(1);
    if (a >= 100) return v.toFixed(0);
    if (a >= 10)  return v.toFixed(1);
    if (a >= 1)   return v.toFixed(2);
    return v.toFixed(3);
  }

  /** Normalise a (theta, rho) pair for MATLAB semantics: when rho is
   *  negative, reflect to the opposite angle so it draws at
   *  (theta + π, |rho|). Returns null for non-finite inputs. */
  function normalizePolar(theta, rho) {
    if (rho == null || !Number.isFinite(rho) || theta == null || !Number.isFinite(theta)) {
      return null;
    }
    return rho < 0
      ? { theta: theta + Math.PI, rho: -rho }
      : { theta,                  rho };
  }

  function ptFor(theta, rho) {
    const r = rScale(rho);
    const a = zero + dirSign * theta;
    return [Math.cos(a) * r, -Math.sin(a) * r];
  }


  /** Build an SVG arc path on a circle of radius `r` between two
   *  angles (radians, screen coords — i.e. already through `zero +
   *  dirSign * th`). Used for rings and the outer frame when
   *  thetalim restricts the sweep. */
  function arcPath(r, a0, a1) {
    const x0 = Math.cos(a0) * r, y0 = -Math.sin(a0) * r;
    const x1 = Math.cos(a1) * r, y1 = -Math.sin(a1) * r;
    const sweep = a1 - a0;
    const large = Math.abs(sweep) > Math.PI ? 1 : 0;
    // SVG sweep-flag: 1 = ccw (positive direction in math), 0 = cw.
    // Our world maps math-positive theta to negative screen-y, so a
    // CCW math angle increase is a CW screen angle increase →
    // sweep-flag 0.
    const sf = sweep > 0 ? 0 : 1;
    return `M${x0.toFixed(2)},${y0.toFixed(2)} `
         + `A${r},${r} 0 ${large} ${sf} ${x1.toFixed(2)},${y1.toFixed(2)}`;
  }

  /** Pie-wedge path from origin → arc → back to origin. Used as the
   *  series clip-path when thetalim is set. */
  function wedgePath(r, a0, a1) {
    const x0 = Math.cos(a0) * r, y0 = -Math.sin(a0) * r;
    const x1 = Math.cos(a1) * r, y1 = -Math.sin(a1) * r;
    const sweep = a1 - a0;
    const large = Math.abs(sweep) > Math.PI ? 1 : 0;
    const sf = sweep > 0 ? 0 : 1;
    return `M0,0 L${x0.toFixed(2)},${y0.toFixed(2)} `
         + `A${r},${r} 0 ${large} ${sf} ${x1.toFixed(2)},${y1.toFixed(2)} Z`;
  }

  // Pre-compute screen-space sweep bounds for arcs (apply `zero +
  // dirSign * theta` to user-set thMin/thMax).
  const aSweep0 = zero + dirSign * thMin;
  const aSweep1 = zero + dirSign * thMax;

  // ── interaction ───────────────────────────────────────────────────────
  function onMouseDown(e) {
    if (!interactive || !setViewport || e.button !== 0) return;
    dragRef.current = { sy: e.clientY, r0: vp.r.slice() };
    e.currentTarget.style.cursor = 'grabbing';
  }
  function onMouseMove(e) {
    if (!dragRef.current || !setViewport) return;
    const d = dragRef.current;
    // Drag-up shrinks rMax (zoom in), drag-down grows it (zoom out).
    // Sensitivity: full-modal-height drag ≈ 2× change.
    const factor = Math.exp((e.clientY - d.sy) / Math.max(150, height));
    const lo = d.r0[0];
    const hi = d.r0[1];
    // Preserve theta — drag-zoom only touches r.
    setViewport({ ...vp, r: [lo, lo + (hi - lo) * factor] });
  }
  function onMouseUp(e) {
    dragRef.current = null;
    if (e.currentTarget) e.currentTarget.style.cursor = 'grab';
  }
  function onMouseLeave(e) { onMouseUp(e); }
  function onDblClick() {
    if (!interactive || !setViewport) return;
    setViewport(defaultPolarViewport(figure));
  }
  function onContextMenu(e) {
    if (!interactive) return;
    e.preventDefault();
    setCtxMenu({ x: e.clientX, y: e.clientY });
  }
  // Per-series fit (ПКМ "Fit single curve r") stays a data-scan
  // analogous to CompositePlot.applyFitSeries — scans ONE series's
  // rho for the tightest possible r-extent. Different scope from
  // the "Fit R" button (which is "fit cell to all data") so kept
  // as its own function. seriesName === 'all' is no longer used
  // here — the all-series case goes through fitR / fitAllPolar.
  function fitRho(seriesName) {
    if (!setViewport) return;
    const list = figure.series.filter((s) => s.name === seriesName);
    let m = 0;
    list?.forEach((s) => s.rho?.forEach((v) => {
      if (Number.isFinite(v) && Math.abs(v) > m) m = Math.abs(v);
    }));
    setViewport({ ...vp, r: [vp.r[0], nicePolarMax(m || 1)] });
  }
  // Cell-level fits — routed through the unified fitCellViewport so
  // toolbar fit ▾, SubplotGrid.fitSignal, and ПКМ Fit all share one
  // implementation. defaultPolarViewport encapsulates rlim/thetalim
  // priority + data-extent fallback.
  function fitR() {
    if (!setViewport) return;
    setViewport(fitCellViewport(figure, vp, 'r'));
  }
  function fitTheta() {
    if (!setViewport) return;
    setViewport(fitCellViewport(figure, vp, 'theta'));
  }
  function fitAllPolar() {
    if (!setViewport) return;
    setViewport(fitCellViewport(figure, vp, 'both'));
  }

  // ✓-prefix helper for active toggle rows — same `tag(active, label)`
  // pattern CompositePlot's ПКМ uses so toggle rows render checkmarks
  // identically across plot kinds.
  const tag = (active, label) => active ? `✓ ${label}` : label;

  // House icon — same path used by the standalone toolbar Reset button
  // and CompositePlot's ПКМ Reset row. Inlined here so PolarPlot's ПКМ
  // top row matches the rest of the IDE.
  const houseIcon = (
    <svg width="11" height="11" viewBox="0 0 12 12"
         style={{ verticalAlign: '-1px', marginRight: '6px' }}>
      <path d="M1 6l5-5 5 5 M2 5v6h8V5"
            stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinejoin="round"/>
    </svg>
  );

  // ПКМ structure parity with CompositePlot:
  //   🏠 Reset · Save / Export ▶ · Axes ▶ · ───── · Fit · series rows
  //
  // Axes ▶ — polar-specialised, so it carries ONLY the grid section
  // (master + R + θ + minor). No `visible / box`, no `reverse`, no
  // `log scale` — those would need renderer support PolarPlot doesn't
  // ship today.

  const exportItems = [
    { head: 'image · screen' },
    { label: 'SVG (vector)',
      onClick: () => exportSvgNode(svgRef.current, `figure_${figure.id}.svg`) },
    { label: 'PNG @2×',
      onClick: () => exportPngNode(svgRef.current, width, height, 2, `figure_${figure.id}.png`) },
    { head: 'image · print (300 DPI)' },
    { label: 'PNG · 1 column (85 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 85, 300, `figure_${figure.id}`) },
    { label: 'PNG · 2 columns (170 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 170, 300, `figure_${figure.id}`) },
    { label: 'PNG · A4 width (210 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 210, 300, `figure_${figure.id}`) },
  ];

  // Grid ▶ — mirrors CompositePlot's grid ПКМ split, polar-specialised.
  // Matrix layout: each axis row carries TWO buttons (major / minor).
  // Replaces the 2-row-per-axis split (`R` / `R minor`) with a single
  // row whose two buttons toggle the major / minor bit independently.
  const gridMatrixRow = (label, major, minor, setMajor, setMinor) => ({
    row: true, name: label,
    buttons: [
      { label: major ? '✓' : '', active: !!major, keepOpen: true, toggle: true,
        title: 'major grid',
        onClick: setMajor ? () => setMajor((v) => !v) : null,
        disabled: !setMajor },
      { label: minor ? '✓' : '', active: !!minor, keepOpen: true, toggle: true,
        title: 'minor grid',
        onClick: setMinor ? () => setMinor((v) => !v) : null,
        disabled: !setMinor },
    ],
  });
  const gridItems = (setShowMajor || setShowMinor || setRGrid || setThetaGrid) ? [
    ...(onDisplayReset ? [{ label: 'default', onClick: onDisplayReset },
                          { separator: true }] : []),
    { head: 'grid' },
    { rowHead: true, columns: ['maj', 'min'] },
    gridMatrixRow('all', major, minor, setShowMajor, setShowMinor),
    { head: 'Polar' },
    gridMatrixRow('R', rGridOn, rMinorOn, setRGrid, setRMinor),
    gridMatrixRow('θ', thetaGridOn, thetaMinorOn, setThetaGrid, setThetaMinor),
  ] : null;

  // Top-level Reset: prefer parent-supplied `onResetAll` (full
  // viewport + display reset, same as toolbar's 🏠 button). Fall back
  // to viewport-only when the parent didn't wire one.
  const onReset = onResetAll || (() => setViewport && setViewport(defaultPolarViewport(figure)));

  // Decoration ▶ — title + legend toggles + legend Location picker.
  // Mirrors CompositePlot's Decoration ▶ shape but with the
  // polar-relevant subset (no X/Y/Z labels — polar doesn't model
  // them; no colorbar — polar isn't a heatmap surface). Sections
  // labelled the same way for visual parity.
  const decorationItems = (setShowTitle || setShowLegend) ? [
    ...(onDisplayReset ? [{ label: 'default', onClick: onDisplayReset },
                          { separator: true }] : []),
    { head: 'labels' },
    ...(setShowTitle ? [{
      label: tag(showTitle, 'title'), keepOpen: true,
      onClick: () => setShowTitle((v) => !v),
    }] : []),
    { head: 'annotations' },
    ...(setShowLegend ? [{
      label: tag(showLegend, 'legend'), keepOpen: true,
      onClick: () => setShowLegend((v) => !v),
    }] : []),
    ...(setLegendLocation ? [{
      submenu: 'legend location',
      items: [
        { value: null,        label: 'default' },
        { value: 'best',      label: 'best' },
        { value: 'north',     label: 'north' },
        { value: 'south',     label: 'south' },
        { value: 'east',      label: 'east' },
        { value: 'west',      label: 'west' },
        { value: 'northeast', label: 'northeast' },
        { value: 'northwest', label: 'northwest' },
        { value: 'southeast', label: 'southeast' },
        { value: 'southwest', label: 'southwest' },
      ].map((o) => ({
        label: tag((legendLocationProp || null) === o.value, o.label),
        onClick: () => setLegendLocation(o.value),
        keepOpen: true,
      })),
    }] : []),
  ] : null;

  // Fit Series ▶ — per-series fit submenu mirroring CompositePlot's
  // shape. Filtered to series with ≥2 rho samples (fitting a
  // single-point series gives a degenerate range). Each row has one
  // `fit` button driving fitRho(seriesName) — polar's per-series fit
  // only scales R (θ is global).
  const fittableSeries = (figure.series || [])
    .map((s, i) => ({ s, i }))
    .filter(({ s }) => Array.isArray(s.rho) && s.rho.length >= 2);
  const seriesSubmenuItems = fittableSeries.length > 0
    ? fittableSeries.map(({ s, i }) => ({
        row: true,
        color: s.color || PALETTE[i % PALETTE.length],
        name: s.name || `series ${i + 1}`,
        buttons: [{ label: 'fit r',
                    onClick: () => fitRho(s.name),
                    disabled: !setViewport }],
      }))
    : null;

  const ctxItems = [
    { label: <span>{houseIcon}Reset</span>, onClick: onReset },
    { submenu: 'Save / Export', items: exportItems },
    ...(gridItems ? [{ submenu: 'Grid', items: gridItems }] : []),
    ...(decorationItems ? [{ submenu: 'Decoration', items: decorationItems }] : []),
    ...(seriesSubmenuItems ? [{
      submenu: `Fit Series${fittableSeries.length > 1 ? ` (${fittableSeries.length})` : ''}`,
      items: seriesSubmenuItems,
    }] : []),
    { separator: true },
    // ПКМ Fit — figure-wide / data-extent rows. Per-series fit lives
    // in Fit Series ▶ above (parity with CompositePlot).
    { head: 'Fit' },
    { label: 'all', onClick: fitAllPolar, disabled: !setViewport },
    { label: 'R',   onClick: fitR,        disabled: !setViewport },
    { label: 'θ',   onClick: fitTheta,    disabled: !setViewport },
  ];

  // Wheel listener attached imperatively because React's onWheel is passive.
  useEffect(() => {
    if (!interactive || !setViewport) return;
    const el = svgRef.current;
    if (!el) return;
    function onWheel(e) {
      e.preventDefault();
      const factor = Math.exp(e.deltaY * 0.0015);
      const lo = vp.r[0];
      const hi = vp.r[1];
      // Preserve theta — wheel only re-scales R. Without `...vp` the
      // theta sweep would silently snap back to default on every wheel
      // tick, undoing any user-narrowed thetalim.
      setViewport({ ...vp, r: [lo, lo + (hi - lo) * factor] });
    }
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  });

  return (
    <>
    {ctxMenu && (
      <ContextMenu x={ctxMenu.x} y={ctxMenu.y} items={ctxItems}
        onClose={() => setCtxMenu(null)} />
    )}
    <div style={glLive
        ? { position: 'relative', width: '100%', height: '100%' }
        : { display: 'contents' }}>
    <svg
      ref={svgRef}
      width="100%" height="100%"
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid meet"
      style={{
        display: 'block',
        cursor: interactive && setViewport ? 'grab' : 'default',
        userSelect: 'none',
        fontFamily: 'JetBrains Mono, monospace',
        pointerEvents: interactive ? 'auto' : 'none',
      }}
      onMouseDown={onMouseDown}
      onMouseMove={onMouseMove}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseLeave}
      onDoubleClick={onDblClick}
      onContextMenu={onContextMenu}
    >
      <rect x={0} y={0} width={width} height={height} fill="var(--bg-1)" />

      {/* Title — gated on showTitle so the ПКМ Decoration ▶ / toolbar
          decoration ▾ `title` toggle can hide it. Same convention as
          CompositePlot. */}
      {figure.title && showTitle && (
        <text x={cx} y={padTop - 10} fill="var(--plot-text-strong)"
          fontSize={12 * fontScale} textAnchor="middle">{figure.title}</text>
      )}

      <g transform={`translate(${cx}, ${cy})`}>
        {/* Minor rings: faint, no labels. MATLAB R2025b parity —
            RMinorGrid is INDEPENDENT of RGrid (set(pax,'RMinorGrid',
            'on') shows minor rings even with RGrid off). Same for
            θ. Previously gated on rGridOn AND rMinorOn which made
            the per-axis minor toggle a no-op when the major was
            off — visible bug. */}
        {rMinorOn && rTicksMinor.map((rho, i) => {
          const r = rScale(rho);
          if (r <= 0 || r > radius + 0.5) return null;
          return (
            <circle key={`rm${i}`} cx={0} cy={0} r={r} fill="none"
              stroke="var(--plot-grid-min)" />
          );
        })}
        {/* Minor spokes — every 15°, between the 30° majors.
            ThetaMinorGrid independent of ThetaGrid (same MATLAB
            parity rule). */}
        {thetaMinorOn && Array.from({ length: 12 }, (_, k) => k * 30 + 15).map((deg) => {
          const a = zero + dirSign * (deg * Math.PI / 180);
          const x = Math.cos(a) * radius;
          const y = -Math.sin(a) * radius;
          return (
            <line key={`smn${deg}`} x1={0} y1={0} x2={x} y2={y}
              stroke="var(--plot-grid-min)" />
          );
        })}

        {/* Major rings + radial tick labels. MATLAB R2025b parity:
            RGrid hides ONLY the ring strokes, not the tick labels —
            labels belong to RAxis.Visible, a separate concern. So we
            render the labels unconditionally; the ring is gated. */}
        {rTicksMajor.map((rho, i) => {
          const r = rScale(rho);
          if (r <= 0 || r > radius + 0.5) return null;
          return (
            <g key={`rt${i}`}>
              {rGridOn && (
                <circle cx={0} cy={0} r={r} fill="none" stroke="var(--plot-grid)" strokeDasharray="2 4" />
              )}
              <text x={3} y={-r - 2} fill="var(--plot-text)" fontSize={9 * fontScale}>
                {fmtR(rho, i)}
              </text>
            </g>
          );
        })}
        {/* Outer frame — full circle by default, otherwise an arc
            spanning the user-set thetalim sweep. */}
        {isFullSweep ? (
          <circle cx={0} cy={0} r={radius} fill="none" stroke="var(--plot-frame)" />
        ) : (
          <>
            <path d={arcPath(radius, aSweep0, aSweep1)}
              fill="none" stroke="var(--plot-frame)" />
            {/* Two radial spokes closing the wedge to the origin. */}
            <line x1={0} y1={0}
              x2={Math.cos(aSweep0) * radius}
              y2={-Math.sin(aSweep0) * radius}
              stroke="var(--plot-frame)" />
            <line x1={0} y1={0}
              x2={Math.cos(aSweep1) * radius}
              y2={-Math.sin(aSweep1) * radius}
              stroke="var(--plot-frame)" />
          </>
        )}

        {/* Major angular spokes. Default = every 30°; overridden by
            `figure.thetaticks` (MATLAB convention: degrees). Custom
            label text comes from `figure.thetaticklabels` when its
            length matches the active tick array, otherwise we fall
            back to the numeric `<deg>°` label. */}
        {(() => {
          const customTicks  = Array.isArray(figure?.thetaticks)
                                 && figure.thetaticks.length > 0
                               ? figure.thetaticks : null;
          const customLabels = customTicks
                            && Array.isArray(figure?.thetaticklabels)
                            && figure.thetaticklabels.length === customTicks.length
                               ? figure.thetaticklabels : null;
          const ticks = customTicks
            ? customTicks
            : Array.from({ length: 12 }, (_, k) => k * 30);
          return ticks.map((deg, i) => {
            if (!isFullSweep) {
              let d = deg - thMinDeg;
              d = ((d % 360) + 360) % 360;
              if (d > (thMaxDeg - thMinDeg) + 1e-6) return null;
            }
            const a = zero + dirSign * (deg * Math.PI / 180);
            const x = Math.cos(a) * radius;
            const y = -Math.sin(a) * radius;
            const xt = Math.cos(a) * (radius + 14);
            const yt = -Math.sin(a) * (radius + 14);
            const label = customLabels ? String(customLabels[i]) : `${deg}°`;
            return (
              <g key={`sp${i}-${deg}`}>
                {thetaGridOn && (
                  <line x1={0} y1={0} x2={x} y2={y}
                    stroke="var(--plot-grid)" strokeDasharray="2 4" />
                )}
                <text x={xt} y={yt + 3} fill="var(--plot-text)" fontSize={9 * fontScale}
                  textAnchor="middle">{label}</text>
              </g>
            );
          });
        })()}

        {/* Series clip path. Full sweep → disc; partial sweep → pie
            wedge so series points outside the angular range get
            clipped cleanly. */}
        <clipPath id={`pclip-${figure.id}-${Math.round(width)}`}>
          {isFullSweep ? (
            <circle cx={0} cy={0} r={radius} />
          ) : (
            <path d={wedgePath(radius, aSweep0, aSweep1)} />
          )}
        </clipPath>
        <g clipPath={`url(#pclip-${figure.id}-${Math.round(width)})`}>
          {/* Preview: the GL series rasterized to an image. Coords are
              absolute viewBox px, but this group is translated to the disc
              centre, so offset by (-cx,-cy) to land at (0,0). */}
          {previewImg && (
            <image href={previewImg} x={-cx} y={-cy} width={width} height={height}
              preserveAspectRatio="none" />
          )}
          {figure.series?.map((s, idx) => {
            if (!s.theta?.length || !s.rho?.length) return null;
            if (glPolar.routed.has(idx)) return null;   // drawn on the WebGL overlay
            const color = s.color || PALETTE[idx % PALETTE.length];
            const mode = s.mode || 'line';

            if (mode === 'scatter') {
              // polarscatter — circle marker at each (theta, rho). Open
              // (outline in the series colour) by default like MATLAB;
              // 'filled' fills. Preview thumbnails subsample (one SVG circle
              // per point janks the window at 10k+; interactive uses GL).
              const N = s.theta.length;
              const step = previewStride(N, interactive);
              const filled = !!s.filled;
              const markers = [];
              for (let i = 0; i < N; i += step) {
                const norm = normalizePolar(s.theta[i], s.rho[i]);
                if (!norm) continue;
                const [x, y] = ptFor(norm.theta, norm.rho);
                markers.push(<circle key={i} cx={x} cy={y} r="3"
                  fill={filled ? color : 'none'} stroke={color}
                  strokeWidth={filled ? 0.6 : 1.2} />);
              }
              return <g key={s.name}>{markers}</g>;
            }
            if (mode === 'bubble') {
              // polarbubblechart — scatter with per-point area
              // (sizes[i]) interpreted MATLAB-style: marker area in
              // points^2 → diameter = sqrt(area)·k. SVG radius =
              // sqrt(sizes[i]) / 2.
              //
              // Per-point colour (s.pointColors) accepts two shapes:
              //   • nested RGB rows [[r,g,b], …] (1 row = shared,
              //     N rows = per-point), each component in [0, 1]
              //   • flat numeric vector — colormap-index data;
              //     for now we just normalise into the palette as a
              //     visual hint (full colormap support is a separate
              //     follow-up). Cell colours fall back to `s.color`
              //     when pointColors is null.
              const sizes = Array.isArray(s.sizes) ? s.sizes : null;
              const sz0   = sizes && sizes.length === 1 ? sizes[0] : null;
              const pc    = Array.isArray(s.pointColors) ? s.pointColors : null;
              const isRgbMatrix = pc && pc.length > 0 && Array.isArray(pc[0]);
              const colorFor = (i) => {
                if (!pc) return color;
                if (isRgbMatrix) {
                  const row = pc.length === 1 ? pc[0] : pc[i];
                  if (!row) return color;
                  const r = Math.round(255 * (row[0] || 0));
                  const g = Math.round(255 * (row[1] || 0));
                  const b = Math.round(255 * (row[2] || 0));
                  return `rgb(${r},${g},${b})`;
                }
                // Flat numeric → simple palette wraparound.
                const idx = Math.abs((pc[i] | 0)) % PALETTE.length;
                return PALETTE[idx];
              };
              return (
                <g key={s.name}>
                  {s.theta.map((th, i) => {
                    const norm = normalizePolar(th, s.rho[i]);
                    if (!norm) return null;
                    const [x, y] = ptFor(norm.theta, norm.rho);
                    const area = sz0 != null ? sz0
                               : (sizes && Number.isFinite(sizes[i])) ? sizes[i]
                               : 36;
                    const r = Math.max(1.5, Math.sqrt(Math.max(0, area)) / 2);
                    const fc = colorFor(i);
                    return <circle key={i} cx={x} cy={y} r={r}
                      fill={fc} fillOpacity="0.5"
                      stroke={fc} strokeWidth="1" />;
                  })}
                </g>
              );
            }
            if (mode === 'compass') {
              // compass — arrow from origin to each (theta, rho).
              // Arrowhead is a small filled triangle perpendicular to
              // the shaft at the tip. Shaft length matches rScale(rho)
              // so longer vectors visibly reach further out. LineSpec
              // string `s.dash` controls dash pattern (e.g. 'r--'
              // sets dash='dashed'); arrowhead stays solid for
              // readability even on dashed shafts.
              const dashMap = { dashed: '4 3', dotted: '1 3', dashdot: '4 3 1 3' };
              const dashArr = dashMap[s.dash] || null;
              return (
                <g key={s.name}>
                  {s.theta.map((th, i) => {
                    const norm = normalizePolar(th, s.rho[i]);
                    if (!norm) return null;
                    const [xT, yT] = ptFor(norm.theta, norm.rho);
                    const len = Math.hypot(xT, yT);
                    const head = Math.max(3, Math.min(10, len * 0.18));
                    const ux = xT / (len || 1), uy = yT / (len || 1);
                    const px = -uy, py = ux;
                    const bX = xT - ux * head;
                    const bY = yT - uy * head;
                    const w = head * 0.55;
                    const x1 = bX + px * w, y1 = bY + py * w;
                    const x2 = bX - px * w, y2 = bY - py * w;
                    return (
                      <g key={i}>
                        <line x1={0} y1={0} x2={xT} y2={yT}
                              stroke={color} strokeWidth={s.width || 1.6}
                              strokeLinecap="round"
                              {...(dashArr ? { strokeDasharray: dashArr } : {})} />
                        <path d={`M${xT.toFixed(2)},${yT.toFixed(2)} `
                               + `L${x1.toFixed(2)},${y1.toFixed(2)} `
                               + `L${x2.toFixed(2)},${y2.toFixed(2)} Z`}
                              fill={color} />
                      </g>
                    );
                  })}
                </g>
              );
            }
            if (mode === 'bar' || mode === 'rose') {
              // rose differs visually from polarhistogram: classic
              // MATLAB rose draws translucent wedges whose vertices
              // touch the origin (i.e. triangle-like petals) WITHOUT
              // a strong outline, evoking the "rose diagram" look.
              // polarhistogram = solid filled bars with a stroke.
              const isRose = mode === 'rose';
              const fillOpacity = isRose ? 0.35 : 0.6;
              const strokeW     = isRose ? 0.4  : 0.8;
              // polarhistogram — radial bars: theta is the bin centre,
              // rho is the count, and the wedge spans (theta - dθ/2,
              // theta + dθ/2) where dθ is inferred from neighbour spacing.
              // Default span = 2π / N when only one bin spacing is known.
              const N = s.theta.length;
              const halfSpan = N > 1
                ? Math.abs(s.theta[1] - s.theta[0]) / 2
                : Math.PI / Math.max(1, N);
              return (
                <g key={s.name}>
                  {s.theta.map((th, i) => {
                    const rho = s.rho[i];
                    if (!Number.isFinite(rho) || rho <= 0) return null;
                    // Wedge as four-corner polygon: (origin path inward
                    // would simplify to triangle since the inner edge
                    // collapses; render a polygon from origin out and
                    // back across the bin span).
                    const a0 = th - halfSpan;
                    const a1 = th + halfSpan;
                    const [xo0, yo0] = ptFor(a0, 0);
                    const [xo1, yo1] = ptFor(a1, 0);
                    const [xr0, yr0] = ptFor(a0, rho);
                    const [xr1, yr1] = ptFor(a1, rho);
                    const d = `M${xo0.toFixed(2)},${yo0.toFixed(2)} `
                            + `L${xr0.toFixed(2)},${yr0.toFixed(2)} `
                            + `L${xr1.toFixed(2)},${yr1.toFixed(2)} `
                            + `L${xo1.toFixed(2)},${yo1.toFixed(2)} Z`;
                    return <path key={i} d={d}
                      fill={color} fillOpacity={fillOpacity}
                      stroke={color} strokeWidth={strokeW} />;
                  })}
                </g>
              );
            }

            // Default: line / polyline. Negative-rho samples are
            // reflected to the opposite angle (MATLAB semantics). Preview
            // thumbnails subsample the path (interactive uses the GL overlay).
            let d = '';
            let started = false;
            const Nl = s.theta.length;
            const stepL = previewStride(Nl, interactive);
            for (let i = 0; i < Nl; i += stepL) {
              const norm = normalizePolar(s.theta[i], s.rho[i]);
              if (!norm) { started = false; continue; }
              const [x, y] = ptFor(norm.theta, norm.rho);
              d += (started ? 'L' : 'M') + x.toFixed(2) + ',' + y.toFixed(2) + ' ';
              started = true;
            }
            // Close the path if theta sweeps a full revolution
            const range = Math.abs(s.theta[s.theta.length - 1] - s.theta[0]);
            if (range >= Math.PI * 1.95) d += 'Z';
            return (
              <path key={s.name} d={d} stroke={color} fill="none"
                strokeWidth={s.width || 1.6}
                strokeLinejoin="round" strokeLinecap="round" />
            );
          })}
        </g>
      </g>

      {/* Legend — ported from CompositePlot. Renders only when:
            • showLegend toggle is on (toolbar decoration ▾ default), AND
            • user asked via script (figure.legend non-empty OR explicit
              legendLocation set — same rule as CompositePlot).
          Positioning uses width/height as the anchor frame (polar has
          no padL/W panel rect like cartesian); top-row positions reserve
          the title strip so the legend doesn't overlap the figure name. */}
      {(() => {
        if (showLegend === false) return null;
        const userAsked = (figure.legend && figure.legend.length > 0)
                       || (figure.legendLocation && figure.legendLocation !== 'none');
        if (!userAsked || !Array.isArray(figure.series) || figure.series.length === 0) return null;
        const labels = (figure.legend && figure.legend.length > 0)
          ? figure.legend
          : figure.series.map((s) => s.name).filter(Boolean);
        const haveLabels = labels.some((s) => s && s.trim() !== '');
        if (!haveLabels) return null;
        const items = figure.series.slice(0, labels.length).map((s, i) => ({
          color: s.color || PALETTE[i % PALETTE.length],
          text:  labels[i] || s.name || `series ${i + 1}`,
          mode:  s.mode || 'line',
        }));
        if (items.length === 0) return null;
        const fontSize = 10 * fontScale;
        const lineH    = fontSize + 4;
        const swatchW  = 14;
        const padInner = 6;
        const longest = items.reduce((m, it) => Math.max(m, it.text.length), 0);
        const boxW = padInner * 2 + swatchW + 4 + Math.min(longest, 24) * 6.5;
        const boxH = padInner * 2 + items.length * lineH;
        const loc = ((legendLocationProp != null ? legendLocationProp : figure.legendLocation) || 'best')
                    .replace(/outside$/, '');
        // Anchor inside the FULL svg bounds (polar has no inner panel
        // rect). Top edge respects padTop so the title strip stays clear.
        const anchorMargin = 8;
        const topEdge    = padTop + anchorMargin;
        const bottomEdge = height - padBot - anchorMargin;
        let bx, by;
        switch (loc) {
          case 'north':     bx = (width - boxW) / 2;          by = topEdge; break;
          case 'south':     bx = (width - boxW) / 2;          by = bottomEdge - boxH; break;
          case 'east':      bx = width - boxW - anchorMargin; by = (height - boxH) / 2; break;
          case 'west':      bx = anchorMargin;                by = (height - boxH) / 2; break;
          case 'northwest': bx = anchorMargin;                by = topEdge; break;
          case 'southeast': bx = width - boxW - anchorMargin; by = bottomEdge - boxH; break;
          case 'southwest': bx = anchorMargin;                by = bottomEdge - boxH; break;
          case 'none':      return null;
          // 'best' / 'northeast' / default → top-right.
          default:          bx = width - boxW - anchorMargin; by = topEdge; break;
        }
        return (
          <g pointerEvents="none">
            <rect x={bx} y={by} width={boxW} height={boxH}
              fill="var(--plot-bg)" stroke="var(--plot-frame)" strokeWidth="0.5"
              rx="3" opacity="0.92" />
            {items.map((it, i) => {
              const cyL = by + padInner + i * lineH + lineH / 2;
              const swX0 = bx + padInner;
              const swX1 = swX0 + swatchW;
              let swatch;
              if (it.mode === 'scatter' || it.mode === 'stem') {
                swatch = <circle cx={(swX0 + swX1) / 2} cy={cyL} r="3"
                  fill={it.color} stroke="var(--plot-frame)" strokeWidth="0.6" />;
              } else if (it.mode === 'bar') {
                swatch = <rect x={swX0} y={cyL - 4} width={swatchW} height="8"
                  fill={it.color} stroke={it.color} strokeWidth="1" />;
              } else {
                swatch = <line x1={swX0} x2={swX1} y1={cyL} y2={cyL}
                  stroke={it.color} strokeWidth="2" />;
              }
              return (
                <g key={i}>
                  {swatch}
                  <text x={swX1 + 4} y={cyL + fontSize / 3} fontSize={fontSize}
                    fill="var(--plot-text-strong)">{it.text}</text>
                </g>
              );
            })}
          </g>
        );
      })()}
    </svg>
    {glLive && (
      <GLChart series={glPolar.series} proj={glProj}
        plotRect={{ x: 0, y: 0, w: width, h: height }}
        width={width} height={height} dpr={glDpr} clip={glClip} />
    )}
    </div>
    </>
  );
}
