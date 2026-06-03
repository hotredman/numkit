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

// Resize the canvas backing store to the CSS box × DPR (crisp output) and
// update the GL viewport. Returns true when the size actually changed.
export function resizeToDisplay(gl, canvas, cssW, cssH, dpr = 1) {
  const { w, h } = displaySize(cssW, cssH, dpr);
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
    gl.viewport(0, 0, w, h);
    return true;
  }
  return false;
}
