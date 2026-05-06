/**
 * Heatmap (imagesc) renderer. Mirrors InteractivePlot's outer shape — same
 * SVG / viewBox / pan-zoom hooks — but renders a pixel image inside the
 * plot area instead of line paths.
 *
 * Figure shape it expects (built by adapters.adaptFigure when type='imagesc'):
 *   {
 *     id, title, xLabel, yLabel,
 *     xRange, yRange,           // matrix coordinate extents
 *     z: number[][],            // rows × cols
 *     cmin, cmax, colormap,     // value-range and colormap name
 *   }
 */
import { useEffect, useMemo, useRef, useState } from 'react';
import { buildHeatmapLUT, renderHeatmapDataURLFromIndices,
         renderHeatmapDataURLFromFlat, getColormap } from './colormaps';
import ContextMenu from './ContextMenu';
import { exportSvgNode, exportPngNode, exportPngForPrint } from './plotUtils';

export default function Heatmap({
  figure,
  width,
  height,
  viewport,
  setViewport,
  major = true,
  minor = true,
  fontScale = 1,
  interactive = true,
  engine = null,
}) {
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
      setColorOverride(null);
      figIdRef.current = fid;
    }
  }, [figure._raw?.id, figure.id]);

  // ── Log-axis state ─────────────────────────────────────────────────
  // Per-Heatmap toggle (independent of figure-level xscale/yscale config —
  // the user can switch a linear-emit imagesc to log axes interactively
  // without recreating the figure). When set, getFigureDisplayTile applies
  // log10 inverse to that axis when resampling.
  const [xLog, setXLog] = useState(false);
  const [yLog, setYLog] = useState(false);

  // ── Color-limit override ────────────────────────────────────────────
  // "Fit colors to visible" pulls cmin/cmax from the currently-visible
  // source-rect via getFigureTile, so a low-contrast region zoomed in
  // gets full colormap dynamic range. null = use figure.cminOrig/cmaxOrig.
  // The override is a window/level remap on top of the engine-baked
  // quantization range — no requantization, just a different LUT.
  const [colorOverride, setColorOverride] = useState(null);
  const cminEff = colorOverride ? colorOverride.cmin : figure.cminOrig ?? figure.cmin;
  const cmaxEff = colorOverride ? colorOverride.cmax : figure.cmaxOrig ?? figure.cmax;
  const cminOrig = figure.cminOrig ?? figure.cmin;
  const cmaxOrig = figure.cmaxOrig ?? figure.cmax;

  // 256-entry RGBA LUT — rebuilt only when colormap or window/level changes.
  const lut = useMemo(
    () => buildHeatmapLUT(figure.colormap, cminOrig, cmaxOrig, cminEff, cmaxEff),
    [figure.colormap, cminOrig, cmaxOrig, cminEff, cmaxEff]
  );

  const padL = 60 * fontScale;
  const padR = 70 * fontScale;  // wider to fit the colorbar
  const padT = 36 * fontScale;
  const padB = 44 * fontScale;
  const W = Math.max(50, width - padL - padR);
  const H = Math.max(50, height - padT - padB);

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
  // memo on (z) and another on (lut) chained together.
  const dataURL = useMemo(() => {
    if (!figure.z) return null;
    return renderHeatmapDataURLFromIndices(figure.z, lut);
  }, [figure.z, lut]);

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
    if (axisMode === 'both' || axisMode === 'x') next.x = figure.xRange.slice();
    if (axisMode === 'both' || axisMode === 'y') next.y = figure.yRange.slice();
    setViewport(next);
  }

  // Recompute cmin/cmax from the currently-visible source-rect. Pulls a
  // coarse-LOD tile of uint8 indices, finds idxMin/idxMax, maps them back
  // through the engine's quantization range to original-domain values.
  // No requantization happens — the override is a window/level remap of
  // the LUT (see buildHeatmapLUT). 256-level resolution is the cap.
  function fitColorsToVisible() {
    if (!engine || typeof engine.getFigureTile !== 'function') return;
    if (typeof figure._figId !== 'number' || figure._figId < 0) return;
    if (!figure.originalRows || !figure.originalCols) return;

    const fullCols = figure.originalCols;
    const fullRows = figure.originalRows;
    const xExt = figure.xRange[1] - figure.xRange[0];
    const yExt = figure.yRange[1] - figure.yRange[0];
    const colsPerUnit = fullCols / (xExt || 1);
    const rowsPerUnit = fullRows / (yExt || 1);
    const c0 = Math.max(0, Math.floor((Math.min(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const c1 = Math.min(fullCols, Math.ceil((Math.max(xMin, xMax) - figure.xRange[0]) * colsPerUnit));
    const r0 = Math.max(0, Math.floor((figure.yRange[1] - Math.max(yMin, yMax)) * rowsPerUnit));
    const r1 = Math.min(fullRows, Math.ceil((figure.yRange[1] - Math.min(yMin, yMax)) * rowsPerUnit));
    const tileW = c1 - c0, tileH = r1 - r0;
    if (tileW <= 0 || tileH <= 0) return;

    const lod = Math.max(1, Math.ceil(Math.max(tileH, tileW) / 256));
    const tile = engine.getFigureTile(figure._figId, figure._axIdx, figure._dsIdx,
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
    { head: 'Fit data extent' },
    { label: 'Fit both axes', onClick: () => fitAxes('both') },
    { label: 'Fit X only',    onClick: () => fitAxes('x') },
    { label: 'Fit Y only',    onClick: () => fitAxes('y') },
    { head: 'Color range' },
    { label: 'Fit colors to visible',
      onClick: fitColorsToVisible,
      disabled: !engine || typeof engine.getFigureTile !== 'function'
                || typeof figure._figId !== 'number' || figure._figId < 0 },
    { label: colorOverride
        ? `Reset colors (${Number(figure.cmin).toPrecision(3)} … ${Number(figure.cmax).toPrecision(3)})`
        : 'Reset colors',
      onClick: resetColors,
      disabled: !colorOverride },
    { head: 'Axes' },
    { label: xLog ? '✓ X axis · log' : 'X axis · log',
      onClick: () => {
        // Switching to log requires a strictly-positive xMin. Clamp viewport
        // up if the user is currently viewing through zero.
        if (!xLog && (xMin <= 0 || xMax <= 0)) {
          const safeLo = Math.max(figure.xRange[0], 1e-6);
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
          const safeLo = Math.max(figure.yRange[0], 1e-6);
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
      // Linear: zoom in around cursor by `factor`. Log: same idea but
      // ratios apply multiplicatively in log space so the cursor's data
      // value stays put on screen.
      let nx, ny;
      if (xLogActive) {
        nx = [cx * Math.pow(xMin / cx, factor), cx * Math.pow(xMax / cx, factor)];
      } else {
        nx = [cx - (cx - xMin) * factor, cx + (xMax - cx) * factor];
      }
      if (yLogActive) {
        ny = [cy * Math.pow(yMin / cy, factor), cy * Math.pow(yMax / cy, factor)];
      } else {
        ny = [cy - (cy - yMin) * factor, cy + (yMax - cy) * factor];
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
    if (typeof figure._figId !== 'number' || figure._figId < 0) return;
    if (!figure.originalRows || !figure.originalCols) return;
    if (W < 4 || H < 4) return;

    const handle = setTimeout(() => {
      // Map viewport (in original-data coords) to fractional source-cell
      // indices. xRange/yRange always span the source extent, regardless
      // of inline-preview downsampling.
      const fullCols = figure.originalCols;
      const fullRows = figure.originalRows;
      const xExt = figure.xRange[1] - figure.xRange[0];
      const yExt = figure.yRange[1] - figure.yRange[0];
      const colsPerUnit = fullCols / (xExt || 1);
      const rowsPerUnit = fullRows / (yExt || 1);

      const xLo = Math.min(xMin, xMax);
      const xHi = Math.max(xMin, xMax);
      const yLo = Math.min(yMin, yMax);
      const yHi = Math.max(yMin, yMax);
      let srcC0 = (xLo - figure.xRange[0]) * colsPerUnit;
      let srcC1 = (xHi - figure.xRange[0]) * colsPerUnit;
      // y axis: yRange[1] is at the TOP of the matrix (row 0), yRange[0] at bottom
      let srcR0 = (figure.yRange[1] - yHi) * rowsPerUnit;
      let srcR1 = (figure.yRange[1] - yLo) * rowsPerUnit;

      // Clamp to source bounds; log axes need strictly positive lo.
      srcC0 = Math.max(xLogActive ? 1e-6 : 0, srcC0);
      srcR0 = Math.max(yLogActive ? 1e-6 : 0, srcR0);
      srcC1 = Math.min(fullCols, srcC1);
      srcR1 = Math.min(fullRows, srcR1);
      const srcH = srcR1 - srcR0;
      const srcW = srcC1 - srcC0;
      if (srcH <= 0 || srcW <= 0) { setTileOverlay(null); return; }

      const buf = engine.getFigureDisplayTile(
        figure._figId, figure._axIdx, figure._dsIdx,
        srcR0, srcC0, srcH, srcW,
        H, W,
        xLogActive, yLogActive
      );
      if (!buf) { setTileOverlay(null); return; }

      const dataURL = renderHeatmapDataURLFromFlat(buf, H, W, lut);
      setTileOverlay({ dataURL });
    }, 100);

    return () => clearTimeout(handle);
  }, [interactive, engine, figure._figId, figure._axIdx, figure._dsIdx,
      figure.originalRows, figure.originalCols,
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
  const cbarInterp = getColormap(figure.colormap);
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
    {figure.downsampled && (
      <div className="hm-preview-banner" title="Engine downsampled this figure to keep the inline preview small. Zoom-in detail will arrive once tile-fetch lands.">
        preview · downsampled from {figure.originalRows}×{figure.originalCols}
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
          stretched across the data extent. The display-tile overlay, when
          present, fills the plot area at panel-pixel resolution with log/
          linear axes already applied — so it always matches what the axes
          ticks claim, including under log y or log x. */}
      {dataURL && (
        <g clipPath={`url(#${clipId})`}>
          <image href={dataURL}
            x={imgX} y={imgY} width={imgW} height={imgH}
            preserveAspectRatio="none"
            imageRendering="pixelated" />
          {tileOverlay && tileOverlay.dataURL && (
            <image href={tileOverlay.dataURL}
              x={padL} y={padT} width={W} height={H}
              preserveAspectRatio="none"
              imageRendering="pixelated" />
          )}
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

      {/* Colorbar */}
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
          {/* Sample uint8 idx (row, col) closest to cursor; reconstruct
              original-domain value via cminOrig + (idx/254) * range. If
              colorScaleBaked === 'log', the reconstructed value is in
              log10 space — show 10^v as the "real" scalar. */}
          {(() => {
            if (!figure.z) return null;
            const nR = figure.z.length, nC = figure.z[0]?.length || 0;
            if (!nR || !nC) return null;
            const u = (hover.x - figure.xRange[0]) / (figure.xRange[1] - figure.xRange[0]);
            const v = (figure.yRange[1] - hover.y) / (figure.yRange[1] - figure.yRange[0]);
            const c = Math.max(0, Math.min(nC - 1, Math.floor(u * nC)));
            const r = Math.max(0, Math.floor(v * nR));
            const rr = Math.max(0, Math.min(nR - 1, r));
            const idx = figure.z[rr]?.[c];
            let zStr = '—';
            if (idx != null && idx !== 255) {
              const range = cmaxOrig - cminOrig;
              const reconstructed = cminOrig + (idx / 254) * range;
              if (figure.colorScaleBaked === 'log') {
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
        </g>
      )}
    </svg>
    </>
  );
}
