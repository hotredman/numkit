/**
 * Shared plot helpers — kept out of FigureWindow / InteractivePlot etc. so
 * the per-plot context menus can reuse the same fit + export logic the
 * top-level toolbar uses.
 */
import { defaultPolarViewport, nicePolarMax } from './PolarPlot';

/**
 * Fit a cartesian viewport to the data bounds. `mode` selects which series
 * to scan ('all' or a series name); `axisMode` is 'both' / 'x' / 'y'.
 * Window-clipping for the inactive axis matches the FigureWindow toolbar.
 */
export function computeFitViewport(series, mode, axisMode, currentVp, figDefault) {
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

/** Polar fit — pick max |rho| across selected series, round up to a nice tick. */
export function computeFitPolar(series, mode, currentVp) {
  const list = mode === 'all' ? series : series.filter((s) => s.name === mode);
  let m = 0;
  list.forEach((s) => s.rho?.forEach((v) => {
    if (Number.isFinite(v) && Math.abs(v) > m) m = Math.abs(v);
  }));
  return { r: [currentVp.r[0], nicePolarMax(m || 1)] };
}

/** Default viewport for a figure — picks the right shape per kind. */
export function defaultViewport(figure) {
  if (figure.kind === 'polar') return defaultPolarViewport(figure);
  if (figure.xRange && figure.yRange) {
    return { x: figure.xRange.slice(), y: figure.yRange.slice() };
  }
  return { x: [-1, 1], y: [-1, 1] };
}

// ── exports ─────────────────────────────────────────────────────────────

export function downloadBlob(blob, name) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 100);
}

/** Serialise a single SVG node and trigger an SVG download. */
export function exportSvgNode(svgEl, name) {
  if (!svgEl) return;
  const xml = new XMLSerializer().serializeToString(svgEl);
  downloadBlob(new Blob([xml], { type: 'image/svg+xml' }), name);
}

/** Rasterise an SVG node to PNG at `scale`× and download. */
export function exportPngNode(svgEl, w, h, scale, name) {
  if (!svgEl) return;
  const xml = new XMLSerializer().serializeToString(svgEl);
  const ww = Math.max(1, Math.round(w * scale));
  const hh = Math.max(1, Math.round(h * scale));
  const img = new Image();
  const url = URL.createObjectURL(new Blob([xml], { type: 'image/svg+xml' }));
  img.onload = () => {
    const c = document.createElement('canvas');
    c.width = ww; c.height = hh;
    const ctx = c.getContext('2d');
    const bg = getComputedStyle(document.documentElement).getPropertyValue('--plot-bg').trim() || '#1a1f24';
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, ww, hh);
    ctx.drawImage(img, 0, 0, ww, hh);
    c.toBlob((b) => { downloadBlob(b, name); URL.revokeObjectURL(url); });
  };
  img.src = url;
}
