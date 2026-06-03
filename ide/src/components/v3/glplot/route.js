// glplot/route.js — pure selection of which layers go to the WebGL overlay.
//
// Picks line/stairs layers with full in-JS data above a point threshold
// (downsampled previews and comet animations stay on the SVG/decimation
// path), and packs each into a GL series. The caller gates this on the
// feature flag + WebGL availability, so this logic stays unit-testable
// without a GPU.

import { packXY } from './pack';
import { cssColorToRGBA } from './color';

// Whether the WebGL overlay should be used for this plot AT ALL.
//
// GL is an *interactive-window-only* acceleration. Non-interactive figure
// preview cards (FiguresPane → interactive=false) must stay on the SVG
// decimation path for two reasons:
//   1. Each live GL plot holds a WebGL context and browsers cap those at
//      ~16 — routing every preview thumbnail to GL exhausts contexts and
//      the oldest cards lose their context (blank / garbage render).
//   2. At preview size the raw line buffer is tiny; a dense oscillation
//      drawn raw reads as a solid fill, whereas SVG M4 gives the familiar
//      decimated thumbnail. Previews don't pan/zoom, so GL's O(1) viewport
//      update buys nothing there.
// → boolean
export function glOverlayEnabled({ interactive, flag, webgl2 }) {
  return !!(interactive && flag && webgl2);
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

// → { routed: Set<layerIndex>, series: [{ data, segments, color }] }
export function selectGLSeries(layers, minPoints) {
  const routed = new Set();
  const series = [];
  const ls = layers || [];
  for (let i = 0; i < ls.length; i++) {
    const ly = ls[i];
    if (!ly || ly.kind !== 'series') continue;
    if (ly.mode !== 'line' && ly.mode !== 'stairs') continue;
    if (ly.seriesDownsampled || ly.cometAnim) continue;
    if (!Array.isArray(ly.x) || ly.x.length <= minPoints) continue;
    const packed = packXY(ly.x, ly.y);
    series.push({
      data: packed.data,
      segments: packed.segments,
      color: cssColorToRGBA(ly.color),
    });
    routed.add(i);
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
// → [{ idx, figId, axIdx, dsIdx, color }]
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
    });
  }
  return out;
}
