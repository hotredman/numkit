/**
 * Colormaps for heatmap (imagesc) rendering. Each colormap is a function
 * that maps t ∈ [0,1] → "rgb(r,g,b)" string, plus a precomputed 256-entry
 * Uint8 LUT (built lazily and cached) for fast canvas rendering.
 *
 * Names match MATLAB / Octave conventions: parula (default), jet, hot,
 * cool, gray, bone, copper, spring, summer, autumn, winter, hsv, viridis.
 */

function lerpColor(stops, t) {
  t = Math.max(0, Math.min(1, t));
  if (t <= stops[0][0]) return stops[0];
  if (t >= stops[stops.length - 1][0]) return stops[stops.length - 1];
  for (let i = 0; i < stops.length - 1; i++) {
    const [p0, r0, g0, b0] = stops[i];
    const [p1, r1, g1, b1] = stops[i + 1];
    if (t >= p0 && t <= p1) {
      const f = (t - p0) / (p1 - p0);
      return [t, r0 + f * (r1 - r0), g0 + f * (g1 - g0), b0 + f * (b1 - b0)];
    }
  }
  return stops[stops.length - 1];
}

function makeInterpolator(stops) {
  return (t) => {
    const [, r, g, b] = lerpColor(stops, t);
    return `rgb(${Math.round(r * 255)},${Math.round(g * 255)},${Math.round(b * 255)})`;
  };
}

const interpolateParula = makeInterpolator([
  [0.00, 0.2422, 0.1504, 0.6603], [0.10, 0.2810, 0.1856, 0.7468],
  [0.20, 0.2272, 0.3391, 0.8007], [0.30, 0.1397, 0.5083, 0.7437],
  [0.40, 0.0200, 0.6400, 0.6500], [0.50, 0.1657, 0.7240, 0.5265],
  [0.60, 0.4544, 0.7678, 0.3723], [0.70, 0.7372, 0.7636, 0.2227],
  [0.80, 0.9644, 0.7150, 0.0777], [0.90, 0.9926, 0.7993, 0.1672],
  [1.00, 0.9769, 0.9839, 0.0805],
]);
const interpolateJet = makeInterpolator([
  [0.000, 0.0, 0.0, 0.5], [0.125, 0.0, 0.0, 1.0], [0.250, 0.0, 0.5, 1.0],
  [0.375, 0.0, 1.0, 1.0], [0.500, 0.5, 1.0, 0.5], [0.625, 1.0, 1.0, 0.0],
  [0.750, 1.0, 0.5, 0.0], [0.875, 1.0, 0.0, 0.0], [1.000, 0.5, 0.0, 0.0],
]);
const interpolateHot = makeInterpolator([
  [0.000, 0.04, 0.0, 0.0], [0.375, 1.0, 0.0, 0.0],
  [0.750, 1.0, 1.0, 0.0], [1.000, 1.0, 1.0, 1.0],
]);
const interpolateCool = makeInterpolator([[0.0, 0.0, 1.0, 1.0], [1.0, 1.0, 0.0, 1.0]]);
const interpolateGray = makeInterpolator([[0.0, 0.0, 0.0, 0.0], [1.0, 1.0, 1.0, 1.0]]);
const interpolateBone = makeInterpolator([
  [0.000, 0.0, 0.0, 0.0], [0.375, 0.3215, 0.3215, 0.4460],
  [0.750, 0.6540, 0.7840, 0.7840], [1.000, 1.0, 1.0, 1.0],
]);
const interpolateCopper = makeInterpolator([
  [0.0, 0.0, 0.0, 0.0], [0.8, 1.0, 0.504, 0.320], [1.0, 1.0, 0.630, 0.400],
]);
const interpolateSpring = makeInterpolator([[0.0, 1.0, 0.0, 1.0], [1.0, 1.0, 1.0, 0.0]]);
const interpolateSummer = makeInterpolator([[0.0, 0.0, 0.5, 0.4], [1.0, 1.0, 1.0, 0.4]]);
const interpolateAutumn = makeInterpolator([[0.0, 1.0, 0.0, 0.0], [1.0, 1.0, 1.0, 0.0]]);
const interpolateWinter = makeInterpolator([[0.0, 0.0, 0.0, 1.0], [1.0, 0.0, 1.0, 0.5]]);

function interpolateHsv(t) {
  t = Math.max(0, Math.min(1, t));
  const h = t * 360, c = 1, x = c * (1 - Math.abs((h / 60) % 2 - 1));
  let r, g, b;
  if (h < 60)       { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else              { r = c; g = 0; b = x; }
  return `rgb(${Math.round(r * 255)},${Math.round(g * 255)},${Math.round(b * 255)})`;
}

// Approx viridis stops (close to matplotlib viridis without d3 dependency).
const interpolateViridis = makeInterpolator([
  [0.00, 0.2670, 0.0049, 0.3294], [0.10, 0.2825, 0.1404, 0.4569],
  [0.20, 0.2533, 0.2654, 0.5298], [0.30, 0.2068, 0.3717, 0.5532],
  [0.40, 0.1638, 0.4717, 0.5581], [0.50, 0.1281, 0.5654, 0.5510],
  [0.60, 0.1349, 0.6585, 0.5176], [0.70, 0.2666, 0.7484, 0.4411],
  [0.80, 0.4776, 0.8214, 0.3181], [0.90, 0.7415, 0.8731, 0.1505],
  [1.00, 0.9932, 0.9062, 0.1439],
]);

const NAMED = {
  parula: interpolateParula,
  jet:    interpolateJet,
  hot:    interpolateHot,
  cool:   interpolateCool,
  gray:   interpolateGray,
  grey:   interpolateGray,
  bone:   interpolateBone,
  copper: interpolateCopper,
  spring: interpolateSpring,
  summer: interpolateSummer,
  autumn: interpolateAutumn,
  winter: interpolateWinter,
  hsv:    interpolateHsv,
  viridis: interpolateViridis,
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
