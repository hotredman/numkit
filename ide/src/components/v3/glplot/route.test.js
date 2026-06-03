import { describe, it, expect } from 'vitest';
import { selectGLSeries } from './route';

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
