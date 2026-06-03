import { describe, it, expect } from 'vitest';
import { GLPlotRenderer } from './GLPlotRenderer';
import { displaySize } from './glcontext';
import { ATLAS_R_FRAC } from './markerAtlas';

// Minimal mock WebGL2 context: returns success for all setup queries and
// records the draw/uniform calls we assert on. Lets us unit-test the
// renderer's orchestration without a real GPU (jsdom has no WebGL).
function mockGL() {
  const calls = {
    drawArrays: [], uniform4f: [], uniform2f: [], uniform1f: [], uniform3f: [],
    buffers: 0, deleted: 0,
  };
  const gl = {
    VERTEX_SHADER: 1, FRAGMENT_SHADER: 2, COMPILE_STATUS: 3, LINK_STATUS: 4,
    ARRAY_BUFFER: 5, STATIC_DRAW: 6, FLOAT: 7, LINE_STRIP: 8,
    COLOR_BUFFER_BIT: 9, BLEND: 10, SRC_ALPHA: 11, ONE_MINUS_SRC_ALPHA: 12,
    POINTS: 13,
    createShader: () => ({}), shaderSource() {}, compileShader() {},
    getShaderParameter: () => true, getShaderInfoLog: () => '', deleteShader() {},
    createProgram: () => ({}), attachShader() {}, linkProgram() {},
    getProgramParameter: () => true, getProgramInfoLog: () => '', deleteProgram() {},
    getAttribLocation: () => 0, getUniformLocation: () => ({}),
    createBuffer: () => { calls.buffers++; return {}; },
    bindBuffer() {}, bufferData() {}, deleteBuffer() { calls.deleted++; },
    useProgram() {}, clearColor() {}, clear() {}, enable() {}, blendFunc() {},
    enableVertexAttribArray() {}, vertexAttribPointer() {},
    uniform4f: (...a) => calls.uniform4f.push(a.slice(1)),
    uniform2f: (...a) => calls.uniform2f.push(a.slice(1)),
    uniform1f: (...a) => calls.uniform1f.push(a.slice(1)),
    uniform3f: (...a) => calls.uniform3f.push(a.slice(1)),
    uniform4fv() {},
    drawArrays: (...a) => calls.drawArrays.push(a),
  };
  return { gl, calls };
}

describe('GLPlotRenderer', () => {
  it('draws each gap segment as a line strip with the right offset/count', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setProjection({ ax: 2, bx: -1, ay: 3, by: -1, xLog: false, yLog: false });
    r.setSeries([{
      data: new Float32Array([0, 0, 1, 1, 2, 0, 5, 5]),
      segments: [{ offset: 0, count: 2 }, { offset: 2, count: 2 }],
      color: [1, 0, 0, 1],
    }]);
    r.draw();
    expect(calls.drawArrays).toEqual([
      [gl.LINE_STRIP, 0, 2],
      [gl.LINE_STRIP, 2, 2],
    ]);
  });

  it('uploads the projection into the uA / uLog uniforms', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setProjection({ ax: 2, bx: -1, ay: 3, by: -0.5, xLog: true, yLog: false });
    r.setSeries([{
      data: new Float32Array([0, 0, 1, 1]),
      segments: [{ offset: 0, count: 2 }],
      color: [0, 0, 0, 1],
    }]);
    r.draw();
    expect(calls.uniform4f[0]).toEqual([2, -1, 3, -0.5]);
    expect(calls.uniform2f[0]).toEqual([1, 0]);   // xLog on, yLog off
  });

  it('skips degenerate line segments (< 2 vertices)', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setSeries([{
      data: new Float32Array([0, 0, 1, 1, 2, 2]),
      segments: [{ offset: 0, count: 1 }, { offset: 1, count: 2 }],
      color: [0, 0, 0, 1],
    }]);
    r.draw();
    expect(calls.drawArrays).toEqual([[gl.LINE_STRIP, 1, 2]]);
  });

  it('draws scatter as POINTS with pointSize = size·dpr / atlas-fraction (matches SVG)', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setProjection({ ax: 1, bx: 0, ay: 1, by: 0, xLog: false, yLog: false });
    r.setPixelRatio(2);
    r.setSeries([{
      data: new Float32Array([0, 0, 1, 1, 2, 2]),
      segments: [{ offset: 0, count: 3 }],
      color: [0, 0, 1, 1],
      mode: 'scatter',
      size: 3,
    }]);
    r.draw();
    expect(calls.drawArrays).toEqual([[gl.POINTS, 0, 3]]);
    expect(calls.uniform1f).toContainEqual([(3 * 2) / ATLAS_R_FRAC]);   // uSize
    expect(calls.uniform1f).toContainEqual([0]);    // uCell = disc (default marker)
  });

  it('draws a line as LINE_STRIP only — markers come as a separate series', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setSeries([{
      data: new Float32Array([0, 0, 1, 1, 2, 0]),
      segments: [{ offset: 0, count: 3 }],
      color: [1, 0, 0, 1],
      mode: 'line',
      marker: 2,            // even with a marker field, a line draws no POINTS
    }]);
    r.draw();
    expect(calls.drawArrays).toEqual([[gl.LINE_STRIP, 0, 3]]);
  });

  it('draws single-point scatter segments (count >= 1, unlike lines)', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setSeries([{
      data: new Float32Array([0, 0, 5, 5]),
      segments: [{ offset: 0, count: 1 }, { offset: 1, count: 1 }],
      color: [0, 0, 0, 1],
      mode: 'scatter',
    }]);
    r.draw();
    expect(calls.drawArrays).toEqual([[gl.POINTS, 0, 1], [gl.POINTS, 1, 1]]);
  });

  it('sets uFilled from the series fill flag (0 = open default, 1 = filled)', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setSeries([{
      data: new Float32Array([0, 0]), segments: [{ offset: 0, count: 1 }],
      color: [0, 0, 0, 1], mode: 'scatter', marker: 0, filled: true,
    }]);
    r.draw();
    expect(calls.uniform1f).toContainEqual([1]);   // uFilled = 1 (filled row)
  });

  it('uploads the clip disc into uClip — off (0,0,0) by default, set by setClip', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setSeries([{
      data: new Float32Array([0, 0, 1, 1]),
      segments: [{ offset: 0, count: 2 }],
      color: [0, 0, 0, 1],
    }]);
    r.draw();
    expect(calls.uniform3f[0]).toEqual([0, 0, 0]);          // cartesian → no clip
    r.setClip([100, 120, 40]);
    r.draw();
    expect(calls.uniform3f.at(-1)).toEqual([100, 120, 40]); // polar disc
    r.setClip(null);
    r.draw();
    expect(calls.uniform3f.at(-1)).toEqual([0, 0, 0]);      // cleared
  });

  it('frees old buffers when series are replaced and on dispose', () => {
    const { gl, calls } = mockGL();
    const r = new GLPlotRenderer(gl);
    r.setSeries([{ data: new Float32Array([0, 0]), segments: [], color: [0, 0, 0, 1] }]);
    expect(calls.buffers).toBe(1);
    r.setSeries([{ data: new Float32Array([1, 1]), segments: [], color: [0, 0, 0, 1] }]);
    expect(calls.deleted).toBe(1);   // old buffer freed
    expect(calls.buffers).toBe(2);
    r.dispose();
    expect(calls.deleted).toBe(2);
  });
});

describe('displaySize', () => {
  it('scales the CSS box by devicePixelRatio, rounded, min 1', () => {
    expect(displaySize(800, 600, 2)).toEqual({ w: 1600, h: 1200 });
    expect(displaySize(100.4, 50.6, 1)).toEqual({ w: 100, h: 51 });
    expect(displaySize(0, 0, 2)).toEqual({ w: 1, h: 1 });
  });
});
