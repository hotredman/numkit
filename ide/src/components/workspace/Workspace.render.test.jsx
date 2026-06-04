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
import { VariableEditor as VE, MatrixPanel } from './Workspace';
import SyntaxEditor from '../editor/SyntaxEditor';

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

describe('MatrixPanel — 3-D / N-D slice navigator', () => {
  const baseProps = {
    rows: 2, cols: 2, name: 'V', type: 'double',
    getCellValue: (r, c) => r * 2 + c, getSlice: () => [], stats: null,
  };

  it('renders no navigator for a 2-D array', () => {
    const { container } = render(
      <MatrixPanel {...baseProps} dims={[2, 2]} page={0} setPage={() => {}} />,
    );
    expect(container.querySelector('.ve-slice-nav')).toBeNull();
  });

  it('renders one spinner for a 3-D array and steps the page on ▶', () => {
    const setPage = vi.fn();
    const { container } = render(
      <MatrixPanel {...baseProps} dims={[2, 2, 3]} pages={3} page={0} setPage={setPage} />,
    );
    const nav = container.querySelector('.ve-slice-nav');
    expect(nav).toBeTruthy();
    const input = nav.querySelector('.ve-slice-input');
    expect(input.value).toBe('1');         // 1-based slice index
    expect(nav.textContent).toContain('/3');
    // Two arrows: [‹ prev, › next]. Next → page 1.
    const arrows = nav.querySelectorAll('.ve-slice-arrow');
    expect(arrows[0].disabled).toBe(true);  // prev disabled on slice 1
    fireEvent.click(arrows[1]);
    expect(setPage).toHaveBeenCalledWith(1);
  });

  it('renders one spinner per dimension ≥3 for an N-D array', () => {
    const { container } = render(
      <MatrixPanel {...baseProps} dims={[2, 2, 3, 4]} pages={12} page={0} setPage={() => {}} />,
    );
    expect(container.querySelectorAll('.ve-slice-input').length).toBe(2);
  });

  it('typing a slice number jumps to that page', () => {
    const setPage = vi.fn();
    const { container } = render(
      <MatrixPanel {...baseProps} dims={[2, 2, 5]} pages={5} page={0} setPage={setPage} />,
    );
    const input = container.querySelector('.ve-slice-input');
    fireEvent.change(input, { target: { value: '4' } });
    expect(setPage).toHaveBeenCalledWith(3);   // 1-based 4 → 0-based page 3
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
    // MATLAB-style Field · Value · Size · Bytes · Class table.
    const table = container.querySelector('.vt-table');
    expect(table).toBeTruthy();
    const heads = [...table.querySelectorAll('thead th')].map((th) => th.textContent);
    expect(heads).toEqual(['Field', 'Value', 'Size', 'Bytes', 'Class']);
    // Size + Class cells are populated from the payload.
    expect(within(table).getByText('1x5')).toBeTruthy();   // name's size
    expect(within(table).getByText('char')).toBeTruthy();  // name's class
  });

  it('populates the Bytes column from each field cell.bytes (parity with the Workspace list)', () => {
    // Regression: the struct inspector reused the shared ValueTable (which
    // renders row.bytes) but dropped cell.bytes when building rows, so the
    // Bytes column was always blank here even though the Workspace list had
    // it. Each field must show its own rawBytes, human-formatted.
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 1, numel: 1,
        fields: ['hp', 'name'],
        elems: [[
          { type: 'double', size: '1x1', summary: '203', drill: true, bytes: 8 },
          { type: 'char', size: '1x5', summary: "'turbo'", drill: true, bytes: 10 },
        ]],
      }),
    });
    const { container } = render(
      <VE variable={structVar} onClose={() => {}} engine={engine} />,
    );
    const table = container.querySelector('.vt-table');
    expect(within(table).getByText('8 B')).toBeTruthy();    // hp
    expect(within(table).getByText('10 B')).toBeTruthy();   // name
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

  it('field list has the unified browser toolbar (filter · Σ▾ · view) and filters fields', () => {
    try { localStorage.setItem('numkit.ide.struct.view', 'list'); } catch { /* none */ }
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 1, numel: 1,
        fields: ['alpha', 'beta', 'gamma'],
        elems: [[
          { type: 'double', size: '1x1', summary: '1', drill: true },
          { type: 'double', size: '1x1', summary: '2', drill: true },
          { type: 'char', size: '1x3', summary: "'xyz'", drill: true },
        ]],
      }),
    });
    const { container } = render(<VE variable={structVar} onClose={() => {}} engine={engine} />);
    expect(container.querySelector('.entity-browser')).toBeTruthy();
    expect(container.querySelector('.ws-search input')).toBeTruthy();
    expect(container.querySelector('.ws-cols-btn')).toBeTruthy();   // Σ▾ in list view
    const names = () => [...container.querySelectorAll('.vt-name')].map((e) => e.textContent);
    expect(names()).toEqual(['alpha', 'beta', 'gamma']);
    fireEvent.change(container.querySelector('.ws-search input'), { target: { value: 'bet' } });
    expect(names()).toEqual(['beta']);
  });

  it('field list cards view renders a card per field', () => {
    try { localStorage.setItem('numkit.ide.struct.view', 'cards'); } catch { /* none */ }
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 1, numel: 1,
        fields: ['a', 'b'],
        elems: [[
          { type: 'double', size: '1x1', summary: '1', drill: true },
          { type: 'double', size: '1x1', summary: '2', drill: true },
        ]],
      }),
    });
    const { container } = render(<VE variable={structVar} onClose={() => {}} engine={engine} />);
    expect(container.querySelectorAll('.var-card').length).toBe(2);
    try { localStorage.setItem('numkit.ide.struct.view', 'list'); } catch { /* none */ }  // restore
  });

  it('drops the inline "+ new field" row; right-click the table area opens Insert field', () => {
    try { localStorage.setItem('numkit.ide.struct.view', 'list'); } catch { /* none */ }
    const engine = makeEngine({
      inspectPath: () => ({
        kind: 'struct', rows: 1, cols: 1, numel: 1,
        fields: ['hp'],
        elems: [[{ type: 'double', size: '1x1', summary: '1', drill: true }]],
      }),
    });
    const { container } = render(<VE variable={structVar} onClose={() => {}} engine={engine} />);
    // The inline add-field control is gone.
    expect(container.querySelector('.ve-addfield')).toBeNull();
    // Right-clicking the table area (not a row) opens the field-agnostic menu.
    fireEvent.contextMenu(container.querySelector('.ws-list'));
    const labels = [...document.querySelectorAll('.ctx-item')].map((b) => b.textContent);
    expect(labels).toContain('Insert field');
    expect(labels).not.toContain('Rename');   // no field target → row actions absent
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
