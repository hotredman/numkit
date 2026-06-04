import { describe, it, expect } from 'vitest';
import {
  seriesBody, compositeCellBody, buildDelimited, buildJsonObject,
} from './figureExport';

// These pure builders carry FigureWindow's whole per-kind export branch
// logic (3-D / heatmap / subplot / composite / plain series). Extracting
// them lets the branch matrix be asserted directly, delimiter + shape.

describe('seriesBody', () => {
  it('emits a header + one row per sample for a plain series figure', () => {
    const fig = { series: [{ name: 'A', x: [1, 2], y: [10, 20] }] };
    expect(seriesBody(fig, ',')).toBe('name,x,y\nA,1,10\nA,2,20');
  });

  it('adds a z column only when some series is 3-D', () => {
    const list = [{ name: 'C', x: [1], y: [2], z: [3] }];
    expect(seriesBody(list, ',')).toBe('name,x,y,z\nC,1,2,3');
  });

  it('falls back to theta / rho for polar series', () => {
    const list = [{ name: 'P', theta: [0, 1], rho: [5, 6] }];
    expect(seriesBody(list, ',')).toBe('name,x,y\nP,0,5\nP,1,6');
  });

  it('honours the delimiter (tab)', () => {
    const list = [{ name: 'A', x: [1], y: [2] }];
    expect(seriesBody(list, '\t')).toBe('name\tx\ty\nA\t1\t2');
  });

  it('skips a missing y without shifting columns', () => {
    const list = [{ name: 'A', x: [1, 2], y: [10] }];   // y[1] absent
    expect(seriesBody(list, ',')).toBe('name,x,y\nA,1,10\nA,2');
  });
});

describe('compositeCellBody', () => {
  it('dumps the heatmap z grid when a heatmap layer is present', () => {
    const cell = { layers: [{ kind: 'heatmap', z: [[1, 2], [3, 4]] }] };
    expect(compositeCellBody(cell, ',')).toBe('1,2\n3,4');
  });

  it('blanks nulls inside the heatmap grid', () => {
    const cell = { layers: [{ kind: 'heatmap', z: [[1, null], [null, 4]] }] };
    expect(compositeCellBody(cell, ',')).toBe('1,\n,4');
  });

  it('falls through to series body when there is no heatmap layer', () => {
    const cell = { layers: [{ kind: 'series', name: 'S', x: [1], y: [2] }] };
    expect(compositeCellBody(cell, ',')).toBe('name,x,y\nS,1,2');
  });
});

describe('buildDelimited — per-kind branch matrix', () => {
  const base = {
    is3D: false, isHeatmap: false, isSubplot: false, isComposite: false,
    heatmapLayer: null, seriesLayers: [], compositeLayers: [], threeRows: [],
  };

  it('plain figure → seriesBody(figure)', () => {
    const ctx = { ...base, figure: { series: [{ name: 'A', x: [1], y: [2] }] } };
    expect(buildDelimited(ctx, ',')).toBe('name,x,y\nA,1,2');
  });

  it('3-D figure → seriesBody(threeRows)', () => {
    const ctx = { ...base, is3D: true, figure: {}, threeRows: [{ name: 'T', x: [1], y: [2], z: [3] }] };
    expect(buildDelimited(ctx, ',')).toBe('name,x,y,z\nT,1,2,3');
  });

  it('heatmap figure → raw z grid', () => {
    const ctx = { ...base, isHeatmap: true, figure: {}, heatmapLayer: { z: [[1, 2], [3, 4]] } };
    expect(buildDelimited(ctx, '\t')).toBe('1\t2\n3\t4');
  });

  it('composite figure → seriesBody(seriesLayers)', () => {
    const ctx = { ...base, isComposite: true, figure: {}, seriesLayers: [{ name: 'L', x: [1], y: [2] }] };
    expect(buildDelimited(ctx, ',')).toBe('name,x,y\nL,1,2');
  });

  it('subplot figure → tagged blocks joined by a blank line', () => {
    const ctx = {
      ...base, isSubplot: true,
      figure: { cells: [
        { kind: 'line', subplotIndex: 1, title: 'one', series: [{ name: 'A', x: [1], y: [2] }] },
        { kind: 'composite', subplotIndex: 2, title: 'two', layers: [{ kind: 'series', name: 'B', x: [3], y: [4] }] },
      ] },
    };
    const out = buildDelimited(ctx, ',');
    expect(out).toContain('# subplot 1 — one\nname,x,y\nA,1,2');
    expect(out).toContain('# subplot 2 — two\nname,x,y\nB,3,4');
    expect(out).toContain('\n\n');                 // blank-line block separator
  });
});

describe('buildJsonObject — per-kind shape', () => {
  const base = {
    is3D: false, isHeatmap: false, isSubplot: false, isComposite: false,
    heatmapLayer: null, compositeLayers: [], threeRows: [],
  };

  it('plain figure projects id/kind/title + flattened series', () => {
    const ctx = { ...base, figure: { id: 7, kind: 'line', title: 'T', series: [{ name: 'A', color: '#111', theta: [1], rho: [2] }] } };
    const o = buildJsonObject(ctx);
    expect(o).toMatchObject({ id: 7, kind: 'line', title: 'T' });
    expect(o.series[0]).toEqual({ name: 'A', color: '#111', x: [1], y: [2] });  // theta/rho aliased
  });

  it('3-D figure carries view + zLabel + threeRows', () => {
    const ctx = { ...base, is3D: true, figure: { id: 1, title: 'g', zLabel: 'Z', view: [30, 60] }, threeRows: [{ name: 'T' }] };
    const o = buildJsonObject(ctx);
    expect(o.kind).toBe('composite3d');
    expect(o.zLabel).toBe('Z');
    expect(o.view).toEqual([30, 60]);
    expect(o.series).toEqual([{ name: 'T' }]);
  });

  it('heatmap figure carries colormap + z + cmin/cmax', () => {
    const ctx = { ...base, isHeatmap: true, figure: { id: 2 }, heatmapLayer: { z: [[1]], cmin: 0, cmax: 1, colormap: 'jet' } };
    const o = buildJsonObject(ctx);
    expect(o.kind).toBe('heatmap');
    expect(o.colormap).toBe('jet');
    expect(o.z).toEqual([[1]]);
  });

  it('composite figure projects only series/heatmap layer fields', () => {
    const ctx = {
      ...base, isComposite: true, figure: { id: 3, title: 'c' },
      compositeLayers: [{ kind: 'series', mode: 'line', name: 'S', color: '#abc', x: [1], y: [2], _internal: 99 }],
    };
    const o = buildJsonObject(ctx);
    expect(o.kind).toBe('composite');
    expect(o.layers[0]).toEqual({ kind: 'series', mode: 'line', name: 'S', color: '#abc', x: [1], y: [2] });
    expect(o.layers[0]._internal).toBeUndefined();   // internal field dropped
  });

  it('subplot figure projects per-cell composite vs plain cells', () => {
    const ctx = {
      ...base, isSubplot: true,
      figure: { id: 4, title: 's', grid: [1, 2], cells: [
        { kind: 'line', subplotIndex: 1, title: 'one', series: [{ name: 'A', color: '#111', x: [1], y: [2] }] },
        { kind: 'composite', subplotIndex: 2, title: 'two', layers: [{ kind: 'heatmap', z: [[1]], cmin: 0, cmax: 1 }] },
      ] },
    };
    const o = buildJsonObject(ctx);
    expect(o.kind).toBe('subplot');
    expect(o.cells[0]).toMatchObject({ subplotIndex: 1, kind: 'line', title: 'one' });
    expect(o.cells[1]).toMatchObject({ subplotIndex: 2, kind: 'composite', title: 'two' });
    expect(o.cells[1].layers[0]).toEqual({ kind: 'heatmap', z: [[1]], cmin: 0, cmax: 1 });
  });
});
