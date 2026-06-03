// glplot/GLPlotRenderer.js — imperative WebGL2 line renderer.
//
// Owns the GL program + per-series vertex buffers and draws line strips. Data
// is uploaded once (setSeries); pan/zoom is a projection-uniform change +
// draw() — O(1) per frame regardless of point count. The gl context is
// injected so this orchestration is unit-testable with a mock (the real
// context comes from glcontext.createGL).
//
// Series shape: { data: Float32Array (interleaved x,y), segments: [{offset,
// count}] (gap-free runs from pack.js), color: [r,g,b,a] in 0..1 }.
//
// Thin 1-px line strips for now (gl.lineWidth is unreliable > 1 on most
// platforms); thick / anti-aliased lines are a later geometry-expansion pass.

import { createProgram } from './glcontext';

const LINE_VS = `#version 300 es
in vec2 aPos;
uniform vec4 uA;     // ax, bx, ay, by  (data → clip; see projection.js)
uniform vec2 uLog;   // xLog, yLog (1.0 = natural-log that axis)
void main() {
  float tx = uLog.x > 0.5 ? log(aPos.x) : aPos.x;
  float ty = uLog.y > 0.5 ? log(aPos.y) : aPos.y;
  gl_Position = vec4(uA.x * tx + uA.y, uA.z * ty + uA.w, 0.0, 1.0);
}`;

const LINE_FS = `#version 300 es
precision highp float;
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }`;

export class GLPlotRenderer {
  constructor(gl) {
    this.gl = gl;
    this.program = createProgram(gl, LINE_VS, LINE_FS);
    this.loc = {
      aPos:   gl.getAttribLocation(this.program, 'aPos'),
      uA:     gl.getUniformLocation(this.program, 'uA'),
      uLog:   gl.getUniformLocation(this.program, 'uLog'),
      uColor: gl.getUniformLocation(this.program, 'uColor'),
    };
    this.series = [];     // [{ vbo, segments, color }]
    this.proj = { ax: 1, bx: 0, ay: 1, by: 0, xLog: false, yLog: false };
  }

  setProjection(proj) { this.proj = proj; }

  // Upload series data into per-series static VBOs (once per data change).
  setSeries(series) {
    this._freeBuffers();
    const gl = this.gl;
    this.series = (series || []).map((s) => {
      const vbo = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
      gl.bufferData(gl.ARRAY_BUFFER, s.data, gl.STATIC_DRAW);
      return { vbo, segments: s.segments || [], color: s.color || [0, 0, 0, 1] };
    });
  }

  draw() {
    const gl = this.gl, p = this.proj, l = this.loc;
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    gl.useProgram(this.program);
    gl.uniform4f(l.uA, p.ax, p.bx, p.ay, p.by);
    gl.uniform2f(l.uLog, p.xLog ? 1 : 0, p.yLog ? 1 : 0);
    for (const s of this.series) {
      gl.bindBuffer(gl.ARRAY_BUFFER, s.vbo);
      gl.enableVertexAttribArray(l.aPos);
      gl.vertexAttribPointer(l.aPos, 2, gl.FLOAT, false, 0, 0);
      gl.uniform4fv(l.uColor, s.color);
      for (const seg of s.segments) {
        if (seg.count >= 2) gl.drawArrays(gl.LINE_STRIP, seg.offset, seg.count);
      }
    }
  }

  _freeBuffers() {
    const gl = this.gl;
    for (const s of this.series) gl.deleteBuffer(s.vbo);
    this.series = [];
  }

  dispose() {
    this._freeBuffers();
    this.gl.deleteProgram(this.program);
  }
}
