// glplot/previewRaster.js — render GL series to a PNG dataURL through ONE
// shared, offscreen WebGL context, for static preview thumbnails.
//
// Why not a live GLChart per preview card: browsers cap WebGL contexts (~16),
// so one-context-per-card exhausts them (and a canvas overlay isn't part of
// the SVG, so it vanishes on SVG export). Instead a single reused context
// rasterizes each thumbnail and the caller embeds the result as an SVG
// <image> — pixel-identical to the interactive window (same GLPlotRenderer),
// one context for the whole app, and the data lives inside the SVG.
//
// Synchronous: draw + toDataURL run in the same tick (no compositing in
// between), and the context is created with preserveDrawingBuffer so the
// readback is reliable. Inert (returns null) when WebGL2 is unavailable —
// the caller then keeps the decimated SVG fallback.

import { createGL, resizeToDisplay, isWebGL2Available } from './glcontext';
import { GLPlotRenderer } from './GLPlotRenderer';

let _canvas = null;
let _renderer = null;
let _failed = false;

function ensure() {
  if (_renderer) return _renderer;
  if (_failed || typeof document === 'undefined' || !isWebGL2Available()) return null;
  _canvas = document.createElement('canvas');
  const gl = createGL(_canvas, { preserveDrawingBuffer: true });
  if (!gl) { _failed = true; return null; }
  _renderer = new GLPlotRenderer(gl);
  return _renderer;
}

// Render `series` (same shape as GLChart/GLPlotRenderer) at the projection
// `proj`, into a pxW×pxH (CSS) canvas at devicePixelRatio `dpr`, optionally
// clipped to a disc (polar). The caller places the returned image at the plot
// rect. → PNG dataURL string, or null when WebGL is unavailable / nothing to
// draw.
export function renderGLPreviewDataURL({ series, proj, pxW, pxH, dpr = 1, clip = null }) {
  const r = ensure();
  if (!r || !series || series.length === 0 || !(pxW > 0) || !(pxH > 0)) return null;
  try {
    resizeToDisplay(_canvas, pxW, pxH, dpr);
    const gl = r.gl;
    gl.viewport(0, 0, _canvas.width, _canvas.height);
    gl.disable(gl.SCISSOR_TEST);
    r.setSeries(series);
    r.setProjection(proj);
    r.setPixelRatio(dpr);
    r.setClip(clip);
    r.draw();
    return _canvas.toDataURL('image/png');
  } catch {
    return null;
  }
}

// Test seam: drop the cached context so a fresh ensure() runs next time.
export function _resetPreviewRaster() {
  _canvas = null;
  _renderer = null;
  _failed = false;
}
