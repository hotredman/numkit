import { describe, it, expect } from 'vitest';
import {
  selectGLSeries, glRoutable, glFlagFromStorage, selectGLBigSeries,
  selectGLPolarSeries,
} from './route';

const line = (n, color = '#ff0000') => ({
  kind: 'series', mode: 'line', color,
  x: Array.from({ length: n }, (_, i) => i),
  y: Array.from({ length: n }, (_, i) => i),
});
const scatter = (n, over = {}) => ({ ...line(n), mode: 'scatter', ...over });

describe('selectGLSeries', () => {
  it('routes a large full-data line layer and packs it', () => {
    const { routed, series } = selectGLSeries([line(60000)], 50000);
    expect(routed.has(0)).toBe(true);
    expect(series).toHaveLength(1);
    expect(series[0].data).toBeInstanceOf(Float32Array);
    expect(series[0].data.length).toBe(60000 * 2);
    expect(series[0].color).toEqual([1, 0, 0, 1]);
    expect(series[0].mode).toBe('line');
  });

  it('routes a large scatter as POINTS mode with size + disc marker', () => {
    const { routed, series } = selectGLSeries([scatter(5000, { size: 4 })], 50000);
    expect(routed.has(0)).toBe(true);
    expect(series[0].mode).toBe('scatter');
    expect(series[0].size).toBe(4);
    expect(series[0].marker).toBe(0);             // default → disc cell
    expect(series[0].filled).toBe(false);         // MATLAB default → open
    expect(series[0].data.length).toBe(5000 * 2);
  });

  it('carries the filled flag (scatter(...,"filled"))', () => {
    expect(selectGLSeries([scatter(5000, { filled: true })], 50000).series[0].filled).toBe(true);
    expect(selectGLSeries([scatter(5000)], 50000).series[0].filled).toBe(false);
  });

  it('routes ANY marker shape, carrying its atlas cell', () => {
    expect(selectGLSeries([scatter(5000, { marker: 's' })], 50000).series[0].marker).toBe(1);
    expect(selectGLSeries([scatter(5000, { marker: 'd' })], 50000).series[0].marker).toBe(2);
    expect(selectGLSeries([scatter(5000, { marker: 'x' })], 50000).series[0].marker).toBe(10);
  });

  it('a marked line emits a full strip + a DECIMATED marker scatter', () => {
    const plain = selectGLSeries([line(60000)], 50000);
    expect(plain.series).toHaveLength(1);
    expect(plain.series[0]).toMatchObject({ mode: 'line', marker: -1 });

    const marked = selectGLSeries([{ ...line(60000), marker: 'o' }], 50000);
    expect(marked.routed.has(0)).toBe(true);
    expect(marked.series).toHaveLength(2);
    expect(marked.series[0]).toMatchObject({ mode: 'line', marker: -1 });    // full strip
    expect(marked.series[0].data.length).toBe(60000 * 2);
    expect(marked.series[1]).toMatchObject({ mode: 'scatter', marker: 0 });  // markers
    expect(marked.series[1].data.length).toBeLessThanOrEqual(4000 * 2);      // capped

    // marker 'none' → plain line, no marker overlay
    const none = selectGLSeries([{ ...line(60000), marker: 'none' }], 50000);
    expect(none.series).toHaveLength(1);
    expect(none.series[0].marker).toBe(-1);
  });

  it('skips small series, downsampled and comet', () => {
    const layers = [
      line(100),                                    // line below threshold
      scatter(100),                                 // scatter below SCATTER_MIN
      { ...line(60000), seriesDownsampled: true },  // engine preview → selectGLBigSeries
      { ...line(60000), cometAnim: true },          // animated
      { kind: 'heatmap' },                          // not a series
    ];
    const { routed, series } = selectGLSeries(layers, 50000);
    expect(routed.size).toBe(0);
    expect(series).toHaveLength(0);
  });

  it('routes by index, leaving the rest on SVG', () => {
    const { routed } = selectGLSeries([line(100), line(60000), scatter(5000)], 50000);
    expect([...routed]).toEqual([1, 2]);
  });

  it('tolerates missing / empty input', () => {
    expect(selectGLSeries(null, 50000).series).toEqual([]);
    expect(selectGLSeries([], 50000).routed.size).toBe(0);
  });
});

describe('glRoutable', () => {
  // GL is usable (live overlay OR preview <image>) whenever the flag + WebGL2
  // hold — interactivity no longer gates routing (previews rasterize via the
  // shared previewRaster context, so they match the window).
  it('is ON when the flag and WebGL2 both hold', () => {
    expect(glRoutable(true, true)).toBe(true);
  });
  it('is OFF when the flag is off or WebGL2 is missing', () => {
    expect(glRoutable(false, true)).toBe(false);
    expect(glRoutable(true, false)).toBe(false);
  });
});

describe('selectGLBigSeries (engine-downsampled >1M line layers)', () => {
  const big = (over = {}) => ({
    kind: 'series', mode: 'line', color: '#00ff00',
    seriesDownsampled: true, figId: 1, axIdx: 0, dsIdx: 2,
    x: [0, 1, 2], y: [0, 1, 0],          // small preview only
    ...over,
  });

  it('selects a downsampled line layer with full engine coordinates', () => {
    const out = selectGLBigSeries([big()]);
    expect(out).toHaveLength(1);
    expect(out[0]).toMatchObject({ idx: 0, figId: 1, axIdx: 0, dsIdx: 2 });
    expect(out[0].color).toEqual([0, 1, 0, 1]);
  });

  it('skips non-downsampled, scatter, comet and incomplete coords', () => {
    const out = selectGLBigSeries([
      big({ seriesDownsampled: false }),  // full-data → selectGLSeries path
      big({ mode: 'scatter' }),           // not a line
      big({ cometAnim: true }),           // animated
      big({ dsIdx: null }),               // missing engine index
      { kind: 'heatmap' },                // not a series
    ]);
    expect(out).toHaveLength(0);
  });

  it('reports the layer index so the rest stay on SVG', () => {
    const out = selectGLBigSeries([{ kind: 'heatmap' }, big(), { kind: 'series', mode: 'scatter' }]);
    expect(out.map((d) => d.idx)).toEqual([1]);
  });

  it('carries the marker cell so a downsampled marked line keeps markers', () => {
    expect(selectGLBigSeries([big()])[0].marker).toBe(-1);             // plain line
    expect(selectGLBigSeries([big({ marker: 'o' })])[0].marker).toBe(0);
    expect(selectGLBigSeries([big({ marker: 's' })])[0].marker).toBe(1);
  });

  it('tolerates null / empty', () => {
    expect(selectGLBigSeries(null)).toEqual([]);
    expect(selectGLBigSeries([])).toEqual([]);
  });
});

describe('selectGLPolarSeries (polar line/scatter in screen space)', () => {
  const layout = { cx: 100, cy: 100, radius: 50, rMin: 0, rMax: 1, zero: 0, dirSign: 1 };
  const ser = (n, mode, over = {}) => ({
    mode, color: '#00ff00',
    theta: Array.from({ length: n }, (_, i) => (i / n) * 2 * Math.PI),
    rho: Array.from({ length: n }, () => 0.5),
    ...over,
  });

  it('routes a big polar scatter as POINTS packed in screen coords', () => {
    const { routed, series } = selectGLPolarSeries([ser(5000, 'scatter')], layout,
      { lineMin: 50000, scatterMin: 2000 });
    expect(routed.has(0)).toBe(true);
    expect(series[0].mode).toBe('scatter');
    expect(series[0].data.length).toBe(5000 * 2);
    expect(series[0].color).toEqual([0, 1, 0, 1]);
  });

  it('routes a big polar line; small line + bubble/rose stay on SVG', () => {
    const layers = [
      ser(60000, 'line'),    // big line → routed (idx 0)
      ser(3000, 'line'),     // small line → one cheap SVG path
      ser(5000, 'bubble'),   // bubble → SVG
      ser(5000, 'rose'),     // rose → SVG
    ];
    const { routed } = selectGLPolarSeries(layers, layout, { lineMin: 50000, scatterMin: 2000 });
    expect([...routed]).toEqual([0]);
  });

  it('tolerates missing input', () => {
    expect(selectGLPolarSeries(null, layout, { lineMin: 1, scatterMin: 1 }).series).toEqual([]);
  });
});

describe('glFlagFromStorage (default ON, opt-out with "0")', () => {
  const store = (v) => ({ getItem: () => v });
  it('defaults ON when the key is unset', () => {
    expect(glFlagFromStorage(store(null))).toBe(true);
  });
  it('stays ON for any value other than "0" (incl. legacy "1")', () => {
    expect(glFlagFromStorage(store('1'))).toBe(true);
    expect(glFlagFromStorage(store('yes'))).toBe(true);
  });
  it('opts OUT only on the explicit string "0"', () => {
    expect(glFlagFromStorage(store('0'))).toBe(false);
  });
  it('defaults ON when storage is unavailable (SSR / null)', () => {
    expect(glFlagFromStorage(null)).toBe(true);
  });
});
