// @vitest-environment jsdom
//
// Render smoke for the 3-D figure renderer. jsdom has no WebGL context,
// so a real THREE.WebGLRenderer constructor throws on mount. We mock
// ONLY WebGLRenderer (a tiny no-op stub) and keep every other THREE
// export real — so the refactor-fragile code actually runs: computeBBox,
// computeScales, buildSurfaceMesh / buildVertices / buildLineSegments,
// buildAxesFrame, and the real OrbitControls + CSS2DRenderer addons
// (both DOM-only, jsdom-safe). Catches the mount-time ReferenceError
// class (a dangling reference to a moved / renamed identifier) that
// parse + build + pure-logic tests all miss.

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

// Stub WebGLRenderer only. The component reads back a real <canvas> ref
// (not renderer.domElement) for OrbitControls, so the fake just needs
// the handful of methods Composite3DPlot calls on it.
vi.mock('three', async (importActual) => {
  const actual = await importActual();
  class FakeWebGLRenderer {
    constructor(opts = {}) { this.domElement = opts.canvas || null; }
    setPixelRatio() {}
    getPixelRatio() { return 1; }
    setClearColor() {}
    setSize() {}
    setAnimationLoop() {}
    render() {}
    dispose() {}
  }
  return { ...actual, WebGLRenderer: FakeWebGLRenderer };
});

import { render, cleanup } from '@testing-library/react';
import Composite3DPlot from './Composite3DPlot';

beforeEach(() => {
  // Deterministic, loop-free: never fire the animation tick. The
  // geometry-build effect runs synchronously on mount regardless, so
  // the meaningful code path is still exercised.
  globalThis.requestAnimationFrame = () => 0;
  globalThis.cancelAnimationFrame = () => {};
  globalThis.ResizeObserver = globalThis.ResizeObserver
    || class { observe() {} unobserve() {} disconnect() {} };
});
afterEach(cleanup);

const surfaceFig = {
  kind: 'composite3d', title: 'surf', xLabel: 'x', yLabel: 'y', zLabel: 'z',
  axisMode: '', grid: 'on', gridMinor: 'off', view: [-37.5, 30],
  xlim: null, ylim: null, zlim: null,
  layers: [{
    kind: 'series', mode: 'surface', color: '#3a7',
    xRaw: [0, 1, 0, 1], yRaw: [0, 0, 1, 1], z: [0, 1, 1, 2],
    surfaceGrid: { Xs: [0, 1], Ys: [0, 1], Z: [[0, 1], [1, 2]] },
  }],
};

const line3Fig = {
  kind: 'composite3d', title: 'line3', xLabel: 'x', yLabel: 'y', zLabel: 'z',
  axisMode: '', grid: 'off', gridMinor: 'off', view: [-37.5, 30],
  layers: [{
    kind: 'series', mode: 'line', color: '#1f77b4',
    xRaw: [0, 1, 2, 3], yRaw: [0, 1, 0, 1], z: [0, 1, 2, 3],
  }],
};

const scatter3Fig = {
  kind: 'composite3d', title: 's3', xLabel: '', yLabel: '', zLabel: '',
  axisMode: 'equal', grid: 'on', gridMinor: 'on', view: [30, 45],
  layers: [{
    kind: 'series', mode: 'scatter', color: '#d62728', size: 4,
    xRaw: [0, 1, 2], yRaw: [2, 0, 1], z: [1, 2, 0],
  }],
};

describe('Composite3DPlot render smoke', () => {
  it('mounts a surface figure without throwing', () => {
    const { container } = render(
      <Composite3DPlot figure={surfaceFig} width={400} height={300} interactive={false} />,
    );
    expect(container.querySelector('canvas')).toBeTruthy();
  });

  it('mounts a line3 figure without throwing', () => {
    const { container } = render(
      <Composite3DPlot figure={line3Fig} width={400} height={300} interactive />,
    );
    expect(container.querySelector('canvas')).toBeTruthy();
  });

  it('mounts a scatter3 figure (axis equal, minor grid) without throwing', () => {
    const { container } = render(
      <Composite3DPlot figure={scatter3Fig} width={320} height={240} interactive />,
    );
    expect(container.querySelector('canvas')).toBeTruthy();
  });

  it('mounts with a null figure (empty state) without throwing', () => {
    const { container } = render(
      <Composite3DPlot figure={null} width={320} height={240} />,
    );
    expect(container.querySelector('canvas')).toBeTruthy();
  });
});
