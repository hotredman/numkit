import { describe, it, expect } from 'vitest';
import { GLPlotRenderer } from './GLPlotRenderer';
import { displaySize } from './glcontext';

// Minimal mock WebGL2 context: returns success for all setup queries and
// records the draw/uniform calls we assert on. Lets us unit-test the
// renderer's orchestration without a real GPU (jsdom has no WebGL).
function mockGL() {
  const calls = { drawArrays: [], uniform4f: [], uniform2f: [], buffers: 0, deleted: 0 };
  const gl = {
    VERTEX_SHADER: 1, FRAGMENT_SHADER: 2, COMPILE_STATUS: 3, LINK_STATUS: 4,
    ARRAY_BUFFER: 5, STATIC_DRAW: 6, FLOAT: 7, LINE_STRIP: 8,
    COLOR_BUFFER_BIT: 9, BLEND: 10, SRC_ALPHA: 11, ONE_MINUS_SRC_ALPHA: 12,
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
    r.setSeries([]);
    r.draw();
    expect(calls.uniform4f[0]).toEqual([2, -1, 3, -0.5]);
    expect(calls.uniform2f[0]).toEqual([1, 0]);   // xLog on, yLog off
  });

  it('skips degenerate segments (< 2 vertices)', () => {
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
