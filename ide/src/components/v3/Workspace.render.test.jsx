// @vitest-environment jsdom
//
// Render smoke tests for the Variable Editor / struct inspector.
//
// These mount the real components and assert they don't throw on render.
// This is the layer that the pure-logic tests + esbuild parse + Vite
// build all MISS: a dangling reference to a moved/removed identifier is
// a valid parse and bundles fine, but throws a ReferenceError the moment
// the component renders. The MatrixPanel extraction shipped exactly such
// a bug (a stray setActiveCell in VariableEditor's fetch effect) — a
// render smoke would have caught it before it reached a build.

import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, within, fireEvent } from '@testing-library/react';
import { VariableEditor as VE } from './Workspace';
import SyntaxEditor from '../SyntaxEditor';

// jsdom lacks ResizeObserver and a real canvas 2-D context (the editor's
// minimap uses both). Stub them so the component's effects run — these
// are environment gaps, not component bugs.
globalThis.ResizeObserver = globalThis.ResizeObserver
  || class { observe() {} unobserve() {} disconnect() {} };
const NOOP_CTX = new Proxy({}, { get: () => () => {}, set: () => true });
HTMLCanvasElement.prototype.getContext = () => NOOP_CTX;

afterEach(cleanup);

// Minimal engine stub covering every method the editor may call.
function makeEngine(overrides = {}) {
  return {
    getVarData: (name) => ({ name, type: 'double', rows: 2, cols: 2, data: [[1, 2], [3, 4]] }),
    getVarShape: (name) => ({ name, type: 'double', rows: 2, cols: 2, numel: 4 }),
    getVarTile: () => null,
    getVarStats: () => ({ rows: 2, cols: 2, n: 4, min: 1, max: 4, mean: 2.5, hasNaN: false }),
    inspectPath: () => ({ kind: 'matrix', type: 'double', rows: 2, cols: 2, data: [[1, 2], [3, 4]] }),
    execute: vi.fn(() => ({ output: '', error: null })),
    complete: () => [],
    ...overrides,
  };
}

const matrixVar = {
  name: 'M', type: 'double', kind: 'matrix', size: '2×2', bytes: 32,
  data: [[1, 2], [3, 4]], preview: '[2×2 double]', min: 1, max: 4, mean: 2.5,
};
const scalarVar = {
  name: 'x', type: 'double', kind: 'scalar', size: '1×1', bytes: 8,
  data: [[42]], preview: '42', min: 42, max: 42, mean: 42,
};
const structVar = {
  name: 'car', type: 'struct', kind: 'struct', size: '1×1', bytes: 0,
  data: [['<struct>']], preview: '2 fields',
};

describe('VariableEditor — matrix render smoke', () => {
  it('mounts a matrix variable without throwing', () => {
    // Would have thrown ReferenceError: setActiveCell is not defined.
    const { container } = render(
      <VE variable={matrixVar} onClose={() => {}} engine={makeEngine()} />,
    );
    expect(container.querySelector('.ve-window')).toBeTruthy();
    // The MatrixPanel toolbar (notation segmented control) is present.
    expect(container.querySelector('.ve-toolbar')).toBeTruthy();
  });

  it('mounts a scalar variable without throwing', () => {
    const { container } = render(
      <VE variable={scalarVar} onClose={() => {}} engine={makeEngine()} />,
    );
    expect(container.querySelector('.ve-window')).toBeTruthy();
  });

  it('survives a null engine (preview-only fallback)', () => {
    const { container } = render(
      <VE variable={matrixVar} onClose={() => {}} engine={null} />,
    );
    expect(container.querySelector('.ve-window')).toBeTruthy();
  });
});

describe('VariableEditor — struct inspector render smoke', () => {
  it('mounts a single struct (field list) without throwing', () => {
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 1, numel: 1,
        fields: ['hp', 'name'],
        elems: [[
          { type: 'double', size: '1x1', summary: '203', drill: true },
          { type: 'char', size: '1x5', summary: "'turbo'", drill: true },
        ]],
      }),
    });
    const { container } = render(
      <VE variable={structVar} onClose={() => {}} engine={engine} />,
    );
    // Struct layout uses the dedicated window modifier + breadcrumb.
    expect(container.querySelector('.ve-window-struct')).toBeTruthy();
    expect(container.querySelector('.ve-crumbs')).toBeTruthy();
    expect(within(container).getByText('hp')).toBeTruthy();
    // MATLAB-style Field · Value · Size · Class table.
    const table = container.querySelector('.vt-table');
    expect(table).toBeTruthy();
    const heads = [...table.querySelectorAll('thead th')].map((th) => th.textContent);
    expect(heads).toEqual(['Field', 'Value', 'Size', 'Class']);
    // Size + Class cells are populated from the payload.
    expect(within(table).getByText('1x5')).toBeTruthy();   // name's size
    expect(within(table).getByText('char')).toBeTruthy();  // name's class
  });

  it('opens a context menu (Open · Rename · Duplicate · Insert · Delete) on right-click', () => {
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 1, numel: 1,
        fields: ['hp'],
        elems: [[{ type: 'double', size: '1x1', summary: '203', drill: true }]],
      }),
    });
    const { container } = render(
      <VE variable={structVar} onClose={() => {}} engine={engine} />,
    );
    const row = container.querySelector('.vt-table tbody tr');
    expect(row).toBeTruthy();
    fireEvent.contextMenu(row);
    const labels = [...document.querySelectorAll('.ctx-item')].map((b) => b.textContent);
    for (const expected of ['Open', 'Rename', 'Duplicate', 'Insert field', 'Delete']) {
      expect(labels).toContain(expected);
    }
  });

  it('mounts a struct array (element×field table) without throwing', () => {
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 2, numel: 2,
        fields: ['a'],
        elems: [
          [{ type: 'double', size: '1x1', summary: '1', drill: true }],
          [{ type: 'double', size: '1x1', summary: '2', drill: true }],
        ],
      }),
    });
    const { container } = render(
      <VE variable={{ ...structVar, size: '1×2' }} onClose={() => {}} engine={engine} />,
    );
    expect(container.querySelector('.ve-arr-table')).toBeTruthy();
  });

  it('mounts a drilled matrix field (MatrixPanel via inspector) without throwing', () => {
    const engine = makeEngine({
      inspectPath: () => ({ kind: 'matrix', type: 'double', rows: 2, cols: 2, data: [[1, 2], [3, 4]] }),
    });
    const { container } = render(
      <VE variable={structVar} onClose={() => {}} engine={engine} />,
    );
    expect(container.querySelector('.ve-window-struct')).toBeTruthy();
  });

  it('shows the missing-binding message when inspectPath is absent', () => {
    const engine = makeEngine({ inspectPath: undefined });
    const { container } = render(
      <VE variable={structVar} onClose={() => {}} engine={engine} />,
    );
    expect(container.querySelector('.ve-struct-empty')).toBeTruthy();
  });
});

describe('SyntaxEditor — render smoke', () => {
  it('mounts with value/onChange without throwing', () => {
    const { container } = render(
      <SyntaxEditor value={"x = 1\nfor i = 1:3\n  y = i\nend"} onChange={() => {}} />,
    );
    // The textarea is the editing surface.
    expect(container.querySelector('textarea')).toBeTruthy();
  });

  it('mounts with engine (autocomplete enabled) without throwing', () => {
    const { container } = render(
      <SyntaxEditor value={"plot(x)"} onChange={() => {}} engine={makeEngine()} />,
    );
    expect(container.querySelector('textarea')).toBeTruthy();
  });
});
