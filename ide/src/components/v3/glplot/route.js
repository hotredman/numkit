// glplot/route.js — pure selection of which layers go to the WebGL overlay.
//
// Picks line/stairs layers with full in-JS data above a point threshold
// (downsampled previews and comet animations stay on the SVG/decimation
// path), and packs each into a GL series. The caller gates this on the
// feature flag + WebGL availability, so this logic stays unit-testable
// without a GPU.

import { packXY } from './pack';
import { cssColorToRGBA } from './color';

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
