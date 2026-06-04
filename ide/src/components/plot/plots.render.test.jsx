// @vitest-environment jsdom
//
// Render smoke tests for the big plot components — CompositePlot,
// PolarPlot, SubplotGrid, FigureWindow, and the FiguresPane preview.
// Mount with minimal valid figures and assert no throw. Catches the
// mount-time ReferenceError class (a dangling reference to a moved /
// removed identifier) that parse + build + pure-logic tests all miss.
//
// Composite3DPlot (WebGL) and Sidebar (IndexedDB-backed fs) need heavier
// environment stubs, so they live in their own files:
// Composite3DPlot.render.test.jsx (mocks THREE.WebGLRenderer) and
// Sidebar.render.test.jsx (stubs fetch / tolerates no-IDB).

import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup } from '@testing-library/react';
import CompositePlot from './CompositePlot';
import PolarPlot from './PolarPlot';
import SubplotGrid from './SubplotGrid';
import FigureWindow from './FigureWindow';
import FiguresPane from './FiguresPane';

// jsdom env gaps the plot components touch.
globalThis.ResizeObserver = globalThis.ResizeObserver
  || class { observe() {} unobserve() {} disconnect() {} };
const NOOP_CTX = new Proxy({}, { get: () => () => {}, set: () => true });
HTMLCanvasElement.prototype.getContext = () => NOOP_CTX;

afterEach(cleanup);

const seriesLayer = { kind: 'series', name: 's1', mode: 'line',
  x: [1, 2, 3, 4], y: [2, 4, 3, 8], color: '#7fd99a' };

const compositeFig = {
  kind: 'composite', id: 1, title: 't', xLabel: 'x', yLabel: 'y',
  xRange: [0, 5], yRange: [0, 10],
  grid: 'on', gridMinor: 'off', xscale: 'linear', yscale: 'linear',
  axisMode: '', axisVisible: true, boxOn: true, legend: [],
  layers: [seriesLayer],
};

// loglog figure whose data-padded range dips ≤0 — exercises the log
// clamp path in both the window and the preview.
const logFig = {
  ...compositeFig, id: 4, xscale: 'log', yscale: 'log',
  xRange: [-39, 1040], yRange: [-2, 100],
  layers: [{ ...seriesLayer, x: [1, 10, 100, 1000], y: [1, 5, 50, 90] }],
};

const polarFig = {
  kind: 'polar', id: 2,
  thetaDir: 'counterclockwise', thetaZeroLocation: 'right',
  series: [{ name: 'p', theta: [0, 1, 2, 3], rho: [1, 2, 1.5, 2], color: '#7fd99a' }],
};

const subplotFig = {
  kind: 'subplot', id: 3, title: 'grid', grid: [1, 2],
  cells: [
    { ...compositeFig, id: 31, subplotIndex: 1 },
    { ...compositeFig, id: 32, subplotIndex: 2 },
  ],
};

const noop = () => {};
const mockEngine = {
  getVarData: () => null, getVarShape: () => null,
  getFigureTile: () => null, getFigureDisplayTile: () => null,
  execute: vi.fn(),
};

describe('CompositePlot render smoke', () => {
  it('mounts a linear composite without throwing', () => {
    const { container } = render(
      <CompositePlot figure={compositeFig} width={400} height={300}
        viewport={{ x: [0, 5], y: [0, 10] }} setViewport={noop} interactive={false} />,
    );
    expect(container.querySelector('svg')).toBeTruthy();
  });

  it('mounts a loglog composite (clamped viewport) without throwing', () => {
    const { container } = render(
      <CompositePlot figure={logFig} width={400} height={300}
        viewport={{ x: [0.1, 1040], y: [0.01, 100] }} setViewport={noop}
        xLog yLog interactive={false} />,
    );
    expect(container.querySelector('svg')).toBeTruthy();
  });

  it('connects the line ACROSS a ≤0 point on a log axis (no break, no NaN)', () => {
    // x = -5 can't be plotted on a log axis. MATLAB drops it and joins
    // the neighbours; the path must be one subpath (single M), not a
    // break (M…M…), and must never emit a NaN coordinate.
    const mixedLogFig = {
      ...compositeFig, id: 7, xscale: 'log', yscale: 'log',
      xRange: [1, 100], yRange: [1, 100],
      layers: [{ kind: 'series', name: 's', mode: 'line',
        x: [1, -5, 100], y: [1, 5, 100], color: '#7fd99a' }],
    };
    const { container } = render(
      <CompositePlot figure={mixedLogFig} width={400} height={300}
        viewport={{ x: [1, 100], y: [1, 100] }} setViewport={noop}
        xLog yLog interactive={false} />,
    );
    const path = container.querySelector('path[stroke="#7fd99a"][fill="none"]');
    expect(path).toBeTruthy();
    const d = path.getAttribute('d');
    expect(d).not.toContain('NaN');
    expect((d.match(/M/g) || []).length).toBe(1);   // one subpath = joined
  });

  it('area fill drops a ≤0 point on a log x-axis (no NaN, one subpath)', () => {
    // semilogx area (x log, y linear). The x = -5 vertex must be dropped
    // and connected across — never emitted as a NaN coordinate.
    const areaLogFig = {
      ...compositeFig, id: 8, xscale: 'log', yscale: 'linear',
      xRange: [1, 100], yRange: [0, 100],
      layers: [{ kind: 'series', name: 'a', mode: 'area',
        x: [1, -5, 100], y: [10, 50, 90], color: '#abcdef', baseline: 0 }],
    };
    const { container } = render(
      <CompositePlot figure={areaLogFig} width={400} height={300}
        viewport={{ x: [1, 100], y: [0, 100] }} setViewport={noop}
        xLog interactive={false} />,
    );
    const path = container.querySelector('path[fill="#abcdef"]');
    expect(path).toBeTruthy();
    const d = path.getAttribute('d');
    expect(d).not.toContain('NaN');
    expect((d.match(/M/g) || []).length).toBe(1);   // one filled subpath
  });
});

describe('PolarPlot render smoke', () => {
  it('mounts without throwing', () => {
    const { container } = render(
      <PolarPlot figure={polarFig} width={400} height={300}
        viewport={{ r: [0, 2], theta: [0, 360] }} setViewport={noop} interactive={false} />,
    );
    expect(container.querySelector('svg')).toBeTruthy();
  });
});

describe('SubplotGrid render smoke', () => {
  it('mounts a 1×2 grid without throwing', () => {
    const { container } = render(
      <SubplotGrid figure={subplotFig} width={600} height={300}
        viewport={null} setViewport={noop} interactive={false} engine={mockEngine} />,
    );
    expect(container).toBeTruthy();
  });
});

describe('FigureWindow render smoke', () => {
  it('mounts a composite figure window without throwing', () => {
    const { container } = render(
      <FigureWindow figure={compositeFig} onClose={noop} engine={mockEngine} />,
    );
    expect(container.querySelector('svg')).toBeTruthy();
  });

  it('mounts a polar figure window without throwing', () => {
    const { container } = render(
      <FigureWindow figure={polarFig} onClose={noop} engine={mockEngine} />,
    );
    expect(container.querySelector('svg')).toBeTruthy();
  });
});

describe('FiguresPane preview render smoke', () => {
  it('renders a log-figure preview card without throwing', () => {
    const { container } = render(
      <FiguresPane figures={[logFig]} unsupportedCount={0}
        onExpand={noop} onCloseFigure={noop} onCloseAll={noop} engine={mockEngine} />,
    );
    expect(container.querySelector('.fp-card')).toBeTruthy();
  });
});

// Per-layer-type render net. Each mounts a one-layer figure with a distinctive
// colour and asserts the characteristic SVG glyph for that mode is present.
// This is the safety net for the CompositePlot layer-renderer extraction: if a
// renderer is dropped or mis-wired, its glyph stops appearing and the matching
// test fails (vs. the old smoke tests which only caught mount-time throws).
describe('CompositePlot — every layer type renders its glyph', () => {
  const mountLayer = (layer) => render(
    <CompositePlot figure={{ ...compositeFig, id: 100, layers: [layer] }}
      width={400} height={300} viewport={{ x: [0, 5], y: [0, 10] }}
      setViewport={noop} interactive={false} />,
  ).container;
  const S = { kind: 'series', x: [1, 2, 3], y: [2, 4, 3] };

  it('line → stroked path', () => {
    const c = mountLayer({ ...S, mode: 'line', color: '#c10001' });
    expect(c.querySelector('path[stroke="#c10001"][fill="none"]')).toBeTruthy();
  });
  it('stairs → stroked path', () => {
    const c = mountLayer({ ...S, mode: 'stairs', color: '#c10002' });
    expect(c.querySelector('path[stroke="#c10002"]')).toBeTruthy();
  });
  it('scatter → marker circles', () => {
    const c = mountLayer({ ...S, mode: 'scatter', marker: 'o', color: '#c10003' });
    expect(c.querySelector('circle[stroke="#c10003"]')).toBeTruthy();
  });
  it('stem → stems + dots', () => {
    const c = mountLayer({ ...S, mode: 'stem', color: '#c10004' });
    expect(c.querySelector('line[stroke="#c10004"]')).toBeTruthy();
    expect(c.querySelector('circle[fill="#c10004"]')).toBeTruthy();
  });
  it('bar → filled rects', () => {
    const c = mountLayer({ ...S, mode: 'bar', color: '#c10005' });
    expect(c.querySelector('rect[fill="#c10005"]')).toBeTruthy();
  });
  it('barh → filled rects', () => {
    const c = mountLayer({ ...S, mode: 'barh', x: [2, 4, 3], y: [1, 2, 3], color: '#c10006' });
    expect(c.querySelector('rect[fill="#c10006"]')).toBeTruthy();
  });
  it('area → filled path', () => {
    const c = mountLayer({ ...S, mode: 'area', baseline: 0, color: '#c10007' });
    expect(c.querySelector('path[fill="#c10007"]')).toBeTruthy();
  });
  it('polygon → filled path', () => {
    const c = mountLayer({ ...S, mode: 'polygon', x: [1, 2, 3], y: [1, 3, 1], color: '#c10008' });
    expect(c.querySelector('path[fill="#c10008"]')).toBeTruthy();
  });
  it('quiver → arrow lines', () => {
    const c = mountLayer({ ...S, mode: 'quiver', x: [1, 2], y: [1, 2], u: [1, 1], v: [1, -1], color: '#c10009' });
    expect(c.querySelector('line[stroke="#c10009"]')).toBeTruthy();
  });
  it('errorbar → bars + dot', () => {
    const c = mountLayer({ ...S, mode: 'errorbar', eNeg: [0.5, 0.5, 0.5], ePos: [0.5, 0.5, 0.5], color: '#c1000a' });
    expect(c.querySelector('line[stroke="#c1000a"]')).toBeTruthy();
    expect(c.querySelector('circle[fill="#c1000a"]')).toBeTruthy();
  });
  it('xline → vertical reference line', () => {
    const c = mountLayer({ ...S, mode: 'xline', x: [2], color: '#c1000b' });
    expect(c.querySelector('line[stroke="#c1000b"]')).toBeTruthy();
  });
  it('yline → horizontal reference line', () => {
    const c = mountLayer({ ...S, mode: 'yline', x: [5], color: '#c1000c' });
    expect(c.querySelector('line[stroke="#c1000c"]')).toBeTruthy();
  });
  it('text layer → svg <text> with content', () => {
    const c = mountLayer({ kind: 'text', x: 2, y: 5, text: 'GLYPHTEST', color: '#c1000d' });
    expect(c.querySelector('text')).toBeTruthy();
    expect(c.textContent).toContain('GLYPHTEST');
  });
});
