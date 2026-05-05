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
  return NAMED[name?.toLowerCase()] || interpolateParula;
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
 * Render a 2D z-matrix to a data: URL via an offscreen canvas. Z is rows × cols
 * of numeric values, mapped through `colormap` (a name or interpolator) over
 * [cmin, cmax]. Returns a data:image/png;base64,… string suitable for SVG <image>.
 */
export function renderHeatmapDataURL(z, cmin, cmax, colormap = 'parula') {
  const interp = typeof colormap === 'function' ? colormap : getColormap(colormap);
  const numRows = z.length;
  const numCols = z[0]?.length || 0;
  if (!numRows || !numCols) return null;
  const canvas = document.createElement('canvas');
  canvas.width  = numCols;
  canvas.height = numRows;
  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(numCols, numRows);
  const px  = img.data;
  const lut = getLUT(interp);
  const range = cmax - cmin || 1;
  const inv = 255 / range;
  for (let r = 0; r < numRows; r++) {
    const row = z[r];
    const off = r * numCols * 4;
    for (let c = 0; c < numCols; c++) {
      const v = row[c];
      const o = off + c * 4;
      if (v == null || !Number.isFinite(v)) {
        px[o] = 0; px[o + 1] = 0; px[o + 2] = 0; px[o + 3] = 0;
      } else {
        let i = ((v - cmin) * inv) | 0;
        if (i < 0) i = 0; else if (i > 255) i = 255;
        const li = i * 3;
        px[o]     = lut[li];
        px[o + 1] = lut[li + 1];
        px[o + 2] = lut[li + 2];
        px[o + 3] = 255;
      }
    }
  }
  ctx.putImageData(img, 0, 0);
  return canvas.toDataURL();
}
