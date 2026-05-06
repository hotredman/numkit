/**
 * CompositePlot — universal 2-D renderer.
 *
 * Holds the shared infrastructure (sx/sy with linear/log mapping, pan &
 * zoom with modifier keys, tick generation, ПКМ menu, hover crosshair,
 * tile-fetch + LOD pyramid, color override, colormap selection, keyboard
 * shortcuts) and walks `figure.layers[]` rendering each layer through a
 * dedicated draw function:
 *
 *   layer.kind === 'heatmap'  → image + colorbar + tile overlay
 *   layer.kind === 'series'   → line / scatter / stem / stairs / bar
 *                              (mode field discriminates)
 *   layer.kind === 'text'     → SVG <text> annotation
 *
 * Heatmap-specific UI (colorbar, color autoscale, colormap select) is
 * conditional on whether any heatmap layer is present in the array. A
 * pure line plot or annotation-only figure simply skips those branches.
 *
 * Figure shape (built by adapters.adaptFigure for non-polar/non-subplot):
 *   {
 *     kind: 'composite', id, title, xLabel, yLabel,
 *     xRange, yRange,
 *     grid, xscale, yscale,
 *     layers: [...],               // heterogeneous, see above
 *   }
 */
import { useEffect, useMemo, useRef, useState } from 'react';
import { buildHeatmapLUT, renderHeatmapDataURLFromIndices,
         renderHeatmapDataURLFromFlat, getColormap } from './colormaps';
import ContextMenu from './ContextMenu';
import { computeFitViewport, exportSvgNode, exportPngNode, exportPngForPrint } from './plotUtils';

export default function CompositePlot({
  figure,
  width,
  height,
  viewport,
  setViewport,
  major = true,
  minor = true,
  showLegend = true,
  fontScale = 1,
  interactive = true,
  engine = null,
  // Log axis state owned by FigureWindow so the toolbar buttons + the
  // ПКМ menu inside the panel stay in sync. Defaults respect the
  // figure's xscale/yscale config when no parent provides setters.
  xLog: xLogProp,
  yLog: yLogProp,
  setXLog: setXLogProp,
  setYLog: setYLogProp,
  colorOverride: colorOverrideProp,
  setColorOverride: setColorOverrideProp,
  colormapOverride = null,
}) {
  // Layers — empty array if none. The renderer walks them in order so the
  // user controls z-order via call sequence (heatmap first, scatter on top,
  // text labels last — exactly mirroring `imagesc; hold on; scatter; text`).
  const layers = Array.isArray(figure.layers) ? figure.layers : [];
  const heatmapLayer = layers.find((l) => l.kind === 'heatmap') || null;
  const seriesLayers = layers.filter((l) => l.kind === 'series');
  const textLayers = layers.filter((l) => l.kind === 'text');
  const hasHeatmap = !!heatmapLayer;

  // Effective colormap: runtime override (toolbar combo) > script-level
  // colormap on the heatmap layer > default 'parula'.
  const effectiveColormap = colormapOverride
    || heatmapLayer?.colormap
    || 'parula';

  // ── Heatmap-layer field aliases ─────────────────────────────────────
  // Aliases for the heatmap layer's data/metadata fields so the existing
  // render code (originally written for a flat figure shape with `figure.z`,
  // `hcminOrig`, etc.) reads naturally. All access is guarded — pure
  // line/scatter composites have heatmapLayer === null and these aliases
  // fall through to safe defaults; the heatmap-specific render blocks gate
  // on `hasHeatmap` to avoid touching them.
  const hZ              = heatmapLayer?.z;
  const hcmin           = heatmapLayer?.cmin ?? 0;
  const hcmax           = heatmapLayer?.cmax ?? 1;
  const hcminOrig       = heatmapLayer?.cminOrig ?? hcmin;
  const hcmaxOrig       = heatmapLayer?.cmaxOrig ?? hcmax;
  const hColorScaleBaked= heatmapLayer?.colorScaleBaked;
  const hFullRows       = heatmapLayer?.originalRows ?? 0;
  const hFullCols       = heatmapLayer?.originalCols ?? 0;
  const hFigId          = heatmapLayer?._figId ?? -1;
  const hAxIdx          = heatmapLayer?._axIdx ?? 0;
  const hDsIdx          = heatmapLayer?._dsIdx ?? 0;
  const hDownsampled    = heatmapLayer?.downsampled;

  const svgRef = useRef(null);
  const [hover, setHover] = useState(null);
  const [ctxMenu, setCtxMenu] = useState(null);
  const dragRef = useRef(null);

  // ── display-tile state ──────────────────────────────────────────────
  // tileOverlay holds the most recent display-pixel-grid sample of the
  // visible source-rect. Rendered as an SVG <image> filling the plot area.
  // Reset on figure identity change (new dataset → stale tile is wrong).
  const [tileOverlay, setTileOverlay] = useState(null);
  const figIdRef = useRef(figure._raw?.id ?? figure.id);
  useEffect(() => {
    const fid = figure._raw?.id ?? figure.id;
    if (figIdRef.current !== fid) {
      setTileOverlay(null);
      // colorOverride reset is owned by FigureWindow when interactive
      // (it has its own useEffect on figure id). Reset local fallback
      // here for preview / non-interactive paths.
      if (setColorOverrideProp === undefined) setColorOverrideLocal(null);
      figIdRef.current = fid;
    }
  }, [figure._raw?.id, figure.id, setColorOverrideProp]);

  // ── Log-axis state ─────────────────────────────────────────────────
  // Owned by the parent (FigureWindow) when it supplies setXLog/setYLog
  // so the toolbar buttons and ПКМ menu stay in sync. Otherwise (preview
  // cards, fallback callers) fall back to local state seeded from
  // figure.xscale/yscale.
  const [xLogLocal, setXLogLocal] = useState(figure.xscale === 'log');
  const [yLogLocal, setYLogLocal] = useState(figure.yscale === 'log');
  const xLog = (xLogProp !== undefined) ? xLogProp : xLogLocal;
  const yLog = (yLogProp !== undefined) ? yLogProp : yLogLocal;
  const setXLog = setXLogProp || setXLogLocal;
  const setYLog = setYLogProp || setYLogLocal;

  // ── Color-limit override ────────────────────────────────────────────
  // "Fit colors to visible" pulls cmin/cmax from the currently-visible
  // source-rect via getFigureTile, so a low-contrast region zoomed in
  // gets full colormap dynamic range. null = use hcminOrig/cmaxOrig.
  // The override is a window/level remap on top of the engine-baked
  // quantization range — no requantization, just a different LUT.
  // Use parent-owned override when supplied (FigureWindow holds it so the
  // toolbar fit menu and ПКМ menu share state). Else fall back to local.
  const [colorOverrideLocal, setColorOverrideLocal] = useState(null);
  const colorOverride = (colorOverrideProp !== undefined) ? colorOverrideProp : colorOverrideLocal;
  const setColorOverride = setColorOverrideProp || setColorOverrideLocal;
  const cminEff = colorOverride ? colorOverride.cmin : hcminOrig;
  const cmaxEff = colorOverride ? colorOverride.cmax : hcmaxOrig;
  const cminOrig = hcminOrig;
  const cmaxOrig = hcmaxOrig;

  // 256-entry RGBA LUT — rebuilt only when colormap or window/level changes.
  const lut = useMemo(
    () => buildHeatmapLUT(effectiveColormap, cminOrig, cmaxOrig, cminEff, cmaxEff),
    [effectiveColormap, cminOrig, cmaxOrig, cminEff, cmaxEff]
  );

  const padL = 60 * fontScale;
  const padR = hasHeatmap ? 70 * fontScale : 18;  // wider when colorbar is shown
  const padT = 36 * fontScale;
  const padB = 44 * fontScale;
  // Force integer dims — non-integer panel sizes from fractional fontScale
  // would produce a diagonal-stripe artefact in the tile renderer because
  // row strides drift by frac-cols each iteration when arr is indexed.
  const W = Math.max(50, Math.floor(width - padL - padR));
  const H = Math.max(50, Math.floor(height - padT - padB));

  const [xMin, xMax] = viewport.x;
  const [yMin, yMax] = viewport.y;
  // Log axes: viewport bounds are still in original-data coordinates
  // (xMin..xMax = the user-visible range). The screen-mapping is log when
  // the corresponding axis flag is on. Requires lo > 0 — we sanitise by
  // clamping at the call sites that set viewport.
  const xLogActive = xLog && xMin > 0 && xMax > 0;
  const yLogActive = yLog && yMin > 0 && yMax > 0;
  const sx = xLogActive
    ? (v) => padL + (Math.log(v / xMin) / Math.log(xMax / xMin)) * W
    : (v) => padL + ((v - xMin) / (xMax - xMin)) * W;
  const sy = yLogActive
    ? (v) => padT + H - (Math.log(v / yMin) / Math.log(yMax / yMin)) * H
    : (v) => padT + H - ((v - yMin) / (yMax - yMin)) * H;
  const isx = xLogActive
    ? (px) => xMin * Math.exp(((px - padL) / W) * Math.log(xMax / xMin))
    : (px) => xMin + ((px - padL) / W) * (xMax - xMin);
  const isy = yLogActive
    ? (py) => yMin * Math.exp(((padT + H - py) / H) * Math.log(yMax / yMin))
    : (py) => yMax - ((py - padT) / H) * (yMax - yMin);

  // Pre-render the inline preview to a dataURL via the LUT. uint8 indices
  // are stable; only the LUT changes on window/level — so we keep a separate
  // memo on (z) and another on (lut) chained together. Skips entirely if
  // there's no heatmap layer.
  const dataURL = useMemo(() => {
    if (!hZ) return null;
    return renderHeatmapDataURLFromIndices(hZ, lut);
  }, [hZ, lut]);

  function niceTicks(min, max, target = 6) {
    const range = max - min;
    if (range <= 0) return { major: [min], minor: [] };
    const rough = range / target;
    const pow = Math.pow(10, Math.floor(Math.log10(rough)));
    const norm = rough / pow;
    const step = norm < 1.5 ? pow : norm < 3 ? 2 * pow : norm < 7 ? 5 * pow : 10 * pow;
    const start = Math.ceil(min / step) * step;
    const majorArr = [];
    for (let v = start; v <= max + step * 1e-6; v += step) majorArr.push(+v.toFixed(12));
    const minorStep = step / 5;
    const minorArr = [];
    for (let v = Math.ceil(min / minorStep) * minorStep; v <= max + minorStep * 1e-6; v += minorStep) {
      if (Math.abs(((v - start) / step) - Math.round((v - start) / step)) > 1e-6) minorArr.push(+v.toFixed(12));
    }
    return { major: majorArr, minor: minorArr };
  }
  // Log-axis tick generator: powers of 10 as major, intermediate 2..9
  // multiples as minor. Used when {x,y}LogActive.
  function logTicks(min, max) {
    if (min <= 0 || max <= 0 || max <= min) return { major: [], minor: [] };
    const lmin = Math.floor(Math.log10(min));
    const lmax = Math.ceil(Math.log10(max));
    const major = [], minor = [];
    for (let p = lmin; p <= lmax; p++) {
      const base = Math.pow(10, p);
      if (base >= min && base <= max) major.push(base);
      for (let m = 2; m <= 9; m++) {
        const v = base * m;
        if (v >= min && v <= max) minor.push(v);
      }
    }
    return { major, minor };
  }
  const xTicks = xLogActive ? logTicks(xMin, xMax) : niceTicks(xMin, xMax, 8);
  const yTicks = yLogActive ? logTicks(yMin, yMax) : niceTicks(yMin, yMax, 6);

  function fmtTick(v) {
    const a = Math.abs(v);
    if (a !== 0 && (a < 1e-3 || a >= 1e5)) return v.toExponential(1);
    if (a >= 100) return v.toFixed(0);
    if (a >= 10)  return v.toFixed(1);
    if (a >= 1)   return v.toFixed(2);
    return v.toFixed(3);
  }

  /* ─── pan/zoom (same as InteractivePlot) ─── */
  function onMouseDown(e) {
    if (!interactive || e.button !== 0) return;
    const rect = svgRef.current.getBoundingClientRect();
    dragRef.current = { sx: e.clientX, sy: e.clientY, x0: viewport.x.slice(), y0: viewport.y.slice(), W, H, rect };
    e.currentTarget.style.cursor = 'grabbing';
  }
  function onMouseMove(e) {
    if (!svgRef.current || !interactive) return;
    const rect = svgRef.current.getBoundingClientRect();
    const px = (e.clientX - rect.left) * (width / rect.width);
    const py = (e.clientY - rect.top)  * (height / rect.height);
    if (px >= padL && px <= padL + W && py >= padT && py <= padT + H) {
      setHover({ px, py, x: isx(px), y: isy(py) });
    } else setHover(null);
    if (!dragRef.current) return;
    const d = dragRef.current;
    const xPxFrac = (e.clientX - d.sx) / (d.W * (rect.width / width));
    const yPxFrac = (e.clientY - d.sy) / (d.H * (rect.height / height));
    // Pan: linear axes translate additively, log axes translate multiplicatively
    // (a constant screen-pixel delta = a constant log-space delta = a fixed ratio
    // applied to both bounds).
    let nx, ny;
    if (xLogActive) {
      const ratio = Math.exp(-xPxFrac * Math.log(d.x0[1] / d.x0[0]));
      nx = [d.x0[0] * ratio, d.x0[1] * ratio];
    } else {
      const dx = xPxFrac * (d.x0[1] - d.x0[0]);
      nx = [d.x0[0] - dx, d.x0[1] - dx];
    }
    if (yLogActive) {
      const ratio = Math.exp(yPxFrac * Math.log(d.y0[1] / d.y0[0]));
      ny = [d.y0[0] * ratio, d.y0[1] * ratio];
    } else {
      const dy = yPxFrac * (d.y0[1] - d.y0[0]);
      ny = [d.y0[0] + dy, d.y0[1] + dy];
    }
    setViewport({ x: nx, y: ny });
  }
  function onMouseUp(e)    { dragRef.current = null; if (e.currentTarget) e.currentTarget.style.cursor = 'grab'; }
  function onMouseLeave(e) { setHover(null); onMouseUp(e); }
  function onDblClick()    { if (interactive) setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() }); }
  function onContextMenu(e) {
    if (!interactive) return;
    e.preventDefault();
    setCtxMenu({ x: e.clientX, y: e.clientY });
  }
  function fitAxes(axisMode) {
    const next = { x: viewport.x.slice(), y: viewport.y.slice() };
    // Under log mode the figure's natural xRange/yRange straddle zero
    // for heatmap (cellH/2 padding). Clamp the lo bound to half-cell so
    // log doesn't silently snap back to linear. For pure-series figures
    // there's no half-cell; use a small positive seed.
    if (axisMode === 'both' || axisMode === 'x') {
      if (xLog) {
        const cellW = hFullCols > 0
          ? (figure.xRange[1] - figure.xRange[0]) / hFullCols
          : 0;
        const lo = Math.max(cellW * 0.5, figure.xRange[0] > 0 ? figure.xRange[0] : 1e-6);
        next.x = [lo, Math.max(lo * 10, figure.xRange[1])];
      } else {
        next.x = figure.xRange.slice();
      }
    }
    if (axisMode === 'both' || axisMode === 'y') {
      if (yLog) {
        const cellH = hFullRows > 0
          ? (figure.yRange[1] - figure.yRange[0]) / hFullRows
          : 0;
        const lo = Math.max(cellH * 0.5, figure.yRange[0] > 0 ? figure.yRange[0] : 1e-6);
        next.y = [lo, Math.max(lo * 10, figure.yRange[1])];
      } else {
        next.y = figure.yRange.slice();
      }
    }
    setViewport(next);
  }

  // Recompute cmin/cmax from the currently-visible source-rect. Pulls a
  // coarse-LOD tile of uint8 indices, finds idxMin/idxMax, maps them back
  // through the engine's quantization range to original-domain values.
  // No requantization happens — the override is a window/level remap of
  // the LUT (see buildHeatmapLUT). 256-level resolution is the cap.
  function fitColorsToVisible() {
    if (!hasHeatmap) return;
    if (!engine || typeof engine.getFigureTile !== 'function') return;
    if (hFigId < 0 || !hFullRows || !hFullCols) return;

    const xExt = figure.xRange[1] - figure.xRange[0];
    const yExt = figure.yRange[1] - figure.yRange[0];
    const colsPerUnit = hFullCols / (xExt || 1);
    const rowsPerUnit = hFullRows / (yExt || 1);
    const c0 = Math.max(0, Math.floor((Math.min(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const c1 = Math.min(hFullCols, Math.ceil((Math.max(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const r0 = Math.max(0, Math.floor((figure.yRange[1] - Math.max(yMin, yMax)) * rowsPerUnit));
    const r1 = Math.min(hFullRows, Math.ceil((figure.yRange[1] - Math.min(yMin, yMax)) * rowsPerUnit));
    const tileW = c1 - c0, tileH = r1 - r0;
    if (tileW <= 0 || tileH <= 0) return;

    const lod = Math.max(1, Math.ceil(Math.max(tileH, tileW) / 256));
    const tile = engine.getFigureTile(hFigId, hAxIdx, hDsIdx,
                                      r0, c0, tileH, tileW, lod);
    if (!tile || tile.error || !tile.data) return;

    // Find min/max indices, skipping the NaN sentinel (255).
    let idxMn = 256, idxMx = -1;
    for (let i = 0; i < tile.data.length; i++) {
      const idx = tile.data[i];
      if (idx === 255) continue;
      if (idx < idxMn) idxMn = idx;
      if (idx > idxMx) idxMx = idx;
    }
    if (idxMn > 254 || idxMx < 0 || idxMn === idxMx) return;
    // Map indices back to original-domain values via the engine's
    // quantization range. Resolution: cmaxOrig - cminOrig per 254 levels.
    const range = cmaxOrig - cminOrig;
    const newMin = cminOrig + (idxMn / 254) * range;
    const newMax = cminOrig + (idxMx / 254) * range;
    setColorOverride({ cmin: newMin, cmax: newMax });
  }
  function resetColors() { setColorOverride(null); }
  // Per-series fit (line / scatter): scan x/y of selected layer and shrink
  // viewport to its data extent. Mirrors InteractivePlot's "Fit single
  // curve". `axisMode` is 'both' / 'x' / 'y'.
  function applyFitSeries(seriesIdx, axisMode) {
    const ly = seriesLayers[seriesIdx];
    if (!ly) return;
    const figDefault = { x: figure.xRange.slice(), y: figure.yRange.slice() };
    setViewport(computeFitViewport([{ name: ly.name, x: ly.x, y: ly.y }],
                                   ly.name, axisMode, viewport, figDefault));
  }
  function applyFitAllSeries(axisMode) {
    if (seriesLayers.length === 0) return fitAxes(axisMode);
    const figDefault = { x: figure.xRange.slice(), y: figure.yRange.slice() };
    const all = seriesLayers.map((s) => ({ name: s.name, x: s.x, y: s.y }));
    setViewport(computeFitViewport(all, 'all', axisMode, viewport, figDefault));
  }

  const multiSeries = seriesLayers.length > 1;
  const ctxItems = [
    { label: 'Reset to default',
      onClick: () => {
        setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() });
        setColorOverride(null);
        setXLog(false);
        setYLog(false);
      } },
    { label: 'Save as SVG (vector)',
      onClick: () => exportSvgNode(svgRef.current, `figure_${figure.id}.svg`) },
    { label: 'Save as PNG (screen 2×)',
      onClick: () => exportPngNode(svgRef.current, width, height, 2, `figure_${figure.id}.png`) },
    { head: 'Save for print (300 DPI)' },
    { label: 'PNG · 1 column (85 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 85, 300, `figure_${figure.id}`) },
    { label: 'PNG · 2 columns (170 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 170, 300, `figure_${figure.id}`) },
    { label: 'PNG · A4 width (210 mm)',
      onClick: () => exportPngForPrint(svgRef.current, width, height, 210, 300, `figure_${figure.id}`) },
    { separator: true },
    // For figures with series layers (line/scatter), surface "fit all curves"
    // and per-curve rows like InteractivePlot did. Falls back to data-extent
    // fit when there are no series (pure heatmap / annotations only).
    ...(seriesLayers.length > 0 ? [
      { head: multiSeries ? 'Fit all curves' : 'Fit data extent' },
      { label: 'Fit both axes', onClick: () => applyFitAllSeries('both') },
      { label: 'Fit X only',    onClick: () => applyFitAllSeries('x') },
      { label: 'Fit Y only',    onClick: () => applyFitAllSeries('y') },
      ...(multiSeries ? [
        { head: 'Fit single curve' },
        ...seriesLayers.map((s, i) => ({
          row: true, color: s.color, name: s.name || `series ${i + 1}`,
          buttons: [
            { label: 'xy', onClick: () => applyFitSeries(i, 'both') },
            { label: 'x',  onClick: () => applyFitSeries(i, 'x') },
            { label: 'y',  onClick: () => applyFitSeries(i, 'y') },
          ],
        })),
      ] : []),
    ] : [
      { head: 'Fit data extent' },
      { label: 'Fit both axes', onClick: () => fitAxes('both') },
      { label: 'Fit X only',    onClick: () => fitAxes('x') },
      { label: 'Fit Y only',    onClick: () => fitAxes('y') },
    ]),
    // Color range — only meaningful for heatmap layers.
    ...(hasHeatmap ? [
      { head: 'Color range' },
      { label: 'Fit colors to visible',
        onClick: fitColorsToVisible,
        disabled: !engine || typeof engine.getFigureTile !== 'function'
                  || hFigId < 0 },
      { label: colorOverride
          ? `Reset colors (${Number(hcmin).toPrecision(3)} … ${Number(hcmax).toPrecision(3)})`
          : 'Reset colors',
        onClick: resetColors,
        disabled: !colorOverride },
    ] : []),
    { head: 'Axes' },
    { label: xLog ? '✓ X axis · log' : 'X axis · log',
      onClick: () => {
        // Switching to log requires a strictly-positive xMin. Clamp viewport
        // up to half-cell-width (the lowest positive cell-centre worth showing)
        // when currently viewing through zero. For pure-series figures with
        // no cell grid, fall back to a small positive seed.
        if (!xLog && (xMin <= 0 || xMax <= 0)) {
          const cellW = hFullCols > 0 ? (figure.xRange[1] - figure.xRange[0]) / hFullCols : 0;
          const safeLo = Math.max(cellW * 0.5, figure.xRange[0] > 0 ? figure.xRange[0] : 1e-6);
          const safeHi = Math.max(safeLo * 10, figure.xRange[1]);
          setViewport({ ...viewport, x: [safeLo, safeHi] });
        }
        setXLog((v) => !v);
      },
      disabled: figure.xRange[1] <= 0,
    },
    { label: yLog ? '✓ Y axis · log' : 'Y axis · log',
      onClick: () => {
        if (!yLog && (yMin <= 0 || yMax <= 0)) {
          const cellH = hFullRows > 0 ? (figure.yRange[1] - figure.yRange[0]) / hFullRows : 0;
          const safeLo = Math.max(cellH * 0.5, figure.yRange[0] > 0 ? figure.yRange[0] : 1e-6);
          const safeHi = Math.max(safeLo * 10, figure.yRange[1]);
          setViewport({ ...viewport, y: [safeLo, safeHi] });
        }
        setYLog((v) => !v);
      },
      disabled: figure.yRange[1] <= 0,
    },
  ];

  useEffect(() => {
    if (!interactive) return;
    const el = svgRef.current; if (!el) return;
    function onWheel(e) {
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const px = (e.clientX - rect.left) * (width / rect.width);
      const py = (e.clientY - rect.top)  * (height / rect.height);
      const cx = isx(px), cy = isy(py);
      const factor = Math.exp(e.deltaY * 0.0015);
      // Modifier convention:
      //   plain wheel  → zoom both axes
      //   Ctrl + wheel → zoom X only
      //   Shift+ wheel → zoom Y only
      // (Ctrl+Shift falls through to "both" — not worth a third gesture.)
      const onlyX = e.ctrlKey  && !e.shiftKey;
      const onlyY = e.shiftKey && !e.ctrlKey;
      let nx = viewport.x, ny = viewport.y;
      if (!onlyY) {
        nx = xLogActive
          ? [cx * Math.pow(xMin / cx, factor), cx * Math.pow(xMax / cx, factor)]
          : [cx - (cx - xMin) * factor, cx + (xMax - cx) * factor];
      }
      if (!onlyX) {
        ny = yLogActive
          ? [cy * Math.pow(yMin / cy, factor), cy * Math.pow(yMax / cy, factor)]
          : [cy - (cy - yMin) * factor, cy + (yMax - cy) * factor];
      }
      setViewport({ x: nx, y: ny });
    }
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  });

  // ── display-tile fetch on viewport / log / panel-size changes ──────
  // The engine resamples its zQuantized to the panel's W×H pixel grid in
  // one pass, applying log10 inverse on either axis when active. The
  // returned uint8 buffer is colormapped via the LUT and blitted as an
  // SVG <image> filling the plot area. Debounced 100 ms so continuous
  // wheel-zoom doesn't hammer the engine.
  useEffect(() => {
    if (!interactive) return;
    if (!engine || typeof engine.getFigureDisplayTile !== 'function') return;
    if (hFigId < 0) return;
    if (!hFullRows || !hFullCols) return;
    if (W < 4 || H < 4) return;

    const handle = setTimeout(() => {
      // Map viewport (in original-data coords) to fractional source-cell
      // indices. xRange/yRange always span the source extent, regardless
      // of inline-preview downsampling.
      const fullCols = hFullCols;
      const fullRows = hFullRows;
      const xExt = figure.xRange[1] - figure.xRange[0];
      const yExt = figure.yRange[1] - figure.yRange[0];
      const colsPerUnit = fullCols / (xExt || 1);
      const rowsPerUnit = fullRows / (yExt || 1);

      const xLo = Math.min(xMin, xMax);
      const xHi = Math.max(xMin, xMax);
      const yLo = Math.min(yMin, yMax);
      const yHi = Math.max(yMin, yMax);
      // Cell-index in a system where cell 0 = first cell-CENTRE (= y0, the
      // first y-vector value). Subtract 0.5 from the naïve "yLo - yRange[0]"
      // calc because yRange[0] = y0 - cellH/2 (the cell's lower edge).
      // Critical for log mode: the engine log-distributes cell-indices, and
      // log(cellIdx_shifted) is a linear shift of log(data_y), so cell-log
      // and data-y-log distributions agree pixel-for-pixel. Without this
      // shift, the small-data-y end of the tile drifts relative to the
      // gridlines as the viewport pans through log space.
      // axis-xy: low cell index = low data y = bottom of plot.
      let srcC0 = (xLo - figure.xRange[0]) * colsPerUnit - 0.5;
      let srcC1 = (xHi - figure.xRange[0]) * colsPerUnit - 0.5;
      let srcR0 = (yLo - figure.yRange[0]) * rowsPerUnit - 0.5;
      let srcR1 = (yHi - figure.yRange[0]) * rowsPerUnit - 0.5;

      // Clamp to source bounds; log axes need strictly positive lo.
      srcC0 = Math.max(xLogActive ? 1e-6 : 0, srcC0);
      srcR0 = Math.max(yLogActive ? 1e-6 : 0, srcR0);
      srcC1 = Math.min(fullCols, srcC1);
      srcR1 = Math.min(fullRows, srcR1);
      const srcH = srcR1 - srcR0;
      const srcW = srcC1 - srcC0;
      if (srcH <= 0 || srcW <= 0) { setTileOverlay(null); return; }

      const buf = engine.getFigureDisplayTile(
        hFigId, hAxIdx, hDsIdx,
        srcR0, srcC0, srcH, srcW,
        H, W,
        xLogActive, yLogActive
      );
      if (!buf) { setTileOverlay(null); return; }

      const dataURL = renderHeatmapDataURLFromFlat(buf, H, W, lut);
      // Remember the source-rect this tile covers — when the viewport
      // changes during the next debounce window the tile is repositioned
      // so the image tracks the gridlines instead of lagging behind.
      setTileOverlay({ dataURL, srcR0, srcC0, srcH, srcW, xLog: xLogActive, yLog: yLogActive });
    }, 60);

    return () => clearTimeout(handle);
  }, [interactive, engine, hFigId, hAxIdx, hDsIdx,
      hFullRows, hFullCols,
      figure.xRange, figure.yRange,
      xMin, xMax, yMin, yMax, xLogActive, yLogActive,
      W, H, lut]);

  const clipId = `clip-h-${figure.id}-${Math.round(width)}`;
  // The heatmap image is stretched to fill the figure's xRange × yRange in
  // viewport coordinates — pan/zoom moves the SVG rect, the image follows.
  const imgX = sx(figure.xRange[0]);
  const imgY = sy(figure.yRange[1]);   // top-left of viewport in screen space
  const imgW = sx(figure.xRange[1]) - sx(figure.xRange[0]);
  const imgH = sy(figure.yRange[0]) - sy(figure.yRange[1]);

  /* ─── colorbar (right of plot area) ─── */
  const cbarW = 12;
  const cbarX = padL + W + 14;
  const cbarH = H;
  const cbarTicks = niceTicks(cminEff, cmaxEff, 5);
  const cbarInterp = getColormap(effectiveColormap);
  const cbarStops = Array.from({ length: 11 }, (_, i) => ({
    offset: `${i * 10}%`,
    color:  cbarInterp(i / 10),
  }));
  const cbarGradId = `cbar-${figure.id}-${Math.round(width)}`;

  return (
    <>
    {ctxMenu && (
      <ContextMenu x={ctxMenu.x} y={ctxMenu.y} items={ctxItems}
        onClose={() => setCtxMenu(null)} />
    )}
    {hDownsampled && (
      <div className="hm-preview-banner" title="Engine downsampled this figure to keep the inline preview small. Zoom-in detail will arrive once tile-fetch lands.">
        preview · downsampled from {hFullRows}×{hFullCols}
      </div>
    )}
    <svg
      ref={svgRef}
      width="100%" height="100%"
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid meet"
      style={{
        display: 'block',
        cursor: interactive ? 'grab' : 'default',
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
      <defs>
        <clipPath id={clipId}>
          <rect x={padL} y={padT} width={W} height={H} />
        </clipPath>
        <linearGradient id={cbarGradId} x1="0" y1="1" x2="0" y2="0">
          {cbarStops.map((s, i) => <stop key={i} offset={s.offset} stopColor={s.color} />)}
        </linearGradient>
      </defs>

      <rect x={0} y={0} width={width} height={height} fill="var(--bg-1)" />
      <rect x={padL} y={padT} width={W} height={H} fill="var(--plot-bg)" />

      {/* Heatmap image — base preview (the inline-JSON uint8 grid) is drawn
          stretched LINEARLY across the data extent. Under a log axis the
          preview's row/col grid no longer aligns with the gridlines (which
          are logarithmically spaced via sx/sy), so we hide it whenever
          either axis is in log mode and let the display-tile (which the
          engine resamples log-aware) carry the image alone. During the
          ~60 ms refetch gap that follows a log-toggle the user briefly
          sees the empty plot background — acceptable, vs. showing a
          visibly-misaligned preview. */}
      {dataURL && (
        <g clipPath={`url(#${clipId})`}>
          {!xLogActive && !yLogActive && (
            <image href={dataURL}
              x={imgX} y={imgY} width={imgW} height={imgH}
              preserveAspectRatio="none"
              imageRendering="pixelated" />
          )}
          {tileOverlay && tileOverlay.dataURL
            && tileOverlay.xLog === xLogActive
            && tileOverlay.yLog === yLogActive
            && (() => {
            // axis-xy: cell-index 0 = data y bottom, cell-index N = data y top.
            // tile spans [srcR0, srcR0+srcH] in cell coords; data-y at low end
            // is tyLow, at high end is tyHigh. The renderer's vertical flip
            // (in renderHeatmapDataURLFromFlat) makes canvas row 0 represent
            // the high-cell-index = top of the plot.
            const fullCols = hFullCols || 1;
            const fullRows = hFullRows || 1;
            const xExt = figure.xRange[1] - figure.xRange[0];
            const yExt = figure.yRange[1] - figure.yRange[0];
            // Inverse of the -0.5 shift applied in tile-fetch: data y at a
            // cell-index c (in the shifted system) is yRange[0] + (c+0.5) *
            // cellH = yRange[0] + (c+0.5)/fullRows * yExt.
            let tx0 = figure.xRange[0] + (tileOverlay.srcC0                  + 0.5) / fullCols * xExt;
            let tx1 = figure.xRange[0] + (tileOverlay.srcC0 + tileOverlay.srcW + 0.5) / fullCols * xExt;
            let tyLow  = figure.yRange[0] + (tileOverlay.srcR0                  + 0.5) / fullRows * yExt;
            let tyHigh = figure.yRange[0] + (tileOverlay.srcR0 + tileOverlay.srcH + 0.5) / fullRows * yExt;
            // Under log axes data-coord ≤ 0 is undefined; clamp to the
            // visible viewport bound so sx/sy stay finite. Edge pixels
            // were already snapped to the boundary by the engine's log
            // inverse, so no content shifts.
            if (xLogActive) {
              if (tx0 <= 0) tx0 = xMin;
              if (tx1 <= 0) tx1 = xMin;
            }
            if (yLogActive) {
              if (tyLow  <= 0) tyLow  = yMin;
              if (tyHigh <= 0) tyHigh = yMin;
            }
            const sx0 = sx(tx0);
            const sx1 = sx(tx1);
            const syTop = sy(tyHigh);     // top of element = high data y
            const syBot = sy(tyLow);      // bottom = low data y
            const ox = Math.min(sx0, sx1);
            const oy = Math.min(syTop, syBot);
            const ow = Math.abs(sx1 - sx0);
            const oh = Math.abs(syBot - syTop);
            if (!Number.isFinite(ow) || !Number.isFinite(oh) || ow <= 0 || oh <= 0) return null;
            return (
              <image href={tileOverlay.dataURL}
                x={ox} y={oy} width={ow} height={oh}
                preserveAspectRatio="none"
                imageRendering="pixelated" />
            );
          })()}
        </g>
      )}

      {/* Optional minor + major grid (faint, over the heatmap) */}
      {minor && xTicks.minor.map((v, i) => (
        <line key={`mx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + H} stroke="var(--plot-grid-min)" />
      ))}
      {minor && yTicks.minor.map((v, i) => (
        <line key={`my${i}`} x1={padL} x2={padL + W} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid-min)" />
      ))}
      {major && xTicks.major.map((v, i) => (
        <line key={`gx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + H} stroke="var(--plot-grid)" />
      ))}
      {major && yTicks.major.map((v, i) => (
        <line key={`gy${i}`} x1={padL} x2={padL + W} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid)" />
      ))}

      <rect x={padL} y={padT} width={W} height={H} fill="none" stroke="var(--plot-frame)" />

      {/* Tick labels */}
      {xTicks.major.map((v, i) => {
        const x = sx(v);
        if (x < padL - 1 || x > padL + W + 1) return null;
        return (
          <g key={`xl${i}`}>
            <line x1={x} x2={x} y1={padT + H} y2={padT + H + 4} stroke="var(--plot-tick)" />
            <text x={x} y={padT + H + 14 * fontScale + 2} fill="var(--plot-text)" fontSize={10 * fontScale} textAnchor="middle">{fmtTick(v)}</text>
          </g>
        );
      })}
      {yTicks.major.map((v, i) => {
        const y = sy(v);
        if (y < padT - 1 || y > padT + H + 1) return null;
        return (
          <g key={`yl${i}`}>
            <line x1={padL - 4} x2={padL} y1={y} y2={y} stroke="var(--plot-tick)" />
            <text x={padL - 7} y={y + 3} fill="var(--plot-text)" fontSize={10 * fontScale} textAnchor="end">{fmtTick(v)}</text>
          </g>
        );
      })}

      {/* Series + text layers — drawn in original z-order (= call order in
          script: imagesc → hold on → scatter → text). Coordinates go
          through the panel's current sx/sy so they track pan/zoom + log
          axes automatically. Clipped to plot area so off-screen content
          doesn't bleed into colorbar / axis margins. */}
      {(seriesLayers.length > 0 || textLayers.length > 0) && (
        <g clipPath={`url(#${clipId})`}>
          {layers.map((ly, idx) => {
            if (ly.kind === 'heatmap') return null;            // image already drawn above
            if (ly.kind === 'series') {
              const mode = ly.mode || 'line';
              const w = ly.width || 1.5;
              const op = ly.opacity ?? 1;
              if (mode === 'scatter') {
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)) return null;
                      const px = sx(xv), py = sy(yv);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      return <circle key={i} cx={px} cy={py} r={ly.size || 3}
                        fill={ly.color} stroke="var(--plot-frame)" strokeWidth="0.6" />;
                    })}
                  </g>
                );
              }
              if (mode === 'stem') {
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)) return null;
                      const px = sx(xv), py = sy(yv);
                      const py0 = sy(yLogActive ? yMin : 0);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      return (
                        <g key={i}>
                          <line x1={px} x2={px} y1={py0} y2={py}
                            stroke={ly.color} strokeWidth={w * 0.7} />
                          <circle cx={px} cy={py} r={2.5} fill={ly.color} />
                        </g>
                      );
                    })}
                  </g>
                );
              }
              if (mode === 'bar') {
                // Bar mode: filled rects centred on x; width derived from
                // inter-x spacing. Per-series offset spreads multiple bar
                // layers so they don't overlap exactly.
                const xs = ly.x.filter(Number.isFinite);
                let bw = 8;
                if (xs.length > 1) {
                  const spacing = Math.abs(sx(xs[1]) - sx(xs[0]));
                  bw = Math.max(2, spacing * 0.7);
                }
                const baseY = sy(yLogActive ? yMin : Math.max(0, yMin));
                const sIdx = seriesLayers.indexOf(ly);
                const off = (sIdx - (seriesLayers.length - 1) / 2) * bw * 1.05;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const px = sx(xv) + off, py = sy(ly.y[i]);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const top = Math.min(py, baseY);
                      const h = Math.abs(py - baseY);
                      return <rect key={i} x={px - bw / 2} y={top}
                        width={bw} height={h} fill={ly.color} stroke="none" />;
                    })}
                  </g>
                );
              }
              // 'line' or 'stairs'
              let d = '';
              let started = false;
              for (let i = 0; i < ly.x.length; i++) {
                const xv = ly.x[i], yv = ly.y[i];
                if (!Number.isFinite(xv) || !Number.isFinite(yv)) { started = false; continue; }
                const px = sx(xv), py = sy(yv);
                if (!Number.isFinite(px) || !Number.isFinite(py)) { started = false; continue; }
                if (mode === 'stairs' && started) {
                  d += `L${px.toFixed(2)},${(sy(ly.y[i - 1])).toFixed(2)} `;
                }
                d += (started ? 'L' : 'M') + px.toFixed(2) + ',' + py.toFixed(2) + ' ';
                started = true;
              }
              return <path key={`ly${idx}`} d={d} stroke={ly.color} fill="none"
                strokeWidth={w} opacity={op}
                strokeLinejoin="round" strokeLinecap="round" />;
            }
            if (ly.kind === 'text') {
              const px = sx(ly.x), py = sy(ly.y);
              if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
              return (
                <text key={`ly${idx}`} x={px} y={py}
                  fill={ly.color} fontSize={(ly.fontSize || 11) * fontScale}
                  className="hm-overlay-text"
                  pointerEvents="none">{ly.text}</text>
              );
            }
            return null;
          })}
        </g>
      )}

      {/* Colorbar — only rendered when there's a heatmap layer. Pure
          line/scatter composites skip it (their legend lives on series). */}
      {hasHeatmap && (
        <>
          <rect x={cbarX} y={padT} width={cbarW} height={cbarH}
            fill={`url(#${cbarGradId})`}
            stroke="var(--plot-frame)" strokeWidth="0.5" />
          {cbarTicks.major.map((v, i) => {
            const y = padT + cbarH - ((v - cminEff) / (cmaxEff - cminEff)) * cbarH;
            if (y < padT - 1 || y > padT + cbarH + 1) return null;
            return (
              <g key={`cb${i}`}>
                <line x1={cbarX + cbarW} x2={cbarX + cbarW + 3} y1={y} y2={y} stroke="var(--plot-tick)" />
                <text x={cbarX + cbarW + 6} y={y + 3} fill="var(--plot-text)" fontSize={9 * fontScale} textAnchor="start">{fmtTick(v)}</text>
              </g>
            );
          })}
        </>
      )}

      {/* Axis titles */}
      {figure.xLabel && (
        <text x={padL + W / 2} y={height - 8} fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle">{figure.xLabel}</text>
      )}
      {figure.yLabel && (
        <text x={14} y={padT + H / 2} fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle"
          transform={`rotate(-90 14 ${padT + H / 2})`}>{figure.yLabel}</text>
      )}
      {figure.title && (
        <text x={padL + W / 2} y={padT - 12 * fontScale} fill="var(--plot-text-strong)" fontSize={12 * fontScale} textAnchor="middle">{figure.title}</text>
      )}

      {/* Crosshair + value at hover */}
      {hover && (
        <g pointerEvents="none">
          <line x1={hover.px} x2={hover.px} y1={padT} y2={padT + H} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
          <line x1={padL} x2={padL + W} y1={hover.py} y2={hover.py} stroke="var(--plot-cross)" strokeDasharray="2 3"/>
          {/* Heatmap path: sample the cell at the cursor, reconstruct the
              original-domain value via cminOrig + (idx/254) * range. If
              colorScaleBaked === 'log', the reconstructed value is in log10
              space — show 10^v as the "real" scalar. */}
          {hasHeatmap && (() => {
            if (!hZ) return null;
            const nR = hZ.length, nC = hZ[0]?.length || 0;
            if (!nR || !nC) return null;
            const u = (hover.x - figure.xRange[0]) / (figure.xRange[1] - figure.xRange[0]);
            const v = (figure.yRange[1] - hover.y) / (figure.yRange[1] - figure.yRange[0]);
            const c = Math.max(0, Math.min(nC - 1, Math.floor(u * nC)));
            const r = Math.max(0, Math.floor(v * nR));
            const rr = Math.max(0, Math.min(nR - 1, r));
            const idx = hZ[rr]?.[c];
            let zStr = '—';
            if (idx != null && idx !== 255) {
              const range = cmaxOrig - cminOrig;
              const reconstructed = cminOrig + (idx / 254) * range;
              if (hColorScaleBaked === 'log') {
                zStr = `10^${fmtTick(reconstructed)} ≈ ${fmtTick(Math.pow(10, reconstructed))}`;
              } else {
                zStr = fmtTick(reconstructed);
              }
            }
            return (
              <g transform={`translate(${Math.min(hover.px + 8, padL + W - 110)}, ${Math.max(hover.py - 38, padT + 4)})`}>
                <rect width="104" height="36" fill="var(--plot-tip-bg)" stroke="var(--plot-cross)" rx="3" />
                <text x="6" y="11" fill="var(--plot-tip-text)" fontSize="10">x = {fmtTick(hover.x)}</text>
                <text x="6" y="22" fill="var(--plot-tip-text)" fontSize="10">y = {fmtTick(hover.y)}</text>
                <text x="6" y="33" fill="var(--plot-tip-text)" fontSize="10">z = {zStr}</text>
              </g>
            );
          })()}

          {/* Series path (no heatmap): snap to nearest x-point on the
              first series layer. Renders a marker + (x, y) tooltip. */}
          {!hasHeatmap && seriesLayers.length > 0 && (() => {
            const s = seriesLayers[0];
            if (!s.x?.length) return null;
            let bestI = 0, bestD = Infinity;
            for (let i = 0; i < s.x.length; i++) {
              const d = Math.abs(s.x[i] - hover.x);
              if (d < bestD) { bestD = d; bestI = i; }
            }
            const hx = s.x[bestI], hy = s.y[bestI];
            if (!Number.isFinite(hx) || !Number.isFinite(hy)) return null;
            return (
              <>
                <circle cx={sx(hx)} cy={sy(hy)} r="3"
                  fill={s.color} stroke="white" strokeWidth="1" />
                <g transform={`translate(${Math.min(hover.px + 8, padL + W - 110)}, ${Math.max(hover.py - 28, padT + 4)})`}>
                  <rect width="104" height="26" fill="var(--plot-tip-bg)" stroke="var(--plot-cross)" rx="3" />
                  <text x="6" y="11" fill="var(--plot-tip-text)" fontSize="10">x = {fmtTick(hx)}</text>
                  <text x="6" y="22" fill="var(--plot-tip-text)" fontSize="10">y = {fmtTick(hy)}</text>
                </g>
              </>
            );
          })()}
        </g>
      )}
    </svg>
    </>
  );
}
