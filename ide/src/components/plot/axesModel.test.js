import { describe, it, expect } from 'vitest';
import {
  legacyRead, legacyWrite, axesToLegacyCell, viewportFromAxes, applyViewport,
} from './axesModel';

// The axes-model bridge was previously inline + untested inside FigureWindow;
// extracting it to a pure module lets it be unit-tested directly.
describe('axesModel — MATLAB Axes <-> legacy-cell bridge', () => {
  it('legacyWrite then legacyRead round-trips a boolean grid flag', () => {
    let a = legacyWrite({}, 'xGrid', true, { kind: 'composite' });
    expect(legacyRead(a, 'xGrid')).toBe(true);
    a = legacyWrite(a, 'xGrid', false, { kind: 'composite' });
    expect(legacyRead(a, 'xGrid')).toBe(false);
  });

  it('legacyWrite xLog maps to XScale and reads back', () => {
    const a = legacyWrite({}, 'xLog', true, { kind: 'composite' });
    expect(a.XScale).toBe('log');
    expect(legacyRead(a, 'xLog')).toBe(true);
  });

  it('the combined "showMajor" fans grid only to the axes that exist (cartesian)', () => {
    const a = legacyWrite({}, 'showMajor', true, { kind: 'composite' });
    expect(legacyRead(a, 'xGrid')).toBe(true);
    expect(legacyRead(a, 'yGrid')).toBe(true);
    expect(a.RGrid).toBeUndefined();            // polar axes untouched
  });

  it('"showMajor" on a polar figure lights R / Theta, not X / Y', () => {
    const a = legacyWrite({}, 'showMajor', true, { kind: 'polar' });
    expect(legacyRead(a, 'rGrid')).toBe(true);
    expect(legacyRead(a, 'thetaGrid')).toBe(true);
    expect(a.XGrid).toBeUndefined();            // cartesian axes untouched
  });

  it('viewport round-trips through applyViewport / viewportFromAxes', () => {
    const a = applyViewport({}, { x: [0, 10], y: [-1, 1] });
    expect(a.XLim).toEqual([0, 10]);
    expect(a.YLim).toEqual([-1, 1]);
    expect(viewportFromAxes(a)).toEqual({ x: [0, 10], y: [-1, 1] });
  });

  it('polar viewport uses the r/theta array-pair shape', () => {
    const a = applyViewport({}, { r: [0, 5], theta: [0, 360] });
    expect(a.RLim).toEqual([0, 5]);
    expect(viewportFromAxes(a)).toEqual({ r: [0, 5], theta: [0, 360] });
  });

  it('axesToLegacyCell projects an Axes object to the flat cell shape', () => {
    const cell = axesToLegacyCell({ XScale: 'log', XGrid: 'on', Box: 'on' });
    expect(cell.xLog).toBe(true);
    expect(cell.xGrid).toBe(true);
    expect(cell.showBox).toBe(true);
  });
});
