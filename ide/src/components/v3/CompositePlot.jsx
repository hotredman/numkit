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
         renderHeatmapDataURLFromFlat, getColormap,
         makeCustomColormap } from './colormaps';
import ContextMenu, { foldRowsToSubmenu } from './ContextMenu';
import { computeFitViewport, exportSvgNode, exportPngNode, exportPngForPrint, downloadBlob } from './plotUtils';

// MATLAB linespec → SVG strokeDasharray. '-' (or absent) means solid;
// returning undefined keeps the default solid stroke. Pixel patterns
// roughly match MATLAB defaults (LineStyle '--' ≈ 4 px on / 4 px off).
const DASH_FOR = { '-': undefined, '--': '6,4', ':': '1,3', '-.': '6,3,1,3' };

// SVG marker glyph dispatcher used by both line-overlay and scatter
// modes. r is the half-size in pixels (matches MATLAB MarkerSize ≈ r).
function MarkerGlyph({ marker, cx, cy, r, color, idx }) {
  const stroke = 'var(--plot-frame)';
  const sw = 0.6;
  switch (marker) {
    case 'o':
      return <circle key={idx} cx={cx} cy={cy} r={r} fill={color} stroke={stroke} strokeWidth={sw} />;
    case 's':
      return <rect key={idx} x={cx - r} y={cy - r} width={r * 2} height={r * 2}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    case 'd':
      return <path key={idx} d={`M${cx},${cy - r} L${cx + r},${cy} L${cx},${cy + r} L${cx - r},${cy} Z`}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    case '^':
      return <path key={idx} d={`M${cx},${cy - r} L${cx + r},${cy + r} L${cx - r},${cy + r} Z`}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    case 'v':
      return <path key={idx} d={`M${cx},${cy + r} L${cx + r},${cy - r} L${cx - r},${cy - r} Z`}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    case '<':
      return <path key={idx} d={`M${cx - r},${cy} L${cx + r},${cy - r} L${cx + r},${cy + r} Z`}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    case '>':
      return <path key={idx} d={`M${cx + r},${cy} L${cx - r},${cy - r} L${cx - r},${cy + r} Z`}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    case 'p': {  // pentagon (5-pointed) — approximate
      const pts = [];
      for (let k = 0; k < 5; k++) {
        const a = -Math.PI / 2 + k * (2 * Math.PI / 5);
        pts.push(`${cx + r * Math.cos(a)},${cy + r * Math.sin(a)}`);
      }
      return <polygon key={idx} points={pts.join(' ')}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    }
    case 'h': {  // hexagon
      const pts = [];
      for (let k = 0; k < 6; k++) {
        const a = k * (Math.PI / 3);
        pts.push(`${cx + r * Math.cos(a)},${cy + r * Math.sin(a)}`);
      }
      return <polygon key={idx} points={pts.join(' ')}
        fill={color} stroke={stroke} strokeWidth={sw} />;
    }
    case '+':
      return (
        <g key={idx}>
          <line x1={cx - r} y1={cy} x2={cx + r} y2={cy} stroke={color} strokeWidth={1.5} />
          <line x1={cx} y1={cy - r} x2={cx} y2={cy + r} stroke={color} strokeWidth={1.5} />
        </g>
      );
    case 'x':
      return (
        <g key={idx}>
          <line x1={cx - r} y1={cy - r} x2={cx + r} y2={cy + r} stroke={color} strokeWidth={1.5} />
          <line x1={cx - r} y1={cy + r} x2={cx + r} y2={cy - r} stroke={color} strokeWidth={1.5} />
        </g>
      );
    case '*':
      return (
        <g key={idx}>
          <line x1={cx - r} y1={cy} x2={cx + r} y2={cy} stroke={color} strokeWidth={1.2} />
          <line x1={cx} y1={cy - r} x2={cx} y2={cy + r} stroke={color} strokeWidth={1.2} />
          <line x1={cx - r * 0.7} y1={cy - r * 0.7} x2={cx + r * 0.7} y2={cy + r * 0.7} stroke={color} strokeWidth={1.2} />
          <line x1={cx - r * 0.7} y1={cy + r * 0.7} x2={cx + r * 0.7} y2={cy - r * 0.7} stroke={color} strokeWidth={1.2} />
        </g>
      );
    case '.':
      return <circle key={idx} cx={cx} cy={cy} r={Math.max(1, r * 0.4)}
        fill={color} stroke="none" />;
    default:
      return <circle key={idx} cx={cx} cy={cy} r={r} fill={color} stroke={stroke} strokeWidth={sw} />;
  }
}

/** Resolve the display name for one series layer. Priority matches
 *  MATLAB / the legend block:
 *    1. `figure.legend[idx]`  — the i-th label passed to legend(...)
 *    2. `layer.name`          — the script-set DisplayName on the layer
 *    3. `series ${idx+1}`     — guaranteed-non-empty fallback
 *  Whitespace-only legend strings are treated as empty (fall through).
 *  Used by both the in-figure legend block AND the ПКМ Fit Series ▶
 *  rows so they always agree on what to call each curve. */
function resolveSeriesName(figure, layer, idx) {
  const fromLegend = ((figure.legend && figure.legend[idx]) || '').trim();
  return fromLegend || layer.name || `series ${idx + 1}`;
}

export default function CompositePlot({
  figure,
  width,
  height,
  viewport,
  setViewport,
  major = true,
  minor = true,
  // Per-axis grid (MATLAB XGrid / YGrid / XMinorGrid / YMinorGrid).
  // Default = the combined `major` / `minor` so callers that haven't
  // migrated keep current behaviour. When the parent supplies the
  // per-axis prop it wins, allowing fine-grained per-axis control.
  xGrid: xGridProp,
  yGrid: yGridProp,
  xMinor: xMinorProp,
  yMinor: yMinorProp,
  // MATLAB axis-visibility / box / direction overrides. undefined =
  // fall back to the figure JSON (script-set value).
  axisVisible: axisVisibleProp,
  boxOn: boxOnProp,
  xReverse: xReverseProp,
  yReverse: yReverseProp,
  // Z-direction is a property of Axes too. No visual effect on 2-D,
  // wired through so ПКМ axes ▶ stays parity-clean with the toolbar.
  zReverse: zReverseProp,
  // Legend / colorbar placement overrides — null/undefined = follow
  // script's figure.legendLocation / figure.colorbarLocation.
  legendLocation: legendLocationProp,
  colorbarLocation: colorbarLocationProp,
  showLegend = true,
  // Visibility flags owned by FigureWindow's display menus. Default
  // true so non-modal renderers (preview cards, subplot cells without
  // explicit prop forwarding) keep the script's text visible.
  showTitle  = true,
  showXLabel = true,
  showYLabel = true,
  showZLabel = false,
  // ПКМ submenu setters — when provided (modal context), the right-
  // click ПКМ surfaces full Axes ▶ / Decoration ▶ submenus mirroring
  // the toolbar popovers. In preview cards / subplot cells these
  // aren't passed; the submenu rows are simply omitted in that case.
  setShowMajor  = null,
  setShowMinor  = null,
  setXGrid      = null,
  setYGrid      = null,
  setShowAxis   = null,
  setShowBox    = null,
  setXReverse   = null,
  setYReverse   = null,
  setZReverse   = null,
  setShowTitle  = null,
  setShowXLabel = null,
  setShowYLabel = null,
  setShowZLabel = null,
  setShowLegend = null,
  setLegendLocation   = null,
  setColorbarLocation = null,
  // Colorbar visibility — true → render at script-set location or
  // 'east' default; false → hide; null → follow script (figure.color
  // barLocation). Preview cards / standalone use null so they stay in
  // sync with the script. FigureWindow passes a real boolean.
  showColorbar = null,
  setShowColorbar = null,
  // ПКМ bridge — top-level Reset + Save/Export bound from FigureWindow.
  // Each is a no-arg handler; absent → corresponding ПКМ row is omitted.
  onResetAll          = null,
  onExportSvg         = null,
  onExportPng2x       = null,
  onExportPngPrint85  = null,
  onExportPngPrint170 = null,
  onExportPngPrint210 = null,
  onExportCsv         = null,
  onExportTsv         = null,
  onExportJson        = null,
  fontScale = 1,
  interactive = true,
  engine = null,
  // Log axis state owned by FigureWindow so the toolbar buttons + the
  // ПКМ menu inside the panel stay in sync. Defaults respect the
  // figure's xscale/yscale config when no parent provides setters.
  xLog: xLogProp,
  yLog: yLogProp,
  zLog: zLogProp,
  setXLog: setXLogProp,
  setYLog: setYLogProp,
  setZLog: setZLogProp,
  colorOverride: colorOverrideProp,
  setColorOverride: setColorOverrideProp,
  colormapOverride = null,
  setColormapOverride = null,
  // Per-cell reset callbacks for the ПКМ Display ▶ reset / Colormap ▶
  // reset rows. SubplotGrid wires these to clear THIS cell's overrides
  // (cell falls back to figure-wide); for non-subplot CompositePlot the
  // parent (FigureWindow) wires figure-wide displayReset / setColormap
  // Override(null) instead.
  onDisplayReset = null,
  onColormapReset = null,
}) {
  // Layers — empty array if none. The renderer walks them in order so the
  // user controls z-order via call sequence (heatmap first, scatter on top,
  // text labels last — exactly mirroring `imagesc; hold on; scatter; text`).
  const layers = Array.isArray(figure.layers) ? figure.layers : [];
  const heatmapLayer = layers.find((l) => l.kind === 'heatmap') || null;
  const rgbLayer = layers.find((l) => l.kind === 'image-rgb') || null;
  const seriesLayers = layers.filter((l) => l.kind === 'series');
  // Resolve per-axis grid flags. Per-axis prop wins; otherwise fall
  // back to the combined major/minor (legacy behavior).
  const xGridOn = (xGridProp !== undefined) ? !!xGridProp : !!major;
  const yGridOn = (yGridProp !== undefined) ? !!yGridProp : !!major;
  const xMinorOn = (xMinorProp !== undefined) ? !!xMinorProp : !!minor;
  const yMinorOn = (yMinorProp !== undefined) ? !!yMinorProp : !!minor;
  const textLayers = layers.filter((l) => l.kind === 'text');
  const hasHeatmap = !!heatmapLayer;
  // imshow's defining trait — hide axis ticks/labels/box. Default true
  // preserves the existing wire format for figures that didn't set it.
  const axisVisible = (axisVisibleProp !== undefined)
    ? !!axisVisibleProp : (figure.axisVisible !== false);
  const boxOn = (boxOnProp !== undefined) ? !!boxOnProp : (figure.boxOn !== false);

  // Effective colormap: runtime override (toolbar combo) > script-level
  // colormap on the heatmap layer > default 'parula'. A custom M×3
  // RGB matrix (from colormap(M)) wins over the named map when
  // present.
  const customColormap = figure.customColormap;
  const effectiveColormap = (Array.isArray(customColormap)
                             && customColormap.length > 0)
    ? makeCustomColormap(customColormap)
    : (colormapOverride || heatmapLayer?.colormap || 'parula');

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
  // Z scale state — only meaningful for 3-D, but wired through so the
  // ПКМ axes ▶ submenu (which mirrors the toolbar) can flip it
  // without a separate code path. No local fallback: the toggle is
  // simply omitted when the parent doesn't supply setZLog.
  const zLog = !!zLogProp;

  // Auto-clamp viewport when xLog/yLog flips on but the visible range
  // includes ≤0 — log mapping needs strictly positive bounds. The ПКМ
  // path inside CompositePlot used to do this inline, but the toolbar
  // (and toolbar-fanned subplot updates) only flip the flag, leaving
  // the per-cell viewport untouched. Effect makes the clamp universal:
  // any code path that sets xLog/yLog to true with an invalid viewport
  // gets a sane log range without re-implementing the math.
  useEffect(() => {
    if (!xLog || !setViewport || !viewport || !viewport.x) return;
    const [xMinV, xMaxV] = viewport.x;
    if (xMinV > 0 && xMaxV > 0) return;
    const hi = Math.max(figure.xRange?.[1] || xMaxV, 1e-6);
    const lo = Math.max(hi / 1e4, 1e-6);
    const hiClamped = Math.max(lo * 10, hi);
    setViewport({ ...viewport, x: [lo, hiClamped] });
  }, [xLog]);  // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    if (!yLog || !setViewport || !viewport || !viewport.y) return;
    const [yMinV, yMaxV] = viewport.y;
    if (yMinV > 0 && yMaxV > 0) return;
    const hi = Math.max(figure.yRange?.[1] || yMaxV, 1e-6);
    const lo = Math.max(hi / 1e4, 1e-6);
    const hiClamped = Math.max(lo * 10, hi);
    setViewport({ ...viewport, y: [lo, hiClamped] });
  }, [yLog]);  // eslint-disable-line react-hooks/exhaustive-deps

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

  // Comet-animation progress [0, 1]. When any layer has cometAnim
  // set, we step from 0 → 1 over ~1.5s via requestAnimationFrame.
  // Each cometAnim layer renders only its first floor(progress·N)
  // points; static layers render fully.
  const hasCometAnim = useMemo(
    () => layers.some((l) => l.kind === 'series' && l.cometAnim),
    [layers]
  );
  const [cometProgress, setCometProgress] = useState(0);
  useEffect(() => {
    if (!hasCometAnim) { setCometProgress(1); return undefined; }
    setCometProgress(0);
    const start = performance.now();
    const duration = 1500;   // ms
    let raf = 0;
    const tick = (now) => {
      const t = Math.min(1, (now - start) / duration);
      setCometProgress(t);
      if (t < 1) raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [hasCometAnim, figure.id]);
  const cminEff = colorOverride ? colorOverride.cmin : hcminOrig;
  const cmaxEff = colorOverride ? colorOverride.cmax : hcmaxOrig;
  const cminOrig = hcminOrig;
  const cmaxOrig = hcmaxOrig;

  // 256-entry RGBA LUT — rebuilt only when colormap or window/level changes.
  const lut = useMemo(
    () => buildHeatmapLUT(effectiveColormap, cminOrig, cmaxOrig, cminEff, cmaxEff),
    [effectiveColormap, cminOrig, cmaxOrig, cminEff, cmaxEff]
  );

  let padL = 60 * fontScale;
  // Right pad: extra space for colorbar OR for a yyaxis right-side axis.
  // Both reserve roughly the same width for tick labels.
  const padR = (hasHeatmap || figure.yyEnabled) ? 70 * fontScale : 18;
  let padT = 36 * fontScale;
  const padB = 44 * fontScale;
  // Force integer dims — non-integer panel sizes from fractional fontScale
  // would produce a diagonal-stripe artefact in the tile renderer because
  // row strides drift by frac-cols each iteration when arr is indexed.
  // Plot-area dimensions before axisMode adjustments.
  const W0 = Math.max(50, Math.floor(width - padL - padR));
  const H0 = Math.max(50, Math.floor(height - padT - padB));
  let W = W0;
  let H = H0;

  // axisMode === 'square' forces the plot box itself to be square
  // (equal screen pixels for both axes' EXTENT, regardless of data).
  // Apply by shrinking the larger dimension to the smaller.
  if (figure.axisMode === 'square') {
    const side = Math.min(W, H);
    W = side; H = side;
  }

  let [xMin, xMax] = viewport.x;
  let [yMin, yMax] = viewport.y;

  // axisMode === 'image' (imshow / `axis image`) AND axisMode ===
  // 'equal' (`axis equal`) both pin DataAspectRatio = [1 1 1]: 1 data
  // unit on X must occupy the same number of screen pixels as 1 data
  // unit on Y. We honour that by SHRINKING the panel to match the data
  // aspect (letterboxing the unused side) rather than expanding the
  // viewport — MATLAB R2025b's behaviour when xlim/ylim are explicit
  // is to keep the limits and resize the plot box.
  //
  // The difference between the two modes is in *how* xRange/yRange got
  // computed (image is also `axis tight`, equal keeps script margins),
  // not in how the panel is rendered. Both paths shrink the panel.
  if (figure.axisMode === 'image' || figure.axisMode === 'equal') {
    const dx = xMax - xMin;
    const dy = yMax - yMin;
    if (dx > 0 && dy > 0
        && !(xLog && xMin > 0 && xMax > 0)
        && !(yLog && yMin > 0 && yMax > 0)) {
      const dataAspect  = dx / dy;
      const panelAspect = W / H;
      if (panelAspect > dataAspect) {
        W = Math.max(20, Math.floor(H * dataAspect));
      } else if (panelAspect < dataAspect) {
        H = Math.max(20, Math.floor(W / dataAspect));
      }
    }
  }

  // Centre the (possibly shrunken) plot panel inside its allotted cell.
  // `square`/`image`/`equal`-with-data-shrink all reduce W or H below the
  // raw width − padL − padR; without this the panel sticks to the
  // top-left corner of the cell with all leftover space on the right /
  // bottom. Distributing the leftover evenly puts axes-equal/image plots
  // (and especially imshow tiles inside subplots) in the visual centre.
  //
  // When axes are hidden (imshow's axisVisible=false), the original
  // padL/padR reservation for tick labels is wasted space — centre the
  // panel inside the FULL cell instead, so an `imshow(I)` tile sits
  // dead-centre with equal margins on both sides. Colorbar position is
  // anchored to padL + W, so it shifts right with padL and stays attached.
  if (figure.axisVisible === false) {
    padL = Math.max(0, Math.floor((width  - W) / 2));
    padT = Math.max(0, Math.floor((height - H) / 2));
  } else {
    padL += Math.max(0, Math.floor((W0 - W) / 2));
    padT += Math.max(0, Math.floor((H0 - H) / 2));
  }

  // axis equal: panel-shrink path above already enforces 1 data unit
  // X = 1 data unit Y by adjusting W/H. The old viewport-EXTENSION
  // path that used to live here (widening xRange or yRange to fill a
  // rectangular panel) was reverted — it broke explicit xlim/ylim
  // calls in MATLAB-parity scripts like communications/qam_constellation.
  // Log axes: viewport bounds are still in original-data coordinates
  // (xMin..xMax = the user-visible range). The screen-mapping is log when
  // the corresponding axis flag is on. Requires lo > 0 — we sanitise by
  // clamping at the call sites that set viewport.
  const xLogActive = xLog && xMin > 0 && xMax > 0;
  const yLogActive = yLog && yMin > 0 && yMax > 0;
  // Axis direction. MATLAB: set(gca, 'XDir'/'YDir', 'reverse') flips
  // the corresponding axis. xDir='reverse' means x increases right→left;
  // yDir='reverse' means y increases top→bottom (the default for image
  // axes, but here it's an explicit user request, separate from imagesc).
  const xRev = (xReverseProp !== undefined) ? !!xReverseProp : (figure.xDir === 'reverse');
  const yRev = (yReverseProp !== undefined) ? !!yReverseProp : (figure.yDir === 'reverse');
  // zRev: read-only echo for ПКМ axes ▶ ✓ marker. No 2-D renderer
  // path uses it.
  const zRev = !!zReverseProp;
  const sx = xLogActive
    ? (xRev
       ? (v) => padL + W - (Math.log(v / xMin) / Math.log(xMax / xMin)) * W
       : (v) => padL + (Math.log(v / xMin) / Math.log(xMax / xMin)) * W)
    : (xRev
       ? (v) => padL + W - ((v - xMin) / (xMax - xMin)) * W
       : (v) => padL + ((v - xMin) / (xMax - xMin)) * W);
  const sy = yLogActive
    ? (yRev
       ? (v) => padT + (Math.log(v / yMin) / Math.log(yMax / yMin)) * H
       : (v) => padT + H - (Math.log(v / yMin) / Math.log(yMax / yMin)) * H)
    : (yRev
       ? (v) => padT + ((v - yMin) / (yMax - yMin)) * H
       : (v) => padT + H - ((v - yMin) / (yMax - yMin)) * H);
  const isx = xLogActive
    ? (xRev
       ? (px) => xMin * Math.exp(((padL + W - px) / W) * Math.log(xMax / xMin))
       : (px) => xMin * Math.exp(((px - padL) / W) * Math.log(xMax / xMin)))
    : (xRev
       ? (px) => xMax - ((px - padL) / W) * (xMax - xMin)
       : (px) => xMin + ((px - padL) / W) * (xMax - xMin));
  const isy = yLogActive
    ? (yRev
       ? (py) => yMin * Math.exp(((py - padT) / H) * Math.log(yMax / yMin))
       : (py) => yMin * Math.exp(((padT + H - py) / H) * Math.log(yMax / yMin)))
    : (yRev
       ? (py) => yMin + ((py - padT) / H) * (yMax - yMin)
       : (py) => yMax - ((py - padT) / H) * (yMax - yMin));

  // ── yyaxis: secondary Y mapping (right side) ─────────────────────
  // yRange2 stays at its auto-fit value — we don't pan/zoom the right
  // axis with the viewport. This matches MATLAB's "linked yyaxis"
  // default and keeps the model simple. yscale2 'log' is supported.
  const yy2 = figure.yyEnabled && Array.isArray(figure.yRange2);
  const yMin2 = yy2 ? figure.yRange2[0] : 0;
  const yMax2 = yy2 ? figure.yRange2[1] : 1;
  const yLog2 = figure.yscale2 === 'log';
  const yLog2Active = yy2 && yLog2 && yMin2 > 0 && yMax2 > 0;
  const sy2 = !yy2
    ? sy
    : (yLog2Active
       ? (v) => padT + H - (Math.log(v / yMin2) / Math.log(yMax2 / yMin2)) * H
       : (v) => padT + H - ((v - yMin2) / (yMax2 - yMin2)) * H);
  // syOf — pick sy or sy2 by layer.yside. Used in the per-layer render
  // loops (line/scatter/bar/etc.) so right-side data lands on the right
  // mapping without each block re-doing the conditional.
  const syOf = (ly) => (yy2 && ly && ly.yside === 'right') ? sy2 : sy;

  // Pre-render the inline preview to a dataURL via the LUT. uint8 indices
  // are stable; only the LUT changes on window/level — so we keep a separate
  // memo on (z) and another on (lut) chained together. Skips entirely if
  // there's no heatmap layer.
  const dataURL = useMemo(() => {
    if (!hZ) return null;
    // Same flip-CONDITIONAL-on-yDir logic as the tile overlay below.
    return renderHeatmapDataURLFromIndices(hZ, lut, !yRev);
  }, [hZ, lut, yRev]);

  // RGB / RGBA image (imshow with M×N×3 or M×N×4). Pack the per-pixel
  // tuples into a Uint8ClampedArray, push through an off-screen
  // <canvas>, export as PNG data-URL. Memoised on rgbLayer.rgb identity.
  // Wire format always carries 4 ints per pixel — alpha=255 for plain
  // RGB input, true alpha for RGBA. Renderer is unchanged either way.
  const rgbDataURL = useMemo(() => {
    if (!rgbLayer || !rgbLayer.rgb) return null;
    const rgb = rgbLayer.rgb;
    const nR = rgbLayer.nR | 0;
    const nC = rgbLayer.nC | 0;
    if (nR <= 0 || nC <= 0) return null;
    try {
      const cv = document.createElement('canvas');
      cv.width = nC; cv.height = nR;
      const ctx2 = cv.getContext('2d');
      const imgData = ctx2.createImageData(nC, nR);
      // rgb[r][c] = [r,g,b,a] (uint8). Older 3-tuple wire format
      // (pre-RGBA) still works — missing alpha defaults to 255.
      for (let r = 0, p = 0; r < nR; r++) {
        const row = rgb[r];
        for (let c = 0; c < nC; c++, p += 4) {
          const tri = row[c] || [0, 0, 0, 255];
          imgData.data[p]     = tri[0] | 0;
          imgData.data[p + 1] = tri[1] | 0;
          imgData.data[p + 2] = tri[2] | 0;
          imgData.data[p + 3] = (tri[3] === undefined) ? 255 : (tri[3] | 0);
        }
      }
      ctx2.putImageData(imgData, 0, 0);
      return cv.toDataURL('image/png');
    } catch (e) {
      return null;
    }
  }, [rgbLayer]);

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
  // Custom tick positions from xticks() / yticks() override the
  // auto-generated set. Filter to the visible range so off-screen
  // ticks don't bleed into the margin.
  const xTicksAuto = xLogActive ? logTicks(xMin, xMax) : niceTicks(xMin, xMax, 8);
  const yTicksAuto = yLogActive ? logTicks(yMin, yMax) : niceTicks(yMin, yMax, 6);
  const xTicks = (Array.isArray(figure.xTicks) && figure.xTicks.length > 0)
    ? { major: figure.xTicks.filter((v) => v >= xMin && v <= xMax), minor: [] }
    : xTicksAuto;
  const yTicks = (Array.isArray(figure.yTicks) && figure.yTicks.length > 0)
    ? { major: figure.yTicks.filter((v) => v >= yMin && v <= yMax), minor: [] }
    : yTicksAuto;

  // sprintf-style format applier — supports the most common subset
  // of MATLAB's tick formats: %d, %f, %.Nf, %e, %.Ne, %g, %.Ng. No
  // multi-arg printf semantics needed (one numeric value).
  function applyTickFormat(fmt, v) {
    if (!fmt) return null;
    const m = fmt.match(/^%(?:\.(\d+))?([defg])$/);
    if (!m) return null;
    const prec = m[1] !== undefined ? Number(m[1]) : 6;
    const conv = m[2];
    if (conv === 'd') return String(Math.round(v));
    if (conv === 'f') return v.toFixed(prec);
    if (conv === 'e') return v.toExponential(prec);
    if (conv === 'g') return v.toPrecision(prec);
    return null;
  }
  // Custom-label lookups: when xticklabels(["a","b","c"]) was called
  // and matches the xticks count, we substitute the string directly
  // for that tick's numeric format. Otherwise xtickformat fmt string
  // wins; otherwise auto fmtTick.
  const fmtXTickLabel = (v, i) => {
    if (Array.isArray(figure.xTickLabels)
        && figure.xTickLabels.length > 0
        && Array.isArray(figure.xTicks)
        && figure.xTicks.length === figure.xTickLabels.length
        && i < figure.xTickLabels.length) {
      return String(figure.xTickLabels[i]);
    }
    if (figure.xTickFormat) {
      const out = applyTickFormat(figure.xTickFormat, v);
      if (out !== null) return out;
    }
    return fmtTick(v);
  };
  const fmtYTickLabel = (v, i) => {
    if (Array.isArray(figure.yTickLabels)
        && figure.yTickLabels.length > 0
        && Array.isArray(figure.yTicks)
        && figure.yTicks.length === figure.yTickLabels.length
        && i < figure.yTickLabels.length) {
      return String(figure.yTickLabels[i]);
    }
    if (figure.yTickFormat) {
      const out = applyTickFormat(figure.yTickFormat, v);
      if (out !== null) return out;
    }
    return fmtTick(v);
  };
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
    // Pan with reversed axes (xDir/yDir = 'reverse' — imshow / axis ij /
    // explicit user request) needs the screen-delta inverted, otherwise
    // direct-manipulation breaks: drag mouse up on imshow → image went
    // DOWN because dy was added to viewport.y unconditionally. Flip the
    // raw screen-fraction up front so all downstream math (linear AND
    // log) inherits the correct sign without per-branch tweaks.
    const xPxFrac = (e.clientX - d.sx) / (d.W * (rect.width / width)) * (xRev ? -1 : 1);
    const yPxFrac = (e.clientY - d.sy) / (d.H * (rect.height / height)) * (yRev ? -1 : 1);
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
  // Top-level Reset — prefer parent-supplied handler (modal: full reset
  // of viewport + display state, fans out to every cell in subplot
  // mode). Fallback to local viewport-only reset for preview cards
  // / standalone usage.
  const onReset = onResetAll || (() => {
    setViewport({ x: figure.xRange.slice(), y: figure.yRange.slice() });
    setColorOverride(null);
    setXLog(false);
    setYLog(false);
  });
  // Save/Export — bundle into a submenu when parent provided handlers,
  // else fall back to local SVG-node exports (preview cards still get
  // PNG @2× via the local helpers).
  const useParentExport = !!(onExportSvg || onExportPng2x);
  const exportItems = useParentExport ? [
    { head: 'image · screen' },
    { label: 'SVG (vector)',  onClick: onExportSvg,    disabled: !onExportSvg },
    { label: 'PNG @2×',       onClick: onExportPng2x,  disabled: !onExportPng2x },
    { head: 'image · print (300 DPI)' },
    { label: 'PNG · 1 column (85 mm)',  onClick: onExportPngPrint85,  disabled: !onExportPngPrint85 },
    { label: 'PNG · 2 columns (170 mm)', onClick: onExportPngPrint170, disabled: !onExportPngPrint170 },
    { label: 'PNG · A4 width (210 mm)',  onClick: onExportPngPrint210, disabled: !onExportPngPrint210 },
    { head: 'data' },
    { label: 'CSV',  onClick: onExportCsv,  disabled: !onExportCsv },
    { label: 'TSV',  onClick: onExportTsv,  disabled: !onExportTsv },
    { label: 'JSON', onClick: onExportJson, disabled: !onExportJson },
  ] : (() => {
    // Local fallback exporters — used by subplot cells (where parent
    // handlers would save the WHOLE figure, not just this cell). The
    // image exporters use the cell's own svgRef; data exporters walk
    // figure.layers (already cell-scoped when SubplotGrid hands off
    // each cell as `figure`).
    //
    // Filename uses cell.subplotIndex / id when available so multiple
    // saves from different cells don't collide. Falls back to figure.id.
    const tag = figure.subplotIndex
      ? `figure_${figure.id}_cell${figure.subplotIndex}`
      : `figure_${figure.id}`;
    // Build a series-row table per layer: alternating x_<name>, y_<name>
    // columns, blank cells for ragged lengths.
    const dataLayers = (figure.layers || []).filter(
      (ly) => ly.kind === 'series' && Array.isArray(ly.x) && Array.isArray(ly.y)
    );
    function seriesRows(sep) {
      if (dataLayers.length === 0) return '';
      const head = dataLayers
        .flatMap((ly, i) => [`x_${ly.name || 'series' + (i + 1)}`,
                             `y_${ly.name || 'series' + (i + 1)}`])
        .join(sep);
      const N = dataLayers.reduce((m, ly) => Math.max(m, ly.x.length), 0);
      const rows = [head];
      for (let i = 0; i < N; i++) {
        const cells = dataLayers.flatMap((ly) => [
          ly.x[i] != null ? String(ly.x[i]) : '',
          ly.y[i] != null ? String(ly.y[i]) : '',
        ]);
        rows.push(cells.join(sep));
      }
      return rows.join('\n');
    }
    function dumpCsv() {
      downloadBlob(new Blob([seriesRows(',')], { type: 'text/csv' }), `${tag}.csv`);
    }
    function dumpTsv() {
      downloadBlob(new Blob([seriesRows('\t')], { type: 'text/tab-separated-values' }), `${tag}.tsv`);
    }
    function dumpJson() {
      const payload = {
        id: figure.id,
        cellIndex: figure.subplotIndex || null,
        title: figure.title || '',
        xLabel: figure.xLabel || '', yLabel: figure.yLabel || '',
        xRange: figure.xRange, yRange: figure.yRange,
        layers: (figure.layers || []).map((ly) => {
          const out = { kind: ly.kind, mode: ly.mode || '', name: ly.name || '',
                        color: ly.color || '' };
          if (Array.isArray(ly.x)) out.x = ly.x;
          if (Array.isArray(ly.y)) out.y = ly.y;
          if (Array.isArray(ly.z)) out.z = ly.z;
          return out;
        }),
      };
      downloadBlob(new Blob([JSON.stringify(payload, null, 2)],
        { type: 'application/json' }), `${tag}.json`);
    }
    const dataDisabled = dataLayers.length === 0;
    return [
      { head: 'image · screen' },
      { label: 'SVG (vector)',
        onClick: () => exportSvgNode(svgRef.current, `${tag}.svg`) },
      { label: 'PNG @2×',
        onClick: () => exportPngNode(svgRef.current, width, height, 2, `${tag}.png`) },
      { head: 'image · print (300 DPI)' },
      { label: 'PNG · 1 column (85 mm)',
        onClick: () => exportPngForPrint(svgRef.current, width, height, 85, 300, tag) },
      { label: 'PNG · 2 columns (170 mm)',
        onClick: () => exportPngForPrint(svgRef.current, width, height, 170, 300, tag) },
      { label: 'PNG · A4 width (210 mm)',
        onClick: () => exportPngForPrint(svgRef.current, width, height, 210, 300, tag) },
      { head: 'data' },
      { label: 'CSV',  onClick: dumpCsv,  disabled: dataDisabled },
      { label: 'TSV',  onClick: dumpTsv,  disabled: dataDisabled },
      { label: 'JSON', onClick: dumpJson, disabled: dataDisabled },
    ];
  })();
  // ── ПКМ submenus: Axes ▶ / Decoration ▶ ─────────────────────────
  // Specialised — show only what's relevant to THIS plot. CompositePlot
  // is always a 2-D context (3-D figures use Composite3DPlot), so Z
  // toggles are simply absent here. Legend lives in Decoration ▶
  // only when there's at least one series; colorbar + Location only
  // when there's a heatmap. The toolbar popovers stay universal —
  // everything always visible — but ПКМ is per-plot context.
  //
  // Every toggle row carries `keepOpen: true` so the user can flip
  // several values without re-summoning the menu. One-shot rows
  // (`default`, palette pick, Fit option, Save/Export choice, Location
  // pick) leave keepOpen unset and close on click per OS convention.
  const tag = (active, label) => active ? `✓ ${label}` : label;

  const axesSubmenuItems = (setShowMajor || setShowMinor
      || setXGrid || setYGrid
      || setShowAxis || setShowBox
      || setXReverse || setYReverse
      || setXLog || setYLog) ? [
    ...(onDisplayReset ? [{ label: 'default', onClick: onDisplayReset },
                          { separator: true }] : []),
    { head: 'visible' },
    ...(setShowAxis ? [{ label: tag(axisVisible, 'axis'), keepOpen: true,
                         onClick: () => setShowAxis((v) => !v) }] : []),
    // Box is masked when axis is off — MATLAB HG2: Axes.Visible='off'
    // hides the Box regardless of Box='on'. State is preserved; row
    // stays clickable so the user can pre-set a value.
    ...(setShowBox  ? [{ label: tag(boxOn, 'box'), keepOpen: true,
                         masked: !axisVisible,
                         maskedHint: 'Box is hidden because axis is off.',
                         onClick: () => setShowBox((v) => !v) }] : []),
    { head: 'grid' },
    ...(setShowMajor ? [{ label: tag(major, 'grid'), keepOpen: true,
                          onClick: () => setShowMajor((v) => !v) }] : []),
    ...(setXGrid     ? [{ label: tag(xGridOn, 'X grid'), keepOpen: true,
                          onClick: () => setXGrid((v) => !v) }] : []),
    ...(setYGrid     ? [{ label: tag(yGridOn, 'Y grid'), keepOpen: true,
                          onClick: () => setYGrid((v) => !v) }] : []),
    ...(setShowMinor ? [{ label: tag(minor, 'minor'), keepOpen: true,
                          onClick: () => setShowMinor((v) => !v) }] : []),
    { head: 'direction' },
    ...(setXReverse ? [{ label: tag(xRev, 'X reverse'), keepOpen: true,
                         onClick: () => setXReverse((v) => !v) }] : []),
    ...(setYReverse ? [{ label: tag(yRev, 'Y reverse'), keepOpen: true,
                         onClick: () => setYReverse((v) => !v) }] : []),
    { head: 'scale' },
    ...(setXLogProp ? [{
      label: tag(xLog, 'X log'),
      keepOpen: true,
      disabled: figure.xRange[1] <= 0,
      onClick: () => {
        if (!xLog && (xMin <= 0 || xMax <= 0)) {
          const cellW = hFullCols > 0 ? (figure.xRange[1] - figure.xRange[0]) / hFullCols : 0;
          const safeLo = Math.max(cellW * 0.5, figure.xRange[0] > 0 ? figure.xRange[0] : 1e-6);
          const safeHi = Math.max(safeLo * 10, figure.xRange[1]);
          setViewport({ ...viewport, x: [safeLo, safeHi] });
        }
        setXLog((v) => !v);
      },
    }] : []),
    ...(setYLogProp ? [{
      label: tag(yLog, 'Y log'),
      keepOpen: true,
      disabled: figure.yRange[1] <= 0,
      onClick: () => {
        if (!yLog && (yMin <= 0 || yMax <= 0)) {
          const cellH = hFullRows > 0 ? (figure.yRange[1] - figure.yRange[0]) / hFullRows : 0;
          const safeLo = Math.max(cellH * 0.5, figure.yRange[0] > 0 ? figure.yRange[0] : 1e-6);
          const safeHi = Math.max(safeLo * 10, figure.yRange[1]);
          setViewport({ ...viewport, y: [safeLo, safeHi] });
        }
        setYLog((v) => !v);
      },
    }] : []),
  ] : null;

  // Location options shared by legend / colorbar Location submenus.
  // null → "default" (follow script). Order mirrors the toolbar's
  // FwPopLocationSubmenu options.
  const legendLocOptions = [
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
  ];
  const colorbarLocOptions = [
    { value: null,    label: 'default' },
    { value: 'east',  label: 'east' },
    { value: 'west',  label: 'west' },
    { value: 'north', label: 'north' },
    { value: 'south', label: 'south' },
  ];

  // Decoration ▶ — specialised per figure shape:
  //   • zlabel:   absent (CompositePlot is 2-D only).
  //   • legend:   only when seriesLayers.length > 0 (heatmap-only or
  //               text-only figures don't get a legend).
  //   • colorbar: only when hasHeatmap (no colorscale without a
  //               colormap-driven layer).
  // The annotations head itself is dropped if both legend AND colorbar
  // are gated out — avoids an empty section header on figures that
  // have neither.
  const hasSeriesLayer = seriesLayers.length > 0;
  // Label rows: never hard-disabled by empty-text alone — use `masked`
  // instead, so the user can flip Visible state in advance even before
  // the script sets the text. If `xlabel(...)` is later called, the
  // pre-set ✓ already takes effect. The only true hard-disable left is
  // `titleAuto` (MATLAB auto-generated title from data) — that's a
  // separate semantic, not a no-text condition.
  //
  // Naming: per-axis rows read `X foo / Y foo / Z foo` (matches X grid
  // / X reverse / X log uppercased + spaced).
  const labelRows = [
    ...(setShowTitle ? [{
      label: tag(showTitle, 'title'),
      keepOpen: true,
      disabled: !!figure.titleAuto,
      masked: !figure.title && !figure.titleAuto,
      maskedHint: 'No title text — set title(...) in your script to add one.',
      onClick: () => setShowTitle((v) => !v),
    }] : []),
    ...(setShowXLabel ? [{
      label: tag(showXLabel, 'X label'),
      keepOpen: true,
      masked: !figure.xLabel,
      maskedHint: 'No xlabel text — set xlabel(...) in your script to add one.',
      onClick: () => setShowXLabel((v) => !v),
    }] : []),
    ...(setShowYLabel ? [{
      label: tag(showYLabel, 'Y label'),
      keepOpen: true,
      masked: !figure.yLabel,
      maskedHint: 'No ylabel text — set ylabel(...) in your script to add one.',
      onClick: () => setShowYLabel((v) => !v),
    }] : []),
  ];
  const annotationRows = [
    ...(hasSeriesLayer && setShowLegend ? [{
      label: tag(showLegend, 'legend'),
      keepOpen: true,
      onClick: () => setShowLegend((v) => !v),
    }] : []),
    ...(hasSeriesLayer && setLegendLocation ? [{
      submenu: 'legend location',
      items: legendLocOptions.map((o) => ({
        label: tag((legendLocationProp || null) === o.value, o.label),
        onClick: () => setLegendLocation(o.value),
      })),
    }] : []),
    ...(hasHeatmap && setShowColorbar ? [{
      label: tag(showColorbar, 'colorbar'),
      keepOpen: true,
      onClick: () => setShowColorbar((v) => !v),
    }] : []),
    ...(hasHeatmap && setColorbarLocation ? [{
      submenu: 'colorbar location',
      items: colorbarLocOptions.map((o) => ({
        label: tag((colorbarLocationProp || null) === o.value, o.label),
        onClick: () => setColorbarLocation(o.value),
      })),
    }] : []),
  ];
  const decorationSubmenuItems = (labelRows.length || annotationRows.length) ? [
    ...(onDisplayReset ? [{ label: 'default', onClick: onDisplayReset },
                          { separator: true }] : []),
    ...(labelRows.length      ? [{ head: 'labels' },      ...labelRows]      : []),
    ...(annotationRows.length ? [{ head: 'annotations' }, ...annotationRows] : []),
  ] : null;

  // Colormap submenu — list of available palettes; click sets
  // colormapOverride which propagates back to FigureWindow's state.
  // Only built when the parent provided the setter (modal mode) AND
  // there's a heatmap layer to colour. Marks the active palette with ✓.
  const COLORMAP_NAMES = ['parula', 'jet', 'hot', 'cool', 'gray', 'bone',
    'copper', 'spring', 'summer', 'autumn', 'winter', 'hsv', 'viridis'];
  const colormapSubmenuItems = (setColormapOverride && hasHeatmap) ? [
    // default on top mirrors the toolbar layout. Restores the script
    // colormap (clears any UI override).
    ...(onColormapReset ? [{ label: 'default', onClick: onColormapReset },
                           { separator: true }] : []),
    ...COLORMAP_NAMES.map((name) => ({
      label: (effectiveColormap === name ? '✓ ' : '') + name,
      onClick: () => {
        const scriptDefault = heatmapLayer?.colormap || 'parula';
        setColormapOverride(name === scriptDefault ? null : name);
      },
    })),
  ] : null;

  // House icon used for the Reset row. Same SVG as the toolbar
  // standalone Reset button — uses currentColor so it inherits the
  // menu text colour (no emoji colour).
  const houseIcon = (
    <svg width="11" height="11" viewBox="0 0 12 12"
         style={{ verticalAlign: '-1px', marginRight: '6px' }}>
      <path d="M1 6l5-5 5 5 M2 5v6h8V5"
            stroke="currentColor" strokeWidth="1.2" fill="none" strokeLinejoin="round"/>
    </svg>
  );

  // Series ▶ submenu — per-series fit rows lifted into their own
  // top-level submenu, matching the Display ▶ / Colormap ▶ layout.
  // "Series" matches MATLAB legend / docs terminology and is generic
  // enough for line / scatter / bar / area / stem / quiver layers.
  //
  // Filtered to FITTABLE series only:
  //   • text layers already excluded by the kind === 'series' filter
  //     above (text has kind === 'text')
  //   • require ≥2 data points — fitting a viewport to a single point
  //     gives a degenerate (zero-width) range; skip those rows
  //
  // Preserve the ORIGINAL seriesLayers index so applyFitSeries(i, axis)
  // still targets the right layer after filtering.
  const fittableSeries = seriesLayers
    .map((s, i) => ({ s, i }))
    .filter(({ s }) =>
      Array.isArray(s.x) && s.x.length >= 2
      && Array.isArray(s.y) && s.y.length >= 2);
  const seriesSubmenuItems = fittableSeries.length > 0
    ? fittableSeries.map(({ s, i }) => ({
        row: true, color: s.color, name: resolveSeriesName(figure, s, i),
        buttons: [
          { label: 'xy', onClick: () => applyFitSeries(i, 'both') },
          { label: 'x',  onClick: () => applyFitSeries(i, 'x') },
          { label: 'y',  onClick: () => applyFitSeries(i, 'y') },
        ],
      }))
    : null;

  const ctxItems = [
    // Order: Reset · Save · Axes · Decoration · Colormap · Fit Series · Fit All.
    // Axes ▶ / Decoration ▶ mirror the toolbar's axes ▾ / decoration ▾
    // split (HG2 object vs. children).
    { label: <span>{houseIcon}Reset</span>, onClick: onReset },
    { submenu: 'Save / Export', items: exportItems },
    ...(axesSubmenuItems ? [{ submenu: 'Axes', items: axesSubmenuItems }] : []),
    ...(decorationSubmenuItems ? [{ submenu: 'Decoration', items: decorationSubmenuItems }] : []),
    ...(colormapSubmenuItems ? [{ submenu: 'Colormap', items: colormapSubmenuItems }] : []),
    ...(seriesSubmenuItems ? [{
      submenu: `Fit Series${fittableSeries.length > 1 ? ` (${fittableSeries.length})` : ''}`,
      items: seriesSubmenuItems,
    }] : []),
    { separator: true },
    // Fit section now carries only figure-wide / data-extent rows;
    // per-curve fit lives in the Fit Series ▶ submenu above. Head
    // "Fit All" makes child labels redundant so they shorten to
    // bare axis names (matches the toolbar fit ▾ popover layout).
    // ПКМ Fit — specialized to the cell's coordinate system.
    // CompositePlot is always cartesian (polar uses PolarPlot, 3-D
    // Composite3DPlot), so we expose only X / Y axes here. No `Z` (no
    // 3-D context); no `R / θ` (no polar). Rows mirror the toolbar
    // fit ▾ Cartesian block but specialised to THIS cell.
    ...(seriesLayers.length > 0 ? [
      { head: 'Fit' },
      { label: 'all',    onClick: () => applyFitAllSeries('both') },
      { label: 'X only', onClick: () => applyFitAllSeries('x') },
      { label: 'Y only', onClick: () => applyFitAllSeries('y') },
    ] : [
      { head: 'Fit' },
      { label: 'all',    onClick: () => fitAxes('both') },
      { label: 'X only', onClick: () => fitAxes('x') },
      { label: 'Y only', onClick: () => fitAxes('y') },
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

      // Tile buffer is in source-row order (top of source-rect = buf row 0).
      // For axis-xy (yDir='normal') we need vertical flip so matrix row 1
      // (low data y) lands at panel BOTTOM; for axis-ij (yDir='reverse')
      // we DON'T flip — matrix row 1 belongs at panel TOP, and the canvas
      // is drawn top-down from buf row 0 already.
      const dataURL = renderHeatmapDataURLFromFlat(buf, H, W, lut, !yRev);
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
  // Heatmap image rectangle. With xDir/yDir reverse the corner mapping
  // flips, so derive the bounding box from min/max of the four corners
  // rather than picking specific ones — this keeps width/height positive.
  const sxLo = sx(figure.xRange[0]);
  const sxHi = sx(figure.xRange[1]);
  const syLo = sy(figure.yRange[0]);
  const syHi = sy(figure.yRange[1]);
  const imgX = Math.min(sxLo, sxHi);
  const imgY = Math.min(syLo, syHi);
  const imgW = Math.abs(sxHi - sxLo);
  const imgH = Math.abs(syLo - syHi);

  /* ─── colorbar — placement honours figure.colorbarLocation ─────
     MATLAB parity: the bar appears ONLY when the script called
     colorbar() (which sets figure.colorbarLocation to a non-empty
     placement string) OR when the user enabled it via the toolbar /
     ПКМ display toggle (showColorbar === true).
     'east' / 'eastoutside'  → vertical bar right of plot
     'west' / 'westoutside'  → vertical bar left of plot
     'north' / 'northoutside' → horizontal bar above plot
     'south' / 'southoutside' → horizontal bar below plot
     We collapse 'inside' / 'outside' variants to the same screen
     position; 'inside' would overlap data. */
  // colorbarLocationProp override wins — UI submenu can pin the bar
  // to a specific edge regardless of script's choice.
  const cbarLocRaw = colorbarLocationProp || figure.colorbarLocation || '';
  // Resolve "want bar?" — explicit toggle wins; otherwise follow script.
  const cbarWanted = showColorbar === true
                  || (showColorbar !== false && !!cbarLocRaw);
  // When wanted but script didn't set a location, fall back to 'east'.
  const cbarLoc = cbarWanted ? (cbarLocRaw || 'east') : null;
  // Strip the 'outside' suffix so the placement switch is compact.
  const cbarSide = cbarLoc ? cbarLoc.replace(/outside$/, '') : null;
  const cbarThick = 12;
  const cbarGap   = 14;
  const cbarVertical = cbarSide === 'east' || cbarSide === 'west' || !cbarSide;
  const cbarW = cbarVertical ? cbarThick : Math.max(40, W - 20);
  const cbarH = cbarVertical ? H : cbarThick;
  // Place by side. The space outside padL/padR/padT/padB is reserved
  // by the layout pad numbers (see padR === 60 below); we don't auto-
  // expand to make room for the bar — the renderer keeps the existing
  // padding budget.
  let cbarX, cbarY;
  if (cbarSide === 'west') {
    cbarX = padL - cbarThick - cbarGap;
    cbarY = padT;
  } else if (cbarSide === 'north') {
    cbarX = padL + (W - cbarW) / 2;
    cbarY = padT - cbarThick - cbarGap;
  } else if (cbarSide === 'south') {
    cbarX = padL + (W - cbarW) / 2;
    cbarY = padT + H + cbarGap;
  } else {
    // east + default
    cbarX = padL + W + cbarGap;
    cbarY = padT;
  }
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
        {/* Two gradient orientations. Vertical bars (east/west) flow
            bottom→top so cmin sits at the bottom; horizontal bars
            (north/south) flow left→right with cmin at the left. */}
        {cbarVertical ? (
          <linearGradient id={cbarGradId} x1="0" y1="1" x2="0" y2="0">
            {cbarStops.map((s, i) => <stop key={i} offset={s.offset} stopColor={s.color} />)}
          </linearGradient>
        ) : (
          <linearGradient id={cbarGradId} x1="0" y1="0" x2="1" y2="0">
            {cbarStops.map((s, i) => <stop key={i} offset={s.offset} stopColor={s.color} />)}
          </linearGradient>
        )}
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
      {/* image-rgb (imshow truecolor). Pure SVG <image> — no LUT, no
          tile pyramid, no log-axis weirdness. Sits in the same z-order
          slot as the heatmap dataURL above. */}
      {rgbDataURL && (
        <g clipPath={`url(#${clipId})`}>
          <image href={rgbDataURL}
            x={imgX} y={imgY} width={imgW} height={imgH}
            preserveAspectRatio="none"
            imageRendering="pixelated" />
        </g>
      )}
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

      {/* Optional minor + major grid (faint, over the heatmap).
          axisVisible=false (imshow / `axis off`) suppresses gridlines,
          frame box, and tick labels — image-only viewport. */}
      {/* Grid lines split per MATLAB XGrid / YGrid semantics:
            X grid → vertical lines at X-tick positions
            Y grid → horizontal lines at Y-tick positions
          xMinorOn / yMinorOn (XMinorGrid / YMinorGrid) drive the
          fainter sub-tick lines independently. */}
      {axisVisible && xMinorOn && xTicks.minor.map((v, i) => (
        <line key={`mx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + H} stroke="var(--plot-grid-min)" />
      ))}
      {axisVisible && yMinorOn && yTicks.minor.map((v, i) => (
        <line key={`my${i}`} x1={padL} x2={padL + W} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid-min)" />
      ))}
      {axisVisible && xGridOn && xTicks.major.map((v, i) => (
        <line key={`gx${i}`} x1={sx(v)} x2={sx(v)} y1={padT} y2={padT + H} stroke="var(--plot-grid)" />
      ))}
      {axisVisible && yGridOn && yTicks.major.map((v, i) => (
        <line key={`gy${i}`} x1={padL} x2={padL + W} y1={sy(v)} y2={sy(v)} stroke="var(--plot-grid)" />
      ))}

      {axisVisible && (boxOn ? (
        <rect x={padL} y={padT} width={W} height={H} fill="none" stroke="var(--plot-frame)" />
      ) : (
        // box off — only left + bottom edges (MATLAB convention).
        <>
          <line x1={padL} x2={padL} y1={padT} y2={padT + H} stroke="var(--plot-frame)" />
          <line x1={padL} x2={padL + W} y1={padT + H} y2={padT + H} stroke="var(--plot-frame)" />
        </>
      ))}

      {/* Tick labels */}
      {axisVisible && xTicks.major.map((v, i) => {
        const x = sx(v);
        if (x < padL - 1 || x > padL + W + 1) return null;
        const ang = Number(figure.xTickAngle) || 0;
        const ty = padT + H + 14 * fontScale + 2;
        const transform = ang ? `rotate(${ang}, ${x}, ${ty})` : undefined;
        const anchor = ang ? (ang > 0 ? 'end' : 'start') : 'middle';
        return (
          <g key={`xl${i}`}>
            <line x1={x} x2={x} y1={padT + H} y2={padT + H + 4} stroke="var(--plot-tick)" />
            <text x={x} y={ty} fill="var(--plot-text)" fontSize={10 * fontScale}
                  textAnchor={anchor} transform={transform}>{fmtXTickLabel(v, i)}</text>
          </g>
        );
      })}
      {axisVisible && yTicks.major.map((v, i) => {
        const y = sy(v);
        if (y < padT - 1 || y > padT + H + 1) return null;
        const ang = Number(figure.yTickAngle) || 0;
        const tx = padL - 7;
        const transform = ang ? `rotate(${ang}, ${tx}, ${y})` : undefined;
        return (
          <g key={`yl${i}`}>
            <line x1={padL - 4} x2={padL} y1={y} y2={y} stroke="var(--plot-tick)" />
            <text x={tx} y={y + 3} fill="var(--plot-text)" fontSize={10 * fontScale}
                  textAnchor="end" transform={transform}>{fmtYTickLabel(v, i)}</text>
          </g>
        );
      })}
      {/* Right (yyaxis) axis line + ticks. Only rendered when yyEnabled
          AND we have a yRange2 to draw against. Tick set is computed
          independently from the left axis so the two scales are visibly
          decoupled. */}
      {yy2 && (() => {
        const yTicks2 = yLog2Active ? logTicks(yMin2, yMax2) : niceTicks(yMin2, yMax2, 6);
        return (
          <>
            <line x1={padL + W} x2={padL + W} y1={padT} y2={padT + H}
              stroke="var(--plot-frame)" strokeWidth="0.5" />
            {yTicks2.major.map((v, i) => {
              const y = sy2(v);
              if (y < padT - 1 || y > padT + H + 1) return null;
              return (
                <g key={`yr${i}`}>
                  <line x1={padL + W} x2={padL + W + 4} y1={y} y2={y} stroke="var(--plot-tick)" />
                  <text x={padL + W + 7} y={y + 3} fill="var(--plot-text)"
                    fontSize={10 * fontScale} textAnchor="start">{fmtTick(v)}</text>
                </g>
              );
            })}
          </>
        );
      })()}

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
              // yyaxis: pick the right-side mapping if this layer is on
              // the right axis. mySy is then used in place of sy for
              // every data-point coordinate produced by this block.
              const mySy = syOf(ly);
              if (mode === 'xline' || mode === 'yline') {
                // Reference line spanning the visible viewport.
                // xline: vertical at x=ly.x[0]; yline: horizontal at
                // y=ly.y[0].
                const lineColor = ly.color || 'var(--plot-text)';
                const lw = ly.width || 1.2;
                if (mode === 'xline') {
                  const x = sx(ly.x[0]);
                  if (!Number.isFinite(x)) return null;
                  return (
                    <line key={`ly${idx}`} x1={x} x2={x}
                      y1={padT} y2={padT + H}
                      stroke={lineColor} strokeWidth={lw}
                      strokeDasharray={ly.lineStyle === '--' ? '6,4' : undefined}
                      opacity={op} />
                  );
                }
                // yline
                const y = mySy(ly.x[0]);   // engine packed y in xJson[0]
                if (!Number.isFinite(y)) return null;
                return (
                  <line key={`ly${idx}`} x1={padL} x2={padL + W}
                    y1={y} y2={y}
                    stroke={lineColor} strokeWidth={lw}
                    strokeDasharray={ly.lineStyle === '--' ? '6,4' : undefined}
                    opacity={op} />
                );
              }
              if (mode === 'scatter') {
                const mk = ly.marker || 'o';
                const r = ly.size || 3;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)) return null;
                      const px = sx(xv), py = mySy(yv);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      return <MarkerGlyph key={i} idx={i} marker={mk}
                        cx={px} cy={py} r={r} color={ly.color} />;
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
                      const px = sx(xv), py = mySy(yv);
                      const py0 = mySy(yLogActive ? yMin : 0);
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
                const baseY = mySy(yLogActive ? yMin : Math.max(0, yMin));
                const sIdx = seriesLayers.indexOf(ly);
                const off = (sIdx - (seriesLayers.length - 1) / 2) * bw * 1.05;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const px = sx(xv) + off, py = mySy(ly.y[i]);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const top = Math.min(py, baseY);
                      const h = Math.abs(py - baseY);
                      return <rect key={i} x={px - bw / 2} y={top}
                        width={bw} height={h} fill={ly.color} stroke="none" />;
                    })}
                  </g>
                );
              }
              if (mode === 'quiver') {
                // Vector field: per-point arrow from (x[i], y[i]) to
                // (x[i] + u[i]*s, y[i] + v[i]*s). Three SVG segments
                // per arrow: shaft + two head fins forming a chevron.
                const u = ly.u || [];
                const v = ly.v || [];
                const s = Number.isFinite(ly.scale) ? ly.scale : 1;
                const headLen = 6;        // head fin length, pixels
                const headAng = Math.PI / 7;  // head opening angle (radians)
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      const uv = Number(u[i]);
                      const vv = Number(v[i]);
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)
                       || !Number.isFinite(uv) || !Number.isFinite(vv)) return null;
                      const px1 = sx(xv),       py1 = mySy(yv);
                      const px2 = sx(xv + uv * s), py2 = mySy(yv + vv * s);
                      if (!Number.isFinite(px1) || !Number.isFinite(py1)
                       || !Number.isFinite(px2) || !Number.isFinite(py2)) return null;
                      // Skip degenerate zero-length arrows so the head
                      // doesn't end up as two overlapping points.
                      const dx = px2 - px1, dy = py2 - py1;
                      const mag = Math.hypot(dx, dy);
                      if (mag < 0.5) return null;
                      const ang = Math.atan2(dy, dx);
                      const fx1 = px2 - headLen * Math.cos(ang - headAng);
                      const fy1 = py2 - headLen * Math.sin(ang - headAng);
                      const fx2 = px2 - headLen * Math.cos(ang + headAng);
                      const fy2 = py2 - headLen * Math.sin(ang + headAng);
                      return (
                        <g key={i}>
                          <line x1={px1} y1={py1} x2={px2} y2={py2}
                            stroke={ly.color} strokeWidth={Math.max(1, w * 0.8)} />
                          <line x1={px2} y1={py2} x2={fx1} y2={fy1}
                            stroke={ly.color} strokeWidth={Math.max(1, w * 0.8)} />
                          <line x1={px2} y1={py2} x2={fx2} y2={fy2}
                            stroke={ly.color} strokeWidth={Math.max(1, w * 0.8)} />
                        </g>
                      );
                    })}
                  </g>
                );
              }
              if (mode === 'area') {
                // Filled polygon under the curve. Path: (x[0],base) →
                // (x[0],y[0]) → … → (x[N-1],y[N-1]) → (x[N-1],base) → close.
                // NaN points break the polygon — start a new sub-path.
                const base = Number.isFinite(ly.baseline) ? ly.baseline : 0;
                const subpaths = [];
                let cur = '';
                let firstPx = null;
                for (let i = 0; i < ly.x.length; i++) {
                  const xv = ly.x[i], yv = ly.y[i];
                  const finite = Number.isFinite(xv) && Number.isFinite(yv);
                  if (!finite) {
                    if (cur) {
                      // Close current sub-path: drop down to baseline + close.
                      const lastX = ly.x.slice(0, i).reverse().find((v) => Number.isFinite(v));
                      if (lastX != null) cur += `L${sx(lastX).toFixed(2)},${mySy(base).toFixed(2)} Z `;
                      subpaths.push(cur);
                      cur = ''; firstPx = null;
                    }
                    continue;
                  }
                  const px = sx(xv), py = mySy(yv);
                  if (!cur) {
                    firstPx = px;
                    cur = `M${px.toFixed(2)},${mySy(base).toFixed(2)} L${px.toFixed(2)},${py.toFixed(2)} `;
                  } else {
                    cur += `L${px.toFixed(2)},${py.toFixed(2)} `;
                  }
                }
                if (cur) {
                  // Close last sub-path.
                  const lastX = ly.x.slice().reverse().find((v) => Number.isFinite(v));
                  if (lastX != null) cur += `L${sx(lastX).toFixed(2)},${mySy(base).toFixed(2)} Z`;
                  subpaths.push(cur);
                }
                const d = subpaths.join(' ');
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    <path d={d} fill={ly.color} fillOpacity="0.3"
                      stroke={ly.color} strokeWidth={w}
                      strokeLinejoin="round" strokeLinecap="round" />
                  </g>
                );
              }
              if (mode === 'polygon') {
                // Filled polygon(s) — patch / fill / pie wedges / etc.
                // null in x or y starts a new sub-polygon. Each sub-path
                // is closed automatically (Z command) so the renderer
                // doesn't depend on the user repeating the first point.
                let d = '';
                let inSub = false;
                for (let i = 0; i < ly.x.length; i++) {
                  const xv = ly.x[i], yv = ly.y[i];
                  if (!Number.isFinite(xv) || !Number.isFinite(yv)) {
                    if (inSub) { d += 'Z '; inSub = false; }
                    continue;
                  }
                  const px = sx(xv), py = mySy(yv);
                  d += (inSub ? 'L' : 'M') + px.toFixed(2) + ',' + py.toFixed(2) + ' ';
                  inSub = true;
                }
                if (inSub) d += 'Z';
                // fillOpacity defaults to 0.7 (slight see-through so
                // overlapping polygons stay readable). Caller can tune
                // via ly.fillOpacity.
                const fa = Number.isFinite(ly.fillOpacity) ? ly.fillOpacity : 0.7;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    <path d={d} fill={ly.color} fillOpacity={fa}
                      stroke={ly.color} strokeWidth={Math.max(0.5, w * 0.6)}
                      strokeLinejoin="round" strokeLinecap="round" />
                  </g>
                );
              }
              if (mode === 'barh') {
                // Horizontal bars. Adapter has already swapped the
                // engine's xJson/yJson so `ly.y` holds positions
                // (vertical / Y axis) and `ly.x` holds lengths
                // (horizontal / X axis). Bars extend from x=0 to x=ly.x[i].
                const ys = ly.y.filter(Number.isFinite);
                let bh = 8;
                if (ys.length > 1) {
                  const spacing = Math.abs(mySy(ys[1]) - mySy(ys[0]));
                  bh = Math.max(2, spacing * 0.7);
                }
                const baseX = sx(xLogActive ? xMin : Math.max(0, xMin));
                const sIdx = seriesLayers.indexOf(ly);
                const off = (sIdx - (seriesLayers.length - 1) / 2) * bh * 1.05;
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.y.map((yv, i) => {
                      const py = mySy(yv) + off;
                      const px = sx(ly.x[i]);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const left = Math.min(px, baseX);
                      const w2 = Math.abs(px - baseX);
                      return <rect key={i} x={left} y={py - bh / 2}
                        width={w2} height={bh} fill={ly.color} stroke="none" />;
                    })}
                  </g>
                );
              }
              if (mode === 'errorbar') {
                // Three SVG elements per data point:
                //   • centre dot (the y value)
                //   • vertical bar from y-eNeg to y+ePos
                //   • horizontal cap at each end of the bar
                // eNeg / ePos arrays are indexed parallel to x/y. Missing
                // entries fall back to 0 (no bar drawn).
                const eN = ly.eNeg || [];
                const eP = ly.ePos || [];
                const cap = 5;  // pixel half-width of the end caps
                return (
                  <g key={`ly${idx}`} opacity={op}>
                    {ly.x.map((xv, i) => {
                      const yv = ly.y[i];
                      if (!Number.isFinite(xv) || !Number.isFinite(yv)) return null;
                      const px = sx(xv), py = mySy(yv);
                      if (!Number.isFinite(px) || !Number.isFinite(py)) return null;
                      const eNeg = Number(eN[i]) || 0;
                      const ePos = Number(eP[i]) || 0;
                      const yLo = mySy(yv - eNeg);
                      const yHi = mySy(yv + ePos);
                      return (
                        <g key={i}>
                          {(eNeg !== 0 || ePos !== 0) && (
                            <>
                              <line x1={px} x2={px} y1={yLo} y2={yHi}
                                stroke={ly.color} strokeWidth={Math.max(1, w * 0.7)} />
                              <line x1={px - cap} x2={px + cap} y1={yLo} y2={yLo}
                                stroke={ly.color} strokeWidth={Math.max(1, w * 0.7)} />
                              <line x1={px - cap} x2={px + cap} y1={yHi} y2={yHi}
                                stroke={ly.color} strokeWidth={Math.max(1, w * 0.7)} />
                            </>
                          )}
                          <circle cx={px} cy={py} r={2.5} fill={ly.color} />
                        </g>
                      );
                    })}
                  </g>
                );
              }
              // 'line' or 'stairs'
              let d = '';
              let started = false;
              const markerPts = [];   // collect finite pts for overlay
              // Comet animation: render only first floor(progress·N)
              // points. When cometAnim flag is false, use full length.
              const totalN = ly.x.length;
              const animN = ly.cometAnim
                ? Math.max(1, Math.floor(cometProgress * totalN))
                : totalN;
              for (let i = 0; i < animN; i++) {
                const xv = ly.x[i], yv = ly.y[i];
                if (!Number.isFinite(xv) || !Number.isFinite(yv)) { started = false; continue; }
                const px = sx(xv), py = mySy(yv);
                if (!Number.isFinite(px) || !Number.isFinite(py)) { started = false; continue; }
                if (mode === 'stairs' && started) {
                  d += `L${px.toFixed(2)},${(mySy(ly.y[i - 1])).toFixed(2)} `;
                }
                d += (started ? 'L' : 'M') + px.toFixed(2) + ',' + py.toFixed(2) + ' ';
                started = true;
                if (ly.marker) markerPts.push({ px, py });
              }
              // Linespec dash pattern. 'none' (or '') line-style would
              // suppress the path entirely (markers-only call) — emit no
              // <path> in that case.
              const ls = ly.lineStyle || '-';
              const dash = DASH_FOR[ls];
              const drawPath = ls !== 'none' && ls !== '';
              const r = ly.size || 4;
              return (
                <g key={`ly${idx}`} opacity={op}>
                  {drawPath && (
                    <path d={d} stroke={ly.color} fill="none"
                      strokeWidth={w} strokeDasharray={dash}
                      strokeLinejoin="round" strokeLinecap="round" />
                  )}
                  {ly.marker && markerPts.map((p, i) => (
                    <MarkerGlyph key={`m${i}`} idx={i} marker={ly.marker}
                      cx={p.px} cy={p.py} r={r} color={ly.color} />
                  ))}
                </g>
              );
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

      {/* Colorbar — placement governed by figure.colorbarLocation
          (resolved into cbarSide above). Renders only when a heatmap
          layer exists AND the location is not 'off'. */}
      {hasHeatmap && cbarSide && (
        <>
          <rect x={cbarX} y={cbarY} width={cbarW} height={cbarH}
            fill={`url(#${cbarGradId})`}
            stroke="var(--plot-frame)" strokeWidth="0.5" />
          {cbarTicks.major.map((v, i) => {
            const t = (v - cminEff) / (cmaxEff - cminEff);
            // Vertical bars: high values at top → invert t. Horizontal:
            // low values at left, high at right → t directly.
            if (cbarVertical) {
              const y = cbarY + cbarH - t * cbarH;
              if (y < cbarY - 1 || y > cbarY + cbarH + 1) return null;
              return (
                <g key={`cb${i}`}>
                  <line x1={cbarX + cbarW} x2={cbarX + cbarW + 3} y1={y} y2={y} stroke="var(--plot-tick)" />
                  <text x={cbarX + cbarW + 6} y={y + 3} fill="var(--plot-text)" fontSize={9 * fontScale} textAnchor="start">{fmtTick(v)}</text>
                </g>
              );
            }
            const x = cbarX + t * cbarW;
            if (x < cbarX - 1 || x > cbarX + cbarW + 1) return null;
            return (
              <g key={`cb${i}`}>
                <line x1={x} x2={x} y1={cbarY + cbarH} y2={cbarY + cbarH + 3} stroke="var(--plot-tick)" />
                <text x={x} y={cbarY + cbarH + 13} fill="var(--plot-text)" fontSize={9 * fontScale} textAnchor="middle">{fmtTick(v)}</text>
              </g>
            );
          })}
        </>
      )}

      {/* Legend — only rendered when the user explicitly called
          legend(...) (figure.legend is non-empty) OR set a Location,
          AND the toolbar toggle hasn't switched it off. We don't
          auto-show on default series names like "series 1" — MATLAB
          also requires an explicit legend call. */}
      {(() => {
        if (showLegend === false) return null;
        const userAsked = (figure.legend && figure.legend.length > 0)
                       || (figure.legendLocation && figure.legendLocation !== 'none');
        if (!userAsked || seriesLayers.length === 0) return null;
        // When legendLocation is set but no labels — fall back to the
        // per-layer auto-names so the box still has content.
        const labels = (figure.legend && figure.legend.length > 0)
          ? figure.legend
          : seriesLayers.map((l) => l.name).filter(Boolean);
        const haveLabels = labels.some((s) => s && s.trim() !== '');
        if (!haveLabels) return null;
        const items = seriesLayers.slice(0, labels.length).map((l, i) => ({
          color: l.color,
          text: resolveSeriesName(figure, l, i),
          // Mode drives swatch shape: 'circle' for point-like marks,
          // 'rect' for filled-region marks, 'line' for everything else.
          mode: l.mode || 'line',
        }));
        if (items.length === 0) return null;
        const fontSize = 10 * fontScale;
        const lineH    = fontSize + 4;
        const swatchW  = 14;
        const padInner = 6;
        // Approximate text width: 6.5 px per char at fontSize ≈ 10.
        // Conservative; SVG won't reflow but the box won't be tiny.
        const longest = items.reduce((m, it) => Math.max(m, it.text.length), 0);
        const boxW = padInner * 2 + swatchW + 4 + Math.min(longest, 24) * 6.5;
        const boxH = padInner * 2 + items.length * lineH;
        const loc = ((legendLocationProp != null ? legendLocationProp : figure.legendLocation) || 'best')
                    .replace(/outside$/, '');
        // Resolve box anchor to (x, y) inside the panel rect.
        const anchorMargin = 8;
        let bx, by;
        switch (loc) {
          case 'north':
            bx = padL + (W - boxW) / 2; by = padT + anchorMargin; break;
          case 'south':
            bx = padL + (W - boxW) / 2; by = padT + H - boxH - anchorMargin; break;
          case 'east':
            bx = padL + W - boxW - anchorMargin;
            by = padT + (H - boxH) / 2; break;
          case 'west':
            bx = padL + anchorMargin; by = padT + (H - boxH) / 2; break;
          case 'northwest':
            bx = padL + anchorMargin; by = padT + anchorMargin; break;
          case 'southeast':
            bx = padL + W - boxW - anchorMargin;
            by = padT + H - boxH - anchorMargin; break;
          case 'southwest':
            bx = padL + anchorMargin;
            by = padT + H - boxH - anchorMargin; break;
          case 'none':
            return null;
          // 'best' / 'northeast' / unrecognised → top-right corner.
          default:
            bx = padL + W - boxW - anchorMargin;
            by = padT + anchorMargin; break;
        }
        return (
          <g pointerEvents="none">
            {/* legendBoxOn=false hides the frame stroke + bg fill;
                the swatches + labels still render. */}
            {figure.legendBoxOn !== false && (
              <rect x={bx} y={by} width={boxW} height={boxH}
                fill="var(--plot-bg)" stroke="var(--plot-frame)" strokeWidth="0.5"
                rx="3" opacity="0.92" />
            )}
            {items.map((it, i) => {
              const cy = by + padInner + i * lineH + lineH / 2;
              const swX0 = bx + padInner;
              const swX1 = swX0 + swatchW;
              // Swatch matches the series mode so the legend keeps a
              // visual link to the actual mark on the plot.
              let swatch;
              if (it.mode === 'scatter' || it.mode === 'stem') {
                swatch = <circle cx={(swX0 + swX1) / 2} cy={cy} r="3"
                  fill={it.color} stroke="var(--plot-frame)" strokeWidth="0.6" />;
              } else if (it.mode === 'bar' || it.mode === 'barh' || it.mode === 'area') {
                swatch = <rect x={swX0} y={cy - 4} width={swatchW} height="8"
                  fill={it.color} fillOpacity={it.mode === 'area' ? 0.3 : 1}
                  stroke={it.color} strokeWidth="1" />;
              } else {
                // line / stairs / errorbar / quiver / default
                swatch = <line x1={swX0} x2={swX1} y1={cy} y2={cy}
                  stroke={it.color} strokeWidth="2" />;
              }
              return (
                <g key={`leg${i}`}>
                  {swatch}
                  <text x={swX1 + 4} y={cy + fontSize / 3}
                    fill="var(--plot-text)" fontSize={fontSize} textAnchor="start">
                    {it.text}
                  </text>
                </g>
              );
            })}
          </g>
        );
      })()}

      {/* Axis titles. Each render gates on the corresponding showXxx
          flag from FigureWindow's display ▾ menu so the user can hide
          the text without losing it from the figure data. */}
      {showXLabel && figure.xLabel && (
        <text x={padL + W / 2} y={height - 8} fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle">{figure.xLabel}</text>
      )}
      {showYLabel && figure.yLabel && (
        <text x={14} y={padT + H / 2} fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle"
          transform={`rotate(-90 14 ${padT + H / 2})`}>{figure.yLabel}</text>
      )}
      {yy2 && showYLabel && figure.yLabel2 && (
        <text x={padL + W + 56 * fontScale} y={padT + H / 2}
          fill="var(--plot-text)" fontSize={11 * fontScale} textAnchor="middle"
          transform={`rotate(90 ${padL + W + 56 * fontScale} ${padT + H / 2})`}>
          {figure.yLabel2}
        </text>
      )}
      {showTitle && figure.title && (
        <text x={padL + W / 2} y={padT - (figure.subtitle ? 22 : 12) * fontScale}
              fill="var(--plot-text-strong)" fontSize={12 * fontScale} textAnchor="middle">{figure.title}</text>
      )}
      {figure.subtitle && (
        <text x={padL + W / 2} y={padT - 8 * fontScale}
              fill="var(--plot-text)" fontSize={10 * fontScale}
              textAnchor="middle" fontStyle="italic">{figure.subtitle}</text>
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
                <circle cx={sx(hx)} cy={syOf(s)(hy)} r="3"
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
