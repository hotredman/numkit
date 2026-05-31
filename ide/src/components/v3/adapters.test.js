import { describe, it, expect } from 'vitest';
import { adaptFigure } from './adapters';

// Build a raw engine figure (the JSON shape figure_manager.hpp emits:
// axes[].config carries xscale/yscale) with one line dataset.
function rawLineFig(config, x = [1, 10, 100, 1000], y = [1, 10, 100, 1000]) {
  return { id: 1, axes: [{ datasets: [{ x, y, type: 'line' }], config }] };
}

describe('adaptFigure — log-axis auto-range padding', () => {
  it('carries xscale/yscale through from config', () => {
    const f = adaptFigure(rawLineFig({ xscale: 'log', yscale: 'log' }));
    expect(f.kind).toBe('composite');
    expect(f.xscale).toBe('log');
    expect(f.yscale).toBe('log');
  });

  it('pads a log axis in log space — lower bound stays > 0', () => {
    // The bug: a 4% LINEAR margin on [1,1000] → [-39, 1040], so xMin ≤ 0
    // disabled the log mapping (xLogActive) and the axis rendered LINEAR.
    const f = adaptFigure(rawLineFig({ xscale: 'log', yscale: 'log' }));
    expect(f.xRange[0]).toBeGreaterThan(0);
    expect(f.yRange[0]).toBeGreaterThan(0);
    // Padded outward in log space: below the data min, above the data max.
    expect(f.xRange[0]).toBeLessThan(1);
    expect(f.xRange[1]).toBeGreaterThan(1000);
  });

  it('keeps LINEAR padding for linear axes (regression guard)', () => {
    const f = adaptFigure(rawLineFig({}));   // no scale → linear
    expect(f.xscale).toBe('linear');
    // Flat 4% margin on [1,1000] pushes the lower bound below 0 — that's
    // fine for a linear axis and must stay as-is.
    expect(f.xRange[0]).toBeLessThan(0);
  });

  it('semilogx pads x in log space but y linearly', () => {
    const f = adaptFigure(rawLineFig({ xscale: 'log' }));   // x log, y linear
    expect(f.xRange[0]).toBeGreaterThan(0);   // log-padded
    expect(f.yRange[0]).toBeLessThan(0);      // linear-padded (data from 1)
  });

  it('axis tight + log → no padding, exact decade bounds', () => {
    const f = adaptFigure(rawLineFig({ xscale: 'log', yscale: 'log', axisMode: 'tight' }));
    expect(f.xRange[0]).toBe(1);
    expect(f.xRange[1]).toBe(1000);
  });

  it('explicit xlim wins over log padding', () => {
    const f = adaptFigure(rawLineFig({ xscale: 'log', xlim: [2, 500] }));
    expect(f.xRange).toEqual([2, 500]);
  });
});
