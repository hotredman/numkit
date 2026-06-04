// glplot/route.js — pure selection of which layers go to the WebGL overlay.
//
// Picks line/stairs layers with full in-JS data above a point threshold
// (downsampled previews and comet animations stay on the SVG/decimation
// path), and packs each into a GL series. The caller gates this on the
// feature flag + WebGL availability, so this logic stays unit-testable
// without a GPU.

import { packXY } from './pack';
import { cssColorToRGBA } from './color';
import { polarToScreen } from './transforms';
import { markerCell } from './markerAtlas';

// GL is usable for a plot when the feature flag is on AND WebGL2 exists —
// regardless of interactivity. The interactive window renders a live GL
// overlay; non-interactive preview cards rasterize the same GL series to an
// <image> via previewRaster (one shared context, see previewRaster.js), so
// previews stay pixel-identical to the window without a context per card.
// → boolean
export function glRoutable(flag, webgl2) {
  return !!(flag && webgl2);
}

// The GL overlay is ON by default; users opt OUT by setting
// localStorage 'numkit.plot.gl' = '0' (any other value, or unset, keeps it
// on). `storage` is window.localStorage (or null when unavailable, e.g. SSR)
// so this stays a pure, unit-testable derivation.
// → boolean
export function glFlagFromStorage(storage) {
  if (!storage) return true;
  return storage.getItem('numkit.plot.gl') !== '0';
}

// Scatter routes to GL much earlier than lines: SVG draws one DOM node per
// marker, so a few thousand already janks, while one POINTS draw is free.
export const SCATTER_MIN = 2000;
// Markers on a LINE are annotations: 300k markers on a curve overlap into a
// solid mush you can't read, and drawing them all is huge textured-sprite
// overdraw. Cap to this many (strided); the strip itself stays full-res.
const MAX_LINE_MARKERS = 4000;

// Even-stride subsample of (x, y) down to ≤ max points.
function strideXY(x, y, max) {
  const n = Math.min(x.length, y.length);
  if (n <= max) return [x, y];
  const step = Math.ceil(n / max);
  const sx = [];
  const sy = [];
  for (let i = 0; i < n; i += step) { sx.push(x[i]); sy.push(y[i]); }
  return [sx, sy];
}

// Selects full-data line/stairs (above `minPoints`) and scatter (above
// SCATTER_MIN) layers, packing each into a GL series tagged with its draw mode
// + marker atlas cell. Any MATLAB marker shape routes (the point shader samples
// the shape atlas). A marked line (plot(x,y,'o-')) emits TWO series: a
// full-resolution strip plus a *decimated* marker scatter. Downsampled (>1M)
// lines stay for selectGLBigSeries; comet stays on SVG.
// → { routed: Set<idx>, series: [{ data, segments, color, mode, size, marker }] }
export function selectGLSeries(layers, minPoints) {
  const routed = new Set();
  const series = [];
  const ls = layers || [];
  const push = (x, y, color, mode, size, marker, filled = false) => {
    const p = packXY(x, y);
    series.push({ data: p.data, segments: p.segments, color, mode, size, marker, filled });
  };
  for (let i = 0; i < ls.length; i++) {
    const ly = ls[i];
    if (!ly || ly.kind !== 'series' || ly.cometAnim || !Array.isArray(ly.x)) continue;
    const color = cssColorToRGBA(ly.color);
    if (ly.mode === 'line' || ly.mode === 'stairs') {
      if (ly.seriesDownsampled) continue;            // >1M → selectGLBigSeries
      const mkr = (ly.marker && ly.marker !== 'none') ? markerCell(ly.marker) : -1;
      const hasLine = ly.lineStyle !== 'none' && ly.lineStyle !== '';
      const size = ly.size || 4;
      if (hasLine) {
        if (ly.x.length <= minPoints) continue;      // line: pixel-width threshold
        push(ly.x, ly.y, color, 'line', size, -1);   // full-res strip
        if (mkr >= 0) {                              // + decimated marker overlay
          const [mx, my] = strideXY(ly.x, ly.y, MAX_LINE_MARKERS);
          push(mx, my, color, 'scatter', size, mkr, !!ly.filled);
        }
        routed.add(i);
      } else if (mkr >= 0) {
        if (ly.x.length <= SCATTER_MIN) continue;    // markers-only line → scatter
        push(ly.x, ly.y, color, 'scatter', size, mkr, !!ly.filled);
        routed.add(i);
      }
      // else: no line, no marker → skip
    } else if (ly.mode === 'scatter') {
      if (ly.x.length <= SCATTER_MIN) continue;
      push(ly.x, ly.y, color, 'scatter', ly.size || 3, markerCell(ly.marker), !!ly.filled);
      routed.add(i);
    }
  }
  return { routed, series };
}

// Pure selection of the engine-downsampled (>1M) line/stairs layers that the
// WebGL overlay takes over via GPU-LOD tiles. These layers ship only a small
// preview in JS (ly.x is the M4 preview, not the full signal), so we DON'T
// pack here — we return descriptors the caller resolves by fetching a
// decimated viewport tile (engine.getSeriesTile) and packing that. Keeping the
// engine call out of this function keeps it unit-testable.
//
// `marker` (atlas cell, <0 = none) + `size` ride along so the caller can draw
// markers on a downsampled marked line — the tile is the line AND the marker
// positions, so >1M plot(x,y,'o-') keeps its (decimated) markers.
// → [{ idx, figId, axIdx, dsIdx, color, marker, size }]
export function selectGLBigSeries(layers) {
  const out = [];
  const ls = layers || [];
  for (let i = 0; i < ls.length; i++) {
    const ly = ls[i];
    if (!ly || ly.kind !== 'series') continue;
    if (ly.mode !== 'line' && ly.mode !== 'stairs') continue;
    if (!ly.seriesDownsampled || ly.cometAnim) continue;
    if (ly.dsIdx == null || ly.figId == null || ly.axIdx == null) continue;
    out.push({
      idx: i,
      figId: ly.figId,
      axIdx: ly.axIdx,
      dsIdx: ly.dsIdx,
      color: cssColorToRGBA(ly.color),
      marker: (ly.marker && ly.marker !== 'none') ? markerCell(ly.marker) : -1,
      size: ly.size || 4,
      filled: !!ly.filled,
    });
  }
  return out;
}

// Selects polar line / scatter series big enough to benefit from GL and packs
// them in SCREEN space (polarToScreen → exactly PolarPlot's pixels), so the GL
// overlay lands on the SVG grid. Polar lines are one cheap SVG path, so they
// route only past `lineMin` (like cartesian); polarscatter draws one DOM node
// per point, so it routes past `scatterMin`. bubble/compass/bar/rose stay SVG.
//   series: [{ theta, rho, mode, color, size }]  (color pre-resolved)
//   layout: { cx, cy, radius, rMin, rMax, zero, dirSign }
// → { routed: Set<seriesIndex>, series: [{ data, segments, color, mode, size }] }
export function selectGLPolarSeries(series, layout, { lineMin, scatterMin }) {
  const routed = new Set();
  const out = [];
  const ls = series || [];
  for (let i = 0; i < ls.length; i++) {
    const s = ls[i];
    if (!s || !Array.isArray(s.theta) || !Array.isArray(s.rho)) continue;
    const n = Math.min(s.theta.length, s.rho.length);
    let mode = null;
    if ((s.mode || 'line') === 'line') {
      if (n <= lineMin) continue;
      mode = 'line';
    } else if (s.mode === 'scatter') {
      if (n <= scatterMin) continue;
      mode = 'scatter';
    } else {
      continue;   // bubble / compass / bar / rose → SVG
    }
    const { x, y } = polarToScreen(s.theta, s.rho, layout);
    const packed = packXY(x, y);
    out.push({
      data: packed.data,
      segments: packed.segments,
      color: cssColorToRGBA(s.color),
      mode,
      size: s.size || 3,
      marker: mode === 'scatter' ? markerCell(s.marker) : -1,
      filled: !!s.filled,      // polarscatter is open by default (MATLAB), like scatter
    });
    routed.add(i);
  }
  return { routed, series: out };
}
