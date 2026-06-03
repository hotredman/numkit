import { describe, it, expect } from 'vitest';
import {
  selectGLSeries, glOverlayEnabled, glFlagFromStorage, selectGLBigSeries,
} from './route';

const line = (n, color = '#ff0000') => ({
  kind: 'series', mode: 'line', color,
  x: Array.from({ length: n }, (_, i) => i),
  y: Array.from({ length: n }, (_, i) => i),
});

describe('selectGLSeries', () => {
  it('routes a large full-data line layer and packs it', () => {
    const { routed, series } = selectGLSeries([line(60000)], 50000);
    expect(routed.has(0)).toBe(true);
    expect(series).toHaveLength(1);
    expect(series[0].data).toBeInstanceOf(Float32Array);
    expect(series[0].data.length).toBe(60000 * 2);
    expect(series[0].color).toEqual([1, 0, 0, 1]);
  });

  it('skips small lines, scatter, downsampled and comet', () => {
    const layers = [
      line(100),                                 // below threshold
      { ...line(60000), mode: 'scatter' },       // not a line
      { ...line(60000), seriesDownsampled: true }, // engine preview
      { ...line(60000), cometAnim: true },        // animated
      { kind: 'heatmap' },                        // not a series
    ];
    const { routed, series } = selectGLSeries(layers, 50000);
    expect(routed.size).toBe(0);
    expect(series).toHaveLength(0);
  });

  it('routes by index, leaving the rest on SVG', () => {
    const { routed } = selectGLSeries([line(100), line(60000), line(200)], 50000);
    expect([...routed]).toEqual([1]);
  });

  it('tolerates missing / empty input', () => {
    expect(selectGLSeries(null, 50000).series).toEqual([]);
    expect(selectGLSeries([], 50000).routed.size).toBe(0);
  });
});

describe('glOverlayEnabled', () => {
  // Regression: non-interactive figure preview cards were routed to GL,
  // which (a) burns a WebGL context per thumbnail (browser cap ~16) and
  // (b) renders a dense line as a solid fill at preview size. Previews must
  // stay on SVG; GL is interactive-window only.
  it('is OFF for a non-interactive preview even with flag + webgl2', () => {
    expect(glOverlayEnabled({ interactive: false, flag: true, webgl2: true })).toBe(false);
  });
  it('is ON only when interactive AND flag AND webgl2 all hold', () => {
    expect(glOverlayEnabled({ interactive: true, flag: true, webgl2: true })).toBe(true);
  });
  it('is OFF when the feature flag is off', () => {
    expect(glOverlayEnabled({ interactive: true, flag: false, webgl2: true })).toBe(false);
  });
  it('is OFF when WebGL2 is unavailable', () => {
    expect(glOverlayEnabled({ interactive: true, flag: true, webgl2: false })).toBe(false);
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

  it('tolerates null / empty', () => {
    expect(selectGLBigSeries(null)).toEqual([]);
    expect(selectGLBigSeries([])).toEqual([]);
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
