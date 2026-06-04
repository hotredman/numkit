// @vitest-environment jsdom
import { describe, it, expect } from 'vitest';
import { markerCell, MARKERS, buildMarkerAtlas } from './markerAtlas';

describe('markerCell', () => {
  it('maps each marker shape to its atlas column', () => {
    MARKERS.forEach((m, i) => expect(markerCell(m)).toBe(i));
  });
  it('falls back to the disc (column 0) for dot / null / unknown', () => {
    expect(markerCell('.')).toBe(0);
    expect(markerCell(null)).toBe(0);
    expect(markerCell(undefined)).toBe(0);
    expect(markerCell('nope')).toBe(0);
  });
});

describe('buildMarkerAtlas', () => {
  it('returns null (no throw) when no 2-D context is available', () => {
    // jsdom has no real canvas 2-D context → graceful null; the renderer then
    // skips the atlas texture. Pixels are verified live.
    expect(() => buildMarkerAtlas()).not.toThrow();
    expect(buildMarkerAtlas()).toBeNull();
  });
});
