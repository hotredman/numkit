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

/** Axis-equal/-image cells lock the cartesian axes together
 *  (DataAspectRatio = [1 1 1]) — single-axis fit on them dispatches
 *  as 'both' to preserve the contract. Takes the EFFECTIVE aspect
 *  (UI override > script) so the UI's aspect radio is honoured even
 *  when the script didn't set axis equal. Exposed separately because
 *  CompositePlot's log-clamp branch needs the upgraded mode to
 *  decide which axes to clamp. */
export function upgradeFitAxis(aspectMode, axisMode) {
  if ((aspectMode === 'equal' || aspectMode === 'image')
      && (axisMode === 'x' || axisMode === 'y' || axisMode === 'z')) {
    return 'both';
  }
  return axisMode;
}

/** Single source of truth for "fit this cell on this axis". Used by
 *  EVERY fit pathway in the IDE — toolbar fit ▾ (figure-wide or
 *  fan-out across subplot cells), ПКМ Fit submenu, and direct calls
 *  from per-axis handlers.
 *
 *  `cell`     — the figure JSON for ONE cell (cartesian / polar /
 *               composite3d).
 *  `currentViewport` — previous viewport (`{x,y[,z]}` or `{r,theta}`).
 *  `axisMode` — 'both' | 'x' | 'y' | 'z' | 'r' | 'theta'.
 *
 *  Behaviour:
 *    • axis-equal / axis-image cells upgrade single-axis fit to
 *      'both' (DataAspectRatio = [1 1 1] contract — can't refit one
 *      axis without the other).
 *    • Polar cells honour 'r' / 'theta' only (cartesian axes resolve
 *      to no-op on polar; vice versa). Mirrors the toolbar=universal
 *      policy where the click on an inapplicable axis is a no-op.
 *    • Returns the target viewport from `defaultViewport(cell)` for
 *      the requested axis (script xlim/ylim if set, padded data
 *      extent otherwise). Always returns a NEW object — callers can
 *      pass it straight to setViewport.
 *
 *  Per-series fit (e.g. ПКМ "Fit single curve") stays in the per-
 *  plot component because it scans ONE series's data points — a
 *  different scope.
 *
 *  `options.aspectMode` — explicit aspect override (UI radio). Falls
 *  back to `cell.axisMode` (script value) when not provided.
 */
export function fitCellViewport(cell, currentViewport, axisMode, options = {}) {
  if (!cell) return currentViewport || { x: [-1, 1], y: [-1, 1] };
  const aspect = (options.aspectMode !== undefined && options.aspectMode !== null)
    ? options.aspectMode
    : (cell.axisMode || '');
  axisMode = upgradeFitAxis(aspect, axisMode);
  const def = defaultViewport(cell);
  const cur = currentViewport || def;
  if (axisMode === 'both') return def;
  // Polar cells: only r / theta apply.
  if (cell.kind === 'polar') {
    if (axisMode === 'r')     return { ...cur, r:     def.r };
    if (axisMode === 'theta') return { ...cur, theta: def.theta };
    return cur;
  }
  // Cartesian + 3D cells: only x / y / z apply.
  if (axisMode === 'x') return { ...cur, x: def.x };
  if (axisMode === 'y') return { ...cur, y: def.y };
  if (axisMode === 'z') return def.z ? { ...cur, z: def.z } : cur;
  return cur;
}

// ── exports ─────────────────────────────────────────────────────────────

export function downloadBlob(blob, name) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 100);
}

/**
 * Light-theme palette baked for export. Two reasons we need this:
 *  1. Plots are nearly always reused on white paper / light slides.
 *     Saving a dark-theme plot just to white-correct it later is annoying.
 *  2. When the SVG is loaded via Image() for PNG rasterisation, it runs in
 *     its OWN document context — the host page's CSS variables are gone.
 *     Without an inlined `<style>`, every `var(--plot-grid)` etc. resolves
 *     to nothing, so grids / text / frames disappear.
 *
 * Opacities are slightly stronger than the on-screen light theme to
 * survive print compression / low-end laser printers.
 */
const EXPORT_LIGHT_STYLE = `
  :root, svg {
    --bg-0: #ffffff;
    --bg-1: #ffffff;
    --bg-2: #f6f8fa;
    --bg-3: #eaeef2;
    --line: #d0d7de;
    --line-soft: #e5e8eb;
    --fg-0: #1f2328;
    --fg-1: #2c3137;
    --fg-2: #57606a;
    --fg-3: #818b96;
    --plot-bg:          #ffffff;
    --plot-frame:       rgba(31, 35, 40, 0.55);
    --plot-grid:        rgba(31, 35, 40, 0.22);
    --plot-grid-min:    rgba(31, 35, 40, 0.09);
    --plot-tick:        rgba(31, 35, 40, 0.70);
    --plot-text:        rgba(31, 35, 40, 0.85);
    --plot-text-strong: rgba(31, 35, 40, 0.95);
    --plot-cross:       rgba(31, 35, 40, 0.25);
    --plot-tip-bg:      #ffffff;
    --plot-tip-stroke:  rgba(31, 35, 40, 0.35);
    --plot-tip-text:    rgba(31, 35, 40, 0.92);
  }
`;
const EXPORT_BG = '#ffffff';

/** Inject the light-theme `<style>` block right after `<svg ...>`. */
function withLightTheme(xml) {
  return xml.replace(/<svg([^>]*)>/, (_, attrs) => `<svg${attrs}><style>${EXPORT_LIGHT_STYLE}</style>`);
}

/** Serialise a single SVG node and trigger an SVG download. */
export function exportSvgNode(svgEl, name) {
  if (!svgEl) return;
  const xml = withLightTheme(new XMLSerializer().serializeToString(svgEl));
  downloadBlob(new Blob([xml], { type: 'image/svg+xml' }), name);
}

/** Rasterise an SVG node to PNG at `scale`× and download. Always light-themed. */
export function exportPngNode(svgEl, w, h, scale, name) {
  if (!svgEl) return;
  const xml = withLightTheme(new XMLSerializer().serializeToString(svgEl));
  const ww = Math.max(1, Math.round(w * scale));
  const hh = Math.max(1, Math.round(h * scale));
  const img = new Image();
  const url = URL.createObjectURL(new Blob([xml], { type: 'image/svg+xml' }));
  img.onload = () => {
    const c = document.createElement('canvas');
    c.width = ww; c.height = hh;
    const ctx = c.getContext('2d');
    ctx.fillStyle = EXPORT_BG;
    ctx.fillRect(0, 0, ww, hh);
    ctx.drawImage(img, 0, 0, ww, hh);
    c.toBlob((b) => { downloadBlob(b, name); URL.revokeObjectURL(url); });
  };
  img.src = url;
}

/**
 * Print-ready PNG export. `mmWidth` is the physical width of the figure on
 * paper, `dpi` the dot density (300 is the de-facto standard for journals).
 * Height auto-derives from the on-screen aspect ratio so the data isn't
 * stretched. Filename is suffixed with `_<mm>mm` for traceability.
 */
export function exportPngForPrint(svgEl, w, h, mmWidth, dpi, baseName) {
  if (!svgEl) return;
  const targetPx = (mmWidth / 25.4) * dpi;
  const scale = targetPx / w;
  const safeBase = baseName.replace(/\.png$/i, '');
  exportPngNode(svgEl, w, h, scale, `${safeBase}_${mmWidth}mm.png`);
}

/**
 * Build one composite SVG string from many side-by-side SVGs (used for
 * subplot figures: each cell is its own <svg>). `layouts` is an array of
 * { x, y, w, h } in container-local coordinates that mirror the screen
 * positioning of each cell.
 *
 * Inner <svg> elements are cloned and have their width/height set to the
 * cell's pixel size — left at "100%" they'd all stretch to fill the outer
 * viewBox and overlap.
 */
export function composeSvgsToString(svgs, layouts, totalW, totalH) {
  const inner = Array.from(svgs).map((svg, i) => {
    const { x, y, w, h } = layouts[i];
    const cloned = svg.cloneNode(true);
    cloned.setAttribute('width',  String(w));
    cloned.setAttribute('height', String(h));
    cloned.setAttribute('preserveAspectRatio', 'xMidYMid meet');
    const xml = new XMLSerializer().serializeToString(cloned);
    return `<g transform="translate(${x},${y})">${xml}</g>`;
  }).join('');
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${totalW}" height="${totalH}" viewBox="0 0 ${totalW} ${totalH}"><rect width="${totalW}" height="${totalH}" fill="${EXPORT_BG}"/>${inner}</svg>`;
}

/**
 * Wrap a serialised XML string with the light-theme style block. Useful
 * when a caller already has its own composed SVG string (subplot exports
 * in FigureWindow) and wants the same theme override that exportSvgNode
 * applies for single-SVG flows.
 */
export function applyLightTheme(xml) { return withLightTheme(xml); }

/** Save a pre-built SVG XML string as .svg, applying the light theme. */
export function exportSvgString(xml, name) {
  downloadBlob(new Blob([withLightTheme(xml)], { type: 'image/svg+xml' }), name);
}

/**
 * Rasterise a pre-built SVG XML string to PNG. Used for subplot composite
 * exports where there's no single source SVG element.
 */
export function exportPngString(xml, w, h, scale, name) {
  const themed = withLightTheme(xml);
  const ww = Math.max(1, Math.round(w * scale));
  const hh = Math.max(1, Math.round(h * scale));
  const img = new Image();
  const url = URL.createObjectURL(new Blob([themed], { type: 'image/svg+xml' }));
  img.onload = () => {
    const c = document.createElement('canvas');
    c.width = ww; c.height = hh;
    const ctx = c.getContext('2d');
    ctx.fillStyle = EXPORT_BG;
    ctx.fillRect(0, 0, ww, hh);
    ctx.drawImage(img, 0, 0, ww, hh);
    c.toBlob((b) => { downloadBlob(b, name); URL.revokeObjectURL(url); });
  };
  img.src = url;
}
