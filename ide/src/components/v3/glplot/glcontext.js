// glplot/glcontext.js — thin WebGL2 helpers (no dependency).
//
// The only module that touches raw GL setup. GLPlotRenderer drives all of
// its work through the injected context, so its orchestration stays
// unit-testable with a mock gl (jsdom has no real WebGL).

// Acquire a WebGL2 context, or null if unavailable (caller falls back to SVG).
export function createGL(canvas, opts = {}) {
  const gl = canvas.getContext('webgl2', {
    antialias: opts.antialias !== false,
    alpha: true,
    premultipliedAlpha: true,
    preserveDrawingBuffer: !!opts.preserveDrawingBuffer,  // needed for PNG export
    desynchronized: true,
  });
  return gl || null;
}

export function compileShader(gl, type, src) {
  const sh = gl.createShader(type);
  gl.shaderSource(sh, src);
  gl.compileShader(sh);
  if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
    const log = gl.getShaderInfoLog(sh);
    gl.deleteShader(sh);
    throw new Error(`glplot shader compile failed: ${log}`);
  }
  return sh;
}

export function createProgram(gl, vsSrc, fsSrc) {
  const vs = compileShader(gl, gl.VERTEX_SHADER, vsSrc);
  const fs = compileShader(gl, gl.FRAGMENT_SHADER, fsSrc);
  const prog = gl.createProgram();
  gl.attachShader(prog, vs);
  gl.attachShader(prog, fs);
  gl.linkProgram(prog);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
    const log = gl.getProgramInfoLog(prog);
    gl.deleteProgram(prog);
    throw new Error(`glplot program link failed: ${log}`);
  }
  return prog;
}

// Pure: drawing-buffer pixel size for a CSS box at a given devicePixelRatio.
export function displaySize(cssW, cssH, dpr = 1) {
  return {
    w: Math.max(1, Math.round(cssW * dpr)),
    h: Math.max(1, Math.round(cssH * dpr)),
  };
}

// Resize the canvas backing store to the CSS box × DPR (crisp output).
// Returns true when the size actually changed. (Viewport is set per-draw via
// glViewportRect, scissored to the plot area, so we don't touch it here.)
export function resizeToDisplay(canvas, cssW, cssH, dpr = 1) {
  const { w, h } = displaySize(cssW, cssH, dpr);
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
    return true;
  }
  return false;
}

// Pure: map a plot rect (in viewBox units) to a GL viewport/scissor rect in
// drawing-buffer pixels, y-flipped (GL's origin is bottom-left). The canvas
// shares the SVG's viewBox, so the same padL/padT/W/H place the GL drawing
// exactly under the SVG axes.
export function glViewportRect(plotRect, viewBox, canvasPx) {
  const sx = canvasPx.w / viewBox.w;
  const sy = canvasPx.h / viewBox.h;
  const w = Math.round(plotRect.w * sx);
  const h = Math.round(plotRect.h * sy);
  const x = Math.round(plotRect.x * sx);
  const topPx = Math.round(plotRect.y * sy);
  return { x, y: canvasPx.h - (topPx + h), w, h };
}

// Cached: is WebGL2 usable in this environment? false in jsdom / headless →
// callers fall back to the SVG path.
let _gl2;
export function isWebGL2Available() {
  if (_gl2 !== undefined) return _gl2;
  try {
    _gl2 = !!document.createElement('canvas').getContext('webgl2');
  } catch {
    _gl2 = false;
  }
  return _gl2;
}
