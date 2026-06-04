// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { renderGLPreviewDataURL, _resetPreviewRaster } from './previewRaster';

afterEach(_resetPreviewRaster);

describe('renderGLPreviewDataURL', () => {
  const args = {
    series: [{
      data: new Float32Array([0, 0, 1, 1]),
      segments: [{ offset: 0, count: 2 }],
      color: [1, 0, 0, 1],
    }],
    proj: { ax: 1, bx: 0, ay: 1, by: 0, xLog: false, yLog: false },
    pxW: 200, pxH: 120, dpr: 1,
  };

  // jsdom has no WebGL2 → the shared context can't be created, so every call
  // returns null and never throws. The caller then keeps the decimated SVG
  // fallback. Actual rasterization is verified live (jsdom can't run WebGL).
  it('returns null (no throw) when WebGL2 is unavailable', () => {
    expect(renderGLPreviewDataURL(args)).toBeNull();
  });
  it('returns null for empty / missing series and degenerate sizes', () => {
    expect(renderGLPreviewDataURL({ ...args, series: [] })).toBeNull();
    expect(renderGLPreviewDataURL({ ...args, series: null })).toBeNull();
    expect(renderGLPreviewDataURL({ ...args, pxW: 0 })).toBeNull();
  });
});
