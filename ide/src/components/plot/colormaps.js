/**
 * Colormaps for heatmap (imagesc) rendering. The named maps resolve
 * through MATLAB R2025b ground-truth 256-entry tables (see
 * ./colormapTables.js, generated from the same dump the engine export
 * renderer bakes in — byte-identical, portion 35). Custom palettes
 * interpolate a user N×3 matrix. getLUT() pre-samples an interpolator
 * into a 256×3 Uint8 LUT for canvas rendering; buildHeatmapLUT adds the
 * clim window remap on top.
 */

import { tableOf } from './colormapTables.js';

function makeTableInterpolator(name) {
  return (t) => {
    t = Math.max(0, Math.min(1, t));
    const tb = tableOf(name);
    const i = Math.min(255, Math.max(0, Math.round(t * 255))) * 3;
    return `rgb(${tb[i]},${tb[i + 1]},${tb[i + 2]})`;
  };
}

const interpolateParula = makeTableInterpolator('parula');

const NAMED = {
  parula:   makeTableInterpolator('parula'),
  turbo:    makeTableInterpolator('turbo'),
  jet:      makeTableInterpolator('jet'),
  hot:      makeTableInterpolator('hot'),
  cool:     makeTableInterpolator('cool'),
  gray:     makeTableInterpolator('gray'),
  grey:     makeTableInterpolator('gray'),
  bone:     makeTableInterpolator('bone'),
  copper:   makeTableInterpolator('copper'),
  spring:   makeTableInterpolator('spring'),
  summer:   makeTableInterpolator('summer'),
  autumn:   makeTableInterpolator('autumn'),
  winter:   makeTableInterpolator('winter'),
  hsv:      makeTableInterpolator('hsv'),
  viridis:  makeTableInterpolator('viridis'),
};

export function getColormap(name) {
  // Already an interpolator function? (custom palette path.)
  if (typeof name === 'function') return name;
  return NAMED[name?.toLowerCase?.()] || interpolateParula;
}

/**
 * Build an interpolator function from a custom N×3 RGB matrix.
 * Each row is [r, g, b] in [0, 1] (MATLAB convention). The returned
 * function takes t∈[0, 1] and returns "rgb(R,G,B)" (matching the
 * named colormap interpolators).
 */
export function makeCustomColormap(matrix) {
  if (!Array.isArray(matrix) || matrix.length === 0) {
    return interpolateParula;
  }
  return (t) => {
    const ct = Math.max(0, Math.min(1, t));
    const idx = ct * (matrix.length - 1);
    const i0 = Math.floor(idx);
    const i1 = Math.min(matrix.length - 1, i0 + 1);
    const f = idx - i0;
    const a = matrix[i0] || [0, 0, 0];
    const b = matrix[i1] || a;
    const r = Math.round(255 * (a[0] + f * (b[0] - a[0])));
    const g = Math.round(255 * (a[1] + f * (b[1] - a[1])));
    const bb = Math.round(255 * (a[2] + f * (b[2] - a[2])));
    return `rgb(${r},${g},${bb})`;
  };
}

const lutCache = new WeakMap();
export function getLUT(interp) {
  let lut = lutCache.get(interp);
  if (lut) return lut;
  lut = new Uint8Array(256 * 3);
  for (let i = 0; i < 256; i++) {
    const c = interp(i / 255);
    const m = c.match(/(\d+)/g);
    if (m && m.length >= 3) {
      lut[i * 3]     = parseInt(m[0]);
      lut[i * 3 + 1] = parseInt(m[1]);
      lut[i * 3 + 2] = parseInt(m[2]);
    }
  }
  lutCache.set(interp, lut);
  return lut;
}

/**
 * Build a 256-entry RGBA LUT for a colormap, optionally remapped via a
 * window/level overlay (cminEff/cmaxEff) over the original quantization
 * range (cminOrig/cmaxOrig). Index 255 is the NaN sentinel — always RGBA 0.
 *
 * When colorScaleBaked === 'log' the original values were log10()'d before
 * quantization, so the cminEff/cmaxEff values are already in log space —
 * the linear remap inside the LUT is the right thing.
 *
 *   uint8_data[idx] → lut[idx*4 .. idx*4+3] = (R, G, B, A)
 *
 * Returns Uint8Array of length 1024 (256 entries × 4 channels).
 */
export function buildHeatmapLUT(colormap, cminOrig, cmaxOrig, cminEff, cmaxEff) {
  const interp = typeof colormap === 'function' ? colormap : getColormap(colormap);
  const lut = new Uint8Array(256 * 4);
  const colors = getLUT(interp);  // 256 × 3 RGB
  const origRange = cmaxOrig - cminOrig || 1;
  const effRange  = cmaxEff  - cminEff  || 1;

  for (let i = 0; i < 255; i++) {
    // The original value this index represents (linear interp inside the
    // baked quantization range — assumes 254 levels span [cminOrig..cmaxOrig]).
    const origValue = cminOrig + (i / 254) * origRange;
    // Where this falls in the effective display window:
    let t = (origValue - cminEff) / effRange;
    if (t < 0) t = 0;
    else if (t > 1) t = 1;
    // Quantize back into the colormap's 256 sample points.
    const ci = Math.min(255, Math.max(0, Math.round(t * 255)));
    const off = i * 4;
    lut[off]     = colors[ci * 3];
    lut[off + 1] = colors[ci * 3 + 1];
    lut[off + 2] = colors[ci * 3 + 2];
    lut[off + 3] = 255;
  }
  // 255 = NaN sentinel → transparent
  lut[255 * 4 + 3] = 0;
  return lut;
}

/**
 * Render a uint8-quantized heatmap (rows × cols indices 0..254 / 255 NaN) to
 * a data: URL via an offscreen canvas + the precomputed LUT from
 * buildHeatmapLUT(). Three-table indirection per pixel — fast (no float math).
 *
 * Vertical flip CONDITIONAL on `flipY`. zRows[0] = matrix row 1 from the
 * engine. For axis-xy (yDir='normal', low data y at panel BOTTOM) the flip
 * puts matrix row 1 at panel BOTTOM as expected. For axis-ij (yDir='reverse',
 * low data y at panel TOP) NO flip — canvas drawn top-down from zRows[0]
 * naturally puts matrix row 1 at the top. Symptom of unconditional flip:
 * preview cards (which only render this inline path, not the tile overlay)
 * showed imshow / imagesc images mirrored vs. the modal window.
 */
export function renderHeatmapDataURLFromIndices(zRows, lut, flipY = true) {
  const numRows = zRows.length;
  const numCols = zRows[0]?.length || 0;
  if (!numRows || !numCols) return null;
  const canvas = document.createElement('canvas');
  canvas.width  = numCols;
  canvas.height = numRows;
  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(numCols, numRows);
  const px  = img.data;
  for (let r = 0; r < numRows; r++) {
    const row = zRows[flipY ? (numRows - 1 - r) : r];
    const off = r * numCols * 4;
    for (let c = 0; c < numCols; c++) {
      const idx = row[c];
      const o = off + c * 4;
      const li = idx * 4;
      px[o]     = lut[li];
      px[o + 1] = lut[li + 1];
      px[o + 2] = lut[li + 2];
      px[o + 3] = lut[li + 3];
    }
  }
  ctx.putImageData(img, 0, 0);
  return canvas.toDataURL();
}

/** Render a flat Uint8Array (row-major) version — for tile overlay.
 *  Vertical flip CONDITIONAL on `flipY`: the engine's getFigureDisplayTile
 *  emits buf[0..displayW] = top of the source-rect (low matrix row index).
 *  For axis-xy (yDir='normal', low data y at panel BOTTOM) the buffer needs
 *  flipping so matrix row 1 lands at panel bottom. For axis-ij
 *  (yDir='reverse', low data y at panel TOP) NO flip — canvas drawn top-down
 *  from buf[0] puts matrix row 1 at the top naturally.
 *
 *  rows / cols MUST be integers — fractional dims (from non-integer panel
 *  measurements) silently produce diagonal stripes because srcOff drifts
 *  by 0.5 × row each iteration when cols is fractional. Coerce defensively. */
export function renderHeatmapDataURLFromFlat(arr, rows, cols, lut, flipY = true) {
  rows = rows | 0;
  cols = cols | 0;
  if (!rows || !cols) return null;
  const canvas = document.createElement('canvas');
  canvas.width  = cols;
  canvas.height = rows;
  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(cols, rows);
  const px  = img.data;
  for (let r = 0; r < rows; r++) {
    const srcRow = flipY ? (rows - 1 - r) : r;
    const srcOff = srcRow * cols;
    const dstOff = r * cols;
    for (let c = 0; c < cols; c++) {
      const idx = arr[srcOff + c];
      const o = (dstOff + c) * 4;
      const li = idx * 4;
      px[o]     = lut[li];
      px[o + 1] = lut[li + 1];
      px[o + 2] = lut[li + 2];
      px[o + 3] = lut[li + 3];
    }
  }
  ctx.putImageData(img, 0, 0);
  return canvas.toDataURL();
}
