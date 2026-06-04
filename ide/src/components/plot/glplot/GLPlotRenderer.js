// glplot/GLPlotRenderer.js — imperative WebGL2 line + marker renderer.
//
// Owns the GL programs, a marker-shape atlas texture, and per-series vertex
// buffers. Draws line strips and markers (scatter, or markers on a line).
// Data is uploaded once (setSeries); pan/zoom is a projection-uniform change
// + draw() — O(1) per frame regardless of point count. The gl context is
// injected so this orchestration is unit-testable with a mock.
//
// Series shape: { data: Float32Array (interleaved x,y), segments:[{offset,
// count}], color:[r,g,b,a], mode:'line'|'scatter', size: marker RADIUS in
// viewBox units, marker: atlas cell index (scatter) }. One mode per series:
// markers on a line arrive as a separate, decimated scatter series (see
// route.selectGLSeries) — 300k overlapping markers are a slow mush, so the
// router caps them; the strip itself stays full-resolution.
//
// Thin 1-px line strips for now (gl.lineWidth is unreliable > 1 on most
// platforms). Markers sample the shape atlas (markerAtlas.js) and tint it
// with the series colour, so every MATLAB marker shape renders, not just the
// disc.

import { createProgram } from './glcontext';
import { buildMarkerAtlas, ATLAS_R_FRAC } from './markerAtlas';

// Shared data→clip projection (linear or natural-log per axis; see
// projection.js). Reversed axes are folded into ax/bx already.
const PROJECT = `
  float tx = uLog.x > 0.5 ? log(aPos.x) : aPos.x;
  float ty = uLog.y > 0.5 ? log(aPos.y) : aPos.y;
  gl_Position = vec4(uA.x * tx + uA.y, uA.z * ty + uA.w, 0.0, 1.0);`;

// uClip = (cx, cy, radius) in aPos units; radius<=0 disables. vClip carries
// each vertex's position relative to that disc (radius units) so the fragment
// stage can clip to a circle — matches PolarPlot's SVG clipPath. Cartesian
// passes radius 0 → vClip 0 → never clipped.
const CLIP_VS = `
  vClip = uClip.z > 0.0 ? (aPos - uClip.xy) / uClip.z : vec2(0.0);`;

const LINE_VS = `#version 300 es
in vec2 aPos;
uniform vec4 uA;     // ax, bx, ay, by
uniform vec2 uLog;   // xLog, yLog (1.0 = natural-log that axis)
uniform vec3 uClip;
out vec2 vClip;
void main() {${PROJECT}${CLIP_VS}
}`;

const POINT_VS = `#version 300 es
in vec2 aPos;
uniform vec4 uA;
uniform vec2 uLog;
uniform float uSize; // point diameter in framebuffer px
uniform vec3 uClip;
out vec2 vClip;
void main() {${PROJECT}${CLIP_VS}
  gl_PointSize = uSize;
}`;

const SOLID_FS = `#version 300 es
precision highp float;
uniform vec4 uColor;
in vec2 vClip;
out vec4 fragColor;
void main() {
  if (dot(vClip, vClip) > 1.0) discard;       // outside the polar disc
  fragColor = uColor;
}`;

// Marker fragment: sample the shape atlas cell for this series' marker and
// tint it with the series colour. The atlas stores white-on-transparent alpha
// masks, so texture().a is the shape coverage. gl_PointCoord (0,0)=top-left
// maps straight to the cell's top-left (atlas drawn y-down to match).
const MARKER_FS = `#version 300 es
precision highp float;
uniform vec4 uColor;
uniform sampler2D uAtlas;
uniform float uCell;    // marker column index
uniform float uCols;    // atlas column count
uniform float uFilled;  // 0 = open (outline) row, 1 = filled row
in vec2 vClip;
out vec4 fragColor;
void main() {
  if (dot(vClip, vClip) > 1.0) discard;       // outside the polar disc
  // Atlas is 2 rows: row 0 outline (MATLAB default), row 1 filled.
  vec2 tc = vec2((uCell + gl_PointCoord.x) / uCols, (uFilled + gl_PointCoord.y) / 2.0);
  float a = texture(uAtlas, tc).a;
  if (a < 0.02) discard;
  fragColor = vec4(uColor.rgb, uColor.a * a);
}`;

export class GLPlotRenderer {
  constructor(gl) {
    this.gl = gl;
    this.lineProg = createProgram(gl, LINE_VS, SOLID_FS);
    this.pointProg = createProgram(gl, POINT_VS, MARKER_FS);
    this.lineLoc = {
      aPos:   gl.getAttribLocation(this.lineProg, 'aPos'),
      uA:     gl.getUniformLocation(this.lineProg, 'uA'),
      uLog:   gl.getUniformLocation(this.lineProg, 'uLog'),
      uClip:  gl.getUniformLocation(this.lineProg, 'uClip'),
      uColor: gl.getUniformLocation(this.lineProg, 'uColor'),
    };
    this.pointLoc = {
      aPos:   gl.getAttribLocation(this.pointProg, 'aPos'),
      uA:     gl.getUniformLocation(this.pointProg, 'uA'),
      uLog:   gl.getUniformLocation(this.pointProg, 'uLog'),
      uClip:  gl.getUniformLocation(this.pointProg, 'uClip'),
      uColor: gl.getUniformLocation(this.pointProg, 'uColor'),
      uSize:  gl.getUniformLocation(this.pointProg, 'uSize'),
      uAtlas: gl.getUniformLocation(this.pointProg, 'uAtlas'),
      uCell:  gl.getUniformLocation(this.pointProg, 'uCell'),
      uCols:  gl.getUniformLocation(this.pointProg, 'uCols'),
      uFilled: gl.getUniformLocation(this.pointProg, 'uFilled'),
    };
    this.atlas = this._createAtlas();   // { tex, cols } | null
    this.series = [];
    this.proj = { ax: 1, bx: 0, ay: 1, by: 0, xLog: false, yLog: false };
    this.pixelRatio = 1;   // framebuffer-px per viewBox unit (for marker size)
    this.clip = [0, 0, 0]; // disc clip (cx, cy, radius) in aPos units; 0 = off
  }

  setProjection(proj) { this.proj = proj; }
  setPixelRatio(r) { this.pixelRatio = r > 0 ? r : 1; }
  // Clip drawing to a disc (cx, cy, radius) in aPos units; radius<=0 = no clip.
  setClip(clip) { this.clip = (clip && clip.length === 3) ? clip : [0, 0, 0]; }

  _createAtlas() {
    const atlas = buildMarkerAtlas();
    if (!atlas) return null;          // no 2-D context (tests) → markers skip
    const gl = this.gl;
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, atlas.canvas);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return { tex, cols: atlas.cols };
  }

  // Upload series data into per-series static VBOs (once per data change).
  setSeries(series) {
    this._freeBuffers();
    const gl = this.gl;
    this.series = (series || []).map((s) => {
      const vbo = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
      gl.bufferData(gl.ARRAY_BUFFER, s.data, gl.STATIC_DRAW);
      const mode = s.mode === 'scatter' ? 'scatter' : 'line';
      const marker = Number.isInteger(s.marker) ? s.marker : (mode === 'scatter' ? 0 : -1);
      return { vbo, segments: s.segments || [], color: s.color || [0, 0, 0, 1], mode, marker, filled: !!s.filled, size: s.size || 3 };
    });
  }

  draw() {
    const gl = this.gl;
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    for (const s of this.series) {
      if (s.mode === 'scatter') this._drawMarkers(s);
      else this._drawLines(s);
    }
  }

  _projUniforms(prog, loc) {
    const gl = this.gl, p = this.proj;
    gl.useProgram(prog);
    gl.uniform4f(loc.uA, p.ax, p.bx, p.ay, p.by);
    gl.uniform2f(loc.uLog, p.xLog ? 1 : 0, p.yLog ? 1 : 0);
    gl.uniform3f(loc.uClip, this.clip[0], this.clip[1], this.clip[2]);
  }

  _bindPos(loc, vbo) {
    const gl = this.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.enableVertexAttribArray(loc.aPos);
    gl.vertexAttribPointer(loc.aPos, 2, gl.FLOAT, false, 0, 0);
  }

  _drawLines(s) {
    const gl = this.gl, l = this.lineLoc;
    this._projUniforms(this.lineProg, l);
    this._bindPos(l, s.vbo);
    gl.uniform4fv(l.uColor, s.color);
    for (const seg of s.segments) {
      if (seg.count >= 2) gl.drawArrays(gl.LINE_STRIP, seg.offset, seg.count);
    }
  }

  _drawMarkers(s) {
    const gl = this.gl, l = this.pointLoc;
    this._projUniforms(this.pointProg, l);
    this._bindPos(l, s.vbo);
    gl.uniform4fv(l.uColor, s.color);
    // pointSize so the atlas shape (radius ATLAS_R_FRAC·sprite) renders at the
    // SVG marker's on-screen radius (`size`) → identical either side of SVG↔GL.
    gl.uniform1f(l.uSize, (s.size * this.pixelRatio) / ATLAS_R_FRAC);
    if (this.atlas) {
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, this.atlas.tex);
      gl.uniform1i(l.uAtlas, 0);
      gl.uniform1f(l.uCols, this.atlas.cols);
    }
    gl.uniform1f(l.uCell, s.marker >= 0 ? s.marker : 0);
    gl.uniform1f(l.uFilled, s.filled ? 1 : 0);
    for (const seg of s.segments) {
      if (seg.count >= 1) gl.drawArrays(gl.POINTS, seg.offset, seg.count);
    }
  }

  _freeBuffers() {
    const gl = this.gl;
    for (const s of this.series) gl.deleteBuffer(s.vbo);
    this.series = [];
  }

  dispose() {
    this._freeBuffers();
    if (this.atlas) this.gl.deleteTexture(this.atlas.tex);
    this.gl.deleteProgram(this.lineProg);
    this.gl.deleteProgram(this.pointProg);
  }
}
